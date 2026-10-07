#include "Engine.h"
#include <algorithm>

namespace br {

static inline double fracPos (double x) { return x - std::floor (x); }

// ============================================================ static helpers
double Engine::baseChunkFrames (const EngineParams& P, double sr, double bpm, double bpb)
{
    double f = P.chunkSync > 0 ? noteBeats (P.chunkSync, bpb) * 60.0 / std::max (1.0, bpm) * sr
                               : (double) P.chunkMs * 0.001 * sr;
    return std::max (1.0, f);
}

double Engine::sizeMultiplier (const EngineParams& P, const Patterns& pat, i64 m)
{
    const int cnt = std::clamp (pat.sizeCount, 1, kSizes);
    const double v = clampf (P.variation, 0, 1);
    auto at = [&] (int i) { return (double) clampf (pat.sizeMult[std::clamp (i, 0, cnt - 1)], 0.01f, 64.0f); };
    switch (P.sizeMode)
    {
        case SizeMode::Fixed: return 1.0;
        case SizeMode::CycleList: return 1.0 + v * (at ((int) (m % cnt)) - 1.0);
        case SizeMode::Alternate: return 1.0 + v * ((m & 1) ? (cnt > 1 ? at (1) : 1.0) : at (0)) - v;
        case SizeMode::RandomList:
        {
            const i64 key = P.freeze ? m % std::max (1, P.patLen) : m;
            if (hashUnit ((uint64_t) P.seed, dSize, key) >= v) return 1.0;
            double tot = 0; for (int i = 0; i < cnt; ++i) tot += std::max (0.0f, pat.sizeWeight[i]);
            if (tot <= 0) return at (0);
            double r = hashUnit ((uint64_t) P.seed, dSize + 100, key) * tot;
            for (int i = 0; i < cnt; ++i) { r -= std::max (0.0f, pat.sizeWeight[i]); if (r < 0) return at (i); }
            return at (cnt - 1);
        }
    }
    return 1.0;
}

double Engine::maxSizeMultiplier (const EngineParams& P, const Patterns& pat)
{
    if (P.sizeMode == SizeMode::Fixed) return 1.0;
    const int cnt = std::clamp (pat.sizeCount, 1, kSizes);
    const double v = clampf (P.variation, 0, 1);
    double mx = 1.0;
    for (int i = 0; i < (P.sizeMode == SizeMode::Alternate ? std::min (cnt, 2) : cnt); ++i)
        mx = std::max (mx, 1.0 + v * (clampf (pat.sizeMult[i], 0.01f, 64.0f) - 1.0));
    return mx;
}

bool Engine::chunkReversed (const EngineParams& P, const Patterns& pat, i64 m)
{
    bool sel = true;
    switch (P.revScope)
    {
        case RevScope::All: sel = true; break;
        case RevScope::Alternating: sel = (m % 2) == 0; break;
        case RevScope::Random: sel = hashUnit ((uint64_t) P.seed, dRev, P.freeze ? m % std::max (1, P.patLen) : m) < P.revProb; break;
        case RevScope::ChunkLane: sel = (pat.chunkFlags[(size_t) (m % std::clamp (pat.laneLen, 1, kLane))] & 1) != 0; break;
    }
    return P.revMode == RevMode::Forward ? ! sel : sel;
}

Order Engine::effectiveOrder (const EngineParams& P)
{
    return (P.revMode == RevMode::Reordered || P.revMode == RevMode::Forward) ? P.order : Order::Sequential;
}

int Engine::groupSize (const EngineParams& P)
{
    if (P.revMode == RevMode::WholeSource || effectiveOrder (P) == Order::Sequential) return 1;
    return std::clamp (P.patLen, 1, kMaxGroup);
}

int Engine::buildOrder (Order o, const EngineParams& P, const Patterns& pat, i64 group, int G, int lastRef, int* out)
{
    G = std::max (1, G);
    Rng rng (mix64 ((uint64_t) P.seed * 7919u + dOrder) ^ (P.freeze ? 0ull : mix64 ((uint64_t) group + 17)));
    int len = G;
    const i64 gRot = P.freeze ? 0 : group;
    switch (o)
    {
        case Order::Sequential: for (int i = 0; i < G; ++i) out[i] = i; break;
        case Order::ReverseOrder: for (int i = 0; i < G; ++i) out[i] = G - 1 - i; break;
        case Order::Random: for (int i = 0; i < G; ++i) out[i] = rng.below (G); break;
        case Order::Shuffle:
            for (int i = 0; i < G; ++i) out[i] = i;
            for (int i = G - 1; i > 0; --i) std::swap (out[i], out[rng.below (i + 1)]);
            if (G > 1 && out[0] == lastRef) std::swap (out[0], out[1]);
            break;
        case Order::PingPong:
            len = 0;
            for (int i = 0; i < G; ++i) out[len++] = i;
            for (int i = G - 2; i >= 1; --i) out[len++] = i;
            break;
        case Order::OddsEvens: len = 0; for (int i = 1; i < G; i += 2) out[len++] = i; for (int i = 0; i < G; i += 2) out[len++] = i; break;
        case Order::EvensOdds: len = 0; for (int i = 0; i < G; i += 2) out[len++] = i; for (int i = 1; i < G; i += 2) out[len++] = i; break;
        case Order::RotateLeft: for (int i = 0; i < G; ++i) out[i] = (int) ((i + 1 + gRot) % G); break;
        case Order::RotateRight: for (int i = 0; i < G; ++i) out[i] = (int) (((i - 1 - gRot) % G + G) % G); break;
        case Order::ABBanks:
        {
            const int h = (G + 1) / 2; len = 0;
            for (int i = 0; i < h; ++i) { out[len++] = i; if (h + i < G) out[len++] = h + i; }
            break;
        }
        case Order::User:
        {
            len = std::clamp (pat.userLen, 1, kUserPat); int prev = 0;
            for (int i = 0; i < len; ++i)
            {
                const UserTok& t = pat.user[i]; int ref;
                switch (t.kind)
                {
                    case 1: ref = ((prev + t.val) % G + G) % G; break;
                    case 2: ref = -1; break;
                    case 3: ref = rng.below (G); break;
                    default: ref = ((t.val % G) + G) % G; break;
                }
                if (ref >= 0 && t.prob < 1.0f && rng.next() >= t.prob) ref = -1;
                if (ref >= 0) prev = ref;
                out[i] = ref;
            }
            break;
        }
    }
    len = std::max (1, len);
    const int rot = ((P.patStart % len) + len) % len;
    if (rot) std::rotate (out, out + rot, out + len);
    return len;
}

float Engine::shapeEnv (Shape s, float x, const Patterns& pat)
{
    x = clampf (x, 0, 1);
    const float tri = 1.0f - std::fabs (2 * x - 1);
    switch (s)
    {
        case Shape::Hard: return 1.0f;
        case Shape::LinIn: return x;
        case Shape::LinOut: return 1 - x;
        case Shape::Triangle: return tri;
        case Shape::EqualPower: return std::sin (0.5f * kPi * tri);
        case Shape::Sine: return std::sin (kPi * x);
        case Shape::Exp: return (std::exp (5.0f * (1 - x)) - 1) / (std::exp (5.0f) - 1);
        case Shape::Log: return std::log1p (9.0f * (1 - x)) / std::log (10.0f);
        case Shape::Custom:
        {
            const int n = std::clamp (pat.curveCount, 2, kCurvePts);
            if (x <= pat.curveX[0]) return clampf (pat.curveY[0], 0, 1);
            for (int i = 1; i < n; ++i)
                if (x <= pat.curveX[i])
                {
                    const float dx = std::max (1e-6f, pat.curveX[i] - pat.curveX[i - 1]);
                    return clampf (pat.curveY[i - 1] + (pat.curveY[i] - pat.curveY[i - 1]) * (x - pat.curveX[i - 1]) / dx, 0, 1);
                }
            return clampf (pat.curveY[n - 1], 0, 1);
        }
    }
    return 1.0f;
}

// ============================================================ lifecycle
void Engine::prepare (double rate, int mb, double liveSeconds, double captureSeconds)
{
    sr = rate > 0 ? rate : 48000; maxBlock = std::max (1, mb);
    ringCap = (i64) (liveSeconds * sr) + 4 * (i64) maxBlock;
    for (auto& b : ring) b.assign ((size_t) ringCap, 0.0f);
    for (auto& b : capBuf) b.assign ((size_t) std::max (1.0, captureSeconds * sr), 0.0f);
    stut.prepare (sr); dly.prepare (sr); echo.prepare (sr);
    inG.setTime (sr, 10); outG.setTime (sr, 10); mixS.setTime (sr, 15); bypassS.setTime (sr, 10); swapS.setTime (sr, 15);
    polL.setTime (sr, 1.5); polR.setTime (sr, 1.5); phaseMix.setTime (sr, 20); gateG.setTime (sr, 0.7); playG.setTime (sr, 4); scrubMix.setTime (sr, 5);
    for (auto& s : sendS) s.setTime (sr, 3);
    for (auto& s : fxEn) s.setTime (sr, 10);
    peakHopFrames = (int) std::max<i64> (1, ringCap / kPeakHist);
    reset();
}

void Engine::reset()
{
    w = 0; liveLo = 0; lS0 = 0; lGen = 0; lK = -1; lGroup = -1; lExact = 0; D = 0;
    cur = Voice(); fade = Voice(); fadeRemain = 0; zcWait = false; zcRamp = 0;
    fGroupFirst = -1; patPos = 0; slotCounter = 0; prevM = -1; consecutive = 0; lastRef = -1; oneShotDone = false;
    clock = 0; lastGateAbs = -1; gateAbsCounter = 0; holding = false; scrubOn = false; sp = sv = 0;
    needRegrid = true; gridSig = 0; filePlaying = false; fileEnded = false; capLen = 0;
    stut.reset(); dly.reset(); echo.reset(); for (auto& h : hil) h.reset();
    inG.reset (1); outG.reset (1); mixS.reset (1); bypassS.reset (0); panS.reset (0); swapS.reset (0); polL.reset (1); polR.reset (1);
    phaseMix.reset (0); gateG.reset (1); playG.reset (1); scrubMix.reset (0);
    for (auto& s : sendS) s.reset (0);
    for (auto& s : fxEn) s.reset (0);
    for (auto& p : peakHist) p.store (0);
    peakWrite.store (0); peakAcc = 0; peakRun = 0;
}

// ============================================================ reading
void Engine::readInterp (double p, double absVel, float& l, float& r) const
{
    const Src& s = src();
    const i64 i = (i64) std::floor (p); const float f = (float) (p - (double) i);
    const Quality q = Pp ? Pp->quality : Quality::Normal;
    if (q == Quality::Draft)
    {
        l = s.at (0, i) + f * (s.at (0, i + 1) - s.at (0, i));
        r = s.at (1, i) + f * (s.at (1, i + 1) - s.at (1, i));
        return;
    }
    if (q == Quality::High && absVel > 1.001)
    {
        const double fc = 1.0 / absVel; const int half = std::min (16, (int) std::ceil (4.0 / fc));
        double sl = 0, sr2 = 0, ws = 0;
        for (int k = -half + 1; k <= half; ++k)
        {
            const double x = (double) k - f;
            const double a = fc * x * 3.14159265358979;
            const double sinc = std::fabs (a) < 1e-9 ? 1.0 : std::sin (a) / a;
            const double win = 0.5 + 0.5 * std::cos (3.14159265358979 * x / half);
            const double wgt = sinc * win; ws += wgt;
            sl += wgt * s.at (0, i + k); sr2 += wgt * s.at (1, i + k);
        }
        if (std::fabs (ws) < 1e-9) ws = 1;
        l = (float) (sl / ws); r = (float) (sr2 / ws);
        return;
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        const float y0 = s.at (ch, i - 1), y1 = s.at (ch, i), y2 = s.at (ch, i + 1), y3 = s.at (ch, i + 2);
        const float v = y1 + 0.5f * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0)));
        (ch ? r : l) = v;
    }
}

int Engine::grainLen() const
{
    const StretchAlgo a = Pp ? Pp->algo : StretchAlgo::Smooth;
    const double ms = a == StretchAlgo::Rhythmic ? 30 : a == StretchAlgo::Vocal ? 45 : a == StretchAlgo::Surreal ? 160 : 80;
    return std::max (16, ((int) (ms * 0.001 * sr)) & ~1);
}

void Engine::grainTick (GrainCloud& gc, double spawnPos, double dir, double inc, bool jitter, float& l, float& r, i64 key)
{
    const int len = grainLen();
    if (--gc.untilNext <= 0)
    {
        gc.untilNext = len / 2;
        int slot = 0, oldest = -1;
        for (int k = 0; k < 4; ++k) { if (gc.g[k].age < 0) { slot = k; oldest = -2; break; } if (gc.g[k].age > oldest) { oldest = gc.g[k].age; slot = k; } }
        auto& g = gc.g[slot];
        g.age = 0; g.len = len; g.inc = dir * inc;
        g.pos = spawnPos + (jitter ? (hashUnit (Pp ? (uint64_t) Pp->seed : 1, dGrain, key * 4099 + gc.spawned) - 0.5) * len : 0.0);
        ++gc.spawned;
    }
    const Src& s = src();
    l = r = 0;
    for (auto& g : gc.g)
    {
        if (g.age < 0) continue;
        const float win = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) g.age / (float) g.len);
        const i64 i = (i64) std::floor (g.pos); const float f = (float) (g.pos - (double) i);
        l += win * (s.at (0, i) + f * (s.at (0, i + 1) - s.at (0, i)));
        r += win * (s.at (1, i) + f * (s.at (1, i + 1) - s.at (1, i)));
        g.pos += g.inc;
        if (++g.age >= g.len) g.age = -1;
    }
}

double Engine::brake (const Voice& v, double t, double& d) const
{
    d = 1.0;
    if (! v.tape) return t;
    const double T = (double) v.outLen;
    double b = std::clamp ((double) Pp->tapeLen, 0.02, 1.0) * T;
    const TapeMode m = Pp->tapeMode;
    if (m == TapeMode::Both) b = std::min (b, T * 0.5);
    double shift = 0;
    if (m != TapeMode::Stop)
    {
        if (t < b) { const double x = t / b; d = x; return b * x * x * 0.5; }
        shift = b * 0.5;
    }
    if (m != TapeMode::Start && t > T - b)
    {
        const double x = (t - (T - b)) / b; d = std::max (0.0, 1.0 - x);
        return (T - b) - shift + b * (x - x * x * 0.5);
    }
    return t - shift;
}

double Engine::content (const Voice& v, double tb, double& vel) const
{
    if (v.gateFollow)
    {
        const double cl = (double) v.outLen / v.cells;
        const int cell = std::clamp ((int) (tb / cl), 0, v.cells - 1);
        vel = v.cellSpeed[cell];
        return v.cellPrefix[cell] + vel * (tb - cell * cl);
    }
    vel = v.speed;
    return v.speed * tb;
}

double Engine::srcPos (const Voice& v, double c) const
{
    if (v.wrap) { c = std::fmod (c, (double) v.srcLen); if (c < 0) c += (double) v.srcLen; }
    return v.reversed ? (double) (v.srcStart + v.srcLen - 1) - c : (double) v.srcStart + c;
}

void Engine::renderVoice (Voice& v, const GateState* gs, float& l, float& r)
{
    l = r = 0;
    if (! v.active || v.rest) return;
    const EngineParams& P = *Pp;
    if (gs != nullptr)
    {
        if (gs->on && ! gs->active && P.gapMode == GapMode::Hold)
        {
            if (! holding) { holding = true; holdPos = v.lastPos; holdCloud.reset(); }
            grainTick (holdCloud, holdPos, 1.0, 1.0, false, l, r, v.slotIdx);
            return;
        }
        holding = false;
    }
    double t = (double) v.t, flip = 1.0;
    if (gs != nullptr && gs->on && gs->flip)
    {
        t = std::clamp (gs->c0 + gs->c1 - 1.0 - t, 0.0, (double) v.outLen - 1.0);
        flip = -1.0;
    }
    double d, vel;
    const double tb = brake (v, t, d);
    const double c = content (v, tb, vel);
    vel *= d * flip;
    const double p = srcPos (v, c);
    const double svel = v.reversed ? -vel : vel;
    double cf = v.wrap ? std::fmod (c, (double) v.srcLen) : c;
    v.lastDryPos = (double) v.srcStart + cf;
    if (! v.stretch) readInterp (p, std::fabs (svel), l, r);
    else
    {
        const double inc = v.preserve ? 1.0 : std::max (0.05, std::fabs (svel));
        grainTick (v.grains, p, svel >= 0 ? 1.0 : -1.0, inc, P.algo == StretchAlgo::Surreal, l, r, v.slotIdx);
    }
    v.lastPos = p; v.lastVel = svel;
}

// ============================================================ gate
GateState Engine::gateAt (const Voice& v, i64 clockNow)
{
    GateState gs;
    const EngineParams& P = *Pp;
    if (! P.gateOn) return gs;
    gs.on = true;
    const int N = std::clamp (P.gateSteps, 1, kMaxSteps);
    double cl, c0; i64 absCell;
    switch (P.gateTiming)
    {
        case GateTiming::FitChunk:
        {
            cl = std::max (1.0, (double) v.outLen / N);
            const int cell = std::min (N - 1, (int) ((double) v.t / cl));
            c0 = cell * cl; absCell = v.slotIdx * N + cell; gs.step = cell;
            break;
        }
        case GateTiming::HostNote:
        {
            const double beats = std::max (1e-4, noteBeats (std::max (1, P.gateNote), bpb));
            cl = std::max (1.0, beats * 60.0 / bpm * sr);
            const double a = std::floor (ppqNow / beats);
            const double off = (ppqNow - a * beats) * 60.0 / bpm * sr;
            c0 = (double) v.t - off; absCell = (i64) a; gs.step = (int) (((absCell % N) + N) % N);
            break;
        }
        default:
        {
            cl = P.gateTiming == GateTiming::FixedMs ? P.gateMs * 0.001 * sr : sr / std::max (0.01f, P.gateHz);
            cl = std::max (1.0, cl);
            const i64 a = (i64) std::floor ((double) clockNow / cl);
            c0 = (double) v.t - ((double) clockNow - (double) a * cl); absCell = a; gs.step = (int) (a % N);
            break;
        }
    }
    gs.c0 = std::max (0.0, c0); gs.c1 = std::min ((double) v.outLen, c0 + cl); gs.absCell = absCell;
    gs.phase = std::clamp (((double) v.t - c0) / cl, 0.0, 0.999999);
    gs.newCell = absCell != lastGateAbs;
    if (gs.newCell) { lastGateAbs = absCell; ++gateAbsCounter; }
    const GateStep& st = pat->gate[gs.step];
    gs.fx = st.fx;
    gs.flip = (st.dir == 1 && ! v.reversed) || (st.dir == 2 && v.reversed);
    const float width = clampf (st.width > 0 ? st.width : P.gateWidth, 0.01f, 1.0f);
    const float depth = clampf (st.depth >= 0 ? st.depth : P.gateDepth, 0, 1);
    const Shape sh = st.shape >= 0 ? (Shape) st.shape : P.gateShape;
    const float ph = (float) gs.phase;
    gs.active = st.on && ph < width;
    float env = 0;
    if (gs.active)
    {
        const float x = ph / width;
        env = shapeEnv (sh, x, *pat);
        if (st.fadeIn > 0) env *= std::min (1.0f, x / st.fadeIn);
        if (st.fadeOut > 0) env *= std::min (1.0f, (1 - x) / st.fadeOut);
    }
    else if (P.gapMode == GapMode::Crossfade && st.on && width < 1)
        env = 1.0f - std::sin (kPi * (ph - width) / (1 - width));
    gs.gain = 1.0f - depth * (1.0f - env);
    return gs;
}

// ============================================================ scheduling
i64 Engine::computeLatency (bool& clamped)
{
    clamped = false;
    if (! isLive) return 0;
    const EngineParams& P = *Pp;
    const double mm = maxSizeMultiplier (P, *pat);
    const int Gs = Gpending;
    const i64 Dcap = std::max<i64> (16, ringCap / 3 - 2 * maxBlock);
    double Lmax = baseFrames * mm;
    if (P.latPolicy == LatPolicy::FixedMax)
    {
        const i64 Df = std::clamp<i64> ((i64) std::llround (P.maxBufferMs * 0.001 * sr), 1, Dcap);
        if ((double) Gs * Lmax > (double) Df) { clamped = true; baseFrames = std::max (1.0, (double) (Df - Gs) / (Gs * mm)); }
        return Df;
    }
    const bool integral = std::fabs (Lmax - std::round (Lmax)) < 1e-9;
    i64 Dd = (i64) std::ceil ((double) Gs * Lmax - 1e-9) + (integral ? 0 : Gs);
    if (Dd > Dcap)
    {
        clamped = true;
        baseFrames = std::max (1.0, (double) (Dcap - Gs) / (Gs * mm));
        Dd = Dcap;
    }
    return Dd;
}

void Engine::liveRebase (bool flush)
{
    if (flush) liveLo = w;
    lS0 = (w - 1) - D; lExact = 0; lGen = 0; lK = -1; lGroup = -1; patPos = 0; prevM = -1; consecutive = 0; oneShotDone = false;
}

bool Engine::repeatSkip (i64& m, i64 key, bool liveCheck)
{
    const EngineParams& P = *Pp;
    if (prevM >= 0 && consecutive < P.maxRepeat && P.repeatProb > 0 && hashUnit ((uint64_t) P.seed, dRepeat, key) < P.repeatProb)
    {
        bool ok = true;
        if (liveCheck) ok = (lK - prevM) < 200 && cStart[prevM & 255] >= w - ringCap + 2 * cLen[prevM & 255] + maxBlock;
        if (ok) { m = prevM; ++consecutive; }
        else consecutive = 0;
    }
    else consecutive = 0;
    return P.skipProb > 0 && hashUnit ((uint64_t) P.seed, dSkip, key) < P.skipProb;
}

void Engine::fillVoice (Voice& v, i64 m, i64 chunkStart, i64 chunkLen, i64 outLenLive)
{
    const EngineParams& P = *Pp; const Patterns& pt = *pat;
    const bool keepGrains = ! P.stretchReset && cur.active && cur.stretch;
    GrainCloud gcopy = cur.grains;
    v = Voice();
    v.active = true; v.chunkIdx = m; v.slotIdx = slotCounter; v.patPos = patPos;
    v.srcStart = chunkStart; v.srcLen = std::max<i64> (1, chunkLen); v.wrap = isLive;
    v.reversed = P.revMode == RevMode::WholeSource ? true : chunkReversed (P, pt, m);
    const i64 key = P.freeze ? m % std::max (1, P.patLen) : m;
    const int rl = std::clamp (pt.rateLen, 1, kLane);
    auto stepFor = [&] (int idx) { return pt.rate[((idx % rl) + rl) % rl]; };
    RateStep st; st.ratio = P.ratio; st.stretch = 0;
    switch (P.tempoAssign)
    {
        case TempoAssign::Same: break;
        case TempoAssign::Alternating: if (slotCounter & 1) st.ratio = 1; break;
        case TempoAssign::CycleLane: st = stepFor ((int) (m % rl)); break;
        case TempoAssign::PatternFollow: st = stepFor (std::max (0, patPos - 1)); break;
        case TempoAssign::RandomSet: st = stepFor ((int) (hashUnit ((uint64_t) P.seed, dRate, key) * rl)); break;
        case TempoAssign::Weighted:
        {
            double tot = 0; for (int i = 0; i < rl; ++i) tot += std::max (0.0f, pt.rate[i].weight);
            double r = hashUnit ((uint64_t) P.seed, dRate, key) * tot; int pick = rl - 1;
            for (int i = 0; i < rl; ++i) { r -= std::max (0.0f, pt.rate[i].weight); if (r < 0) { pick = i; break; } }
            st = pt.rate[pick]; break;
        }
        case TempoAssign::GateFollow: st = stepFor (0); break;
    }
    if (st.prob < 1.0f && hashUnit ((uint64_t) P.seed, dRate + 50, key) >= st.prob) st.ratio = 1;
    v.speed = std::clamp ((double) st.ratio, 0.05, 8.0);
    v.stretch = st.stretch == 2 || (st.stretch == 0 && P.timeMode == TimeMode::Stretch);
    v.preserve = P.preserve;
    double avg = v.speed;
    if (P.tempoAssign == TempoAssign::GateFollow && P.gateTiming == GateTiming::FitChunk)
    {
        v.gateFollow = true; v.cells = std::clamp (P.gateSteps, 1, kMaxSteps); avg = 0;
        for (int i = 0; i < v.cells; ++i)
        {
            RateStep cs = stepFor (i);
            if (cs.prob < 1.0f && hashUnit ((uint64_t) P.seed, dRate + 60, key * 64 + i) >= cs.prob) cs.ratio = 1;
            v.cellSpeed[i] = (float) std::clamp ((double) cs.ratio, 0.05, 8.0); avg += v.cellSpeed[i];
        }
        avg /= v.cells;
    }
    v.outLen = isLive ? std::max<i64> (1, outLenLive) : std::max<i64> (1, (i64) std::llround ((double) v.srcLen / avg));
    if (v.gateFollow)
    {
        const double cl = (double) v.outLen / v.cells; v.cellPrefix[0] = 0;
        for (int i = 0; i < v.cells; ++i) v.cellPrefix[i + 1] = v.cellPrefix[i] + v.cellSpeed[i] * cl;
    }
    v.tape = P.tapeOn && hashUnit ((uint64_t) P.seed, dTape, key) < P.tapeProb;
    v.lastPos = v.reversed ? (double) (v.srcStart + v.srcLen - 1) : (double) v.srcStart;
    if (keepGrains && v.stretch) v.grains = gcopy;
}

void Engine::liveNext()
{
    const EngineParams& P = *Pp;
    const i64 k = lK + 1;
    const i64 g = k / G, gbase = g * G;
    if (g != lGroup)
    {
        lGroup = g;
        plen = buildOrder (oneShotDone ? Order::Sequential : effectiveOrder (P), P, *pat, g, G, lastRef, order);
    }
    const i64 need = std::max (gbase + G - 1, k);
    while (lGen <= need)
    {
        const double L = baseFrames * sizeMultiplier (P, *pat, lGen);
        const i64 st = lS0 + (i64) std::llround (lExact);
        lExact += L;
        const i64 en = lS0 + (i64) std::llround (lExact);
        cStart[lGen & 255] = st; cLen[lGen & 255] = std::max<i64> (1, en - st); ++lGen;
    }
    const int pos = patPos % plen;
    const int ref = order[pos];
    ++patPos;
    if (! P.patLoop && patPos >= plen) oneShotDone = true;
    lK = k;
    const i64 key = P.freeze ? (i64) pos : slotCounter;
    i64 m = ref < 0 ? -1 : gbase + (ref % G);
    bool rest = ref < 0;
    if (! rest && repeatSkip (m, key, true)) rest = true;
    if (rest)
    {
        cur = Voice(); cur.active = true; cur.rest = true; cur.outLen = cLen[k & 255]; cur.slotIdx = slotCounter; cur.chunkIdx = k;
        return;
    }
    lastRef = ref; prevM = m;
    fillVoice (cur, m, cStart[m & 255], cLen[m & 255], cLen[k & 255]);
}

double Engine::fileChunkStart (i64 m) const
{
    const EngineParams& P = *Pp;
    if (P.sizeMode == SizeMode::Fixed || P.variation <= 0) return (double) regionA + (double) m * baseFrames;
    double s = (double) regionA;
    const i64 cap = std::min<i64> (m, 4000000);
    for (i64 i = 0; i < cap; ++i) s += baseFrames * sizeMultiplier (P, *pat, i);
    return s;
}

void Engine::computeGroupStarts()
{
    for (int i = 0; i < G; ++i) fStarts[i + 1] = fStarts[i] + baseFrames * sizeMultiplier (*Pp, *pat, fGroupFirst + i);
}

i64 Engine::fileInitAt (double pos)
{
    const EngineParams& P = *Pp;
    pos = std::clamp (pos, (double) regionA, (double) std::max (regionA, regionB - 1));
    i64 m;
    if (P.sizeMode == SizeMode::Fixed || P.variation <= 0) m = (i64) std::floor ((pos - (double) regionA) / baseFrames);
    else
    {
        double s = (double) regionA; m = 0;
        while (m < 4000000) { const double L = baseFrames * sizeMultiplier (P, *pat, m); if (s + L > pos) break; s += L; ++m; }
    }
    m = std::max<i64> (0, m);
    fGroupFirst = (m / G) * G; fGroup = fGroupFirst / G;
    fStarts[0] = fileChunkStart (fGroupFirst);
    computeGroupStarts();
    plen = buildOrder (oneShotDone ? Order::Sequential : effectiveOrder (P), P, *pat, fGroup, G, lastRef, order);
    patPos = 0; // reordered groups restart from their first pattern step
    return m;
}

void Engine::fileAdvanceGroup()
{
    fGroupFirst += G; ++fGroup;
    fStarts[0] = fStarts[G];
    computeGroupStarts();
    plen = buildOrder (oneShotDone ? Order::Sequential : effectiveOrder (*Pp), *Pp, *pat, fGroup, G, lastRef, order);
    patPos = 0;
}

void Engine::fileNext()
{
    const EngineParams& P = *Pp;
    for (int guard = 0; guard < 4 * kMaxGroup + 8; ++guard)
    {
        if (fGroupFirst < 0) fileInitAt ((double) regionA);
        if (patPos >= plen) fileAdvanceGroup();
        if (fStarts[0] >= (double) regionB - 0.5)
        {
            if (P.loop && guard < 4 * kMaxGroup) { fGroupFirst = -1; continue; }
            fileEnded = true; filePlaying = false; cur.active = false; return;
        }
        const int ref = order[patPos];
        ++patPos;
        if (! P.patLoop && patPos >= plen) oneShotDone = true;
        const i64 key = P.freeze ? (i64) (patPos - 1) : slotCounter;
        if (ref < 0)
        {
            cur = Voice(); cur.active = true; cur.rest = true; cur.outLen = std::max<i64> (1, (i64) baseFrames); cur.slotIdx = slotCounter;
            return;
        }
        const int r = ref % G;
        const double st = fStarts[r], en = fStarts[r + 1];
        if (st >= (double) regionB - 0.5) continue;
        const i64 s = (i64) std::llround (st), e = std::min<i64> ((i64) std::llround (en), regionB);
        if (e <= s) continue;
        i64 m = fGroupFirst + r;
        i64 cs = s, ce = e;
        if (repeatSkip (m, key, false)) continue;
        if (m != fGroupFirst + r)
        {
            cs = (i64) std::llround (fileChunkStart (m));
            ce = std::min<i64> (regionB, (i64) std::llround (fileChunkStart (m + 1)));
            if (ce <= cs) { cs = s; ce = e; m = fGroupFirst + r; }
        }
        lastRef = ref; prevM = m;
        fillVoice (cur, m, cs, ce - cs, 0);
        return;
    }
    cur = Voice(); cur.active = true; cur.rest = true; cur.outLen = std::max<i64> (1, (i64) baseFrames);
}

void Engine::startNext()
{
    const EngineParams& P = *Pp;
    const bool canFade = cur.active && ! cur.rest && P.xfade != Xfade::Hard;
    if (canFade) fade = cur; else fade.active = false;
    const i64 prevOut = cur.outLen;
    if (needRegrid)
    {
        needRegrid = false; G = Gpending;
        if (isLive) { D = Dpending; liveRebase (false); }
        else
        {
            double pos = cur.active && ! cur.rest ? (double) (cur.srcStart + cur.srcLen) : (double) regionA;
            if (fGroupFirst < 0 || pos >= (double) regionB) pos = (double) regionA;
            fileInitAt (pos);
        }
    }
    if (isLive) liveNext(); else fileNext();
    ++slotCounter;
    chunkStartFlag = true;
    fadeRemain = 0; zcWait = false;
    if (canFade && cur.active)
    {
        double ms = P.xfade == Xfade::Micro ? 2.0 : P.xfade == Xfade::EqualPower ? (double) P.xfadeMs : 5.0;
        i64 xf = (i64) (ms * 0.001 * sr);
        xf = std::min<i64> (xf, (i64) (0.25 * (double) std::min (prevOut, cur.outLen)));
        fadeRemain = fadeTotal = (int) std::max<i64> (1, xf);
        zcWait = P.xfade == Xfade::ZeroCross;
    }
}

void Engine::fileSeek (double pos)
{
    if (regionB <= regionA) return;
    if (cur.active && ! cur.rest) { fade = cur; fadeRemain = fadeTotal = std::max (1, (int) (0.004 * sr)); zcWait = false; }
    if (needRegrid) { needRegrid = false; G = Gpending; }
    pos = std::clamp (pos, (double) regionA, (double) regionB - 1);
    const i64 m = fileInitAt (pos);
    fileNext();
    ++slotCounter; chunkStartFlag = true; fileEnded = false;
    if (cur.active && ! cur.rest && cur.chunkIdx == m)
    {
        const double c = cur.reversed ? (double) (cur.srcStart + cur.srcLen - 1) - pos : pos - (double) cur.srcStart;
        const double sp0 = cur.gateFollow ? (double) cur.srcLen / (double) cur.outLen : cur.speed;
        cur.t = std::clamp<i64> ((i64) std::llround (c / std::max (0.05, sp0)), 0, cur.outLen - 1);
        cur.lastPos = pos;
    }
}

void Engine::restartPattern()
{
    clock = 0; lastGateAbs = -1; stut.periodCount = 0;
    if (isLive) { needRegrid = true; Gpending = G; Dpending = D; }
    else if (fGroupFirst >= 0) { patPos = 0; oneShotDone = false; }
    if (cur.active) cur.t = cur.outLen; // switch at this sample (with crossfade)
}

bool Engine::fxAssigned (Scope s, float prob, uint64_t dom, int bit, const GateState& gs) const
{
    const EngineParams& P = *Pp;
    const i64 m = cur.chunkIdx;
    switch (s)
    {
        case Scope::Global: return true;
        case Scope::Alternating: return (m & 1) != 0;
        case Scope::ChunkLane: return (pat->chunkFlags[(size_t) (m % std::clamp (pat->laneLen, 1, kLane))] & (bit << 1)) != 0;
        case Scope::Random: return hashUnit ((uint64_t) P.seed, dom, P.freeze ? m % std::max (1, P.patLen) : m) < prob;
        case Scope::GateLane: return gs.on && gs.active && (gs.fx & bit) != 0;
    }
    return true;
}

// ============================================================ main process
void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n,
                      const EngineParams& P, const Patterns& patterns, const HostInfo& h)
{
    Pp = &P; pat = &patterns; host = h;
    if (inR == nullptr) inR = inL;

    // ---- source swap (file pointer lifetime is managed by the owner)
    const SourceBuffer* ns = pendingSrc.load (std::memory_order_acquire);
    if (ns != curSrc)
    {
        curSrc = ns; fade.active = false; fadeRemain = 0; holding = false; scrubOn = false; fileEnded = false;
        if (! isLive) { fGroupFirst = -1; cur.active = false; }
    }
    file.L = curSrc ? curSrc->L : nullptr; file.R = curSrc ? (curSrc->R ? curSrc->R : curSrc->L) : nullptr;
    file.lo = 0; file.hi = curSrc ? curSrc->length : 0; file.ring = false;
    const i64 flen = file.hi;
    live.L = ring[0].data(); live.R = ring[1].data(); live.cap = ringCap; live.ring = true;

    bpm = (h.valid && h.bpm > 0) ? h.bpm : std::max (20.0f, P.bpm);
    bpb = (h.valid && h.beatsPerBar > 0) ? h.beatsPerBar : 4.0;

    // ---- source mode
    if (P.source != lastSource)
    {
        lastSource = P.source; cur.active = false; fade.active = false; fadeRemain = 0;
        isLive = P.source == Source::Live; needRegrid = true; fGroupFirst = -1; scrubOn = false;
    }
    isLive = P.source == Source::Live;

    // ---- region
    i64 ra = 0, rb = flen;
    if (P.loop && flen > 0)
    {
        ra = (i64) (clampf (ctl.loopA.load(), 0, 1) * (double) flen); rb = (i64) (clampf (ctl.loopB.load(), 0, 1) * (double) flen);
        if (rb <= ra + 1) { ra = 0; rb = flen; }
    }
    if (ra != regionA || rb != regionB) { regionA = ra; regionB = rb; if (! isLive) needRegrid = true; }

    // ---- transport
    switch (ctl.transport.exchange (0))
    {
        case 1: if (fileEnded) { fGroupFirst = -1; cur.active = false; fileEnded = false; } if (! cur.active) playG.reset (1); filePlaying = flen > 0; break;
        case 2: filePlaying = false; break;
        case 3: filePlaying = false; fGroupFirst = -1; cur.active = false; fileEnded = false; break;
        case 4: fGroupFirst = -1; cur.active = false; fileEnded = false; break;
        default: break;
    }
    if (P.fileHostSync && h.valid && ! isLive)
    {
        if (h.playing && ! lastHostPlaying) filePlaying = flen > 0;
        if (! h.playing && lastHostPlaying) filePlaying = false;
    }

    // ---- grid / latency
    baseFrames = (P.revMode == RevMode::WholeSource && ! isLive) ? (double) std::max<i64> (1, regionB - regionA)
                                                                  : baseChunkFrames (P, sr, bpm, bpb);
    Gpending = (P.revMode == RevMode::WholeSource && ! isLive) ? 1 : groupSize (P);
    bool clamped = false;
    Dpending = computeLatency (clamped);
    {
        uint64_t s = mix64 ((uint64_t) std::llround (baseFrames * 16.0));
        auto add = [&] (uint64_t v) { s = mix64 (s ^ (v + 0x9E37u)); };
        add ((uint64_t) Gpending); add ((uint64_t) effectiveOrder (P)); add ((uint64_t) P.patLen); add ((uint64_t) P.patStart);
        add ((uint64_t) P.seed); add (P.freeze); add ((uint64_t) P.sizeMode); add ((uint64_t) (P.variation * 1000)); add ((uint64_t) P.revMode);
        add ((uint64_t) Dpending); add ((uint64_t) isLive); add ((uint64_t) regionA); add ((uint64_t) regionB); add (P.patLoop);
        add ((uint64_t) patterns.sizeCount); for (int i = 0; i < kSizes; ++i) { add ((uint64_t) (patterns.sizeMult[i] * 1000)); add ((uint64_t) (patterns.sizeWeight[i] * 1000)); }
        add ((uint64_t) patterns.userLen); for (int i = 0; i < kUserPat; ++i) add ((uint64_t) (patterns.user[i].kind * 100000 + patterns.user[i].val * 10 + (int) (patterns.user[i].prob * 9)));
        if (s != gridSig) { gridSig = s; needRegrid = true; }
    }
    if (! isLive) G = needRegrid && fGroupFirst < 0 ? Gpending : G;
    tel.latency.store ((int) (isLive ? Dpending : 0));
    tel.clamped.store (clamped);

    // ---- restart rules / host transport
    int restartAt = -1;
    if (ctl.restart.exchange (false)) restartAt = 0;
    if (h.valid)
    {
        if (P.restart == Restart::HostPlay && h.playing && ! lastHostPlaying) restartAt = 0;
        if ((P.restart == Restart::Bar || P.restart == Restart::Beat) && h.playing && restartAt < 0)
        {
            const double unit = P.restart == Restart::Beat ? 1.0 : bpb;
            const double nb = std::ceil (h.ppq / unit - 1e-9) * unit;
            const double fr = (nb - h.ppq) * 60.0 / bpm * sr;
            if (fr >= 0 && fr < n) restartAt = (int) fr;
        }
        if (h.playing && lastHostPlaying && std::llabs (h.samplePos - expectedHostPos) > 2)
        {
            if (isLive) { needRegrid = true; liveLo = w; }
            else if (P.fileHostSync && flen > 0) fileSeek ((double) (h.samplePos % flen));
        }
        expectedHostPos = h.samplePos + n; lastHostPlaying = h.playing;
    }
    if (ctl.flush.exchange (false)) { needRegrid = true; liveLo = w; }
    { const double sn = ctl.seekNorm.exchange (-1.0); if (sn >= 0 && flen > 0 && ! isLive) fileSeek (sn * (double) flen); }

    // ---- capture
    const bool wantCap = ctl.capture.load();
    if (wantCap && ! capturing) capLen = 0;
    capturing = wantCap;

    // ---- per-block constants
    const float igT = dbToGain (P.inGainDb), ogT = dbToGain (P.outGainDb);
    const bool held = ctl.scrubHeld.load() || P.scrActive;
    const bool freeMode = P.revMode == RevMode::FreeScrub;
    const double scrubN = P.scrActive ? (double) P.scrPos : ctl.scrubNorm.load();
    const double shuttle = ctl.shuttle.load();
    const int perm[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
    const int* chain = perm[std::clamp (P.chainOrder, 0, 5)];
    const bool fxOn[3] = { P.stutOn, P.dlyOn, P.echoOn };
    panS.setTime (sr, std::max (0.5f, P.panSmoothMs));
    const double ppqStep = bpm / 60.0 / sr;
    const double ppq0 = (h.valid && h.playing) ? h.ppq : (double) clock * ppqStep;
    float pkL = 0, pkR = 0;

    for (int i = 0; i < n; ++i)
    {
        ppqNow = ppq0 + i * ppqStep;
        const float ig = inG.step (igT);
        float xl = inL[i] * ig, xr = inR[i] * ig;
        if (! std::isfinite (xl)) xl = 0; if (! std::isfinite (xr)) xr = 0;
        ring[0][(size_t) (w % ringCap)] = xl; ring[1][(size_t) (w % ringCap)] = xr; ++w;
        live.hi = w; live.lo = std::max (liveLo, w - ringCap + 1);
        if (capturing && capLen < (int) capBuf[0].size()) { capBuf[0][(size_t) capLen] = xl; capBuf[1][(size_t) capLen] = xr; ++capLen; }
        peakRun = std::max (peakRun, std::max (std::fabs (xl), std::fabs (xr)));
        if (++peakAcc >= peakHopFrames) { const int pw = peakWrite.load (std::memory_order_relaxed); peakHist[pw].store (peakRun, std::memory_order_relaxed); peakWrite.store ((pw + 1) % kPeakHist, std::memory_order_release); peakRun = 0; peakAcc = 0; }
        if (i == restartAt) restartPattern();

        const bool fileOk = ! isLive && flen > 0;
        const bool running = ! freeMode && (isLive || (fileOk && filePlaying));
        const float pg = playG.step (running ? 1.0f : 0.0f);
        float wl = 0, wr = 0; GateState gs;
        if (running && (! cur.active || cur.t >= cur.outLen)) startNext();
        if (! freeMode && cur.active && (running || pg > 1e-4f))
        {
            gs = gateAt (cur, clock);
            renderVoice (cur, &gs, wl, wr);
            if (fadeRemain > 0 && fade.active)
            {
                float fl, fr; renderVoice (fade, nullptr, fl, fr);
                if (running) ++fade.t;
                if (zcWait)
                {
                    const float s = fl + fr;
                    if (((s >= 0) != (lastFadeOut >= 0)) || fadeRemain == 1) { zcWait = false; fadeRemain = 0; zcRamp = 32; }
                    else { wl = fl; wr = fr; --fadeRemain; }
                    lastFadeOut = s;
                }
                else
                {
                    const float x = 1.0f - (float) fadeRemain / (float) fadeTotal;
                    float gi = x, go = 1 - x;
                    if (P.xfade == Xfade::EqualPower) { gi = std::sin (0.5f * kPi * x); go = std::cos (0.5f * kPi * x); }
                    wl = wl * gi + fl * go; wr = wr * gi + fr * go;
                    --fadeRemain;
                }
            }
            if (zcRamp > 0) { const float g = 1.0f - (float) zcRamp / 32.0f; wl *= g; wr *= g; --zcRamp; }
            wl *= pg; wr *= pg;
        }

        // ---- scrub / scratch (hybrid override or free scrub mode)
        if ((held || freeMode) && ! scrubOn)
        {
            scrubOn = true; scrubCloud.reset();
            sp = cur.active ? cur.lastPos : (isLive ? (double) (w - 1 - D) : (double) regionA);
            sv = cur.active ? cur.lastVel : 0.0;
        }
        float sl = 0, srr = 0;
        const float sm = scrubMix.step (scrubOn ? 1.0f : 0.0f);
        if (scrubOn || sm > 1e-4f)
        {
            const double lo = isLive ? (double) live.lo : 0.0, hi = isLive ? (double) (w - 1) : (double) std::max<i64> (0, flen - 1);
            const double maxR = std::max (0.25f, P.scrMaxRate);
            const double rampFr = std::max (1.0, P.scrRampMs * 0.001 * sr);
            const double motorV = (isLive || filePlaying) ? -(double) std::clamp (P.ratio, 0.05f, 8.0f) : 0.0;
            if (scrubOn)
            {
                if (held)
                {
                    const double target = lo + std::clamp (scrubN, 0.0, 1.0) * (hi - lo);
                    switch (P.scrMode)
                    {
                        case ScrMode::Linear:
                        case ScrMode::Fine:
                        {
                            const double lim = P.scrMode == ScrMode::Fine ? maxR * 0.25 : maxR;
                            sv = std::clamp ((target - sp) * (P.scrMode == ScrMode::Fine ? 0.002 : 0.01), -lim, lim);
                            break;
                        }
                        case ScrMode::Vinyl:
                        {
                            const double tau = (1.0 + 40.0 * P.scrInertia) * 0.001 * sr;
                            const double want = std::clamp ((target - sp) / tau, -maxR, maxR);
                            const double a = 1.0 - std::exp (-1.0 / tau) * (1.0 - 0.5 * P.scrFriction);
                            sv += (want - sv) * std::clamp (a, 0.0005, 1.0);
                            break;
                        }
                        case ScrMode::Shuttle: sv += (std::clamp (shuttle, -1.0, 1.0) * maxR - sv) * 0.002; break;
                    }
                }
                else if (freeMode)
                {
                    sv += (motorV - sv) * (1.0 - std::exp (-1.0 / rampFr)) * (0.25 + 1.5 * P.scrMotor);
                }
                else
                {
                    switch (P.scrRelease)
                    {
                        case ScrRelease::Latch:
                            if (! isLive && flen > 0) fileSeek (sp);
                            scrubOn = false; break;
                        case ScrRelease::Spring:
                        {
                            const double tgt = cur.active ? cur.lastPos : sp;
                            const double want = std::clamp ((tgt - sp) / (rampFr * 0.25), -maxR, maxR) + (cur.active ? cur.lastVel : 0.0);
                            sv += (want - sv) * 0.01;
                            if (std::fabs (tgt - sp) < 64.0) scrubOn = false;
                            break;
                        }
                        case ScrRelease::Continue:
                        {
                            const double tgtV = cur.active ? cur.lastVel : motorV;
                            sv += (tgtV - sv) * (1.0 - std::exp (-1.0 / rampFr)) * (0.2 + P.scrFriction + P.scrMotor);
                            if (std::fabs (sv - tgtV) < 0.02) { if (! isLive && flen > 0) fileSeek (sp); scrubOn = false; }
                            break;
                        }
                    }
                }
            }
            double vOut = sv;
            if (P.scrAccel != 1.0f && std::fabs (sv) > 0) vOut = (sv > 0 ? 1 : -1) * maxR * std::pow (std::min (1.0, std::fabs (sv) / maxR), (double) P.scrAccel);
            sp += vOut;
            if (freeMode && ! isLive && P.loop && regionB > regionA)
            {
                if (sp < (double) regionA) sp += (double) (regionB - regionA);
                if (sp >= (double) regionB) sp -= (double) (regionB - regionA);
            }
            sp = std::clamp (sp, lo, std::max (lo, hi));
            if (P.scrHoldFreeze && std::fabs (vOut) < 0.03 && (held || ! freeMode)) grainTick (scrubCloud, sp, 1.0, 1.0, false, sl, srr, 7);
            else { scrubCloud.reset(); readInterp (sp, std::fabs (vOut), sl, srr); }
            if (P.scrRevLock && vOut > 0) { sl = 0; srr = 0; }
            if (freeMode && ! held && ! isLive && ! filePlaying && std::fabs (vOut) < 1e-4) sl = srr = 0;
            tel.scrubVel.store ((float) vOut);
            wl = wl * (1 - sm) + sl * sm; wr = wr * (1 - sm) + srr * sm;
        }

        // ---- dry paths
        float dal, dar; // latency-aligned live dry (also bypass path)
        { const i64 di = (w - 1) - (isLive ? D : 0); dal = live.at (0, di); dar = live.at (1, di); }
        float dl = dal, dr = dar;
        if (P.dryAlign == DryAlign::Immediate) { dl = xl; dr = xr; }
        if (! isLive) { if (cur.active && ! cur.rest) readInterp (cur.lastDryPos, 1.0, dl, dr); else dl = dr = 0; dl *= pg; dr *= pg; }

        // ---- pan / swap / polarity / phase on the processed path
        {
            double x;
            switch (P.panClock)
            {
                case PanClock::Chunk: x = (double) cur.chunkIdx + (cur.outLen > 0 ? (double) cur.t / (double) cur.outLen : 0.0); break;
                case PanClock::Independent: x = (double) clock / std::max (1.0, P.panClockMs * 0.001 * sr); break;
                case PanClock::HostBeat: x = ppqNow; break;
                default: x = (double) gateAbsCounter + gs.phase; break;
            }
            x += P.panOffset;
            const i64 step = (i64) std::floor (x); const double ph = fracPos (x);
            const i64 key = P.freeze ? step % std::max (1, P.patLen) : step;
            float p = 0;
            switch (P.panMode)
            {
                case PanMode::Off: p = 0; break;
                case PanMode::Static: p = P.pan; break;
                case PanMode::Auto: p = P.pan + P.panDepth * std::sin (2.0f * kPi * (float) ph); break;
                case PanMode::Alternate: p = P.pan + P.panDepth * ((step & 1) ? 1.0f : -1.0f); break;
                case PanMode::Random: p = P.pan + P.panDepth * (float) (2 * hashUnit ((uint64_t) P.seed, dPan, key) - 1); break;
                case PanMode::Lane: p = P.pan + P.panDepth * pat->pan[(size_t) (((step % std::clamp (pat->panLen, 1, kLane)) + kLane) % std::clamp (pat->panLen, 1, kLane))]; break;
            }
            if (P.panMirror) p = -p;
            p = clampf (p, -1, 1);
            const float ps = panS.step (p);
            tel.pan.store (ps, std::memory_order_relaxed);
            const float swT = P.swap == SwapMode::On ? 1.0f : (P.swap == SwapMode::Alternate && (step & 1)) ? 1.0f : 0.0f;
            const float s = swapS.step (swT);
            const float a = wl * (1 - s) + wr * s, b = wr * (1 - s) + wl * s;
            wl = a * std::min (1.0f, 1.0f - ps); wr = b * std::min (1.0f, 1.0f + ps);
            int pm = 0;
            switch (P.polMode)
            {
                case PolMode::None: pm = 0; break;
                case PolMode::Left: pm = 1; break;
                case PolMode::Right: pm = 2; break;
                case PolMode::Both: pm = 3; break;
                case PolMode::AltChannels: pm = (step & 1) ? 2 : 1; break;
                case PolMode::Lane: { const int pl = std::clamp (pat->polLen, 1, kLane); pm = pat->pol[(size_t) (((step % pl) + pl) % pl)] & 3; break; }
                case PolMode::Random: pm = (int) (hashUnit ((uint64_t) P.seed, dPol, key) * 4.0) & 3; break;
            }
            wl *= polL.step ((pm & 1) ? -1.0f : 1.0f); wr *= polR.step ((pm & 2) ? -1.0f : 1.0f);
            float Il, Ql, Ir, Qr; hil[0].process (wl, Il, Ql); hil[1].process (wr, Ir, Qr);
            const float pmx = phaseMix.step (P.phaseMode != PhaseMode::Off ? 1.0f : 0.0f);
            if (pmx > 1e-4f)
            {
                const float th = P.phaseDeg * kPi / 180.0f, thL = P.phaseMode == PhaseMode::RotateBoth ? th : 0.0f;
                const float rl = Il * std::cos (thL) + Ql * std::sin (thL), rr = Ir * std::cos (th) + Qr * std::sin (th);
                wl += pmx * (rl - wl); wr += pmx * (rr - wr);
            }
        }

        // ---- gate gain, gap routing
        const float gg = gateG.step (gs.on ? (P.gapMode == GapMode::Hold && ! gs.active ? 1.0f : gs.gain) : 1.0f);
        float pre = gg, post = 1.0f, dryFill = 0.0f;
        if (gs.on)
        {
            if (P.gapMode == GapMode::Silence) post = gg;
            else if (P.gapMode == GapMode::DryThrough) dryFill = 1.0f - gg;
        }
        float cl = wl * pre, cr = wr * pre;

        // ---- FX chain
        FxContext fc; fc.chunkReversed = cur.reversed; fc.chunkStart = chunkStartFlag; fc.gateStart = gs.on && gs.newCell && gs.active;
        fc.slotIndex = cur.slotIdx; fc.triggerIndex = cur.slotIdx; fc.seed = (uint64_t) P.seed; fc.sr = sr; fc.bpm = bpm;
        chunkStartFlag = false;
        const float procIn[2] = { cl, cr };
        int mask = 0;
        for (int k = 0; k < 3; ++k)
        {
            const int fx = chain[k];
            const float en = fxEn[fx].step (fxOn[fx] ? 1.0f : 0.0f);
            const Scope sc = fx == 0 ? P.stutScope : fx == 1 ? P.dlyScope : P.echoScope;
            const float pr = fx == 0 ? 1.0f : fx == 1 ? P.dlyProb : P.echoProb;
            const bool as = fxAssigned (sc, fx == 0 ? P.stutProb : pr, fx == 0 ? dStut : fx == 1 ? dDly : dEcho, 1 << fx, gs);
            const float snd = sendS[fx].step (as ? 1.0f : 0.0f);
            if (as && fxOn[fx]) mask |= 1 << fx;
            if (en < 1e-4f) continue;
            float a = cl, b = cr;
            if (fx == 0) stut.process (a, b, snd, P, fc);
            else if (fx == 1) dly.process (a, b, snd, P, fc);
            else echo.process (a, b, snd, P, fc);
            cl += en * (a - cl); cr += en * (b - cr);
        }
        const float cm = clampf (P.chainMix, 0, 1);
        cl = procIn[0] + cm * (cl - procIn[0]); cr = procIn[1] + cm * (cr - procIn[1]);
        cl = cl * post + dl * dryFill; cr = cr * post + dr * dryFill;

        // ---- wet/dry, output, bypass, safety
        const float m = mixS.step (clampf (P.mix, 0, 1));
        float ol = dl * (1 - m) + cl * m, orr = dr * (1 - m) + cr * m;
        const float og = outG.step (ogT);
        ol *= og; orr *= og;
        const float bp = bypassS.step (P.bypass ? 1.0f : 0.0f);
        ol = ol * (1 - bp) + dal * bp; orr = orr * (1 - bp) + dar * bp;
        if (! std::isfinite (ol)) ol = 0; if (! std::isfinite (orr)) orr = 0;
        ol = clampf (ol, -8, 8); orr = clampf (orr, -8, 8);
        if (std::fabs (ol) < 1e-20f) ol = 0; if (std::fabs (orr) < 1e-20f) orr = 0;
        outL[i] = ol; if (outR) outR[i] = orr;
        pkL = std::max (pkL, std::fabs (ol)); pkR = std::max (pkR, std::fabs (orr));
        tel.fxMask.store (mask, std::memory_order_relaxed);
        if (running) { ++cur.t; ++clock; }
    }

    // ---- telemetry
    const double len = isLive ? (double) std::max<i64> (1, w - live.lo) : (double) std::max<i64> (1, flen);
    const double base = isLive ? (double) live.lo : 0.0;
    const double pos = scrubOn ? sp : cur.lastPos;
    tel.posNorm.store (std::clamp ((pos - base) / len, 0.0, 1.0));
    tel.chunkStartNorm.store (std::clamp (((double) cur.srcStart - base) / len, 0.0, 1.0));
    tel.chunkLenNorm.store (std::clamp ((double) cur.srcLen / len, 0.0, 1.0));
    tel.chunkIdx.store (cur.chunkIdx); tel.writePos.store (w); tel.validFrom.store (live.lo);
    tel.reversed.store (cur.reversed); tel.speed.store ((float) cur.speed); tel.stretch.store (cur.stretch); tel.rest.store (cur.rest);
    tel.slotPhase.store (cur.outLen > 0 ? (float) cur.t / (float) cur.outLen : 0.0f);
    tel.gateCell.store (P.gateOn ? (int) (((lastGateAbs % std::max (1, P.gateSteps)) + std::max (1, P.gateSteps)) % std::max (1, P.gateSteps)) : -1);
    tel.peakL.store (pkL); tel.peakR.store (pkR); tel.bpm.store ((float) bpm);
    tel.playing.store (isLive || filePlaying); tel.ended.store (fileEnded); tel.scrubbing.store (scrubOn);
    tel.captureLen.store (capLen);
    tel.liveFill.store (isLive ? (float) fracPos ((double) ((w - 1) - lS0) / std::max (1.0, baseFrames)) : 0.0f);
}

} // namespace br
