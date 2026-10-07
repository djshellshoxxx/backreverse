// BackReverse shared core: enums, parameter snapshot, pattern state, PRNG, note table.
// Pure C++17 with no framework dependency so every format and the tests share it.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace br {

using i64 = int64_t;

enum class Source { Live, File };
enum class RevMode { Sequential, Reordered, WholeSource, Forward, FreeScrub };
enum class RevScope { All, Alternating, Random, ChunkLane };
enum class LatPolicy { Dynamic, FixedMax };
enum class Xfade { Hard, Micro, EqualPower, ZeroCross };
enum class Order { Sequential, ReverseOrder, Random, Shuffle, PingPong, OddsEvens, EvensOdds, RotateLeft, RotateRight, ABBanks, User };
enum class SizeMode { Fixed, CycleList, Alternate, RandomList };
enum class Restart { Free, HostPlay, Bar, Beat, Manual };
enum class TimeMode { Rate, Stretch };
enum class Quality { Draft, Normal, High };
enum class StretchAlgo { Smooth, Rhythmic, Vocal, Surreal };
enum class TempoAssign { Same, Alternating, CycleLane, RandomSet, Weighted, GateFollow, PatternFollow };
enum class TapeMode { Stop, Start, Both };
enum class PanMode { Off, Static, Auto, Alternate, Random, Lane };
enum class SwapMode { Off, On, Alternate };
enum class PanClock { Chunk, Independent, HostBeat, Gate };
enum class PolMode { None, Left, Right, Both, AltChannels, Lane, Random };
enum class PhaseMode { Off, RotateBoth, LROffset };
enum class GateTiming { FitChunk, FixedMs, HostNote, FreeHz };
enum class Shape { Hard, LinIn, LinOut, Triangle, EqualPower, Sine, Exp, Log, Custom };
enum class GapMode { Silence, DryThrough, Hold, Crossfade, FxTail };
enum class Scope { Global, Alternating, ChunkLane, Random, GateLane };
enum class StutDir { Forward, Reverse, Alternate, Inherit };
enum class Retrig { Period, ChunkStart, GateStart };
enum class EchoChar { Clean, Tape, Vinyl, LoFi, Dub };
enum class ScrMode { Linear, Vinyl, Shuttle, Fine };
enum class ScrRelease { Latch, Spring, Continue };
enum class DryAlign { Aligned, Immediate };

// ---- Musical note table: index 0 = Off, then 13 bases x {straight, dotted, triplet}
constexpr int kNoteCount = 40;
inline const char* noteName (int i)
{
    static const char* n[kNoteCount] = { "Off",
        "1/128","1/128D","1/128T","1/64","1/64D","1/64T","1/32","1/32D","1/32T","1/16","1/16D","1/16T",
        "1/8","1/8D","1/8T","1/4","1/4D","1/4T","1/2","1/2D","1/2T","1 bar","1 bar D","1 bar T",
        "2 bars","2 bars D","2 bars T","4 bars","4 bars D","4 bars T","8 bars","8 bars D","8 bars T",
        "16 bars","16 bars D","16 bars T","32 bars","32 bars D","32 bars T" };
    return n[(i < 0 || i >= kNoteCount) ? 0 : i];
}
// length in quarter-note beats; bars use beatsPerBar
inline double noteBeats (int i, double beatsPerBar = 4.0)
{
    if (i <= 0 || i >= kNoteCount) return 0.0;
    static const double base[13] = { 1.0/32, 1.0/16, 1.0/8, 1.0/4, 1.0/2, 1, 2, -1, -2, -4, -8, -16, -32 };
    const int b = (i - 1) / 3, v = (i - 1) % 3;
    double beats = base[b] > 0 ? base[b] : -base[b] * beatsPerBar;
    return beats * (v == 1 ? 1.5 : v == 2 ? 2.0 / 3.0 : 1.0);
}

// ---- Deterministic hashing PRNG (stateless per index => identical realtime/offline results)
inline uint64_t mix64 (uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
enum Domain : uint64_t { dOrder = 1, dSize, dRate, dRev, dPan, dPol, dGate, dStut, dDly, dEcho, dTape, dRepeat, dSkip, dGrain, dNoise };
inline double hashUnit (uint64_t seed, uint64_t domain, i64 idx)
{
    return (double) (mix64 (mix64 (seed * 0x100000001B3ull + domain) ^ (uint64_t) idx) >> 11) * (1.0 / 9007199254740992.0);
}
struct Rng
{
    uint64_t s = 1;
    explicit Rng (uint64_t seed = 1) : s (seed) {}
    double next() { s = mix64 (s); return (double) (s >> 11) * (1.0 / 9007199254740992.0); }
    int below (int n) { return n <= 1 ? 0 : (int) (next() * n) % n; }
};

// ---- Host-visible parameter snapshot (plain values, read once per block)
struct EngineParams
{
    // global
    bool bypass = false; float inGainDb = 0, outGainDb = 0, mix = 1; DryAlign dryAlign = DryAlign::Aligned;
    Source source = Source::Live; RevMode revMode = RevMode::Sequential; RevScope revScope = RevScope::All; float revProb = 0.5f;
    LatPolicy latPolicy = LatPolicy::Dynamic; float maxBufferMs = 4000; float bpm = 120;
    // chunk
    float chunkMs = 500; int chunkSync = 0; Xfade xfade = Xfade::Micro; float xfadeMs = 4;
    Order order = Order::Sequential; int patLen = 4; int patStart = 0; bool patLoop = true;
    float repeatProb = 0, skipProb = 0; int maxRepeat = 2;
    SizeMode sizeMode = SizeMode::Fixed; float variation = 1; int seed = 1; bool freeze = true; Restart restart = Restart::Free;
    // time
    TimeMode timeMode = TimeMode::Rate; float ratio = 1; bool preserve = true; Quality quality = Quality::Normal;
    StretchAlgo algo = StretchAlgo::Smooth; bool stretchReset = true; TempoAssign tempoAssign = TempoAssign::Same;
    bool tapeOn = false; TapeMode tapeMode = TapeMode::Stop; float tapeLen = 0.3f, tapeProb = 1;
    // pan / phase
    PanMode panMode = PanMode::Off; float pan = 0, panDepth = 1; SwapMode swap = SwapMode::Off; bool panMirror = false;
    PanClock panClock = PanClock::Chunk; float panClockMs = 500, panOffset = 0, panSmoothMs = 20;
    PolMode polMode = PolMode::None; PhaseMode phaseMode = PhaseMode::Off; float phaseDeg = 90;
    // gate
    bool gateOn = false; int gateSteps = 8; GateTiming gateTiming = GateTiming::FitChunk; float gateMs = 125; int gateNote = 13; float gateHz = 4;
    Shape gateShape = Shape::Hard; float gateWidth = 0.75f, gateDepth = 1; GapMode gapMode = GapMode::Silence;
    // stutter
    bool stutOn = false; Scope stutScope = Scope::Global; float stutProb = 1, stutPeriodMs = 500; int stutSync = 0; float stutLenMs = 60;
    int stutRepeats = 4; float stutDecay = 0.1f, stutDrift = 0; StutDir stutDir = StutDir::Forward; Retrig stutRetrig = Retrig::Period;
    float stutWet = 1, stutDry = 0;
    // delay
    bool dlyOn = false; Scope dlyScope = Scope::Global; float dlyProb = 1, dlyTimeL = 375, dlyTimeR = 375; bool dlyLink = true; int dlySync = 0;
    float dlyFb = 0.35f, dlyXfb = 0, dlyLP = 12000, dlyHP = 60; bool dlyPing = false; float dlyModRate = 0.5f, dlyModDepth = 0; bool dlyFreeze = false; float dlyMix = 0.35f;
    // echo
    bool echoOn = false; Scope echoScope = Scope::Global; float echoProb = 1, echoTime = 300; int echoSync = 0; float echoFb = 0.45f, echoDecay = 0.3f,
    echoTone = 0.6f, echoSpread = 0.3f, echoDrift = 0.2f, echoWow = 0.3f; EchoChar echoChar = EchoChar::Tape; float echoMix = 0.35f;
    // chain
    int chainOrder = 0; float chainMix = 1;
    // scratch
    ScrMode scrMode = ScrMode::Vinyl; float scrInertia = 0.5f, scrFriction = 0.3f, scrAccel = 1, scrMaxRate = 4, scrMotor = 0.5f, scrRampMs = 250;
    ScrRelease scrRelease = ScrRelease::Spring; bool scrRevLock = false, scrHoldFreeze = true; bool scrActive = false; float scrPos = 0;
    // file transport
    bool loop = true; bool fileHostSync = false;
};

// ---- Non-automatable pattern/lane state (published lock-free to the audio thread)
constexpr int kMaxSteps = 64, kLane = 16, kSizes = 8, kUserPat = 64, kCurvePts = 8, kMaxGroup = 64;
struct GateStep { uint8_t on = 1, dir = 0 /*0 inherit,1 force rev,2 force fwd*/, fx = 0 /*1 stut,2 dly,4 echo*/; int8_t shape = -1; float width = -1, depth = -1, fadeIn = 0, fadeOut = 0; };
struct RateStep { float ratio = 1; uint8_t stretch = 0; float prob = 1, weight = 1; };
struct UserTok { int8_t kind = 0 /*0 abs,1 rel,2 rest,3 random*/; int16_t val = 0; float prob = 1; };
struct Patterns
{
    int userLen = 4; UserTok user[kUserPat] {};
    int sizeCount = 1; float sizeMult[kSizes] {}; float sizeWeight[kSizes] {};
    int rateLen = 5; RateStep rate[kLane] {};
    int laneLen = 8; uint8_t chunkFlags[kLane] {}; // 1 reverse-select, 2 stutter, 4 delay, 8 echo
    int panLen = 8; float pan[kLane] {};
    int polLen = 4; uint8_t pol[kLane] {}; // 0 none 1 L 2 R 3 both
    GateStep gate[kMaxSteps] {};
    int curveCount = 4; float curveX[kCurvePts] {}, curveY[kCurvePts] {};
    Patterns()
    {
        for (int i = 0; i < kUserPat; ++i) user[i].val = (int16_t) (i % 4);
        for (int i = 0; i < kSizes; ++i) { sizeMult[i] = 1; sizeWeight[i] = 1; }
        const float r[5] = { 1, 0.5f, 2, 0.25f, 3 };
        for (int i = 0; i < kLane; ++i) { rate[i].ratio = r[i % 5]; chunkFlags[i] = 1; pan[i] = (i % 2) ? 0.8f : -0.8f; pol[i] = (uint8_t) (i % 4); }
        const float cx[4] = { 0, 0.2f, 0.6f, 1 }, cy[4] = { 0, 1, 0.5f, 0 };
        for (int i = 0; i < kCurvePts; ++i) { curveX[i] = i < 4 ? cx[i] : 1; curveY[i] = i < 4 ? cy[i] : 0; }
    }
};

// Single-writer seqlock box: the message thread publishes, the audio thread copies without blocking.
template <class T>
struct SeqBox
{
    std::atomic<uint32_t> seq { 0 };
    T data {};
    void write (const T& v)
    {
        seq.fetch_add (1, std::memory_order_acq_rel);
        std::atomic_thread_fence (std::memory_order_release);
        std::memcpy ((void*) &data, (const void*) &v, sizeof (T));
        std::atomic_thread_fence (std::memory_order_release);
        seq.fetch_add (1, std::memory_order_release);
    }
    bool tryRead (T& out, uint32_t& last) const
    {
        const uint32_t s1 = seq.load (std::memory_order_acquire);
        if ((s1 & 1u) || s1 == last) return false;
        std::memcpy ((void*) &out, (const void*) &data, sizeof (T));
        std::atomic_thread_fence (std::memory_order_acquire);
        if (seq.load (std::memory_order_relaxed) != s1) return false;
        last = s1; return true;
    }
};

struct HostInfo { bool valid = false, playing = false; double bpm = 0, ppq = 0, beatsPerBar = 4; i64 samplePos = 0; bool offline = false; };

// A finite stereo source (loaded file or capture). Owned outside the audio thread.
struct SourceBuffer
{
    const float* L = nullptr; const float* R = nullptr; i64 length = 0; double sampleRate = 44100;
};

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

} // namespace br
