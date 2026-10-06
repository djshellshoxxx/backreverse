#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "backreverse/PatternEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <thread>

namespace {
juce::NormalisableRange<float> skewed(float lo,float hi,float centre){ juce::NormalisableRange<float> r(lo,hi); r.setSkewForCentre(centre); return r; }
}

int BackReverseAudioProcessor::acquireFileBuffer() noexcept {
    for (int attempt=0;attempt<3;++attempt) {
        const int slot=activeFileSlot.load(std::memory_order_seq_cst);
        if(slot<0||slot>=static_cast<int>(loadedAudio.size()))return -1;
        fileReaders[static_cast<std::size_t>(slot)].fetch_add(1,std::memory_order_seq_cst);
        if(activeFileSlot.load(std::memory_order_seq_cst)==slot)return slot;
        fileReaders[static_cast<std::size_t>(slot)].fetch_sub(1,std::memory_order_seq_cst);
    }
    return -1;
}

void BackReverseAudioProcessor::releaseFileBuffer(int slot) noexcept {
    if(slot>=0&&slot<static_cast<int>(loadedAudio.size()))
        fileReaders[static_cast<std::size_t>(slot)].fetch_sub(1,std::memory_order_seq_cst);
}

BackReverseAudioProcessor::BackReverseAudioProcessor()
: AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  state(*this,&undoManager,"STATE",makeLayout()), engine(br::EngineConfig{}) {
    formatManager.registerBasicFormats();
    for(std::size_t i=0;i<gateMask.size();++i){gateMask[i].store(true);gateForward[i].store(false);gateStutter[i].store(false);gateDelay[i].store(false);gateEcho[i].store(false);}
    static constexpr float curve[]={0.0f,0.15f,0.5f,1.0f,1.0f,0.5f,0.15f,0.0f};
    for(std::size_t i=0;i<gateCurve.size();++i)gateCurve[i].store(curve[i]);
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
    p.push_back(std::make_unique<juce::AudioParameterChoice>("gateShape","Gate Shape",juce::StringArray{"Hard","Linear In","Linear Out","Triangle","Equal Power","Sine","Exponential","Logarithmic","Custom"},4));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("gateWidth","Gate Width",0.01f,1.0f,0.88f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("gateGap","Gate Gap",0.0f,0.99f,0.12f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("gapMode","Gap Mode",juce::StringArray{"Silence","Dry Through","Hold","Crossfade","FX Tail"},0));

    p.push_back(std::make_unique<juce::AudioParameterBool>("stutterOn","Stutter",false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stutterMs","Stutter Period",10.0f,1000.0f,120.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stutterWet","Stutter Wet",0.0f,1.0f,0.7f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stutterDry","Stutter Dry",0.0f,1.0f,0.3f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("stutterRepeats","Stutter Repeats",1,16,2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("stutterDecay","Stutter Decay",0.0f,1.5f,1.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("stutterAlternate","Stutter Alternate",true));
    p.push_back(std::make_unique<juce::AudioParameterBool>("delayOn","Delay",false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayMs","Delay Time",1.0f,2000.0f,250.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayFeedback","Delay Feedback",-0.95f,0.95f,0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayWet","Delay Wet",0.0f,1.0f,0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayDry","Delay Dry",0.0f,1.0f,1.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("delayPingPong","Delay Ping Pong",true));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayLowpass","Delay Lowpass",skewed(100.0f,20000.0f,8000.0f),18000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("delayHighpass","Delay Highpass",skewed(5.0f,5000.0f,200.0f),20.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("echoOn","Echo",false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoMs","Echo Time",10.0f,4000.0f,375.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoFeedback","Echo Feedback",-0.95f,0.95f,0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoDamping","Echo Damping",0.0f,0.99f,0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoWet","Echo Wet",0.0f,1.0f,0.25f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoDry","Echo Dry",0.0f,1.0f,1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoSpread","Echo Spread",-1.0f,1.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoDrift","Echo Drift",0.0f,1.0f,0.35f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("echoWow","Echo Wow Flutter",0.0f,1.0f,0.25f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("scratchMode","Scratch Mode",juce::StringArray{"Linear","Vinyl","Tape Shuttle","Fine"},1));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("scratchRelease","Scratch Release",juce::StringArray{"Latch","Spring Return","Continue"},2));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scratchInertia","Scratch Inertia",0.0f,0.9999f,0.92f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scratchFriction","Scratch Friction",0.0f,1.0f,0.12f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("scratchMaxRate","Scratch Max Rate",0.25f,12.0f,8.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("scratchReverseOnly","Scratch Reverse Only",false));
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
    const double initialChunk=std::clamp<double>(*state.getRawParameterValue("chunkSeconds"),0.001,600.0);
    const std::size_t maxFrames=std::max<std::size_t>(1,static_cast<std::size_t>(std::ceil(sr*initialChunk)));
    preparedBufferFramesLimit.store(maxFrames);
    engine.prepare(sr,channels,maxFrames);
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
    if(params.gates.size()!=64) params.gates.assign(64,br::GateStep{});
    params.activeGateCount=static_cast<std::size_t>(gateCount);
    const auto shape=static_cast<br::GateShape>(std::clamp(static_cast<int>(*state.getRawParameterValue("gateShape")),0,8));
    const auto gapMode=static_cast<br::GapMode>(std::clamp(static_cast<int>(*state.getRawParameterValue("gapMode")),0,4));
    const float width=*state.getRawParameterValue("gateWidth");
    const float gap=*state.getRawParameterValue("gateGap");
    for(int i=0;i<64;++i){ auto& g=params.gates[(std::size_t)i]; g.enabled=(i<gateCount)?gateMask[(std::size_t)i].load():false;g.shape=shape;g.width=width;g.gap=gap;g.gapMode=gapMode;g.direction=gateForward[(std::size_t)i].load()?br::GateDirection::ForceForward:br::GateDirection::Inherit;
        g.stutter=gateStutter[(std::size_t)i].load(); g.delay=gateDelay[(std::size_t)i].load(); g.echo=gateEcho[(std::size_t)i].load();
        for(int k=0;k<8;++k)g.customCurve[(std::size_t)k]=gateCurve[(std::size_t)k].load(); }

    engine.setChunkParams(params);
    engine.setReverseMode(static_cast<br::ReverseMode>(static_cast<int>(*state.getRawParameterValue("reverseMode"))));
    engine.setOrderMode(static_cast<br::OrderMode>(static_cast<int>(*state.getRawParameterValue("orderMode"))));
    engine.setRandomSeed(seed.load());
    engine.setDryWet(*state.getRawParameterValue("dry"),*state.getRawParameterValue("wet"));

    fx.stutter.enabled=*state.getRawParameterValue("stutterOn")>0.5f; fx.stutter.periodFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("stutterMs")/1000.0));fx.stutter.repeatFrames=std::max<std::size_t>(1,fx.stutter.periodFrames/2);fx.stutter.wet=*state.getRawParameterValue("stutterWet");fx.stutter.dry=*state.getRawParameterValue("stutterDry");
    fx.stutter.repeats=(int)*state.getRawParameterValue("stutterRepeats");fx.stutter.decay=*state.getRawParameterValue("stutterDecay");fx.stutter.alternateDirection=*state.getRawParameterValue("stutterAlternate")>0.5f;
    fx.delay.enabled=*state.getRawParameterValue("delayOn")>0.5f;fx.delay.delayFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("delayMs")/1000.0));fx.delay.feedback=*state.getRawParameterValue("delayFeedback");fx.delay.wet=*state.getRawParameterValue("delayWet");fx.delay.dry=*state.getRawParameterValue("delayDry");
    fx.delay.pingPong=*state.getRawParameterValue("delayPingPong")>0.5f;fx.delay.lowpassHz=*state.getRawParameterValue("delayLowpass");fx.delay.highpassHz=*state.getRawParameterValue("delayHighpass");
    fx.echo.enabled=*state.getRawParameterValue("echoOn")>0.5f;fx.echo.delayFrames=std::max<std::size_t>(1,static_cast<std::size_t>(sr**state.getRawParameterValue("echoMs")/1000.0));fx.echo.feedback=*state.getRawParameterValue("echoFeedback");fx.echo.damping=*state.getRawParameterValue("echoDamping");fx.echo.wet=*state.getRawParameterValue("echoWet");fx.echo.dry=*state.getRawParameterValue("echoDry");
    fx.echo.spread=*state.getRawParameterValue("echoSpread");fx.echo.drift=*state.getRawParameterValue("echoDrift");fx.echo.wowFlutter=*state.getRawParameterValue("echoWow");
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
    const int latency=static_cast<int>(std::min<std::size_t>(engine.latencyFrames(),static_cast<std::size_t>(std::numeric_limits<int>::max())));
    if(latency!=lastReportedLatency){setLatencySamples(latency);lastReportedLatency=latency;}
}

static float sampleLoadedLinear(const std::vector<float>& d,std::size_t frames,int channels,int ch,double frame){
    if(d.empty()||frames==0||channels<=0)return 0.0f;frame=std::clamp(frame,0.0,(double)(frames-1));auto a=(std::size_t)frame,b=std::min(a+1,frames-1);float t=(float)(frame-a);return d[a*(std::size_t)channels+(std::size_t)ch]+(d[b*(std::size_t)channels+(std::size_t)ch]-d[a*(std::size_t)channels+(std::size_t)ch])*t;
}

void BackReverseAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&){
    juce::ScopedNoDenormals noDenormals;
    if(auto* ph=getPlayHead()){
        if(auto pos=ph->getPosition()) if(auto bpm=pos->getBpm()) lastBpm.store(*bpm);
    }
    syncEngineFromParameters();
    if(resetEngineRequested.exchange(false,std::memory_order_acq_rel))engine.reset();
    const int channels=buffer.getNumChannels(),totalFrames=buffer.getNumSamples(); if(channels<=0||totalFrames<=0)return;
    const int fileSlot=acquireFileBuffer();
    const auto* file=fileSlot>=0?&loadedAudio[static_cast<std::size_t>(fileSlot)]:nullptr;
    const bool hasFile=file&&file->frames>0;
    const int reverseModeIndex=static_cast<int>(*state.getRawParameterValue("reverseMode"));
    if(scratchReleaseRequested.exchange(false,std::memory_order_acq_rel)&&hasFile){
        const int release=static_cast<int>(*state.getRawParameterValue("scratchRelease"));
        if(release==1){scratchAudioFrame=std::clamp(scratchReturnFrame.load(),0.0,static_cast<double>(file->frames-1));fileFrame.store(scratchAudioFrame);scratchAudioVelocity=0.0;}
        else if(release==2){fileFrame.store(std::clamp(scratchAudioFrame,0.0,static_cast<double>(file->frames-1)));scratchAudioVelocity=0.0;}
    }
    const bool directScratch=hasFile&&(scratchActive.load()||reverseModeIndex==3);
    if(directScratch){
        const double sr=std::max(1.0,getSampleRate());
        const double target=scratchTargetNorm.load()*(file->frames-1);
        const double maxRate=*state.getRawParameterValue("scratchMaxRate");
        const int scratchModeIndex=static_cast<int>(*state.getRawParameterValue("scratchMode"));
        double requested=scratchVelocityNormPerSec.load()*file->frames/sr;
        if(scratchModeIndex==3)requested*=0.2;
        if(*state.getRawParameterValue("scratchReverseOnly")>0.5f)requested=-std::abs(requested);
        requested=std::clamp(requested,-maxRate,maxRate);
        if(scratchActive.load()){scratchAudioFrame=target;scratchAudioVelocity=requested;}
        const double inertia=*state.getRawParameterValue("scratchInertia");
        const double friction=*state.getRawParameterValue("scratchFriction");
        for(int f=0;f<totalFrames;++f){
            for(int ch=0;ch<channels;++ch){int sourceCh=std::min(ch,std::max(0,file->channels-1));buffer.setSample(ch,f,sampleLoadedLinear(file->samples,file->frames,file->channels,sourceCh,scratchAudioFrame));}
            scratchAudioFrame=std::clamp(scratchAudioFrame+scratchAudioVelocity,0.0,static_cast<double>(file->frames-1));
            if(!scratchActive.load()){scratchAudioVelocity*=inertia;const double drag=friction/sr;if(scratchAudioVelocity>0)scratchAudioVelocity=std::max(0.0,scratchAudioVelocity-drag);else scratchAudioVelocity=std::min(0.0,scratchAudioVelocity+drag);}
        }
        fileFrame.store(std::clamp(scratchAudioFrame,0.0,static_cast<double>(file->frames-1)));
        releaseFileBuffer(fileSlot);
        return;
    }
    const std::size_t capFrames=inScratch.size()/static_cast<std::size_t>(channels);
    int offset=0;
    while(offset<totalFrames){
        const int frames=std::min<int>(totalFrames-offset,static_cast<int>(capFrames));
        const std::size_t samples=static_cast<std::size_t>(frames*channels);
        for(int f=0;f<frames;++f)for(int ch=0;ch<channels;++ch){
            float v=buffer.getSample(ch,offset+f);
            if(hasFile&&filePlaying.load()){
                double pos=fileFrame.load(); if(!std::isfinite(pos)||pos>=file->frames){pos=0.0;fileFrame.store(0.0);}
                int sourceCh=std::min(ch,std::max(0,file->channels-1));
                const int rm=static_cast<int>(*state.getRawParameterValue("reverseMode"));
                const double srcPos=(rm==1)?(static_cast<double>(file->frames-1)-pos):pos;
                v=sampleLoadedLinear(file->samples,file->frames,file->channels,sourceCh,srcPos);
                if(ch==channels-1){const double step=std::max(1.0e-9,file->sampleRate/std::max(1.0,getSampleRate()));fileFrame.store(std::fmod(pos+step,static_cast<double>(file->frames)));}
            }
            inScratch[static_cast<std::size_t>(f*channels+ch)]=v;
        }
        if(hasFile&&static_cast<int>(*state.getRawParameterValue("reverseMode"))==1){
            for(std::size_t i=0;i<samples;++i) outScratch[i]=inScratch[i];
        }else engine.processLive(std::span<const float>(inScratch.data(),samples),std::span<float>(outScratch.data(),samples));
        for(int f=0;f<frames;++f)for(int ch=0;ch<channels;++ch)buffer.setSample(ch,offset+f,outScratch[static_cast<std::size_t>(f*channels+ch)]);
        offset+=frames;
    }
    releaseFileBuffer(fileSlot);
}

bool BackReverseAudioProcessor::loadAudioFile(const juce::File& file){
    std::unique_ptr<juce::AudioFormatReader> r(formatManager.createReaderFor(file)); if(!r)return false;
    if(r->lengthInSamples<=0||r->lengthInSamples>std::numeric_limits<int>::max())return false;
    juce::AudioBuffer<float> tmp(static_cast<int>(r->numChannels),static_cast<int>(r->lengthInSamples)); if(!r->read(&tmp,0,tmp.getNumSamples(),0,true,true))return false;
    const int channels=tmp.getNumChannels();const std::size_t frames=static_cast<std::size_t>(tmp.getNumSamples());
    const int active=activeFileSlot.load(std::memory_order_seq_cst);const int target=active==0?1:0;
    while(fileReaders[static_cast<std::size_t>(target)].load(std::memory_order_seq_cst)!=0)std::this_thread::yield();
    auto& destination=loadedAudio[static_cast<std::size_t>(target)];
    destination.samples.resize(frames*static_cast<std::size_t>(channels));destination.frames=frames;destination.channels=channels;destination.sampleRate=r->sampleRate;
    for(std::size_t f=0;f<frames;++f)for(int ch=0;ch<channels;++ch)destination.samples[f*static_cast<std::size_t>(channels)+static_cast<std::size_t>(ch)]=tmp.getSample(ch,static_cast<int>(f));
    waveformPeaks.assign(1024,0.0f);
    for(std::size_t b=0;b<waveformPeaks.size();++b){
        const std::size_t a=b*frames/waveformPeaks.size(), z=std::max(a+1,(b+1)*frames/waveformPeaks.size());
        float peak=0; for(std::size_t f=a;f<std::min(z,frames);++f) for(int ch=0;ch<channels;++ch) peak=std::max(peak,std::abs(destination.samples[f*(std::size_t)channels+(std::size_t)ch]));
        waveformPeaks[b]=peak;
    }
    loadedChannels.store(channels);loadedFrames.store(frames);loadedSampleRate.store(r->sampleRate);
    fileFrame.store(0.0);activeFileSlot.store(target,std::memory_order_seq_cst);fileActive.store(true);filePlaying.store(true);resetEngineRequested.store(true);
    return true;
}
void BackReverseAudioProcessor::unloadAudioFile(){fileActive.store(false);filePlaying.store(false);activeFileSlot.exchange(-1,std::memory_order_seq_cst);for(auto& readers:fileReaders)while(readers.load(std::memory_order_seq_cst)!=0)std::this_thread::yield();for(auto& audio:loadedAudio){audio.samples.clear();audio.frames=0;audio.channels=0;audio.sampleRate=0.0;}loadedFrames.store(0);loadedChannels.store(0);loadedSampleRate.store(0.0);waveformPeaks.clear();fileFrame.store(0);resetEngineRequested.store(true);}
void BackReverseAudioProcessor::beginScratch(double n){const auto frames=loadedFrames.load();if(!frames)return;scratchReturnFrame.store(fileFrame.load());scratchTargetNorm.store(std::clamp(n,0.0,1.0));scratchVelocityNormPerSec.store(0);scratchActive.store(true);}
void BackReverseAudioProcessor::updateScratch(double n,double v){if(!loadedFrames)return;scratchTargetNorm.store(std::clamp(n,0.0,1.0));scratchVelocityNormPerSec.store(v);}
void BackReverseAudioProcessor::endScratch(){if(!loadedFrames.load())return;scratchActive.store(false);scratchReleaseRequested.store(true);}
void BackReverseAudioProcessor::seekFile(double n){const auto frames=loadedFrames.load();if(frames){fileFrame.store(std::min<std::size_t>(frames-1,static_cast<std::size_t>(std::clamp(n,0.0,1.0)*(frames-1))));resetEngineRequested.store(true);}}
double BackReverseAudioProcessor::filePlayheadNormalized()const noexcept{return loadedFrames?static_cast<double>(fileFrame.load())/loadedFrames:0.0;}
double BackReverseAudioProcessor::fileLengthSeconds()const noexcept{return loadedSampleRate>0?loadedFrames/loadedSampleRate:0.0;}
void BackReverseAudioProcessor::setGateEnabled(int i,bool e){if(i>=0&&i<(int)gateMask.size()){gateMask[(std::size_t)i].store(e);state.state.setProperty("gateMask"+juce::String(i),e,nullptr);}}
bool BackReverseAudioProcessor::gateEnabled(int i)const{return i>=0&&i<(int)gateMask.size()?gateMask[(std::size_t)i].load():false;}
void BackReverseAudioProcessor::setGateForward(int i,bool e){if(i>=0&&i<(int)gateForward.size()){gateForward[(std::size_t)i].store(e);state.state.setProperty("gateForward"+juce::String(i),e,nullptr);}}
bool BackReverseAudioProcessor::gateForwardState(int i)const{return i>=0&&i<(int)gateForward.size()?gateForward[(std::size_t)i].load():false;}
void BackReverseAudioProcessor::setGateEffect(int i,br::EffectType effect,bool enabled){
    if(i<0||i>=64)return; auto idx=(std::size_t)i;
    if(effect==br::EffectType::Stutter){gateStutter[idx].store(enabled);state.state.setProperty("gateStutter"+juce::String(i),enabled,nullptr);}
    else if(effect==br::EffectType::Delay){gateDelay[idx].store(enabled);state.state.setProperty("gateDelay"+juce::String(i),enabled,nullptr);}
    else {gateEcho[idx].store(enabled);state.state.setProperty("gateEcho"+juce::String(i),enabled,nullptr);}
}
void BackReverseAudioProcessor::setGateCurvePoint(int i,float value){if(i<0||i>=8)return;const float v=juce::jlimit(0.0f,1.0f,value);gateCurve[(std::size_t)i].store(v);state.state.setProperty("curve"+juce::String(i),v,&undoManager);}
float BackReverseAudioProcessor::gateCurvePoint(int i)const{if(i<0||i>=8)return 0;return gateCurve[(std::size_t)i].load();}
void BackReverseAudioProcessor::refreshGateCurveCache(){static constexpr float defaults[]={0.0f,0.15f,0.5f,1.0f,1.0f,0.5f,0.15f,0.0f};for(int i=0;i<8;++i)gateCurve[(std::size_t)i].store((float)state.state.getProperty("curve"+juce::String(i),defaults[i]));}

bool BackReverseAudioProcessor::gateEffectState(int i,br::EffectType effect)const{
    if(i<0||i>=64)return false;auto idx=(std::size_t)i;
    if(effect==br::EffectType::Stutter)return gateStutter[idx].load();
    if(effect==br::EffectType::Delay)return gateDelay[idx].load();
    return gateEcho[idx].load();
}

void BackReverseAudioProcessor::setUserPatternText(const juce::String& text){
    state.state.setProperty("userPattern",text,nullptr);
}
juce::String BackReverseAudioProcessor::userPatternText() const { return state.state.getProperty("userPattern","").toString(); }

void BackReverseAudioProcessor::randomize(std::uint64_t s){
    seed.store(s);std::mt19937_64 r(s);
    for(std::size_t i=0;i<gateMask.size();++i){setGateEnabled((int)i,(r()%100)>20);setGateForward((int)i,(r()%100)>70);}
    if(auto* q=state.getParameter("ratio")){const float choices[]={0.25f,0.5f,1.0f,2.0f,3.0f,1.5f,4.0f};const float plain=choices[r()%7];q->setValueNotifyingHost(q->convertTo0to1(plain));}
    state.state.setProperty("seed",(juce::int64)seed.load(),nullptr);
}
void BackReverseAudioProcessor::loadFactoryPreset(int i){
    auto set=[this](const char* id,float plain){if(auto* q=state.getParameter(id))q->setValueNotifyingHost(q->convertTo0to1(plain));};
    auto setBool=[this](const char* id,bool v){if(auto* q=state.getParameter(id))q->setValueNotifyingHost(v?1.0f:0.0f);};
    // Reset a small common base first.
    set("chunkSeconds",5.0f);set("ratio",1.0f);set("temporalMode",0);set("orderMode",0);set("reverseMode",0);
    setBool("stutterOn",false);setBool("delayOn",false);setBool("echoOn",false);set("pan",0);set("polarity",0);setBool("swapStereo",false);
    switch(i){
        case 0: break;
        case 1:set("reverseMode",1);break;
        case 2:set("chunkSeconds",2);set("ratio",0.5f);break;
        case 3:set("chunkSeconds",0.75f);set("temporalMode",1);set("ratio",0.25f);break;
        case 4:set("chunkSeconds",0.25f);set("ratio",3.0f);break;
        case 5:set("chunkSeconds",0.5f);set("orderMode",2);seed=31337;break;
        case 6:set("chunkSeconds",2);for(int k=0;k<64;++k)setGateForward(k,(k%4)==3);break;
        case 7:setBool("swapStereo",true);break;
        case 8:set("polarity",1);break;
        case 9:setBool("stutterOn",true);setBool("delayOn",true);setBool("echoOn",true);set("fxOrder",0);break;
        case 10:setBool("stutterOn",true);setBool("echoOn",true);set("fxOrder",4);break;
        case 11:set("chunkSeconds",4);set("ratio",0.5f);break;
        case 12:set("chunkSeconds",0.2f);set("temporalMode",1);set("ratio",0.1f);break;
        default:break;
    }
    state.state.setProperty("seed",(juce::int64)seed.load(),&undoManager);
}
void BackReverseAudioProcessor::getStateInformation(juce::MemoryBlock& d){state.state.setProperty("schemaVersion",1,nullptr);state.state.setProperty("seed",(juce::int64)seed.load(),nullptr);auto xml=state.copyState().createXml();copyXmlToBinary(*xml,d);}
void BackReverseAudioProcessor::setStateInformation(const void* d,int n){if(auto xml=getXmlFromBinary(d,n)){auto v=juce::ValueTree::fromXml(*xml);if(v.isValid())state.replaceState(v);}seed.store((std::uint64_t)(juce::int64)state.state.getProperty("seed",(juce::int64)0xBACC0FFEEULL));for(int i=0;i<(int)gateMask.size();++i){gateMask[(std::size_t)i].store((bool)state.state.getProperty("gateMask"+juce::String(i),true));gateForward[(std::size_t)i].store((bool)state.state.getProperty("gateForward"+juce::String(i),false));
        gateStutter[(std::size_t)i].store((bool)state.state.getProperty("gateStutter"+juce::String(i),false));
        gateDelay[(std::size_t)i].store((bool)state.state.getProperty("gateDelay"+juce::String(i),false));
        gateEcho[(std::size_t)i].store((bool)state.state.getProperty("gateEcho"+juce::String(i),false));}
    static constexpr float defaults[]={0.0f,0.15f,0.5f,1.0f,1.0f,0.5f,0.15f,0.0f};
    for(int i=0;i<8;++i)gateCurve[(std::size_t)i].store((float)state.state.getProperty("curve"+juce::String(i),defaults[i]));
    fileFrame.store(0.0);resetEngineRequested.store(true);}
juce::AudioProcessorEditor* BackReverseAudioProcessor::createEditor(){return new BackReverseAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new BackReverseAudioProcessor();}
