// BackReverse effects: Stutter, Delay, Echo, broadband phase rotator. All storage is preallocated in prepare().
#pragma once
#include "Types.h"
#include <vector>

namespace br {

constexpr float kPi = 3.14159265358979f;

struct Smooth
{
    float y = 0, a = 0.99f;
    void setTime (double sr, double ms) { a = (float) std::exp (-1.0 / std::max (1.0, ms * 0.001 * sr)); }
    float step (float target) { y = target + (y - target) * a; return y; }
    void reset (float v) { y = v; }
};

struct OnePole // simple lowpass; hp() derived
{
    float z = 0, c = 1; double hzCached = -1, srCached = -1;
    // Coefficients are only recomputed when the cutoff or rate changes (setHz is called every sample by Delay/Echo).
    void setHz (double sr, double hz)
    {
        if (hz == hzCached && sr == srCached) return;
        hzCached = hz; srCached = sr;
        c = (float) (1.0 - std::exp (-2.0 * 3.14159265358979 * std::min (hz, sr * 0.49) / sr));
    }
    float lp (float x) { z += c * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
};

// Stereo delay line with fractional (cubic) reads.
struct DelayLine
{
    std::vector<float> b[2]; int size = 0, w = 0;
    void prepare (int frames) { size = std::max (4, frames); for (auto& v : b) v.assign ((size_t) size, 0.0f); w = 0; }
    void clear() { for (auto& v : b) std::fill (v.begin(), v.end(), 0.0f); }
    void push (float l, float r) { b[0][(size_t) w] = l; b[1][(size_t) w] = r; if (++w >= size) w = 0; }
    float read (int ch, float delay) const
    {
        delay = clampf (delay, 1.0f, (float) size - 3);
        float p = (float) w - delay; while (p < 0) p += (float) size;
        int i = (int) p; float f = p - (float) i;
        auto at = [&] (int k) { k %= size; if (k < 0) k += size; return b[ch][(size_t) k]; };
        float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        return y1 + 0.5f * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0)));
    }
};

// IIR Hilbert pair (Niemitalo): two all-pass chains ~90 degrees apart, ~20 Hz - 20 kHz at 44.1/48 kHz.
struct Hilbert
{
    struct AP { float a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0; float p (float x) { float y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    AP i[4], q[4]; float qd = 0;
    Hilbert()
    {
        const float ci[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        const float cq[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        for (int k = 0; k < 4; ++k) { i[k].a2 = ci[k] * ci[k]; q[k].a2 = cq[k] * cq[k]; }
    }
    void reset() { for (auto& a : i) a = { a.a2 }; for (auto& a : q) a = { a.a2 }; qd = 0; }
    void process (float x, float& I, float& Q)
    {
        float a = x, b = x;
        for (auto& s : i) a = s.p (a);
        for (auto& s : q) b = s.p (b);
        I = qd; qd = a; Q = b; // in-phase path delayed one sample
    }
};

struct FxContext
{
    bool chunkReversed = true, chunkStart = false, gateStart = false;
    i64 slotIndex = 0, triggerIndex = 0; uint64_t seed = 1; double sr = 48000, bpm = 120;
};

struct Stutter
{
    std::vector<float> buf[2]; int cap = 0;
    int len = 0, wpos = 0, repeat = 0, state = 0; // 0 idle(pass) 1 capture 2 repeat
    double rpos = 0, rate = 1; float gain = 1; bool rev = false; i64 periodCount = 0; i64 trig = 0;
    void prepare (double sr) { cap = (int) (sr * 4.0) + 8; for (auto& v : buf) v.assign ((size_t) cap, 0.0f); reset(); }
    void reset() { state = 0; wpos = repeat = 0; rpos = 0; periodCount = 0; trig = 0; }
    void process (float& l, float& r, float send, const EngineParams& P, const FxContext& c)
    {
        const double periodFr = P.stutSync > 0 ? noteBeats (P.stutSync) * 60.0 / std::max (1.0, c.bpm) * c.sr : P.stutPeriodMs * 0.001 * c.sr;
        bool fire = false;
        if (P.stutRetrig == Retrig::Period) { if (++periodCount >= (i64) std::max (1.0, periodFr)) { periodCount = 0; fire = true; } }
        else if (P.stutRetrig == Retrig::ChunkStart) fire = c.chunkStart;
        else fire = c.gateStart;
        if (fire && send > 0.5f)
        {
            ++trig;
            if (hashUnit (c.seed, dStut, c.triggerIndex * 131 + trig) < P.stutProb)
            {
                state = 1; wpos = 0; repeat = 0;
                len = std::clamp ((int) (P.stutLenMs * 0.001 * c.sr), 16, cap - 4);
            }
        }
        float sl = l, sr2 = r;
        if (state == 1)
        {
            buf[0][(size_t) wpos] = l; buf[1][(size_t) wpos] = r;
            if (++wpos >= len) { state = 2; repeat = 0; startRepeat (P, c); }
        }
        else if (state == 2)
        {
            int i0 = (int) rpos; float f = (float) (rpos - i0);
            int i1 = std::min (i0 + 1, len - 1); i0 = std::clamp (i0, 0, len - 1);
            float env = 1.0f; const float pos = (float) (rev ? (len - 1 - rpos) : rpos);
            const float edge = std::min (64.0f, len * 0.25f);
            env = std::min (1.0f, std::min (pos, (float) len - pos) / std::max (1.0f, edge));
            float a = buf[0][(size_t) i0] + f * (buf[0][(size_t) i1] - buf[0][(size_t) i0]);
            float b = buf[1][(size_t) i0] + f * (buf[1][(size_t) i1] - buf[1][(size_t) i0]);
            sl = a * gain * env; sr2 = b * gain * env;
            rpos += rev ? -rate : rate;
            if (rev ? rpos < 0 : rpos >= len) // each repeat is exactly one slice long in both directions
            {
                if (++repeat >= P.stutRepeats) state = 0; else startRepeat (P, c);
            }
        }
        const float wet = sl, wetR = sr2;
        const float ol = P.stutDry * l + P.stutWet * wet, orr = P.stutDry * r + P.stutWet * wetR;
        l = l + send * (ol - l); r = r + send * (orr - r);
    }
    void startRepeat (const EngineParams& P, const FxContext& c)
    {
        switch (P.stutDir)
        {
            case StutDir::Forward: rev = false; break;
            case StutDir::Reverse: rev = true; break;
            case StutDir::Alternate: rev = (repeat % 2) == 1; break;
            case StutDir::Inherit: rev = c.chunkReversed; break;
        }
        rate = std::clamp (std::pow (1.0 + P.stutDrift, (double) repeat), 0.1, 4.0);
        gain = std::pow (1.0f - P.stutDecay, (float) repeat);
        rpos = rev ? len - 1.001 : 0.0;
    }
};

struct Delay
{
    DelayLine dl; OnePole lp[2], hp[2]; double ph = 0; float fbL = 0, fbR = 0; Smooth tL, tR; bool primed = false;
    void prepare (double sr) { dl.prepare ((int) (sr * 4.2) + 16); reset(); tL.setTime (sr, 80); tR.setTime (sr, 80); }
    void reset() { dl.clear(); fbL = fbR = 0; ph = 0; primed = false; for (auto& f : lp) f.z = 0; for (auto& f : hp) f.z = 0; }
    void process (float& l, float& r, float send, const EngineParams& P, const FxContext& c)
    {
        const double sr = c.sr;
        double tl = P.dlyTimeL, tr = P.dlyLink ? P.dlyTimeL : P.dlyTimeR;
        if (P.dlySync > 0) tl = tr = noteBeats (P.dlySync) * 60000.0 / std::max (1.0, c.bpm);
        ph += P.dlyModRate / sr; if (ph >= 1) ph -= 1;
        const float mod = (float) (std::sin (2 * 3.14159265358979 * ph) * P.dlyModDepth * 0.004 * sr);
        const float tgtL = (float) (std::min (tl, 4000.0) * 0.001 * sr), tgtR = (float) (std::min (tr, 4000.0) * 0.001 * sr);
        if (! primed) { tL.reset (tgtL); tR.reset (tgtR); primed = true; } // start at the set time, not glide up from 0
        const float dL = tL.step (tgtL) + mod;
        const float dR = tR.step (tgtR) - mod;
        float yl = dl.read (0, dL), yr = dl.read (1, dR);
        for (int k = 0; k < 2; ++k) { lp[k].setHz (sr, P.dlyLP); hp[k].setHz (sr, P.dlyHP); }
        yl = hp[0].hp (lp[0].lp (yl)); yr = hp[1].hp (lp[1].lp (yr));
        float fb = clampf (P.dlyFb, 0, 0.95f), xf = clampf (P.dlyXfb, 0, 0.95f);
        if (fb + xf > 0.97f) { const float s = 0.97f / (fb + xf); fb *= s; xf *= s; }
        const bool freeze = P.dlyFreeze;
        if (freeze) { fb = 1.0f; xf = 0; }
        float inL = freeze ? 0 : l * send, inR = freeze ? 0 : r * send;
        float wl, wr;
        if (P.dlyPing) { const float mono = 0.5f * (inL + inR); wl = mono + yr * fb + yl * xf; wr = yl * fb + yr * xf; }
        else { wl = inL + yl * fb + yr * xf; wr = inR + yr * fb + yl * xf; }
        wl = std::tanh (wl); wr = std::tanh (wr); // runaway protection
        dl.push (wl, wr);
        const float m = clampf (P.dlyMix, 0, 1), dg = std::min (1.0f, 2 * (1 - m)), wg = std::min (1.0f, 2 * m);
        l = l * dg + yl * wg; r = r * dg + yr * wg;
    }
};

struct Echo
{
    DelayLine dl; OnePole damp[2], tone[2], dubHp[2]; double wow = 0, flt = 0, drift = 0, driftT = 0; float holdL = 0, holdR = 0; int holdN = 0;
    i64 n = 0; Smooth tS; bool primed = false;
    void prepare (double sr) { dl.prepare ((int) (sr * 2.5) + 16); tS.setTime (sr, 120); reset(); }
    void reset() { dl.clear(); wow = flt = drift = driftT = 0; holdL = holdR = 0; holdN = 0; n = 0; primed = false; for (auto* a : { damp, tone, dubHp }) { a[0].z = a[1].z = 0; } }
    void process (float& l, float& r, float send, const EngineParams& P, const FxContext& c)
    {
        const double sr = c.sr; ++n;
        double t = P.echoSync > 0 ? noteBeats (P.echoSync) * 60000.0 / std::max (1.0, c.bpm) : P.echoTime;
        t = std::clamp (t, 10.0, 2000.0);
        const EchoChar ch = P.echoChar;
        float wowAmt = P.echoWow * (ch == EchoChar::Tape ? 1.5f : ch == EchoChar::Clean ? 0.3f : 1.0f);
        wow += 0.6 / sr; flt += 7.0 / sr; if (wow > 1) wow -= 1; if (flt > 1) flt -= 1;
        if ((n & 1023) == 0) driftT = (hashUnit (c.seed, dEcho, n >> 10) * 2 - 1) * P.echoDrift;
        drift += (driftT - drift) * 0.0005;
        const double modMs = wowAmt * (2.5 * std::sin (2 * 3.14159265358979 * wow) + 0.4 * std::sin (2 * 3.14159265358979 * flt)) + drift * 6.0;
        const float tgtS = (float) (t * 0.001 * sr);
        if (! primed) { tS.reset (tgtS); primed = true; }
        const float base = tS.step (tgtS);
        const float sp = P.echoSpread * 0.25f * base;
        float yl = dl.read (0, base + (float) (modMs * 0.001 * sr) - sp * 0.5f);
        float yr = dl.read (1, base + (float) (modMs * 0.001 * sr) + sp * 0.5f);
        // damping in the loop (decay), character colouring
        const double dampHz = 18000.0 * std::pow (0.08, (double) P.echoDecay) * (ch == EchoChar::Dub ? 0.3 : ch == EchoChar::Vinyl ? 0.5 : 1.0);
        for (int k = 0; k < 2; ++k) { damp[k].setHz (sr, dampHz); dubHp[k].setHz (sr, ch == EchoChar::Dub ? 180 : 25); tone[k].setHz (sr, 800 + 17000 * P.echoTone * P.echoTone); }
        yl = dubHp[0].hp (damp[0].lp (yl)); yr = dubHp[1].hp (damp[1].lp (yr));
        float fb = clampf (P.echoFb, 0, 0.95f) * (ch == EchoChar::Dub ? 1.03f : 1.0f);
        fb = std::min (fb, 0.97f);
        float wl = l * send + yl * fb, wr = r * send + yr * fb;
        if (ch == EchoChar::Tape || ch == EchoChar::Dub) { wl = std::tanh (wl * 1.4f) / 1.4f; wr = std::tanh (wr * 1.4f) / 1.4f; }
        else { wl = std::tanh (wl); wr = std::tanh (wr); }
        if (ch == EchoChar::LoFi)
        {
            if (holdN-- <= 0) { holdN = 3; holdL = std::round (wl * 64) / 64; holdR = std::round (wr * 64) / 64; }
            wl = holdL; wr = holdR;
        }
        dl.push (wl, wr);
        float ol = tone[0].lp (yl), orr = tone[1].lp (yr);
        if (ch == EchoChar::Vinyl)
        {
            const double h = hashUnit (c.seed, dNoise, n);
            if (h > 0.9993) { const float k = (float) (hashUnit (c.seed, dNoise + 1, n) - 0.5) * 0.25f; ol += k; orr += k; }
        }
        const float m = clampf (P.echoMix, 0, 1), dg = std::min (1.0f, 2 * (1 - m)), wg = std::min (1.0f, 2 * m);
        l = l * dg + ol * wg; r = r * dg + orr * wg;
    }
};

} // namespace br
