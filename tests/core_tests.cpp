// BackReverse core DSP tests (spec 06). Plain C++ so they run on every CI platform.
#include "../src/core/Engine.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace br;
static int failures = 0, checks = 0;
#define CHECK(c, msg) do { ++checks; if (!(c)) { ++failures; std::printf ("FAIL %s:%d %s\n", __FILE__, __LINE__, std::string (msg).c_str()); } } while (0)

struct Rig
{
    std::unique_ptr<Engine> e = std::make_unique<Engine>();
    EngineParams P; Patterns pat; HostInfo host;
    std::vector<float> fl, fr; SourceBuffer sb; double sr;
    explicit Rig (double rate, int block = 512) : sr (rate)
    {
        e->prepare (rate, block, 48.0, 2.0);
        P.xfade = Xfade::Hard; P.loop = false; P.mix = 1;
    }
    void file (const std::vector<float>& d) { fl = d; fr = d; sb.L = fl.data(); sb.R = fr.data(); sb.length = (i64) d.size(); sb.sampleRate = sr; e->setSource (&sb); P.source = Source::File; }
    void chunkFrames (double f) { P.chunkMs = (float) (f * 1000.0 / sr); }
    std::vector<float> run (int frames, int block = 512, const std::vector<float>* input = nullptr, std::vector<float>* right = nullptr, std::function<int(int)> blockFn = {})
    {
        std::vector<float> out ((size_t) frames), outR ((size_t) frames), zero ((size_t) std::max (block, 4096), 0.0f);
        int pos = 0, bi = 0;
        while (pos < frames)
        {
            int b = blockFn ? blockFn (bi++) : block; b = std::min (b, frames - pos);
            const float* in = input ? input->data() + pos : zero.data();
            if (! input && b > (int) zero.size()) b = (int) zero.size();
            e->process (in, in, out.data() + pos, outR.data() + pos, b, P, pat, host);
            pos += b;
        }
        if (right) *right = outR;
        return out;
    }
    void play() { e->ctl.transport.store (1); }
};

// ramps are scaled by 2^-14 (exact in float) so values stay inside the engine's +-8 output safety clamp
constexpr float kS = 1.0f / 16384.0f;
static std::vector<float> ramp (int n, float offset = 0) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = ((float) i + offset) * kS; return v; }
static std::vector<float> S (std::initializer_list<float> l) { std::vector<float> v; for (float x : l) v.push_back (x * kS); return v; }
static int I (float v) { return (int) std::lround (v / kS); }
static bool same (const std::vector<float>& a, const std::vector<float>& b, float tol = 1e-5f)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (std::fabs (a[i] - b[i]) > tol) return false;
    return true;
}
static std::string str (const std::vector<float>& v, size_t n) { std::string s; for (size_t i = 0; i < std::min (n, v.size()); ++i) s += std::to_string (I (v[i])) + ","; return s; }

static void reverseCorrectness()
{
    { Rig r (1000); r.file (ramp (8)); r.chunkFrames (4); r.play(); auto o = r.run (8);
      CHECK (same (o, S ({ 3, 2, 1, 0, 7, 6, 5, 4 })), "chunk4 -> " + str (o, 8)); }
    { Rig r (1000); r.file (ramp (8)); r.P.revMode = RevMode::WholeSource; r.play(); auto o = r.run (8);
      CHECK (same (o, S ({ 7, 6, 5, 4, 3, 2, 1, 0 })), "whole -> " + str (o, 8)); }
    { Rig r (1000); r.file (ramp (6)); r.chunkFrames (4); r.play(); auto o = r.run (8);
      CHECK (same (o, S ({ 3, 2, 1, 0, 5, 4, 0, 0 })), "partial -> " + str (o, 8)); CHECK (r.e->tel.ended.load(), "file end reached"); }
    { Rig r (1000); auto in = ramp (12); r.P.source = Source::Live; r.chunkFrames (4); auto o = r.run (12, 512, &in);
      CHECK (same (o, S ({ 0, 0, 0, 0, 3, 2, 1, 0, 7, 6, 5, 4 })), "live chunk4 -> " + str (o, 12)); CHECK (r.e->tel.latency.load() == 4, "live latency 4"); }
}

static void productExamples()
{
    auto check = [] (double fileSec, double chunkSec)
    {
        const double sr = 100; const int N = (int) (fileSec * sr); const int C = (int) (chunkSec * sr);
        Rig r (sr); r.file (ramp (N)); r.chunkFrames (C); r.play(); auto o = r.run (N);
        bool ok = true;
        for (int j = 0; j < N && ok; ++j) { const int k = j / C; const int len = std::min (C, N - k * C); const int expect = k * C + len - 1 - (j - k * C); ok = I (o[(size_t) j]) == expect; }
        CHECK (ok, "example " + std::to_string (fileSec) + "s / " + std::to_string (chunkSec) + "s");
    };
    check (120, 30); check (120, 10); check (180, 180);
    // chunk == length must equal whole-source reverse
    Rig a (100), b (100); a.file (ramp (18000)); b.file (ramp (18000)); a.chunkFrames (18000); b.P.revMode = RevMode::WholeSource; a.play(); b.play();
    CHECK (same (a.run (18000), b.run (18000)), "180s chunk == whole-source reverse");
}

static void fractionalChunks()
{
    for (double ms : { 1.0, 7.5, 50.0, 125.5, 333.333, 500.0, 1250.0 })
    {
        Rig r (44100); const int N = 88200; r.file (ramp (N)); r.P.chunkMs = (float) ms; r.play(); auto o = r.run (N + 100);
        std::vector<float> got (o.begin(), o.begin() + N); std::sort (got.begin(), got.end());
        bool perm = true; for (int i = 0; i < N; ++i) if (I (got[(size_t) i]) != i) { perm = false; break; }
        CHECK (perm, "fractional chunk " + std::to_string (ms) + " ms: every sample exactly once");
        bool tail = true; for (int i = N; i < N + 100; ++i) tail = tail && o[(size_t) i] == 0; CHECK (tail, "no extra samples after end");
    }
}

static void blockSizes()
{
    auto ref = [] (int block, std::function<int(int)> fn)
    {
        Rig r (48000, 1024); r.file (ramp (48000)); r.P.chunkMs = 37.3f; r.P.revMode = RevMode::Reordered; r.P.order = Order::Shuffle; r.P.patLen = 5; r.play();
        return r.run (48000, block, nullptr, nullptr, fn);
    };
    const auto base = ref (1, {});
    for (int b : { 16, 32, 64, 128, 256, 511, 512, 1024 }) CHECK (same (base, ref (b, {})), "block size " + std::to_string (b) + " matches per-sample reference");
    CHECK (same (base, ref (0, [] (int i) { return 1 + (int) (mix64 ((uint64_t) i) % 700); })), "varying block sizes match");
    // live mode, too
    auto liveRef = [] (std::function<int(int)> fn) { Rig r (48000, 1024); auto in = ramp (24000, 1); r.P.source = Source::Live; r.P.chunkMs = 12.34f; return r.run (24000, 64, &in, nullptr, fn); };
    CHECK (same (liveRef ([] (int) { return 1; }), liveRef ([] (int i) { return 1 + (int) (mix64 ((uint64_t) i + 5) % 900); })), "live chunks independent of host blocks");
}

static int zeroCrossings (const std::vector<float>& v, int from, int to) { int z = 0; for (int i = from + 1; i < to; ++i) if ((v[(size_t) i - 1] < 0) != (v[(size_t) i] < 0)) ++z; return z; }

static void rateAndStretch()
{
    const double sr = 48000; const int N = 48000;
    std::vector<float> sine ((size_t) N); for (int i = 0; i < N; ++i) sine[(size_t) i] = std::sin (2 * 3.14159265 * 220.0 * i / sr);
    const int inZ = zeroCrossings (sine, 0, N);
    for (float ratio : { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f })
    {
        for (int mode = 0; mode < 2; ++mode)
        {
            Rig r (sr); r.file (sine); r.P.chunkMs = 250; r.P.ratio = ratio; r.P.timeMode = mode ? TimeMode::Stretch : TimeMode::Rate; r.play();
            const int expect = (int) std::lround (N / ratio);
            auto o = r.run (expect + 4800, 512);
            int last = 0; for (int i = 0; i < (int) o.size(); ++i) if (std::fabs (o[(size_t) i]) > 1e-6f) last = i;
            CHECK (std::abs (last - expect) < 600, "duration ratio " + std::to_string (ratio) + (mode ? " stretch" : " rate") + " got " + std::to_string (last) + " expected " + std::to_string (expect));
            const int oz = zeroCrossings (o, 0, expect);
            const double rateHz = (double) oz / ((double) expect / sr) / 2.0;
            const double want = mode ? 220.0 : 220.0 * ratio;
            CHECK (std::fabs (rateHz - want) / want < 0.12, "pitch " + std::string (mode ? "preserved" : "follows speed") + " at " + std::to_string (ratio) + ": " + std::to_string (rateHz) + " Hz");
        }
    }
    (void) inZ;
}

static void temporalPattern()
{
    Rig r (1000, 1); r.file (ramp (1000)); r.chunkFrames (60); r.P.tempoAssign = TempoAssign::CycleLane; r.pat.rateLen = 5;
    const float rs[5] = { 1, 0.5f, 2, 0.25f, 3 }; for (int i = 0; i < 5; ++i) r.pat.rate[i].ratio = rs[i];
    r.P.loop = true; r.play();
    std::vector<long long> idx; std::vector<float> spd; float buf[2]; float in = 0;
    for (int i = 0; i < 4000; ++i) { r.e->process (&in, &in, buf, buf + 1, 1, r.P, r.pat, r.host); idx.push_back (r.e->tel.chunkIdx.load()); spd.push_back (r.e->tel.speed.load()); }
    // each chunk m must run with ratio rs[m % 5] for 60/ratio frames
    bool ok = true; int run = 1;
    for (size_t i = 1; i < idx.size() && ok; ++i)
    {
        if (idx[i] == idx[i - 1]) { ++run; continue; }
        const long long m = idx[i - 1]; const float want = rs[m % 5];
        if (std::fabs (spd[i - 1] - want) > 1e-6f) ok = false;
        if (m > 0 && m < 16 && std::abs (run - (int) std::lround (60 / want)) > 1) ok = false;
        run = 1;
    }
    CHECK (ok, "per-chunk temporal pattern 1x,0.5x,2x,0.25x,3x");
}

static std::vector<float> randomRender (int seed, int block)
{
    Rig r (48000, 1024); r.file (ramp (96000, 1)); r.P.chunkMs = 40; r.P.revMode = RevMode::Reordered; r.P.order = Order::Random; r.P.patLen = 8;
    r.P.seed = seed; r.P.freeze = false; r.P.sizeMode = SizeMode::RandomList; r.pat.sizeCount = 3; r.pat.sizeMult[1] = 0.5f; r.pat.sizeMult[2] = 2;
    r.P.tempoAssign = TempoAssign::RandomSet; r.P.panMode = PanMode::Random; r.P.polMode = PolMode::Random; r.P.revScope = RevScope::Random;
    r.P.gateOn = true; r.P.gateSteps = 8; r.pat.gate[3].on = 0; r.P.stutOn = true; r.P.stutScope = Scope::Random; r.P.stutProb = 0.5f; r.P.loop = true;
    r.P.xfade = Xfade::EqualPower; r.play();
    return r.run (96000, block);
}

static void determinism()
{
    const auto a = randomRender (42, 512), b = randomRender (42, 512), c = randomRender (43, 512), d = randomRender (42, 37);
    CHECK (same (a, b, 0), "fixed seed repeats exactly");
    CHECK (! same (a, c), "different seed differs");
    CHECK (same (a, d, 1e-6f), "offline/realtime (block size) render identical");
}

static void gates()
{
    Rig r (1000); r.file (ramp (16)); r.chunkFrames (16); r.P.gateOn = true; r.P.gateSteps = 2; r.P.gateWidth = 1; r.pat.gate[1].dir = 2; r.play();
    auto o = r.run (16);
    CHECK (same (o, S ({ 15, 14, 13, 12, 11, 10, 9, 8, 0, 1, 2, 3, 4, 5, 6, 7 })), "force-forward gate inside reversed chunk -> " + str (o, 16));
    for (int steps : { 2, 4, 8, 16, 32, 64 })
    {
        Rig g (48000); std::vector<float> one (48000, 1.0f); g.file (one); g.P.chunkMs = 500; g.P.gateOn = true; g.P.gateSteps = steps; g.P.gateWidth = 0.5f; g.P.gateDepth = 1; g.play();
        auto o2 = g.run (24000); double energy = 0; for (float v : o2) energy += v; energy /= 24000;
        CHECK (energy > 0.4 && energy < 0.6, "gate " + std::to_string (steps) + " steps half-width ~50% duty: " + std::to_string (energy));
    }
    for (int s = 0; s <= (int) Shape::Custom; ++s)
    {
        Rig g (48000); std::vector<float> one (48000, 1.0f); g.file (one); g.P.gateOn = true; g.P.gateShape = (Shape) s; g.play();
        auto o2 = g.run (24000); bool fin = true; for (float v : o2) fin = fin && std::isfinite (v) && v <= 1.0001f && v >= -0.0001f;
        CHECK (fin, "gate shape " + std::to_string (s) + " bounded");
    }
    { // disabled cell silences, dry-through gap passes dry
        Rig g (48000); std::vector<float> one (48000, 1.0f); g.file (one); g.P.chunkMs = 100; g.P.gateOn = true; g.P.gateSteps = 2; g.P.gateWidth = 1; g.pat.gate[1].on = 0; g.play();
        auto o2 = g.run (4800); CHECK (std::fabs (o2[1000]) > 0.99f && std::fabs (o2[3500]) < 1e-3f, "disabled gate cell silent");
        Rig h (48000); h.file (one); h.P.chunkMs = 100; h.P.gateOn = true; h.P.gateSteps = 2; h.P.gateWidth = 1; h.pat.gate[1].on = 0; h.P.gapMode = GapMode::DryThrough; h.play();
        auto o3 = h.run (4800); CHECK (std::fabs (o3[3500] - 1) < 1e-3f, "dry-through gap");
    }
}

static void panPhase()
{
    std::vector<float> L (9600, 1.0f), R (9600, 0.0f);
    auto mk = [&] (std::function<void(Rig&)> cfg, std::vector<float>& outR) { Rig r (48000); r.file (L); r.fr = R; r.sb.R = r.fr.data(); r.P.chunkMs = 50; cfg (r); r.play(); return r.run (9600, 512, nullptr, &outR); };
    std::vector<float> oR;
    auto oL = mk ([] (Rig& r) { r.P.swap = SwapMode::On; }, oR); CHECK (std::fabs (oL[9000]) < 1e-3f && std::fabs (oR[9000] - 1) < 1e-3f, "L/R swap");
    oL = mk ([] (Rig& r) { r.P.polMode = PolMode::Left; }, oR); CHECK (std::fabs (oL[9000] + 1) < 1e-3f, "polarity invert L");
    oL = mk ([] (Rig& r) { r.P.polMode = PolMode::Both; r.fr.assign (9600, 1.0f); }, oR); CHECK (std::fabs (oL[9000] + 1) < 1e-3f && std::fabs (oR[9000] + 1) < 1e-3f, "polarity invert both");
    oL = mk ([] (Rig& r) { r.P.polMode = PolMode::Right; r.fr.assign (9600, 1.0f); }, oR); CHECK (std::fabs (oL[9000] - 1) < 1e-3f && std::fabs (oR[9000] + 1) < 1e-3f, "polarity invert R");
    oL = mk ([] (Rig& r) { r.P.panMode = PanMode::Static; r.P.pan = 1; }, oR); CHECK (std::fabs (oL[9000]) < 1e-3f, "static pan right");
    oL = mk ([] (Rig& r) { r.P.panMode = PanMode::Static; r.P.pan = 1; r.P.panMirror = true; }, oR); CHECK (std::fabs (oL[9000] - 1) < 1e-3f, "mirrored pan");
    oL = mk ([] (Rig& r) { r.P.panMode = PanMode::Alternate; r.P.pan = 0; r.P.panDepth = 1; r.P.panSmoothMs = 1; }, oR);
    CHECK (std::fabs (oL[2000] - 1) < 1e-2f && std::fabs (oL[4400]) < 1e-2f, "alternating pan per chunk");
    for (int m = 1; m <= 2; ++m)
    {
        Rig r (48000); Rng g (5); std::vector<float> nz (48000); for (auto& v : nz) v = (float) (g.next() * 2 - 1); r.file (nz);
        r.P.phaseMode = (PhaseMode) m; r.P.phaseDeg = 90; r.P.panMode = PanMode::Auto; r.play(); auto o = r.run (48000);
        bool ok = true; float pk = 0; for (float v : o) { ok = ok && std::isfinite (v); pk = std::max (pk, std::fabs (v)); }
        CHECK (ok && pk < 4, "phase rotation stable");
    }
}

static void safetyAndFx()
{
    { Rig r (48000); r.P.source = Source::Live; std::vector<float> in (4800, 0.5f); in[100] = NAN; in[200] = INFINITY; in[300] = -INFINITY;
      r.P.stutOn = r.P.dlyOn = r.P.echoOn = true; r.P.phaseMode = PhaseMode::RotateBoth; auto o = r.run (4800, 512, &in);
      bool ok = true; for (float v : o) ok = ok && std::isfinite (v); CHECK (ok, "NaN/Inf never reach output"); }
    auto chain = [] (int order)
    {
        Rig r (48000); Rng g (9); std::vector<float> nz (48000); for (auto& v : nz) v = (float) (g.next() * 2 - 1); r.file (nz);
        r.P.stutOn = r.P.dlyOn = r.P.echoOn = true; r.P.chainOrder = order; r.P.stutPeriodMs = 200; r.P.echoChar = EchoChar::Dub; r.P.dlyMix = 0.5f; r.play();
        return r.run (48000);
    };
    const auto c0 = chain (0), c3 = chain (3), c4 = chain (4);
    CHECK (! same (c0, c3, 1e-3f) && ! same (c0, c4, 1e-3f) && ! same (c3, c4, 1e-3f), "chain order alters output (S>D>E, D>E>S, E>S>D)");
    { Rig r (48000); std::vector<float> in (48000 * 10, 0.0f); Rng g (3); for (int i = 0; i < 48000; ++i) in[(size_t) i] = (float) (g.next() * 2 - 1);
      r.P.source = Source::Live; r.P.chunkMs = 20; r.P.dlyOn = true; r.P.dlyFb = 0.95f; r.P.dlyXfb = 0.95f; r.P.dlyPing = true; r.P.echoOn = true; r.P.echoFb = 0.95f; r.P.echoChar = EchoChar::Dub;
      auto o = r.run ((int) in.size(), 512, &in); float pk = 0; for (float v : o) pk = std::max (pk, std::fabs (v)); CHECK (pk < 4 && std::isfinite (pk), "feedback protected: peak " + std::to_string (pk)); }
    { Rig r (48000); auto in = ramp (4800); for (auto& v : in) v = std::sin (v / kS * 0.01f); r.P.source = Source::Live; r.P.chunkMs = 10; r.P.mix = 1; r.P.stutOn = true; r.P.stutWet = 0; r.P.stutDry = 1;
      auto o = r.run (4800, 512, &in); Rig q (48000); q.P.source = Source::Live; q.P.chunkMs = 10; auto o2 = q.run (4800, 512, &in);
      CHECK (same (o, o2, 1e-5f), "stutter dry-only passes signal"); }
    for (int dir = 0; dir < 4; ++dir)
    { Rig r (48000); r.file (ramp (48000)); r.P.stutOn = true; r.P.stutDir = (StutDir) dir; r.P.stutRetrig = Retrig::ChunkStart; r.play(); auto o = r.run (48000);
      bool ok = true; for (float v : o) ok = ok && std::isfinite (v); CHECK (ok, "stutter direction mode " + std::to_string (dir)); }
    for (int s = 0; s < 5; ++s)
    { Rig r (48000); std::vector<float> one (48000, 1.0f); r.file (one); r.P.dlyOn = true; r.P.dlyScope = (Scope) s; r.P.gateOn = true; r.pat.gate[0].fx = 2; r.play(); auto o = r.run (48000);
      bool ok = true; for (float v : o) ok = ok && std::isfinite (v); CHECK (ok, "delay scope " + std::to_string (s)); }
}

static void scrub()
{
    for (int mode = 0; mode < 4; ++mode)
    {
        Rig r (48000, 64); r.file (ramp (48000, 1)); r.P.scrMode = (ScrMode) mode; r.P.loop = true; r.play(); r.run (4800, 64);
        r.e->ctl.scrubHeld.store (true);
        Rng g (11); bool ok = true; float buf[64], bufR[64], zero[64] = {};
        for (int b = 0; b < 1500; ++b)
        {
            r.e->ctl.scrubNorm.store ((b / 7) % 2 ? 0.9 : 0.1); r.e->ctl.shuttle.store ((b / 5) % 2 ? 1.0 : -1.0);
            r.e->process (zero, zero, buf, bufR, 64, r.P, r.pat, r.host);
            for (float v : buf) ok = ok && std::isfinite (v) && v >= -kS && v <= 48001 * kS;
            const double p = r.e->tel.posNorm.load(); ok = ok && p >= 0 && p <= 1;
        }
        r.e->ctl.scrubHeld.store (false); r.run (48000, 64);
        CHECK (ok, "rapid scrub mode " + std::to_string (mode) + " stays in valid region");
    }
    Rig s (1000, 1); s.file (ramp (1000, 1)); s.P.revMode = RevMode::FreeScrub; s.P.ratio = 1; s.e->ctl.seekNorm.store (0.5); s.play();
    auto o = s.run (400, 1); CHECK (o[399] > 0 && o[399] < o[300], "free scrub motor plays backwards");
}

static void latency()
{
    Rig r (48000); auto in = ramp (48000, 1); r.P.source = Source::Live; r.P.chunkMs = 100; auto o = r.run (48000, 256, &in);
    int first = -1; for (int i = 0; i < 48000; ++i) if (o[(size_t) i] != 0) { first = i; break; }
    CHECK (first >= 4800, "first reversed output not before chunk available: " + std::to_string (first));
    CHECK (r.e->tel.latency.load() == 4800, "displayed latency == chunk");
    Rig f (48000); f.P.source = Source::Live; f.P.latPolicy = LatPolicy::FixedMax; f.P.maxBufferMs = 1000; bool stable = true;
    for (float ms : { 10.f, 100.f, 500.f, 999.f }) { f.P.chunkMs = ms; f.run (4800, 480, &in); stable = stable && f.e->tel.latency.load() == 48000; }
    CHECK (stable, "fixed-max latency stable while chunk changes");
    Rig a (48000); a.P.source = Source::Live; a.P.chunkMs = 100; a.P.mix = 0; auto o2 = a.run (48000, 256, &in);
    CHECK (std::fabs (o2[10000] - in[10000 - 4800]) < 1e-3f, "latency-aligned dry path");
    Rig re (48000); re.file (ramp (8)); re.P.revMode = RevMode::Reordered; re.P.order = Order::ReverseOrder; re.P.patLen = 4; re.chunkFrames (2); re.play();
    auto o3 = re.run (8); CHECK (same (o3, S ({ 7, 6, 5, 4, 3, 2, 1, 0 })), "reverse chunk order of reversed chunks -> " + str (o3, 8));
}

int main()
{
    reverseCorrectness(); productExamples(); fractionalChunks(); blockSizes(); rateAndStretch(); temporalPattern();
    determinism(); gates(); panPhase(); safetyAndFx(); scrub(); latency();
    std::printf ("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
