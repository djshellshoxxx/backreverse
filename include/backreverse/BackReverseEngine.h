#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace br {

enum class ReverseMode { SequentialChunks, WholeSource, ReorderedChunks, FreeScrub, Hybrid };
enum class TemporalMode { Rate, TimeStretch };
enum class BoundaryMode { Hard, LinearCrossfade, EqualPowerCrossfade };
enum class OrderMode { Sequential, ReverseOrder, Random, ShuffleNoRepeat, PingPong, OddsThenEvens, EvensThenOdds, RotateLeft, RotateRight, UserPattern };
enum class GateShape { Rectangular, LinearIn, LinearOut, Triangle, EqualPower, Sine, Exponential, Logarithmic };
enum class GapMode { Silence, DryThrough, HoldPrevious, Crossfade, EffectTailOnly };
enum class GateDirection { Inherit, ForceReverse, ForceForward };
enum class ScratchMode { Linear, Vinyl, TapeShuttle, Fine };
enum class ReleaseMode { Latch, SpringReturn, Continue };

struct GateStep {
    bool enabled {true};
    GateShape shape {GateShape::Rectangular};
    GapMode gapMode {GapMode::Silence};
    GateDirection direction {GateDirection::Inherit};
    float width {1.0f};
    float gap {0.0f};
    float depth {1.0f};
    bool stutter {false};
    bool delay {false};
    bool echo {false};
};
struct ChunkParams {
    double durationSeconds {5.0};
    TemporalMode temporalMode {TemporalMode::Rate};
    double ratio {1.0};
    bool preservePitch {true};
    float pan {0.0f};
    bool swapStereo {false};
    bool invertLeft {false};
    bool invertRight {false};
    double phaseDegrees {0.0};
    std::vector<GateStep> gates {};
    std::size_t activeGateCount {0};
};
struct EngineConfig {
    double sampleRate {48000.0};
    std::size_t channels {2};
    std::size_t maxChunkFrames {48000 * 600};
    BoundaryMode boundary {BoundaryMode::EqualPowerCrossfade};
    std::size_t crossfadeFrames {64};
    ReverseMode reverseMode {ReverseMode::SequentialChunks};
    OrderMode orderMode {OrderMode::Sequential};
    std::uint64_t randomSeed {0xBACC0FFEEULL};
};
enum class EffectType { Stutter, Delay, Echo };
struct EffectSettings {
    std::array<EffectType,3> chain { EffectType::Stutter, EffectType::Delay, EffectType::Echo };
    struct Stutter { bool enabled {false}; std::size_t periodFrames {2400}; std::size_t repeatFrames {1200}; int repeats {2}; float decay {1.0f}; float wet {1.0f}; float dry {0.0f}; bool alternateDirection {false}; } stutter;
    struct Delay { bool enabled {false}; std::size_t delayFrames {12000}; float feedback {0.25f}; float wet {0.25f}; float dry {1.0f}; bool pingPong {false}; } delay;
    struct Echo { bool enabled {false}; std::size_t delayFrames {18000}; float feedback {0.35f}; float damping {0.2f}; float spread {0.0f}; float drift {0.0f}; float wowFlutter {0.0f}; float wet {0.25f}; float dry {1.0f}; } echo;
};
struct ScratchState { ScratchMode mode {ScratchMode::Vinyl}; ReleaseMode release {ReleaseMode::Continue}; double positionFrames {0.0}; double velocity {0.0}; double inertia {0.85}; double friction {0.1}; double maxRate {8.0}; bool reverseOnly {false}; };

class BackReverseEngine {
public:
    explicit BackReverseEngine(EngineConfig cfg = {});
    void prepare(double sampleRate, std::size_t channels, std::size_t maxChunkFrames);
    void reset();
    void setChunkParams(const ChunkParams& params);
    void setChunkPattern(std::vector<ChunkParams> pattern);
    void setReverseMode(ReverseMode mode) noexcept { cfg_.reverseMode=mode; }
    void setOrderMode(OrderMode mode);
    void setUserOrder(std::vector<int> order);
    void setUserPatternText(std::string text) { userPatternText_=std::move(text); }
    void setRandomSeed(std::uint64_t seed);
    void setEffects(EffectSettings settings);
    void setScratchState(ScratchState state);
    void setDryWet(float dry, float wet);
    std::size_t latencyFrames() const;
    std::vector<float> processFinite(const std::vector<float>& interleaved, std::size_t channels);
    void processLive(std::span<const float> inputInterleaved, std::span<float> outputInterleaved);
    float scrubSample(const std::vector<float>& interleaved, std::size_t channels, std::size_t channel, double positionFrames) const;
private:
    EngineConfig cfg_; ChunkParams chunk_; std::vector<ChunkParams> pattern_; std::vector<int> userOrder_; std::string userPatternText_; EffectSettings fx_; ScratchState scratch_;
    float dry_ {0.0f}; float wet_ {1.0f}; std::mt19937_64 rng_;
    std::vector<float> liveCapture_,livePlayback_; std::size_t captureFrames_ {0},playbackFrame_ {0}; bool playbackReady_ {false};
    std::vector<float> delayBuffer_; std::size_t delayWrite_ {0}; std::vector<float> echoBuffer_; std::size_t echoWrite_ {0};
    std::size_t chunkFrames(const ChunkParams& p) const; std::vector<std::size_t> makeOrder(std::size_t chunkCount);
    std::vector<float> renderChunk(std::span<const float>,std::size_t,std::size_t,std::size_t,const ChunkParams&);
    void applyStereoAndPhase(std::vector<float>&,std::size_t,const ChunkParams&) const;
    void applyGates(std::vector<float>&,std::size_t,const ChunkParams&) const;
    void applyEffects(std::vector<float>&,std::size_t);
    static float interpolate(std::span<const float>,std::size_t,std::size_t,double);
    static float gateGain(GateShape,double);
};
} // namespace br
