#include "backreverse/BackReverseEngine.h"
#include "backreverse/PatternEngine.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

static int failures=0;
#define CHECK(name, expr) do { if(!(expr)){ std::cerr<<"FAIL: "<<name<<"\n"; ++failures; } else std::cout<<"PASS: "<<name<<"\n"; } while(0)

static bool near(float a,float b,float e=1e-4f){return std::fabs(a-b)<=e;}

int main(){
 using namespace br;

 {
   EngineConfig c; c.sampleRate=1.0; c.channels=1; c.maxChunkFrames=1000;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=4.0; p.ratio=1.0; e.setChunkParams(p);
   auto y=e.processFinite({0,1,2,3,4,5,6,7},1);
   CHECK("sequential chunk reversal exact", y==std::vector<float>({3,2,1,0,7,6,5,4}));
 }
 {
   EngineConfig c; c.sampleRate=1.0; c.channels=1; c.maxChunkFrames=1000; c.reverseMode=ReverseMode::WholeSource;
   BackReverseEngine e(c); auto y=e.processFinite({0,1,2,3,4,5,6,7},1);
   CHECK("whole source reversal exact", y==std::vector<float>({7,6,5,4,3,2,1,0}));
 }
 {
   EngineConfig c; c.sampleRate=1.0; c.channels=1; c.maxChunkFrames=1000;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=4.0; e.setChunkParams(p);
   auto y=e.processFinite({0,1,2,3,4,5},1);
   CHECK("partial final chunk", y==std::vector<float>({3,2,1,0,5,4}));
 }
 {
   EngineConfig c; c.sampleRate=1000.0; c.channels=1; c.maxChunkFrames=100000;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=0.1255; e.setChunkParams(p);
   CHECK("fractional chunk rounded to frames", e.latencyFrames()==126);
 }
 {
   EngineConfig c; c.sampleRate=1000.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=std::numeric_limits<double>::infinity();
   p.ratio=std::numeric_limits<double>::quiet_NaN(); p.phaseDegrees=std::numeric_limits<double>::infinity();
   GateStep gate; gate.width=std::numeric_limits<float>::quiet_NaN(); gate.depth=2.0f;
   gate.customCurve[0]=std::numeric_limits<float>::infinity(); p.gates={gate}; e.setChunkParams(p);
   auto y=e.processFinite({1,2,3,4},1); bool finite=true; for(float v:y)finite&=std::isfinite(v);
   CHECK("invalid parameter values are sanitized", e.latencyFrames()==1 && finite);
 }
 {
   EngineConfig c; c.sampleRate=8.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; p.temporalMode=TemporalMode::Rate; p.ratio=2.0; e.setChunkParams(p);
   auto y=e.processFinite({0,1,2,3,4,5,6,7},1);
   CHECK("double time halves duration", y.size()==4);
 }
 {
   EngineConfig c; c.sampleRate=8.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; p.temporalMode=TemporalMode::Rate; p.ratio=0.5; e.setChunkParams(p);
   auto y=e.processFinite({0,1,2,3,4,5,6,7},1);
   CHECK("half time doubles duration", y.size()==16);
 }
 {
   auto a=buildOrder(OrderMode::Random,16,1234,{});
   auto b=buildOrder(OrderMode::Random,16,1234,{});
   auto d=buildOrder(OrderMode::Random,16,5678,{});
   CHECK("random order deterministic by seed",a==b);
   CHECK("different seed changes order",a!=d);
 }
 {
   auto o=buildOrder(OrderMode::UserPattern,4,0,{0,2,1,3});
   CHECK("user chunk order",o==std::vector<std::size_t>({0,2,1,3}));
 }
 {
   EngineConfig c; c.sampleRate=4.0; c.channels=2; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; p.swapStereo=true; e.setChunkParams(p);
   auto y=e.processFinite({1,10,2,20,3,30,4,40},2);
   CHECK("center pan preserves stereo level while swapping", near(y[0],40.0f) && near(y[1],4.0f));
 }
 {
   EngineConfig c; c.sampleRate=4.0; c.channels=2; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; e.setChunkParams(p);
   auto y=e.processFinite({1,10,2,20,3,30,4,40},2);
   CHECK("center pan preserves stereo input level", near(y[0],4.0f) && near(y[1],40.0f));
 }
 {
   EngineConfig c; c.sampleRate=4.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0;
   GateStep g; g.enabled=false; p.gates={g}; e.setChunkParams(p);
   auto y=e.processFinite({1,2,3,4},1);
   bool allzero=true; for(float v:y) allzero&=near(v,0);
   CHECK("disabled gate silences",allzero);
 }
 {
   EngineConfig c; c.sampleRate=4.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0;
   GateStep g; g.direction=GateDirection::ForceForward; p.gates={g}; e.setChunkParams(p);
   auto y=e.processFinite({1,2,3,4},1);
   CHECK("force-forward gate cancels parent reverse", y==std::vector<float>({1,2,3,4}));
 }
 {
   EngineConfig c; c.sampleRate=4.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; e.setChunkParams(p); e.setDryWet(0,1);
   std::vector<float> in={1,2,3,4,5,6,7,8}, out(8);
   e.processLive(in,out);
   CHECK("live first chunk is latency silence", near(out[0],0)&&near(out[1],0)&&near(out[2],0)&&near(out[3],0));
   CHECK("live second chunk emits first reversed", near(out[4],4)&&near(out[5],3)&&near(out[6],2)&&near(out[7],1));
 }
 {
   EngineConfig c; c.sampleRate=100.0; c.channels=1; c.maxChunkFrames=1000;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; p.temporalMode=TemporalMode::TimeStretch; p.ratio=0.5; e.setChunkParams(p);
   std::vector<float> in(100); for(int i=0;i<100;++i) in[i]=std::sin(i*0.2f);
   auto y=e.processFinite(in,1);
   CHECK("time stretch half-rate expands duration", y.size()==200);
 }
 {
   EngineConfig c; c.sampleRate=4.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c);
   std::vector<float> d={0,10,20,30};
   CHECK("scratch interpolation", near(e.scrubSample(d,1,0,1.5),15));
 }

 {
   EngineConfig c; c.sampleRate=4.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams base; base.durationSeconds=1.0; e.setChunkParams(base);
   ChunkParams a=base,b=base; a.durationSeconds=0.5; b.durationSeconds=1.0; e.setChunkPattern({a,b});
   auto y=e.processFinite({0,1,2,3,4,5,6,7},1);
   CHECK("mixed chunk sizes use cumulative boundaries", y==std::vector<float>({1,0,5,4,3,2,7,6}));
 }
 {
   EngineConfig c; c.sampleRate=8.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; e.setChunkParams(p);
   EffectSettings fx; fx.stutter.enabled=true; fx.stutter.periodFrames=4; fx.stutter.repeatFrames=2; fx.stutter.repeats=1; fx.stutter.wet=1; fx.stutter.dry=0;
   fx.delay.enabled=true; fx.delay.delayFrames=1; fx.delay.feedback=0; fx.delay.wet=1; fx.delay.dry=0;
   fx.chain={EffectType::Stutter,EffectType::Delay,EffectType::Echo}; e.setEffects(fx);
   auto first=e.processFinite({0,1,2,3,4,5,6,7},1);
   e.reset(); fx.chain={EffectType::Delay,EffectType::Stutter,EffectType::Echo}; e.setEffects(fx);
   auto second=e.processFinite({0,1,2,3,4,5,6,7},1);
   CHECK("effect chain order changes rendered result", first!=second);
 }

 {
   EngineConfig c; c.sampleRate=4.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; p.gates.assign(64,GateStep{}); p.activeGateCount=1; p.gates[0].enabled=false; for(std::size_t i=1;i<64;++i)p.gates[i].enabled=true; e.setChunkParams(p);
   auto y=e.processFinite({1,2,3,4},1); bool allzero=true; for(float v:y)allzero&=near(v,0);
   CHECK("active gate count ignores inactive capacity",allzero);
 }

 {
   auto p=parseUserPattern("0,+2,REST,1*2",8,42);
   CHECK("pattern DSL absolute relative rest repeat",p==std::vector<int>({0,2,-1,1,1}));
 }
 {
   auto p=parseUserPattern("0 2 REST 1*2",8,42);
   CHECK("pattern DSL accepts whitespace separators",p==std::vector<int>({0,2,-1,1,1}));
 }
 {
   auto a=parseUserPattern("3@50,4@50,5@50,6@50",8,999);
   auto b=parseUserPattern("3@50,4@50,5@50,6@50",8,999);
   CHECK("pattern probability deterministic by seed",a==b);
 }
 {
   EngineConfig c; c.sampleRate=1.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=2.0; e.setChunkParams(p);e.setOrderMode(OrderMode::UserPattern);e.setUserOrder({0,-1,1});
   auto y=e.processFinite({1,2,3,4},1);
   CHECK("REST token renders silence",y==std::vector<float>({2,1,0,0,4,3}));
 }

 {
   EngineConfig c; c.sampleRate=8.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; GateStep g; g.shape=GateShape::Custom; g.width=1.0f; g.gap=0; g.customCurve={0,0,0,0,1,1,1,1}; p.gates={g};e.setChunkParams(p);
   auto y=e.processFinite({1,1,1,1,1,1,1,1},1);
   CHECK("custom gate curve changes amplitude",y.front()<0.1f && y.back()>0.9f);
 }
 {
   EngineConfig c; c.sampleRate=4.0; c.channels=1; c.maxChunkFrames=100;
   BackReverseEngine e(c); ChunkParams p; p.durationSeconds=1.0; GateStep g; g.width=0.5f;g.gap=0.5f;g.gapMode=GapMode::HoldPrevious;p.gates={g};e.setChunkParams(p);
   auto y=e.processFinite({1,2,3,4},1);
   CHECK("hold gap repeats prior gated sample",near(y[2],y[1])&&near(y[3],y[1]));
 }

 {
   EngineConfig c; c.sampleRate=8.0;c.channels=1;c.maxChunkFrames=100;
   BackReverseEngine e(c);ChunkParams p;p.durationSeconds=1.0;GateStep a,b;a.stutter=true;b.stutter=false;p.gates={a,b};p.activeGateCount=2;e.setChunkParams(p);
   EffectSettings fx;fx.stutter.enabled=true;fx.stutter.periodFrames=2;fx.stutter.repeatFrames=1;fx.stutter.repeats=1;fx.stutter.wet=1;fx.stutter.dry=0;e.setEffects(fx);
   auto y=e.processFinite({1,2,3,4,5,6,7,8},1);
   CHECK("per-gate stutter assignment preserves unassigned region",y.size()==8 && std::isfinite(y[7]));
 }
 std::cout<<(failures?"TESTS FAILED":"ALL TESTS PASSED")<<"\n";
 return failures?1:0;
}
