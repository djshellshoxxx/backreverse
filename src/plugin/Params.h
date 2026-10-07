// Stable host parameter table. IDs are immutable once released (spec 05 s1); display names may change.
// X(id, name, kind, min, max, def, skewCentre, choices, unit, tooltip, assignment-from-v)
#pragma once
#include "../core/Types.h"

#define BR_NOTES "Off|1/128|1/128D|1/128T|1/64|1/64D|1/64T|1/32|1/32D|1/32T|1/16|1/16D|1/16T|1/8|1/8D|1/8T|1/4|1/4D|1/4T|1/2|1/2D|1/2T|1 bar|1 bar D|1 bar T|2 bars|2 bars D|2 bars T|4 bars|4 bars D|4 bars T|8 bars|8 bars D|8 bars T|16 bars|16 bars D|16 bars T|32 bars|32 bars D|32 bars T"
#define BR_SCOPES "Global|Alternating Chunks|Chunk Lane|Random Per Chunk|Gate Lane"

#define BR_PARAMS(X) \
X(bypass, "Bypass", B, 0,1,0,0, "", "", "Click-free global bypass; outputs the latency-aligned input so host compensation stays valid.", P.bypass = v > 0.5f) \
X(inGain, "Input Gain", F, -24,24,0,0, "", "dB", "Gain applied before the capture buffer.", P.inGainDb = v) \
X(outGain, "Output Gain", F, -24,24,0,0, "", "dB", "Final output level.", P.outGainDb = v) \
X(mix, "Wet/Dry", F, 0,100,100,0, "", "%", "Blend between dry and processed signal. Dry follows the Dry Path setting.", P.mix = v * 0.01f) \
X(dryAlign, "Dry Path", C, 0,1,0,0, "Latency Aligned|Immediate", "", "Aligned dry is delayed by the reverse latency for phase-coherent blending; Immediate dry is not delayed.", P.dryAlign = (br::DryAlign) (int) v) \
X(source, "Source", C, 0,1,0,0, "Live Input|File", "", "Live input (plug-in/device input) or the loaded/captured file.", P.source = (br::Source) (int) v) \
X(revMode, "Reverse Mode", C, 0,4,0,0, "Sequential Chunk|Reordered Chunk|Whole Source|Forward Chunks|Free Scrub", "", "Sequential: reverse inside each chunk, keep order. Reordered: also apply the chunk order pattern. Whole Source: whole file end-to-start (file only). Forward: chunks play forward (reverse via scope/gates). Free Scrub: playhead driven.", P.revMode = (br::RevMode) (int) v) \
X(revScope, "Reverse Scope", C, 0,3,0,0, "Every Chunk|Alternating|Random|Chunk Lane", "", "Which chunks get reversed.", P.revScope = (br::RevScope) (int) v) \
X(revProb, "Reverse Probability", F, 0,100,50,0, "", "%", "Chance a chunk is reversed when Reverse Scope is Random (seeded).", P.revProb = v * 0.01f) \
X(latPolicy, "Latency Policy", C, 0,1,0,0, "Dynamic|Fixed Maximum", "", "Dynamic: latency follows chunk size. Fixed Maximum: reports Max Buffer so chunk size can vary without latency changes. Adds latency.", P.latPolicy = (br::LatPolicy) (int) v) \
X(maxBuffer, "Max Buffer", F, 10,16000,4000,1000, "", "ms", "Reverse buffer reported as fixed latency in Fixed Maximum policy. Adds latency.", P.maxBufferMs = v) \
X(bpm, "Tempo", F, 20,300,120,0, "", "BPM", "Tempo used for musical values when the host provides none (standalone).", P.bpm = v) \
X(chunkMs, "Chunk Length", F, 1,600000,500,1000, "", "ms", "Duration of each reversed window. Fractional values allowed. In live mode this is also the minimum reverse delay. Adds latency.", P.chunkMs = v) \
X(chunkSync, "Chunk Sync", C, 0,39,0,0, BR_NOTES, "", "Host-synced chunk length (overrides ms when not Off). D = dotted, T = triplet.", P.chunkSync = (int) v) \
X(chunkUnit, "Chunk Units", C, 0,3,0,0, "ms|seconds|samples|beats", "", "Display unit for the chunk length readout.", (void) v) \
X(xfade, "Boundary", C, 0,3,1,0, "Hard|Micro Crossfade|Equal-Power|Zero-Cross Assist", "", "How chunk boundaries are joined.", P.xfade = (br::Xfade) (int) v) \
X(xfadeMs, "Crossfade", F, 0.1f,50,4,0, "", "ms", "Equal-power crossfade time; never more than 25% of a chunk.", P.xfadeMs = v) \
X(order, "Chunk Order", C, 0,10,0,0, "Sequential|Reverse Order|Random|Shuffle (no repeat)|Ping-Pong|Odds then Evens|Evens then Odds|Rotate Left|Rotate Right|A/B Banks|User Pattern", "", "Order in which chunks of a group play (Reordered/Forward modes). Live reordering adds Pattern Length x chunk latency.", P.order = (br::Order) (int) v) \
X(patLen, "Pattern Length", I, 1,64,4,0, "", "", "Chunks per reorder group. Adds latency in live reordered mode.", P.patLen = (int) v) \
X(patStart, "Pattern Offset", I, 0,63,0,0, "", "", "Start offset into the order pattern.", P.patStart = (int) v) \
X(patLoop, "Pattern Loop", B, 0,1,1,0, "", "", "On: pattern loops. Off: one-shot, then sequential until restart.", P.patLoop = v > 0.5f) \
X(repeatProb, "Repeat Chance", F, 0,100,0,0, "", "%", "Chance a slot repeats the previous chunk.", P.repeatProb = v * 0.01f) \
X(skipProb, "Skip Chance", F, 0,100,0,0, "", "%", "Chance a slot is skipped (rest in live mode).", P.skipProb = v * 0.01f) \
X(maxRepeat, "Max Repeats", I, 1,8,2,0, "", "", "Maximum consecutive repeats.", P.maxRepeat = (int) v) \
X(sizeMode, "Size Variation", C, 0,3,0,0, "Fixed|Cycle List|Alternate|Random From List", "", "How chunk sizes vary using the size list (multipliers of Chunk Length).", P.sizeMode = (br::SizeMode) (int) v) \
X(variation, "Variation Amount", F, 0,100,100,0, "", "%", "How strongly the size list applies.", P.variation = v * 0.01f) \
X(seed, "Seed", I, 1,99999,1,0, "", "", "Random seed. Same seed = identical result, realtime or offline.", P.seed = (int) v) \
X(freeze, "Freeze Random", B, 0,1,1,0, "", "", "On: random choices repeat every pattern cycle. Off: new (still seeded) choices each cycle.", P.freeze = v > 0.5f) \
X(restart, "Pattern Restart", C, 0,4,0,0, "Free Run|Host Play|Every Bar|Every Beat|Manual", "", "When patterns restart from step one.", P.restart = (br::Restart) (int) v) \
X(timeMode, "Time Mode", C, 0,1,0,0, "Rate (tape)|Time-Stretch", "", "Rate changes speed and pitch together. Time-Stretch changes duration while preserving pitch.", P.timeMode = (br::TimeMode) (int) v) \
X(ratio, "Speed", F, 0.05f,8,1,1, "", "x", "Playback ratio: 0.25 quarter, 0.5 half, 1 normal, 2 double, 3 triple.", P.ratio = v) \
X(preserve, "Preserve Pitch", B, 0,1,1,0, "", "", "Time-Stretch only: keep pitch. Off gives grainy varispeed.", P.preserve = v > 0.5f) \
X(quality, "Interpolation", C, 0,2,1,0, "Draft|Normal|High", "", "Rate interpolation quality. High uses anti-aliased windowed-sinc above 1x.", P.quality = (br::Quality) (int) v) \
X(algo, "Stretch Algorithm", C, 0,3,0,0, "Smooth (polyphonic)|Rhythmic|Vocal|Surreal", "", "Grain size/character of the time-stretcher.", P.algo = (br::StretchAlgo) (int) v) \
X(stretchReset, "Reset At Boundary", B, 0,1,1,0, "", "", "Reset stretch grains at every chunk boundary.", P.stretchReset = v > 0.5f) \
X(tempoAssign, "Speed Assignment", C, 0,6,0,0, "Same For All|Alternating|Cycle Lane|Random From Set|Weighted Random|Gate Follow|Pattern Follow", "", "How each chunk picks its speed from the speed lane.", P.tempoAssign = (br::TempoAssign) (int) v) \
X(tapeOn, "Tape Stop", B, 0,1,0,0, "", "", "Random effect: tape-stop / spin-up ramps inside chunks.", P.tapeOn = v > 0.5f) \
X(tapeMode, "Tape Mode", C, 0,2,0,0, "Stop|Start|Stop + Start", "", "Brake at chunk end, spin up at chunk start, or both.", P.tapeMode = (br::TapeMode) (int) v) \
X(tapeLen, "Tape Length", F, 2,100,30,0, "", "%", "Portion of the chunk used by the ramp.", P.tapeLen = v * 0.01f) \
X(tapeProb, "Tape Chance", F, 0,100,100,0, "", "%", "Chance per chunk (seeded).", P.tapeProb = v * 0.01f) \
X(panMode, "Pan Mode", C, 0,5,0,0, "Off|Static|Auto-Pan|Alternate|Random|Pan Lane", "", "Pan movement source.", P.panMode = (br::PanMode) (int) v) \
X(pan, "Pan", F, -100,100,0,0, "", "%", "Static pan / centre of movement.", P.pan = v * 0.01f) \
X(panDepth, "Pan Depth", F, 0,100,100,0, "", "%", "Amount of pan movement.", P.panDepth = v * 0.01f) \
X(swap, "Stereo Swap", C, 0,2,0,0, "Off|Swap L/R|Alternate Per Step", "", "Swap left and right channels.", P.swap = (br::SwapMode) (int) v) \
X(panMirror, "Mirror Pan", B, 0,1,0,0, "", "", "Mirror the current pan position / invert the pan trajectory.", P.panMirror = v > 0.5f) \
X(panClock, "Pan/Phase Clock", C, 0,3,0,0, "Reverse Chunk|Independent|Host Beat|Gate Step", "", "Clock for pan and polarity patterns: locked to chunks, independent length, host beats or gates.", P.panClock = (br::PanClock) (int) v) \
X(panClockMs, "Pan Step Length", F, 10,10000,500,1000, "", "ms", "Step length of the independent pan/phase clock.", P.panClockMs = v) \
X(panOffset, "Pan Offset", F, 0,100,0,0, "", "%", "Start offset of the pan/phase clock relative to the reverse stream.", P.panOffset = v * 0.01f) \
X(panSmooth, "Pan Smoothing", F, 0,200,20,0, "", "ms", "Glide time for pan changes.", P.panSmoothMs = v) \
X(polMode, "Polarity", C, 0,6,0,0, "None|Invert Left|Invert Right|Invert Both|Alternate L/R|Polarity Lane|Random", "", "Polarity inversion (multiply by -1). Not the same as phase rotation.", P.polMode = (br::PolMode) (int) v) \
X(phaseMode, "Phase Rotation", C, 0,2,0,0, "Off|Rotate Both|L/R Offset", "", "True broadband phase rotation via an all-pass (IIR Hilbert) network.", P.phaseMode = (br::PhaseMode) (int) v) \
X(phaseDeg, "Phase Angle", F, -180,180,90,0, "", "deg", "Rotation angle (both channels, or right channel only for L/R Offset).", P.phaseDeg = v) \
X(gateOn, "Gate", B, 0,1,0,0, "", "", "Enable the gate sequencer.", P.gateOn = v > 0.5f) \
X(gateSteps, "Gate Steps", C, 0,5,2,0, "2|4|8|16|32|64", "", "Number of gate cells.", P.gateSteps = 2 << (int) v) \
X(gateTiming, "Gate Timing", C, 0,3,0,0, "Fit Chunk|Fixed ms|Host Note|Free Hz", "", "Cell timing.", P.gateTiming = (br::GateTiming) (int) v) \
X(gateMs, "Gate Cell", F, 5,2000,125,0, "", "ms", "Cell length for Fixed ms timing.", P.gateMs = v) \
X(gateNote, "Gate Note", C, 0,39,10,0, BR_NOTES, "", "Cell length for Host Note timing.", P.gateNote = (int) v) \
X(gateHz, "Gate Rate", F, 0.1f,50,4,0, "", "Hz", "Cell rate for Free Hz timing.", P.gateHz = v) \
X(gateShape, "Gate Shape", C, 0,8,0,0, "Hard|Linear In|Linear Out|Triangle|Equal-Power|Sine|Exponential|Logarithmic|Custom Curve", "", "Amplitude shape of active cells (cells may override).", P.gateShape = (br::Shape) (int) v) \
X(gateWidth, "Gate Width", F, 1,100,75,0, "", "%", "Active portion of each cell; the rest is the gap.", P.gateWidth = v * 0.01f) \
X(gateDepth, "Gate Depth", F, 0,100,100,0, "", "%", "How far the gate attenuates.", P.gateDepth = v * 0.01f) \
X(gapMode, "Gap Mode", C, 0,4,0,0, "Silence|Dry-Through|Hold|Crossfade|FX Tail Only", "", "What plays in gaps.", P.gapMode = (br::GapMode) (int) v) \
X(stutOn, "Stutter", B, 0,1,0,0, "", "", "Enable stutter.", P.stutOn = v > 0.5f) \
X(stutScope, "Stutter Scope", C, 0,4,0,0, BR_SCOPES, "", "Where stutter applies.", P.stutScope = (br::Scope) (int) v) \
X(stutProb, "Stutter Chance", F, 0,100,100,0, "", "%", "Chance each trigger fires (seeded).", P.stutProb = v * 0.01f) \
X(stutPeriod, "Stutter Period", F, 20,4000,500,0, "", "ms", "Retrigger period (Period mode).", P.stutPeriodMs = v) \
X(stutSync, "Stutter Sync", C, 0,39,0,0, BR_NOTES, "", "Host-synced period.", P.stutSync = (int) v) \
X(stutLen, "Repeat Length", F, 5,2000,60,0, "", "ms", "Length of the captured slice.", P.stutLenMs = v) \
X(stutRepeats, "Repeats", I, 1,16,4,0, "", "", "Number of repeats.", P.stutRepeats = (int) v) \
X(stutDecay, "Stutter Decay", F, 0,100,10,0, "", "%", "Level drop per repeat.", P.stutDecay = v * 0.01f) \
X(stutDrift, "Pitch Drift", F, -50,50,0,0, "", "%", "Rate change per repeat.", P.stutDrift = v * 0.01f) \
X(stutDir, "Stutter Direction", C, 0,3,0,0, "Forward|Reverse|Alternate|Inherit Chunk", "", "Repeat direction.", P.stutDir = (br::StutDir) (int) v) \
X(stutRetrig, "Retrigger", C, 0,2,0,0, "Period|Chunk Start|Gate Start", "", "What starts a stutter.", P.stutRetrig = (br::Retrig) (int) v) \
X(stutWet, "Stutter Wet", F, 0,100,100,0, "", "%", "Stutter wet level.", P.stutWet = v * 0.01f) \
X(stutDry, "Stutter Dry", F, 0,100,0,0, "", "%", "Stutter dry level.", P.stutDry = v * 0.01f) \
X(dlyOn, "Delay", B, 0,1,0,0, "", "", "Enable delay.", P.dlyOn = v > 0.5f) \
X(dlyScope, "Delay Scope", C, 0,4,0,0, BR_SCOPES, "", "Where the delay input is fed (tails always ring out).", P.dlyScope = (br::Scope) (int) v) \
X(dlyProb, "Delay Chance", F, 0,100,50,0, "", "%", "Chance per chunk for Random scope.", P.dlyProb = v * 0.01f) \
X(dlyTimeL, "Delay Time L", F, 1,4000,375,500, "", "ms", "Left delay time.", P.dlyTimeL = v) \
X(dlyTimeR, "Delay Time R", F, 1,4000,375,500, "", "ms", "Right delay time when unlinked.", P.dlyTimeR = v) \
X(dlyLink, "Link L/R", B, 0,1,1,0, "", "", "Use the left time for both channels.", P.dlyLink = v > 0.5f) \
X(dlySync, "Delay Sync", C, 0,39,0,0, BR_NOTES, "", "Host-synced delay time.", P.dlySync = (int) v) \
X(dlyFb, "Delay Feedback", F, 0,95,35,0, "", "%", "Feedback (protected against runaway).", P.dlyFb = v * 0.01f) \
X(dlyXfb, "Cross Feedback", F, 0,95,0,0, "", "%", "L/R cross feedback.", P.dlyXfb = v * 0.01f) \
X(dlyLP, "Delay Low-Pass", F, 200,20000,12000,2000, "", "Hz", "Feedback low-pass.", P.dlyLP = v) \
X(dlyHP, "Delay High-Pass", F, 20,5000,60,300, "", "Hz", "Feedback high-pass.", P.dlyHP = v) \
X(dlyPing, "Ping-Pong", B, 0,1,0,0, "", "", "Bounce repeats between channels.", P.dlyPing = v > 0.5f) \
X(dlyModRate, "Mod Rate", F, 0.05f,10,0.5f,1, "", "Hz", "Delay-time modulation rate.", P.dlyModRate = v) \
X(dlyModDepth, "Mod Depth", F, 0,100,0,0, "", "%", "Delay-time modulation depth.", P.dlyModDepth = v * 0.01f) \
X(dlyFreeze, "Delay Freeze", B, 0,1,0,0, "", "", "Hold the delay buffer (infinite repeat, input muted).", P.dlyFreeze = v > 0.5f) \
X(dlyMix, "Delay Mix", F, 0,100,35,0, "", "%", "Delay wet/dry.", P.dlyMix = v * 0.01f) \
X(echoOn, "Echo", B, 0,1,0,0, "", "", "Enable character echo.", P.echoOn = v > 0.5f) \
X(echoScope, "Echo Scope", C, 0,4,0,0, BR_SCOPES, "", "Where the echo input is fed.", P.echoScope = (br::Scope) (int) v) \
X(echoProb, "Echo Chance", F, 0,100,50,0, "", "%", "Chance per chunk for Random scope.", P.echoProb = v * 0.01f) \
X(echoTime, "Echo Time", F, 10,2000,300,400, "", "ms", "Echo time.", P.echoTime = v) \
X(echoSync, "Echo Sync", C, 0,39,0,0, BR_NOTES, "", "Host-synced echo time.", P.echoSync = (int) v) \
X(echoFb, "Echo Feedback", F, 0,95,45,0, "", "%", "Repeat amount.", P.echoFb = v * 0.01f) \
X(echoDecay, "Echo Decay", F, 0,100,30,0, "", "%", "High-frequency loss per repeat.", P.echoDecay = v * 0.01f) \
X(echoTone, "Echo Tone", F, 0,100,60,0, "", "%", "Dark to bright.", P.echoTone = v * 0.01f) \
X(echoSpread, "Echo Spread", F, 0,100,30,0, "", "%", "Stereo time offset.", P.echoSpread = v * 0.01f) \
X(echoDrift, "Echo Drift", F, 0,100,20,0, "", "%", "Slow random time drift.", P.echoDrift = v * 0.01f) \
X(echoWow, "Wow/Flutter", F, 0,100,30,0, "", "%", "Tape-style pitch wobble.", P.echoWow = v * 0.01f) \
X(echoChar, "Echo Character", C, 0,4,1,0, "Clean|Tape|Vinyl|Lo-Fi|Dub", "", "Echo flavour.", P.echoChar = (br::EchoChar) (int) v) \
X(echoMix, "Echo Mix", F, 0,100,35,0, "", "%", "Echo wet/dry.", P.echoMix = v * 0.01f) \
X(chainOrder, "FX Chain Order", C, 0,5,0,0, "Stutter > Delay > Echo|Stutter > Echo > Delay|Delay > Stutter > Echo|Delay > Echo > Stutter|Echo > Stutter > Delay|Echo > Delay > Stutter", "", "Order of the effect chain (drag the blocks in the FX rack).", P.chainOrder = (int) v) \
X(chainMix, "Chain Mix", F, 0,100,100,0, "", "%", "Wet/dry of the whole effect chain.", P.chainMix = v * 0.01f) \
X(scrMode, "Scrub Mode", C, 0,3,1,0, "Linear|Vinyl/Scratch|Tape Shuttle|Fine", "", "How dragging the playhead or platter moves playback.", P.scrMode = (br::ScrMode) (int) v) \
X(scrInertia, "Inertia", F, 0,100,50,0, "", "%", "Platter mass (vinyl).", P.scrInertia = v * 0.01f) \
X(scrFriction, "Friction", F, 0,100,30,0, "", "%", "Drag/friction.", P.scrFriction = v * 0.01f) \
X(scrAccel, "Accel Curve", F, 0.5f,3,1,0, "", "", "Velocity response curve.", P.scrAccel = v) \
X(scrMaxRate, "Max Speed", F, 1,16,4,0, "", "x", "Maximum scrub speed.", P.scrMaxRate = v) \
X(scrMotor, "Motor Strength", F, 0,100,50,0, "", "%", "How strongly the motor returns to playback speed.", P.scrMotor = v * 0.01f) \
X(scrRamp, "Release Ramp", F, 0,2000,250,0, "", "ms", "Time to return after release.", P.scrRampMs = v) \
X(scrRelease, "Release", C, 0,2,1,0, "Latch|Spring Return|Continue", "", "After letting go: jump the sequence to the release point, spring back, or coast on.", P.scrRelease = (br::ScrRelease) (int) v) \
X(scrRevLock, "Reverse-Only", B, 0,1,0,0, "", "", "Only backward movement is audible.", P.scrRevLock = v > 0.5f) \
X(scrHold, "Freeze On Hold", B, 0,1,1,0, "", "", "Holding still freezes a micro-buffer.", P.scrHoldFreeze = v > 0.5f) \
X(scrActive, "Scrub (Host)", B, 0,1,0,0, "", "", "Automatable scrub: while on, playback follows Scrub Position.", P.scrActive = v > 0.5f) \
X(scrPos, "Scrub Position", F, 0,100,0,0, "", "%", "Automatable playhead position used when Scrub (Host) is on.", P.scrPos = v * 0.01f) \
X(loop, "Loop", B, 0,1,1,0, "", "", "Loop the file/loop region.", P.loop = v > 0.5f) \
X(fileHostSync, "Follow Host Transport", B, 0,1,0,0, "", "", "File playback starts/stops/seeks with the host transport.", P.fileHostSync = v > 0.5f)

namespace brp {
enum Kind { F, I, B, C };
struct Def { const char* id; const char* name; Kind kind; float min, max, def, centre; const char* choices; const char* unit; const char* tip; };
#define BR_DEF(id, name, kind, mn, mx, df, ce, ch, un, tip, as) { #id, name, kind, (float) (mn), (float) (mx), (float) (df), (float) (ce), ch, un, tip },
inline const Def kDefs[] = { BR_PARAMS (BR_DEF) };
#undef BR_DEF
constexpr int kNum = (int) (sizeof (kDefs) / sizeof (kDefs[0]));
enum Index {
#define BR_IDX(id, ...) id,
    BR_PARAMS (BR_IDX)
#undef BR_IDX
};
inline const Def* find (const char* id) { for (auto& d : kDefs) if (std::strcmp (d.id, id) == 0) return &d; return nullptr; }
// raw[i] holds the plain (denormalised) value of parameter i
template <class Raw> inline void read (const Raw& raw, br::EngineParams& P)
{
#define BR_READ(id, name, kind, mn, mx, df, ce, ch, un, tip, as) { const float v = raw[id]->load (std::memory_order_relaxed); as; }
    BR_PARAMS (BR_READ)
#undef BR_READ
}
} // namespace brp
