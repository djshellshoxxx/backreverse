#include "backreverse/BackReverseEngine.h"
#include <algorithm>
#include <cmath>

namespace br {
void BackReverseEngine::applyEffects(std::vector<float>& d,std::size_t channels){
    if(d.empty()||channels==0) return;

    if(fx_.stutter.enabled && fx_.stutter.periodFrames>0 && fx_.stutter.repeatFrames>0){
        const auto src=d;
        const std::size_t frames=d.size()/channels;
        for(std::size_t base=0;base<frames;base+=fx_.stutter.periodFrames){
            const std::size_t capture=std::min(fx_.stutter.repeatFrames,frames-base);
            for(int rep=1;rep<=std::max(0,fx_.stutter.repeats);++rep){
                const std::size_t dstBase=base+static_cast<std::size_t>(rep)*capture;
                if(dstBase>=frames) break;
                const float amp=std::pow(std::clamp(fx_.stutter.decay,0.0f,1.5f),static_cast<float>(rep));
                for(std::size_t i=0;i<capture && dstBase+i<frames;++i){
                    const std::size_t si=(fx_.stutter.alternateDirection && (rep&1))?(capture-1-i):i;
                    for(std::size_t ch=0;ch<channels;++ch){
                        const float w=src[(base+si)*channels+ch]*amp;
                        const auto di=(dstBase+i)*channels+ch;
                        d[di]=fx_.stutter.dry*d[di]+fx_.stutter.wet*w;
                    }
                }
            }
        }
    }

    if(fx_.delay.enabled && !delayBuffer_.empty()){
        const std::size_t capFrames=delayBuffer_.size()/channels;
        const std::size_t df=std::clamp<std::size_t>(fx_.delay.delayFrames,1,capFrames-1);
        for(std::size_t f=0;f<d.size()/channels;++f){
            for(std::size_t ch=0;ch<channels;++ch){
                const std::size_t writeFrame=delayWrite_%capFrames;
                const std::size_t readFrame=(writeFrame+capFrames-df)%capFrames;
                const std::size_t readCh=(fx_.delay.pingPong && channels>1)?(1-ch):ch;
                const float delayed=delayBuffer_[readFrame*channels+readCh];
                const float x=d[f*channels+ch];
                delayBuffer_[writeFrame*channels+ch]=std::clamp(x+delayed*std::clamp(fx_.delay.feedback,-0.99f,0.99f),-8.0f,8.0f);
                d[f*channels+ch]=fx_.delay.dry*x+fx_.delay.wet*delayed;
            }
            ++delayWrite_;
        }
    }

    if(fx_.echo.enabled && !echoBuffer_.empty()){
        const std::size_t capFrames=echoBuffer_.size()/channels;
        const std::size_t ef=std::clamp<std::size_t>(fx_.echo.delayFrames,1,capFrames-1);
        float lpL=0.0f,lpR=0.0f;
        const float damp=std::clamp(fx_.echo.damping,0.0f,0.999f);
        for(std::size_t f=0;f<d.size()/channels;++f){
            for(std::size_t ch=0;ch<channels;++ch){
                const std::size_t writeFrame=echoWrite_%capFrames;
                const std::size_t readFrame=(writeFrame+capFrames-ef)%capFrames;
                float delayed=echoBuffer_[readFrame*channels+ch];
                float& lp=(ch==0?lpL:lpR);
                lp+=(delayed-lp)*(1.0f-damp);
                delayed=lp;
                const float x=d[f*channels+ch];
                const float drift=1.0f+fx_.echo.drift*0.01f*std::sin(static_cast<float>(echoWrite_)*0.00037f);
                echoBuffer_[writeFrame*channels+ch]=std::clamp(x+delayed*std::clamp(fx_.echo.feedback,-0.99f,0.99f)*drift,-8.0f,8.0f);
                d[f*channels+ch]=fx_.echo.dry*x+fx_.echo.wet*delayed;
            }
            ++echoWrite_;
        }
    }

    for(float& x:d) if(!std::isfinite(x)) x=0.0f;
}
} // namespace br
