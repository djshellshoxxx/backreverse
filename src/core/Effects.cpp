#include "backreverse/BackReverseEngine.h"
#include <algorithm>
#include <cmath>

namespace br {

void BackReverseEngine::processStutter(std::vector<float>& d,std::size_t channels){
    if(!fx_.stutter.enabled||fx_.stutter.periodFrames==0||fx_.stutter.repeatFrames==0||d.empty())return;
    if(stutterScratch_.size()<d.size()) stutterScratch_.resize(d.size());
    std::copy(d.begin(),d.end(),stutterScratch_.begin());
    const std::size_t frames=d.size()/channels;
    for(std::size_t base=0;base<frames;base+=fx_.stutter.periodFrames){
        const std::size_t capture=std::min(fx_.stutter.repeatFrames,frames-base);
        for(int rep=1;rep<=std::max(0,fx_.stutter.repeats);++rep){
            const std::size_t dstBase=base+static_cast<std::size_t>(rep)*capture;if(dstBase>=frames)break;
            const float amp=std::pow(std::clamp(fx_.stutter.decay,0.0f,1.5f),static_cast<float>(rep));
            for(std::size_t i=0;i<capture&&dstBase+i<frames;++i){
                const std::size_t si=(fx_.stutter.alternateDirection&&(rep&1))?(capture-1-i):i;
                for(std::size_t ch=0;ch<channels;++ch){
                    const float w=stutterScratch_[(base+si)*channels+ch]*amp;const auto di=(dstBase+i)*channels+ch;
                    d[di]=fx_.stutter.dry*d[di]+fx_.stutter.wet*w;
                }
            }
        }
    }
}

void BackReverseEngine::processDelay(std::vector<float>& d,std::size_t channels){
    if(!fx_.delay.enabled||delayBuffer_.empty()||d.empty())return;
    const std::size_t capFrames=delayBuffer_.size()/channels;const std::size_t df=std::clamp<std::size_t>(fx_.delay.delayFrames,1,capFrames-1);
    for(std::size_t f=0;f<d.size()/channels;++f){
        for(std::size_t ch=0;ch<channels;++ch){
            const std::size_t writeFrame=delayWrite_%capFrames,readFrame=(writeFrame+capFrames-df)%capFrames;
            const std::size_t readCh=(fx_.delay.pingPong&&channels>1)?(channels-1-ch):ch;
            const float delayed=delayBuffer_[readFrame*channels+readCh],x=d[f*channels+ch];
            delayBuffer_[writeFrame*channels+ch]=std::clamp(x+delayed*std::clamp(fx_.delay.feedback,-0.99f,0.99f),-8.0f,8.0f);
            d[f*channels+ch]=fx_.delay.dry*x+fx_.delay.wet*delayed;
        }++delayWrite_;
    }
}

void BackReverseEngine::processEcho(std::vector<float>& d,std::size_t channels){
    if(!fx_.echo.enabled||echoBuffer_.empty()||d.empty())return;
    const std::size_t capFrames=echoBuffer_.size()/channels;const std::size_t ef=std::clamp<std::size_t>(fx_.echo.delayFrames,1,capFrames-1);
    std::array<float,64> lp{};
    const float damp=std::clamp(fx_.echo.damping,0.0f,0.999f);
    for(std::size_t f=0;f<d.size()/channels;++f){
        for(std::size_t ch=0;ch<channels;++ch){
            const std::size_t writeFrame=echoWrite_%capFrames,readFrame=(writeFrame+capFrames-ef)%capFrames;
            float delayed=echoBuffer_[readFrame*channels+ch];
            if(ch<lp.size()){lp[ch]+=(delayed-lp[ch])*(1.0f-damp);delayed=lp[ch];}
            const float x=d[f*channels+ch];
            const float wow=1.0f+fx_.echo.wowFlutter*0.005f*std::sin(static_cast<float>(echoWrite_)*0.0021f);
            const float drift=1.0f+fx_.echo.drift*0.01f*std::sin(static_cast<float>(echoWrite_)*0.00037f);
            echoBuffer_[writeFrame*channels+ch]=std::clamp(x+delayed*std::clamp(fx_.echo.feedback,-0.99f,0.99f)*drift*wow,-8.0f,8.0f);
            const float spreadGain=(channels>1&&ch==1)?(1.0f+fx_.echo.spread*0.25f):(1.0f-fx_.echo.spread*0.25f);
            d[f*channels+ch]=fx_.echo.dry*x+fx_.echo.wet*delayed*spreadGain;
        }++echoWrite_;
    }
}

void BackReverseEngine::processEffect(EffectType t,std::vector<float>& d,std::size_t channels){
    switch(t){case EffectType::Stutter:processStutter(d,channels);break;case EffectType::Delay:processDelay(d,channels);break;case EffectType::Echo:processEcho(d,channels);break;}
}

void BackReverseEngine::applyEffects(std::vector<float>& d,std::size_t channels){
    for(auto effect:fx_.chain)processEffect(effect,d,channels);
    for(float& x:d)if(!std::isfinite(x))x=0.0f;
}

void BackReverseEngine::applyEffectsForGates(std::vector<float>& d,std::size_t channels,const ChunkParams& p){
    const std::size_t steps=p.activeGateCount?std::min(p.activeGateCount,p.gates.size()):p.gates.size();
    if(steps==0){applyEffects(d,channels);return;}
    auto selectedFor=[&](EffectType t,std::size_t step){
        const auto& g=p.gates[step];
        if(t==EffectType::Stutter)return g.stutter;
        if(t==EffectType::Delay)return g.delay;
        return g.echo;
    };
    for(auto effect:fx_.chain){
        bool any=false;for(std::size_t s=0;s<steps;++s)any=any||selectedFor(effect,s);
        if(!any){processEffect(effect,d,channels);continue;}
        if(fxScratch_.size()<d.size())fxScratch_.resize(d.size());
        std::copy(d.begin(),d.end(),fxScratch_.begin());
        const std::size_t frames=d.size()/channels;
        for(std::size_t f=0;f<frames;++f){
            const std::size_t step=std::min(steps-1,f*steps/std::max<std::size_t>(1,frames));
            if(!selectedFor(effect,step))for(std::size_t ch=0;ch<channels;++ch)fxScratch_[f*channels+ch]=0.0f;
        }
        const auto before=d;
        fxScratch_.resize(d.size());processEffect(effect,fxScratch_,channels);
        for(std::size_t f=0;f<frames;++f){
            const std::size_t step=std::min(steps-1,f*steps/std::max<std::size_t>(1,frames));
            for(std::size_t ch=0;ch<channels;++ch){
                const auto i=f*channels+ch;
                if(selectedFor(effect,step))d[i]=fxScratch_[i];
                else d[i]=before[i]+fxScratch_[i];
            }
        }
        fxScratch_.resize(std::max(fxScratch_.size(),cfg_.maxChunkFrames*cfg_.channels));
    }
    for(float& x:d)if(!std::isfinite(x))x=0.0f;
}
} // namespace br
