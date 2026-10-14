// BackReverse DSP engine. One instance per processor; all memory is allocated in prepare().
#pragma once
#include "Fx.h"

namespace br {

struct GrainCloud
{
    struct G { double pos = 0, inc = 1; int age = -1, len = 1; };
    G g[4]; int untilNext = 0; i64 spawned = 0;
    void reset() { for (auto& x : g) x.age = -1; untilNext = 0; }
};

struct Voice
{
    bool active = false, rest = false, reversed = true, wrap = false, stretch = false, preserve = true, tape = false;
    i64 srcStart = 0, srcLen = 1, outLen = 1, t = 0, chunkIdx = 0, slotIdx = 0; int patPos = 0;
    double speed = 1; bool gateFollow = false; int cells = 1; float cellSpeed[kMaxSteps] {}; double cellPrefix[kMaxSteps + 1] {};
    double lastPos = 0, lastVel = 0, lastDryPos = 0; GrainCloud grains;
};

struct GateState { bool on = false, active = true, flip = false, newCell = false; int step = 0; i64 absCell = 0; double c0 = 0, c1 = 1, phase = 0, gain = 1; uint8_t fx = 0; };

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock, double liveSeconds = 48.0, double captureSeconds = 60.0);
    void reset();
    void setSource (const SourceBuffer* s) { pendingSrc.store (s, std::memory_order_release); }
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  const EngineParams& P, const Patterns& pat, const HostInfo& host);

    // Shared helpers (also used by the UI to draw chunk overlays deterministically)
    static double baseChunkFrames (const EngineParams& P, double sr, double bpm, double beatsPerBar);
    static double sizeMultiplier (const EngineParams& P, const Patterns& pat, i64 m);
    static double maxSizeMultiplier (const EngineParams& P, const Patterns& pat);
    static bool chunkReversed (const EngineParams& P, const Patterns& pat, i64 m);
    static int groupSize (const EngineParams& P);
    static Order effectiveOrder (const EngineParams& P);
    static int buildOrder (Order o, const EngineParams& P, const Patterns& pat, i64 group, int G, int lastRef, int* out);
    static float shapeEnv (Shape s, float x, const Patterns& pat);

    struct Controls
    {
        std::atomic<int> transport { 0 };        // 1 play 2 pause 3 stop 4 return-to-start
        std::atomic<double> seekNorm { -1 };     // file seek request 0..1
        std::atomic<bool> scrubHeld { false };
        std::atomic<double> scrubNorm { 0 };     // scrub target 0..1 of visible source
        std::atomic<double> shuttle { 0 };       // -1..1 tape shuttle / platter velocity
        std::atomic<bool> restart { false }, capture { false }, flush { false };
        std::atomic<float> loopA { 0 }, loopB { 1 };
    } ctl;

    struct Telemetry
    {
        std::atomic<double> posNorm { 0 }, chunkStartNorm { 0 }, chunkLenNorm { 0 };
        std::atomic<long long> chunkIdx { 0 }, writePos { 0 }, validFrom { 0 };
        std::atomic<int> latency { 0 }, gateCell { 0 }, captureLen { 0 }, fxMask { 0 };
        std::atomic<float> speed { 1 }, bpm { 120 }, slotPhase { 0 }, peakL { 0 }, peakR { 0 }, liveFill { 0 }, scrubVel { 0 }, pan { 0 };
        std::atomic<bool> reversed { true }, playing { false }, ended { false }, clamped { false }, scrubbing { false }, stretch { false }, rest { false };
    } tel;

    static constexpr int kPeakHist = 2048;
    std::atomic<float> peakHist[kPeakHist];
    std::atomic<int> peakWrite { 0 };
    int peakHop() const { return peakHopFrames; }
    i64 ringCapacity() const { return ringCap; }
    const float* captureData (int ch) const { return capBuf[ch & 1].data(); }
    int captureCapacity() const { return (int) capBuf[0].size(); }
    int latencyFrames() const { return tel.latency.load(); }
    int maxBlockSize() const { return maxBlock; }   // block size the ring slack was sized for (prepare)
    bool isFilePlaying() const { return filePlaying; }
    double sampleRate() const { return sr; }

private:
    struct Src
    {
        const float* L = nullptr; const float* R = nullptr; i64 cap = 1, lo = 0, hi = 0; bool ring = false;
        inline float at (int ch, i64 i) const
        {
            if (i < lo || i >= hi || L == nullptr) return 0.0f;
            const float* b = ch ? R : L; return ring ? b[i % cap] : b[i];
        }
    };
    const Src& src() const { return isLive ? live : file; }
    void readInterp (double p, double absVel, float& l, float& r) const;
    void grainTick (GrainCloud& gc, double spawnPos, double dir, double inc, bool jitter, float& l, float& r, i64 key);
    int grainLen() const;
    void renderVoice (Voice& v, const GateState* gs, float& l, float& r);
    double brake (const Voice& v, double t, double& d) const;
    double content (const Voice& v, double tb, double& vel) const;
    double srcPos (const Voice& v, double c) const;
    GateState gateAt (const Voice& v, i64 clockNow);
    void startNext();
    void fillVoice (Voice& v, i64 m, i64 chunkStart, i64 chunkLen, i64 outLenLive);
    void liveRebase (bool flush);
    i64 fileInitAt (double posFrames);
    void fileSeek (double posFrames);
    void liveNext();
    void fileNext();
    bool repeatSkip (i64& m, i64 key, bool liveCheck);
    void computeGroupStarts();
    void fileAdvanceGroup();
    double fileChunkStart (i64 m) const;
    void restartPattern();
    i64 computeLatency (bool& clamped);
    bool fxAssigned (Scope s, float prob, uint64_t dom, int bit, const GateState& gs) const;

    double sr = 48000; int maxBlock = 512;
    const EngineParams* Pp = nullptr; const Patterns* pat = nullptr;
    HostInfo host; double bpm = 120, bpb = 4, ppqNow = 0;

    // live
    std::vector<float> ring[2]; i64 ringCap = 1, w = 0, liveLo = 0; Src live, file;
    i64 D = 0, lS0 = 0, lGen = 0, lK = -1, lGroup = -1; double lExact = 0; i64 cStart[256] {}, cLen[256] {};
    std::vector<float> capBuf[2]; int capLen = 0; bool capturing = false;
    // file
    std::atomic<const SourceBuffer*> pendingSrc { nullptr }; const SourceBuffer* curSrc = nullptr;
    bool filePlaying = false, fileEnded = false; i64 regionA = 0, regionB = 0;
    i64 fGroupFirst = -1, fGroup = 0; double fStarts[kMaxGroup + 2] {};
    // scheduling shared
    bool isLive = true; int G = 1, plen = 1, order[2 * kMaxGroup + 2] {}; int patPos = 0; i64 slotCounter = 0;
    bool oneShotDone = false; i64 prevM = -1; int consecutive = 0; int lastRef = -1; bool needRegrid = false;
    double baseFrames = 1; uint64_t gridSig = 0; int Gpending = 1; i64 Dpending = 0; bool chunkStartFlag = false; Source lastSource = Source::Live; RevMode lastMode = RevMode::Sequential;
    Voice cur, fade; int fadeRemain = 0, fadeTotal = 1; bool zcWait = false; int zcRamp = 0; float lastFadeOut = 0;
    i64 clock = 0; i64 lastGateAbs = -1;
    // hold / scrub
    GrainCloud holdCloud, scrubCloud; bool holding = false; double holdPos = 0;
    bool scrubOn = false; double sp = 0, sv = 0; Smooth scrubMix;
    // processors
    Smooth inG, outG, mixS, bypassS, panS, swapS, polL, polR, phaseMix, gateG, postS, playG, sendS[3], fxEn[3];
    Hilbert hil[2]; Stutter stut; Delay dly; Echo echo;
    bool lastHostPlaying = false; i64 expectedHostPos = 0; int peakHopFrames = 1, peakAcc = 0; float peakRun = 0;
    i64 gateAbsCounter = 0;
};

} // namespace br
