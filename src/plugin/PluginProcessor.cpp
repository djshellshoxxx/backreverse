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
    gateMask.assign(64,true); gateForward.assign(64,false);
}

juce::AudioProcessorValueTreeState::ParameterLayout BackReverseAudioProcessor::makeLayout(){
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("chunkSeconds","Chunk",skewed(0.001f,600.0f,1.0f),5.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("hostSync","Host Sync",false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("syncDivision","Chunk Division",juce::StringArray{"1/128","1/64","1/32","1/16T","1/16","1/8T","1/8","1/8.","1/4T","1/4","1/4.","1/2","1 bar","2 bars","4 bars","8 bars","16 bars","32 bars"},9));
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

    p.push_back(std::make_unique<juce::AudioParameterChoice>("gateSteps","Gate Steps",juce::StringArray{"2","4","8","16","32","64"},3));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("gateShape","Gate Shape",juce::StringArray{"Hard","Linear In","Linear Out","Triangle","Equal Power","Sine","Exponential","Logarithmic"},4));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("gateWidth","Gate Width",0.01f,1.0f,0.88f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("gateGap","Gate Gap",0.0f,0.99f,0.12f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("gapMode","Gap Mode",juce::StringArray{"Silence","Dry Through","Hold","Crossfade","FX Tail"},0));

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
    p.push_back(std::make_unique<juce::AudioParameterChoice>("fxOrder","FX Order",juce::StringArray{"Stutter > Delay > Echo","Stutter > Echo > Delay","Delay > Stutter > Echo","Delay > Echo > Stutter","Echo > Stutter > Delay","Echo > Delay > Stutter"},0));
    return {p.begin(),p.end()};
}

double BackReverseAudioProcessor::beatsForDivision(int i){
    static constexpr double v[]={0.03125,0.0625,0.125,1.0/6.0,0.25,1.0/3.0,0.5,0.75,2.0/3.0,1.0,1.5,2.0,4.0,8.0,16.0,32.0,64.0,128.0};
    return v[std::clamp(i,0,17)];
}

bool BackReverseAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto out=l.getMainOutputChannelSet(); if(out!=juce::AudioChannelSet::mono()&&out!=juce::AudioChannelSet::stereo()) return false; return l.getMainInputChannelSet()==out;
}

void BackReverseAudioProcessor::prepareToPlay(double sr,int samplesPerBlock){
    const std::size_t channels=static_cast<std::size_t>(std::max(1,getTotalNumOutputChannels()));
    engine.prepare(sr,channels,static_cast<std::size_t>(sr*600.0));
    const std::size_t scratchFrames=std::max<std::size_t>(65536,static_cast<std::size_t>(std::max(1,samplesPerBlock)));
    inScratch.assign(scratchFrames*channels,0.0f); outScratch.assign(scratchFrames*channels,0.0f);
    params.gates.assign(64,br::GateStep{});
    syncEngineFromParameters();
}

int BackReverseAudioProcessor::activeGateCount() const {
    static constexpr int counts[]={2,4,8,16,32,64};
    int idx=static_cast<int>(*state.getRawParameterValue("gateSteps"));
    return counts[std::clamp(idx,0,5)];
}

void BackReverseAudioProcessor::syncEngineFromParameters(){
    const double sr=std::max(1.0,getSampleRate());
    const bool sync=*state.getRawParameterValue("hostSync")>0.5f;
    if(sync){
        const double bpm=std::clamp(lastBpm.load(),20.0,400.0);
        params.durationSeconds=(60.0/bpm)*beatsForDivision(static_cast<int>(*state.getRawParameterValue("syncDivision")));
    } else params.durationSeconds=*state.getRawParameterValue("chunkSeconds");

    params.temporalMode=*state.getRawParameterValue("temporalMode")<0.5f?br::TemporalMode::Rate:br::TemporalMode::TimeStretch;
    params.ratio=*state.getRawParameterValue("ratio"); params.preservePitch=*state.getRawParameterValue("preservePitch")>0.5f;
    params.pan=*state.getRawParameterValue("pan"); params.swapStereo=*state.getRawParameterValue("swapStereo")>0.5f;
    int pol=static_cast<int>(*state.getRawParameterValue("polarity")); params.invertLeft=(pol==1||pol==3);params.invertRight=(pol==2||pol==3);params.phaseDegrees=*state.getRawParameterValue("phase");

    const int gateCount=activeGateCount();
    params.gates.resize(static_cast<std::size_t>(gateCount));
    const auto shape=static_cast<br::GateShape>(std::clamp(static_cast<int>(*state.getRawParameterValue("gateShape")),0,7));
    const auto gapMode=static_cast<br::GapMode>(std::clamp(static_cast<int>(*state.getRawParameterValue("gapMode")),0,4));
    const float width=*state.getRawParameterValue("gateWidth");
    const float gap=*state.getRawParameterValue("gateGap");
    for(int i=0;i<gateCount;++i){ auto& g=params.gates[(std::size_t)i]; g.enabled=gateMask[(std::size_t)i];g.shape=shape;g.width=width;g.gap=gap;g.gapMode=gapMode;g.direction=gateForward[(std::size_t)i]?br::GateDirection::ForceForward:br::GateDirection::Inherit; }

    engine.setChunkParams(params);
    engine.setReverseMode(static_cast<br::ReverseMode>(static_cast<int>(*state.getRawParameterValue("reverseMode"))));
    engine.setOrderMode(static_cast<br::OrderMode>(static_cast<int>(*state.getRawParameterValue("orderMode")))); engine.setRandomSeed(seed);
    engine.setDryWet(*state.getRawParameterValue("dry"),*state.getRawParameterValue("wet"));

    fx.stutter.enabled=*state.getRawParameterValue("stutterOn")>0.5f; fx.stutter.periodFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("stutterMs")/1000.0));fx.stutter.repeatFrames=std::max<std::size_t>(1,fx.stutter.periodFrames/2);fx.stutter.wet=*state.getRawParameterValue("stutterWet");fx.stutter.dry=1.0f-fx.stutter.wet;fx.stutter.alternateDirection=true;
    fx.delay.enabled=*state.getRawParameterValue("delayOn")>0.5f;fx.delay.delayFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("delayMs")/1000.0));fx.delay.feedback=*state.getRawParameterValue("delayFeedback");fx.delay.wet=*state.getRawParameterValue("delayWet");fx.delay.dry=1.0f;fx.delay.pingPong=true;
    fx.echo.enabled=*state.getRawParameterValue("echoOn")>0.5f;fx.echo.delayFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("echoMs")/1000.0));fx.echo.feedback=*state.getRawParameterValue("echoFeedback");fx.echo.damping=*state.getRawParameterValue("echoDamping");fx.echo.wet=*state.getRawParameterValue("echoWet");fx.echo.dry=1.0f;fx.echo.drift=0.35f;fx.echo.wowFlutter=0.25f;
    const int order=static_cast<int>(*state.getRawParameterValue("fxOrder"));
    static constexpr std::array<std::array<br::EffectType,3>,6> orders={{
        {br::EffectType::Stutter,br::EffectType::Delay,br::EffectType::Echo},
        {br::EffectType::Stutter,br::EffectType::Echo,br::EffectType::Delay},
        {br::EffectType::Delay,br::EffectType::Stutter,br::EffectType::Echo},
        {br::EffectType::Delay,br::EffectType::Echo,br::EffectType::Stutter},
        {br::EffectType::Echo,br::EffectType::Stutter,br::EffectType::Delay},
        {br::EffectType::Echo,br::EffectType::Delay,br::EffectType::Stutter}
    }};
    fx.chain=orders[(std::size_t)std::clamp(order,0,5)];
    engine.setEffects(fx);
    setLatencySamples(static_cast<int>(std::min<std::size_t>(engine.latencyFrames(),static_cast<std::size_t>(std::numeric_limits<int>::max()))));
}

void BackReverseAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&){
    juce::ScopedNoDenormals noDenormals;
    if(auto* ph=getPlayHead()){
        if(auto pos=ph->getPosition()) if(auto bpm=pos->getBpm()) lastBpm.store(*bpm);
    }
    syncEngineFromParameters();
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
        }else engine.processLive(std::span<const float>(inScratch.data(),samples),std::span<float>(outScratch.data(),samples));
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
    waveformPeaks.assign(1024,0.0f);
    for(std::size_t b=0;b<waveformPeaks.size();++b){
        const std::size_t a=b*loadedFrames/waveformPeaks.size(), z=std::max(a+1,(b+1)*loadedFrames/waveformPeaks.size());
        float peak=0; for(std::size_t f=a;f<std::min(z,loadedFrames);++f) for(int ch=0;ch<loadedChannels;++ch) peak=std::max(peak,std::abs(loadedInterleaved[f*(std::size_t)loadedChannels+(std::size_t)ch]));
        waveformPeaks[b]=peak;
    }
    fileFrame.store(0);fileActive.store(true);filePlaying.store(true);engine.reset();return true;
}
void BackReverseAudioProcessor::unloadAudioFile(){fileActive.store(false);filePlaying.store(false);loadedInterleaved.clear();waveformPeaks.clear();loadedFrames=0;fileFrame.store(0);engine.reset();}
void BackReverseAudioProcessor::seekFile(double n){if(loadedFrames){fileFrame.store(std::min<std::size_t>(loadedFrames-1,static_cast<std::size_t>(std::clamp(n,0.0,1.0)*(loadedFrames-1))));engine.reset();}}
double BackReverseAudioProcessor::filePlayheadNormalized()const noexcept{return loadedFrames?static_cast<double>(fileFrame.load())/loadedFrames:0.0;}
double BackReverseAudioProcessor::fileLengthSeconds()const noexcept{return loadedSampleRate>0?loadedFrames/loadedSampleRate:0.0;}
void BackReverseAudioProcessor::setGateEnabled(int i,bool e){if(i>=0&&i<(int)gateMask.size()){gateMask[(std::size_t)i]=e;state.state.setProperty("gateMask"+juce::String(i),e,nullptr);}}
bool BackReverseAudioProcessor::gateEnabled(int i)const{return i>=0&&i<(int)gateMask.size()?gateMask[(std::size_t)i]:false;}
void BackReverseAudioProcessor::setGateForward(int i,bool e){if(i>=0&&i<(int)gateForward.size()){gateForward[(std::size_t)i]=e;state.state.setProperty("gateForward"+juce::String(i),e,nullptr);}}
bool BackReverseAudioProcessor::gateForwardState(int i)const{return i>=0&&i<(int)gateForward.size()?gateForward[(std::size_t)i]:false;}

void BackReverseAudioProcessor::randomize(std::uint64_t s){
    seed=s;std::mt19937_64 r(s);
    for(std::size_t i=0;i<gateMask.size();++i){setGateEnabled((int)i,(r()%100)>20);setGateForward((int)i,(r()%100)>70);}
    if(auto* q=state.getParameter("ratio")){const float choices[]={0.25f,0.5f,1.0f,2.0f,3.0f,1.5f,4.0f};const float plain=choices[r()%7];q->setValueNotifyingHost(q->convertTo0to1(plain));}
    state.state.setProperty("seed",(juce::int64)seed,nullptr);
}
void BackReverseAudioProcessor::getStateInformation(juce::MemoryBlock& d){state.state.setProperty("schemaVersion",1,nullptr);state.state.setProperty("seed",(juce::int64)seed,nullptr);auto xml=state.copyState().createXml();copyXmlToBinary(*xml,d);}
void BackReverseAudioProcessor::setStateInformation(const void* d,int n){if(auto xml=getXmlFromBinary(d,n)){auto v=juce::ValueTree::fromXml(*xml);if(v.isValid())state.replaceState(v);}seed=(std::uint64_t)(juce::int64)state.state.getProperty("seed",(juce::int64)0xBACC0FFEEULL);for(int i=0;i<(int)gateMask.size();++i){gateMask[(std::size_t)i]=(bool)state.state.getProperty("gateMask"+juce::String(i),true);gateForward[(std::size_t)i]=(bool)state.state.getProperty("gateForward"+juce::String(i),false);}syncEngineFromParameters();}
juce::AudioProcessorEditor* BackReverseAudioProcessor::createEditor(){return new BackReverseAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new BackReverseAudioProcessor();}
