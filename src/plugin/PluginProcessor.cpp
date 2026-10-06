#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <limits>
#include <random>

namespace {
juce::NormalisableRange<float> skewed(float lo,float hi,float centre){ juce::NormalisableRange<float> r(lo,hi); r.setSkewForCentre(centre); return r; }
}

BackReverseAudioProcessor::BackReverseAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  state(*this,nullptr,"STATE",makeLayout()), engine(br::EngineConfig{}) {
    formatManager.registerBasicFormats();
    gateMask.assign(16,true); gateForward.assign(16,false);
}

juce::AudioProcessorValueTreeState::ParameterLayout BackReverseAudioProcessor::makeLayout(){
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("chunkSeconds","Chunk",skewed(0.001f,600.0f,1.0f),5.0f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("reverseMode","Reverse Mode",juce::StringArray{"Sequential","Whole Source","Reordered","Free Scrub","Hybrid"},0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("orderMode","Order",juce::StringArray{"Sequential","Reverse Order","Random","Shuffle","Ping Pong","Odds Evens","Evens Odds","Rotate Left","Rotate Right","User"},0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("temporalMode","Time Mode",juce::StringArray{"Rate","Time Stretch"},0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ratio","Speed / Stretch",skewed(0.05f,8.0f,1.0f),1.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("preservePitch","Preserve Pitch",true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("pan","Pan",-1.0f,1.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("swapStereo","Swap L/R",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("polarity","Polarity",juce::StringArray{"Normal","Invert L","Invert R","Invert Both"},0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("phase","Phase Rotation",-179.0f,179.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("dry","Dry",0.0f,1.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("wet","Wet",0.0f,1.0f,1.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("stutterOn","Stutter",false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stutterMs","Stutter Period",10.0f,1000.0f,120.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stutterWet","Stutter Wet",0.0f,1.0f,0.7f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("delayOn","Delay",false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayMs","Delay Time",1.0f,2000.0f,250.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayFeedback","Delay Feedback",-0.95f,0.95f,0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayWet","Delay Wet",0.0f,1.0f,0.25f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("echoOn","Echo",false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoMs","Echo Time",10.0f,4000.0f,375.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoFeedback","Echo Feedback",-0.95f,0.95f,0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoDamping","Echo Damping",0.0f,0.99f,0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoWet","Echo Wet",0.0f,1.0f,0.25f));
    return {p.begin(),p.end()};
}

bool BackReverseAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto out=l.getMainOutputChannelSet(); if(out!=juce::AudioChannelSet::mono()&&out!=juce::AudioChannelSet::stereo()) return false; return l.getMainInputChannelSet()==out;
}

void BackReverseAudioProcessor::prepareToPlay(double sr,int samplesPerBlock){
    const std::size_t channels=static_cast<std::size_t>(std::max(1,getTotalNumOutputChannels()));
    engine.prepare(sr,channels,static_cast<std::size_t>(sr*600.0));
    const std::size_t scratchFrames=std::max<std::size_t>(65536,static_cast<std::size_t>(std::max(1,samplesPerBlock)));
    inScratch.assign(scratchFrames*channels,0.0f); outScratch.assign(scratchFrames*channels,0.0f);
    params.gates.assign(16,br::GateStep{});
    syncEngineFromParameters();
}

void BackReverseAudioProcessor::syncEngineFromParameters(){
    const double sr=std::max(1.0,getSampleRate());
    params.durationSeconds=*state.getRawParameterValue("chunkSeconds");
    params.temporalMode=*state.getRawParameterValue("temporalMode")<0.5f?br::TemporalMode::Rate:br::TemporalMode::TimeStretch;
    params.ratio=*state.getRawParameterValue("ratio"); params.preservePitch=*state.getRawParameterValue("preservePitch")>0.5f;
    params.pan=*state.getRawParameterValue("pan"); params.swapStereo=*state.getRawParameterValue("swapStereo")>0.5f;
    int pol=static_cast<int>(*state.getRawParameterValue("polarity")); params.invertLeft=(pol==1||pol==3);params.invertRight=(pol==2||pol==3);params.phaseDegrees=*state.getRawParameterValue("phase");
    if(params.gates.size()!=gateMask.size()) params.gates.assign(gateMask.size(),br::GateStep{});
    for(std::size_t i=0;i<gateMask.size();++i){ auto& g=params.gates[i]; g.enabled=gateMask[i];g.shape=br::GateShape::EqualPower;g.width=0.88f;g.gap=0.12f;g.direction=gateForward[i]?br::GateDirection::ForceForward:br::GateDirection::Inherit; }
    engine.setChunkParams(params);
    engine.setReverseMode(static_cast<br::ReverseMode>(static_cast<int>(*state.getRawParameterValue("reverseMode"))));
    engine.setOrderMode(static_cast<br::OrderMode>(static_cast<int>(*state.getRawParameterValue("orderMode")))); engine.setRandomSeed(seed);
    engine.setDryWet(*state.getRawParameterValue("dry"),*state.getRawParameterValue("wet"));
    fx.stutter.enabled=*state.getRawParameterValue("stutterOn")>0.5f; fx.stutter.periodFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("stutterMs")/1000.0));fx.stutter.repeatFrames=std::max<std::size_t>(1,fx.stutter.periodFrames/2);fx.stutter.wet=*state.getRawParameterValue("stutterWet");fx.stutter.dry=1.0f-fx.stutter.wet;fx.stutter.alternateDirection=true;
    fx.delay.enabled=*state.getRawParameterValue("delayOn")>0.5f;fx.delay.delayFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("delayMs")/1000.0));fx.delay.feedback=*state.getRawParameterValue("delayFeedback");fx.delay.wet=*state.getRawParameterValue("delayWet");fx.delay.dry=1.0f;fx.delay.pingPong=true;
    fx.echo.enabled=*state.getRawParameterValue("echoOn")>0.5f;fx.echo.delayFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("echoMs")/1000.0));fx.echo.feedback=*state.getRawParameterValue("echoFeedback");fx.echo.damping=*state.getRawParameterValue("echoDamping");fx.echo.wet=*state.getRawParameterValue("echoWet");fx.echo.dry=1.0f;fx.echo.drift=0.35f;fx.echo.wowFlutter=0.25f;
    engine.setEffects(fx);
    setLatencySamples(static_cast<int>(std::min<std::size_t>(engine.latencyFrames(),static_cast<std::size_t>(std::numeric_limits<int>::max()))));
}

void BackReverseAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&){
    juce::ScopedNoDenormals noDenormals; syncEngineFromParameters();
    const int channels=buffer.getNumChannels(),totalFrames=buffer.getNumSamples(); if(channels<=0||totalFrames<=0)return;
    const std::size_t capFrames=inScratch.size()/static_cast<std::size_t>(channels);
    int offset=0;
    while(offset<totalFrames){
        const int frames=std::min<int>(totalFrames-offset,static_cast<int>(capFrames));
        const std::size_t samples=static_cast<std::size_t>(frames*channels);
        for(int f=0;f<frames;++f)for(int ch=0;ch<channels;++ch){
            float v=buffer.getSample(ch,offset+f);
            if(fileActive.load()&&filePlaying.load()&&loadedFrames>0){
                std::size_t pos=fileFrame.load(); if(pos>=loadedFrames){pos=0;fileFrame.store(0);}
                int sourceCh=std::min(ch,std::max(0,loadedChannels-1));
                const int rm=static_cast<int>(*state.getRawParameterValue("reverseMode"));
                const std::size_t srcPos=(rm==1)?(loadedFrames-1-pos):pos;
                v=loadedInterleaved[srcPos*static_cast<std::size_t>(loadedChannels)+static_cast<std::size_t>(sourceCh)];
                if(ch==channels-1) fileFrame.store((pos+1)%loadedFrames);
            }
            inScratch[static_cast<std::size_t>(f*channels+ch)]=v;
        }
        if(fileActive.load()&&static_cast<int>(*state.getRawParameterValue("reverseMode"))==1){
            for(std::size_t i=0;i<samples;++i) outScratch[i]=inScratch[i];
        }else{
            engine.processLive(std::span<const float>(inScratch.data(),samples),std::span<float>(outScratch.data(),samples));
        }
        for(int f=0;f<frames;++f)for(int ch=0;ch<channels;++ch)buffer.setSample(ch,offset+f,outScratch[static_cast<std::size_t>(f*channels+ch)]);
        offset+=frames;
    }
}

bool BackReverseAudioProcessor::loadAudioFile(const juce::File& file){
    std::unique_ptr<juce::AudioFormatReader> r(formatManager.createReaderFor(file)); if(!r)return false;
    if(r->lengthInSamples<=0||r->lengthInSamples>std::numeric_limits<int>::max())return false;
    juce::AudioBuffer<float> tmp(static_cast<int>(r->numChannels),static_cast<int>(r->lengthInSamples)); if(!r->read(&tmp,0,tmp.getNumSamples(),0,true,true))return false;
    loadedChannels=tmp.getNumChannels();loadedFrames=static_cast<std::size_t>(tmp.getNumSamples());loadedSampleRate=r->sampleRate;loadedInterleaved.resize(loadedFrames*static_cast<std::size_t>(loadedChannels));
    for(std::size_t f=0;f<loadedFrames;++f)for(int ch=0;ch<loadedChannels;++ch)loadedInterleaved[f*static_cast<std::size_t>(loadedChannels)+static_cast<std::size_t>(ch)]=tmp.getSample(ch,static_cast<int>(f));
    fileFrame.store(0);fileActive.store(true);filePlaying.store(true);engine.reset();return true;
}
void BackReverseAudioProcessor::unloadAudioFile(){fileActive.store(false);filePlaying.store(false);loadedInterleaved.clear();loadedFrames=0;fileFrame.store(0);engine.reset();}
void BackReverseAudioProcessor::seekFile(double n){if(loadedFrames){fileFrame.store(std::min<std::size_t>(loadedFrames-1,static_cast<std::size_t>(std::clamp(n,0.0,1.0)*(loadedFrames-1))));engine.reset();}}
double BackReverseAudioProcessor::filePlayheadNormalized()const noexcept{return loadedFrames?static_cast<double>(fileFrame.load())/loadedFrames:0.0;}
double BackReverseAudioProcessor::fileLengthSeconds()const noexcept{return loadedSampleRate>0?loadedFrames/loadedSampleRate:0.0;}
void BackReverseAudioProcessor::setGateEnabled(int i,bool e){if(i>=0&&i<(int)gateMask.size()){gateMask[(std::size_t)i]=e;state.state.setProperty("gateMask"+juce::String(i),e,nullptr);}}
bool BackReverseAudioProcessor::gateEnabled(int i)const{return i>=0&&i<(int)gateMask.size()?gateMask[(std::size_t)i]:false;}
void BackReverseAudioProcessor::setGateForward(int i,bool e){if(i>=0&&i<(int)gateForward.size()){gateForward[(std::size_t)i]=e;state.state.setProperty("gateForward"+juce::String(i),e,nullptr);}}
void BackReverseAudioProcessor::randomize(std::uint64_t s){seed=s;std::mt19937_64 r(s);for(std::size_t i=0;i<gateMask.size();++i){setGateEnabled((int)i,(r()%100)>20);setGateForward((int)i,(r()%100)>70);} if(auto* q=state.getParameter("ratio")){const float plain=0.25f+static_cast<float>(r()%1200)/200.0f;q->setValueNotifyingHost(q->convertTo0to1(plain));}state.state.setProperty("seed",(juce::int64)seed,nullptr);}
void BackReverseAudioProcessor::getStateInformation(juce::MemoryBlock& d){state.state.setProperty("schemaVersion",1,nullptr);state.state.setProperty("seed",(juce::int64)seed,nullptr);auto xml=state.copyState().createXml();copyXmlToBinary(*xml,d);}
void BackReverseAudioProcessor::setStateInformation(const void* d,int n){if(auto xml=getXmlFromBinary(d,n)){auto v=juce::ValueTree::fromXml(*xml);if(v.isValid())state.replaceState(v);}seed=(std::uint64_t)(juce::int64)state.state.getProperty("seed",(juce::int64)0xBACC0FFEEULL);for(int i=0;i<(int)gateMask.size();++i){gateMask[(std::size_t)i]=(bool)state.state.getProperty("gateMask"+juce::String(i),true);gateForward[(std::size_t)i]=(bool)state.state.getProperty("gateForward"+juce::String(i),false);}syncEngineFromParameters();}
juce::AudioProcessorEditor* BackReverseAudioProcessor::createEditor(){return new BackReverseAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new BackReverseAudioProcessor();}
