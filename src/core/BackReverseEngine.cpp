#include "backreverse/BackReverseEngine.h"
#include "backreverse/PatternEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace br {
namespace {
constexpr double pi = 3.14159265358979323846;

std::vector<float> rateResample(const std::vector<float>& in, std::size_t channels, double ratio) {
    if (in.empty() || channels == 0) return {};
    ratio = std::clamp(ratio, 0.05, 8.0);
    const std::size_t inFrames = in.size() / channels;
    const std::size_t outFrames = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(inFrames / ratio)));
    std::vector<float> out(outFrames * channels, 0.0f);
    for (std::size_t of=0; of<outFrames; ++of) {
        const double src = std::min<double>(inFrames - 1, of * ratio);
        const auto i0 = static_cast<std::size_t>(src);
        const auto i1 = std::min<std::size_t>(i0 + 1, inFrames - 1);
        const float frac = static_cast<float>(src - i0);
        for (std::size_t ch=0; ch<channels; ++ch)
            out[of*channels+ch] = in[i0*channels+ch] + (in[i1*channels+ch]-in[i0*channels+ch]) * frac;
    }
    return out;
}

// Lightweight granular OLA. Playback grains retain original local pitch while output spacing changes time.
std::vector<float> granularStretch(const std::vector<float>& in, std::size_t channels, double ratio, double sampleRate) {
    if (in.empty() || channels == 0) return {};
    ratio = std::clamp(ratio, 0.05, 8.0);
    if (std::abs(ratio - 1.0) < 1.0e-6) return in;

    const std::size_t inFrames = in.size()/channels;
    const std::size_t targetFrames = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(inFrames / ratio)));
    const std::size_t grain = std::clamp<std::size_t>(static_cast<std::size_t>(sampleRate * 0.04), 32, std::max<std::size_t>(32, inFrames));
    const std::size_t analysisHop = std::max<std::size_t>(1, grain/2);
    const std::size_t synthesisHop = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(analysisHop / ratio)));
    std::vector<float> out((targetFrames + grain + 2) * channels, 0.0f);
    std::vector<float> weight(targetFrames + grain + 2, 0.0f);

    std::size_t inPos=0, outPos=0;
    while (inPos < inFrames && outPos < targetFrames + grain) {
        const std::size_t n = std::min(grain, inFrames-inPos);
        for (std::size_t i=0;i<n;++i) {
            const double phase = (n <= 1) ? 0.0 : static_cast<double>(i)/(n-1);
            const float w = static_cast<float>(0.5 - 0.5*std::cos(2.0*pi*phase));
            if (outPos+i >= weight.size()) break;
            weight[outPos+i] += w;
            for (std::size_t ch=0;ch<channels;++ch)
                out[(outPos+i)*channels+ch] += in[(inPos+i)*channels+ch]*w;
        }
        inPos += analysisHop;
        outPos += synthesisHop;
    }

    out.resize(targetFrames*channels);
    for (std::size_t i=0;i<targetFrames;++i) {
        const float w = weight[i];
        if (w > 1.0e-6f)
            for (std::size_t ch=0;ch<channels;++ch) out[i*channels+ch] /= w;
    }
    return out;
}
}

BackReverseEngine::BackReverseEngine(EngineConfig cfg) : cfg_(cfg), rng_(cfg.randomSeed) {
    prepare(cfg.sampleRate, cfg.channels, cfg.maxChunkFrames);
}

void BackReverseEngine::prepare(double sampleRate, std::size_t channels, std::size_t maxChunkFrames) {
    cfg_.sampleRate = std::max(1.0, sampleRate);
    cfg_.channels = std::max<std::size_t>(1, channels);
    cfg_.maxChunkFrames = std::max<std::size_t>(1, maxChunkFrames);
    chunk_.gates.reserve(64);
    liveCapture_.assign(cfg_.maxChunkFrames * cfg_.channels, 0.0f);
    livePlayback_.assign(cfg_.maxChunkFrames * cfg_.channels, 0.0f);
    const std::size_t fxFrames = static_cast<std::size_t>(cfg_.sampleRate * 8.0);
    delayBuffer_.assign(std::max<std::size_t>(1, fxFrames*cfg_.channels), 0.0f);
    echoBuffer_.assign(std::max<std::size_t>(1, fxFrames*cfg_.channels), 0.0f);
    stutterScratch_.assign(cfg_.maxChunkFrames*cfg_.channels,0.0f);
    fxScratch_.assign(cfg_.maxChunkFrames*cfg_.channels,0.0f);
    fxBeforeScratch_.assign(cfg_.maxChunkFrames*cfg_.channels,0.0f);
    reset();
}

void BackReverseEngine::reset() {
    std::fill(liveCapture_.begin(), liveCapture_.end(), 0.0f);
    std::fill(livePlayback_.begin(), livePlayback_.end(), 0.0f);
    std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.0f);
    std::fill(echoBuffer_.begin(), echoBuffer_.end(), 0.0f);
    captureFrames_=0; playbackFrame_=0; playbackReady_=false; delayWrite_=0; echoWrite_=0;
    rng_.seed(cfg_.randomSeed);
}

void BackReverseEngine::setChunkParams(const ChunkParams& input) {
    ChunkParams p=input;
    p.durationSeconds = std::max(0.001, p.durationSeconds);
    p.ratio = std::clamp(p.ratio, 0.05, 8.0);
    p.pan = std::clamp(p.pan, -1.0f, 1.0f);
    chunk_ = std::move(p);
}
void BackReverseEngine::setChunkPattern(std::vector<ChunkParams> p) { pattern_ = std::move(p); }
void BackReverseEngine::setOrderMode(OrderMode m) { cfg_.orderMode=m; }
void BackReverseEngine::setUserOrder(std::vector<int> p) { userOrder_=std::move(p); }
void BackReverseEngine::setRandomSeed(std::uint64_t s) { cfg_.randomSeed=s; rng_.seed(s); }
void BackReverseEngine::setEffects(EffectSettings s) { fx_=s; }
void BackReverseEngine::setScratchState(ScratchState s) { scratch_=s; }
void BackReverseEngine::setDryWet(float dry, float wet) { dry_=std::clamp(dry,0.0f,1.0f); wet_=std::clamp(wet,0.0f,1.0f); }

std::size_t BackReverseEngine::chunkFrames(const ChunkParams& p) const {
    const double f = std::round(p.durationSeconds * cfg_.sampleRate);
    return std::clamp<std::size_t>(static_cast<std::size_t>(std::max(1.0, f)), 1, cfg_.maxChunkFrames);
}
std::size_t BackReverseEngine::latencyFrames() const { return chunkFrames(chunk_); }
std::vector<std::size_t> BackReverseEngine::makeOrder(std::size_t n) {
    if(cfg_.orderMode==OrderMode::UserPattern && !userPatternText_.empty())
        return buildOrder(cfg_.orderMode,n,cfg_.randomSeed,parseUserPattern(userPatternText_,n,cfg_.randomSeed));
    return buildOrder(cfg_.orderMode,n,cfg_.randomSeed,userOrder_);
}

float BackReverseEngine::interpolate(std::span<const float> d,std::size_t channels,std::size_t ch,double frame) {
    if (d.empty()||channels==0) return 0.0f;
    const std::size_t frames=d.size()/channels;
    frame=std::clamp(frame,0.0,static_cast<double>(frames-1));
    auto a=static_cast<std::size_t>(frame); auto b=std::min(a+1,frames-1);
    float t=static_cast<float>(frame-a);
    return d[a*channels+ch]+(d[b*channels+ch]-d[a*channels+ch])*t;
}

float BackReverseEngine::scrubSample(const std::vector<float>& d,std::size_t channels,std::size_t ch,double pos) const {
    if (channels==0 || ch>=channels) return 0.0f;
    return interpolate(d,channels,ch,pos);
}

std::vector<float> BackReverseEngine::renderChunk(std::span<const float> in,std::size_t channels,std::size_t start,std::size_t end,const ChunkParams& p) {
    const std::size_t frames = end>start ? end-start : 0;
    std::vector<float> out(frames*channels,0.0f);
    for (std::size_t i=0;i<frames;++i) {
        const std::size_t src=end-1-i;
        for (std::size_t ch=0;ch<channels;++ch) out[i*channels+ch]=in[src*channels+ch];
    }
    applyGates(out,channels,p);
    if (p.temporalMode==TemporalMode::Rate) out=rateResample(out,channels,p.ratio);
    else out=granularStretch(out,channels,p.ratio,cfg_.sampleRate);
    applyStereoAndPhase(out,channels,p);
    applyEffectsForGates(out,channels,p);
    return out;
}

void BackReverseEngine::applyStereoAndPhase(std::vector<float>& d,std::size_t channels,const ChunkParams& p) const {
    if (channels<1) return;
    const float angle=static_cast<float>((p.pan+1.0f)*pi/4.0);
    const float gl=std::cos(angle), gr=std::sin(angle);
    const float theta=static_cast<float>(std::clamp(p.phaseDegrees,-179.0,179.0)*pi/180.0);
    const float a=std::clamp(std::tan(theta*0.5f),-0.98f,0.98f);
    float zL=0.0f,zR=0.0f;
    const std::size_t frames=d.size()/channels;
    for(std::size_t i=0;i<frames;++i){
        float l=d[i*channels], r=channels>1?d[i*channels+1]:l;
        if(p.swapStereo && channels>1) std::swap(l,r);
        if(p.invertLeft) l=-l; if(p.invertRight) r=-r;
        // First-order all-pass phase coloration. Frequency-dependent by design.
        float yl=-a*l+zL; zL=l+a*yl;
        float yr=-a*r+zR; zR=r+a*yr;
        if(std::abs(p.phaseDegrees)>1.0e-6){l=yl;r=yr;}
        if(channels>1){ d[i*channels]=l*gl; d[i*channels+1]=r*gr; }
        else d[i]=l;
    }
}

float BackReverseEngine::gateGain(GateShape s,double x){
    x=std::clamp(x,0.0,1.0);
    switch(s){
        case GateShape::Rectangular:return 1.0f;
        case GateShape::LinearIn:return static_cast<float>(x);
        case GateShape::LinearOut:return static_cast<float>(1.0-x);
        case GateShape::Triangle:return static_cast<float>(1.0-std::abs(2.0*x-1.0));
        case GateShape::EqualPower:return static_cast<float>(std::sin(pi*x));
        case GateShape::Sine:return static_cast<float>(0.5-0.5*std::cos(2.0*pi*x));
        case GateShape::Exponential:return static_cast<float>(x*x);
        case GateShape::Logarithmic:return static_cast<float>(std::sqrt(x));
        case GateShape::Custom:return 1.0f;
    }
    return 1.0f;
}

void BackReverseEngine::applyGates(std::vector<float>& d,std::size_t channels,const ChunkParams& p) const {
    if(p.gates.empty()||d.empty()) return;
    const std::size_t frames=d.size()/channels;
    const std::size_t steps=p.activeGateCount?std::min(p.activeGateCount,p.gates.size()):p.gates.size();
    for(std::size_t s=0;s<steps;++s){
        const auto& g=p.gates[s];
        const std::size_t a=s*frames/steps,b=(s+1)*frames/steps,n=std::max<std::size_t>(1,b-a);
        if(g.direction==GateDirection::ForceForward)
            for(std::size_t i=0;i<n/2;++i) for(std::size_t ch=0;ch<channels;++ch)
                std::swap(d[(a+i)*channels+ch],d[(b-1-i)*channels+ch]);

        const double activeEnd=std::clamp<double>(g.width*(1.0-g.gap),0.0,1.0);
        const double gapStart=std::clamp<double>(1.0-g.gap,activeEnd,1.0);
        std::array<float,64> held{};
        const std::size_t heldChannels=std::min<std::size_t>(channels,held.size());
        for(std::size_t ch=0;ch<heldChannels;++ch) held[ch]=d[a*channels+ch];

        for(std::size_t i=0;i<n;++i){
            const double ph=static_cast<double>(i)/std::max<std::size_t>(1,n-1);
            double shapePhase=activeEnd>0.0?std::clamp(ph/activeEnd,0.0,1.0):1.0;
            float shaped;
            if(g.shape==GateShape::Custom){
                const double pos=shapePhase*(g.customCurve.size()-1);
                const auto i0=static_cast<std::size_t>(pos),i1=std::min(i0+1,g.customCurve.size()-1);
                const float frac=static_cast<float>(pos-i0);
                shaped=g.customCurve[i0]+(g.customCurve[i1]-g.customCurve[i0])*frac;
            } else shaped=gateGain(g.shape,shapePhase);
            float gain=(1.0f-g.depth)+g.depth*shaped;

            if(!g.enabled) gain=0.0f;
            else if(ph>activeEnd){
                switch(g.gapMode){
                    case GapMode::Silence: gain=0.0f; break;
                    case GapMode::DryThrough: gain=1.0f; break;
                    case GapMode::HoldPrevious: gain=1.0f; break;
                    case GapMode::Crossfade:{
                        const double den=std::max(1.0e-9,1.0-activeEnd);
                        const double x=std::clamp((ph-activeEnd)/den,0.0,1.0);
                        gain=static_cast<float>(std::cos(x*pi*0.5));
                        break;
                    }
                    case GapMode::EffectTailOnly: gain=0.0f; break;
                }
            }

            for(std::size_t ch=0;ch<channels;++ch){
                auto& sample=d[(a+i)*channels+ch];
                if(g.enabled && ph>activeEnd && g.gapMode==GapMode::HoldPrevious && ch<heldChannels) sample=held[ch];
                else sample*=gain;
                if(g.enabled && ph<=activeEnd && ch<heldChannels) held[ch]=sample;
            }
        }
    }
}
std::vector<float> BackReverseEngine::processFinite(const std::vector<float>& in,std::size_t channels){
    if(in.empty()||channels==0||in.size()%channels) return {};
    const std::size_t frames=in.size()/channels;
    if(cfg_.reverseMode==ReverseMode::WholeSource){
        ChunkParams p=chunk_; p.durationSeconds=static_cast<double>(frames)/cfg_.sampleRate;
        return renderChunk(in,channels,0,frames,p);
    }

    struct Slice { std::size_t start; std::size_t end; ChunkParams params; };
    std::vector<Slice> slices;
    std::size_t cursor=0, sequenceIndex=0;
    while(cursor<frames){
        const ChunkParams p=pattern_.empty()?chunk_:pattern_[sequenceIndex%pattern_.size()];
        const std::size_t len=chunkFrames(p);
        const std::size_t endFrame=std::min(frames,cursor+len);
        slices.push_back({cursor,endFrame,p});
        cursor=endFrame;
        ++sequenceIndex;
    }

    auto order=makeOrder(slices.size());
    std::vector<float> out;
    out.reserve(in.size()*2);
    for(std::size_t idx:order){
        if(idx==RestChunk){
            const std::size_t restFrames=chunkFrames(chunk_);
            out.insert(out.end(),restFrames*channels,0.0f);
            continue;
        }
        if(idx>=slices.size()) continue;
        const auto& s=slices[idx];
        auto rendered=renderChunk(in,channels,s.start,s.end,s.params);
        out.insert(out.end(),rendered.begin(),rendered.end());
    }
    return out;
}
void BackReverseEngine::processLive(std::span<const float> in,std::span<float> out){
    const std::size_t channels=cfg_.channels;
    if(channels==0||in.size()!=out.size()||in.size()%channels){ std::fill(out.begin(),out.end(),0.0f); return; }
    const std::size_t wanted=chunkFrames(chunk_);
    const std::size_t frames=in.size()/channels;
    for(std::size_t f=0;f<frames;++f){
        for(std::size_t ch=0;ch<channels;++ch){
            float wetSample=0.0f;
            if(playbackReady_ && playbackFrame_<wanted) wetSample=livePlayback_[playbackFrame_*channels+ch];
            const float drySample=in[f*channels+ch];
            float y=dry_*drySample+wet_*wetSample;
            out[f*channels+ch]=std::isfinite(y)?y:0.0f;
            liveCapture_[captureFrames_*channels+ch]=drySample;
        }
        ++captureFrames_;
        if(playbackReady_) ++playbackFrame_;
        if(captureFrames_>=wanted){
            // Make the just-finished capture the next playback chunk.
            for(std::size_t i=0;i<wanted;++i)
                for(std::size_t ch=0;ch<channels;++ch)
                    livePlayback_[i*channels+ch]=liveCapture_[(wanted-1-i)*channels+ch];
            const auto fullSize=livePlayback_.size();
            livePlayback_.resize(wanted*channels);
            applyGates(livePlayback_,channels,chunk_);
            applyStereoAndPhase(livePlayback_,channels,chunk_);
            applyEffectsForGates(livePlayback_,channels,chunk_);
            livePlayback_.resize(fullSize);
            captureFrames_=0; playbackFrame_=0; playbackReady_=true;
        } else if(playbackReady_ && playbackFrame_>=wanted) {
            playbackReady_=false;
        }
    }
}
} // namespace br
