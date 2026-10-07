#pragma once
// Built-in help (standalone Help/About area and the plug-in's compact Help tab).
namespace help {
struct Topic { const char* title; const char* body; };
inline const Topic topics[] = {
{ "Quick start",
"1. Pick a Source in the header: Live Input (the plug-in input or audio device) or File.\n"
"2. For files: press Open (or drag a file onto the window) and press Play (Space).\n"
"3. Set Chunk Length on the Chunks tab. Each chunk is played backwards, in order.\n"
"4. Try the factory presets (header). Undo/Redo every edit with Ctrl+Z / Ctrl+Y.\n"
"5. Use A/B to compare two settings, Render to export the processed file, Rec to record the output.\n"
"6. Scratch on the waveform or the turntable on the Scratch tab." },
{ "How reversal works",
"BackReverse does NOT reverse the order of chunks by default. It reverses the samples inside each chronological chunk.\n\n"
"Input 0,1,2,3,4,5,6,7 with chunk size 4 plays 3,2,1,0,7,6,5,4.\n"
"Whole Source plays 7,6,5,4,3,2,1,0. A final partial chunk is reversed on its own: 0..5 -> 3,2,1,0,5,4.\n\n"
"Reordered Chunk mode additionally rearranges chunks with the Chunk Order pattern (Patterns tab). Forward Chunks plays chunks forwards so you can reverse only selected chunks (Reverse Scope) or gates." },
{ "Latency",
"To play a chunk backwards its last sample must exist first, so live reversal is always delayed by at least one chunk. The waveform shows the write head (red), read head (yellow) and the orange reverse buffer.\n\n"
"Dynamic policy: latency = chunk length (x pattern length when reordering live, x the largest size multiplier). Changes apply at the next chunk boundary and the host is told the new latency.\n"
"Fixed Maximum policy: the plug-in always reports Max Buffer, so you can change the chunk length freely below it without the host re-compensating.\n"
"Dry Path: Latency Aligned delays dry by the same amount for phase-coherent Wet/Dry blending; Immediate is undelayed.\n"
"File sources have no input latency (reported latency 0). Live buffers are capped at 16 s; the header shows CLAMPED if a setting exceeds it." },
{ "Chunk examples",
"120 s song, 30 s chunks: plays 30->0, 60->30, 90->60, 120->90.\n"
"120 s song, 10 s chunks: 10->0, 20->10 ... 120->110.\n"
"Chunk >= song length: identical to Whole Source reverse.\n"
"Mixed sizes: set Size Variation to Cycle List and enter multipliers on the Patterns tab, e.g. 0.5, 2, 0.25, 4, 1 with a 500 ms base gives 250 ms, 1 s, 125 ms, 2 s, 500 ms.\n"
"Musical sizes: set Chunk Sync to a note value (1/128 to 32 bars, dotted D and triplet T)." },
{ "Rate vs Time-Stretch",
"Rate (tape) changes speed and pitch together: 0.5x is half speed and an octave down; 2x is double speed and an octave up.\n"
"Time-Stretch changes duration but keeps pitch, using a granular overlap-add stretcher. Algorithms: Smooth (80 ms grains, polyphonic), Rhythmic (30 ms, transients), Vocal (45 ms), Surreal (160 ms jittered grains for deliberately strange textures).\n"
"Quick buttons: 1/4x, 1/2x, 1x, 2x, 3x. Any ratio from 0.05x to 8x can be typed.\n"
"Speed Assignment lets every chunk choose its own speed from the Speed Lane (cycle, random, weighted, gate-follow, pattern-follow). Each lane step can force Rate or Stretch.\n"
"Live input: each chunk keeps its slot so latency stays constant; faster speeds loop the chunk, slower speeds play its latest part." },
{ "Polarity vs phase",
"Polarity inversion multiplies a channel by -1 (Invert Left/Right/Both, alternating, lane or random). It is not phase rotation.\n"
"Phase Rotation is a true broadband phase shift built from an all-pass IIR Hilbert network (two 4-stage all-pass chains about 90 degrees apart, flat magnitude, roughly 20 Hz - 20 kHz at 44.1/48 kHz). Rotate Both shifts both channels; L/R Offset shifts only the right channel, widening the image.\n"
"Pan and polarity patterns share the Pan/Phase Clock: locked to reverse chunks, an independent step length, host beats or gate steps, with a start offset." },
{ "Gate tutorial",
"Enable Gate on the Gates tab. Each cell shows on/off, the active width and gap, the envelope shape, its direction (REV/FWD) and FX badges (S, D, E).\n"
"Click toggles a cell, drag paints. Right-click (or Enter) opens properties: direction (inherit, force reverse, force forward), shape, width, depth, entry/exit fades and per-gate Stutter/Delay/Echo.\n"
"Force Forward inside a reversed chunk plays that slice forwards - alternate them for forward/backward cut-ups.\n"
"Gap Mode decides the gap: Silence, Dry-Through (unprocessed audio), Hold (frozen micro-loop), Crossfade, or FX Tail Only (effects keep ringing).\n"
"Choose Custom Curve and draw the envelope in the curve editor." },
{ "Scratch tutorial",
"Drag the waveform (or grab the turntable) to scrub. Moving backwards plays backwards. Shift-drag is fine scrubbing.\n"
"Modes: Linear (follows the mouse), Vinyl/Scratch (platter with inertia and friction), Tape Shuttle (drag distance = speed), Fine.\n"
"Release: Latch (sequence continues from where you let go), Spring Return (glides back to where the sequence is now), Continue (coasts back up to motor speed).\n"
"Free Scrub mode makes the playhead the only transport: the motor plays backwards at Speed until you grab it.\n"
"Scrub (Host) + Scrub Position let you automate scratching from the DAW." },
{ "Effect routing",
"Stutter, Delay and Echo form a chain. Drag the blocks on the FX tab to reorder (6 orders); click a block to switch it on/off.\n"
"Each effect has a scope: Global, Alternating Chunks, Chunk Lane (selected chunks), Random Per Chunk (seeded probability) or Gate Lane (gates with that FX badge). Unassigned regions bypass the effect input while tails ring out.\n"
"Chain Mix blends the whole chain; every effect also has its own wet/dry." },
{ "Random, seeds and presets",
"All randomness is seeded. With the same seed and settings the result is identical in realtime and offline renders.\n"
"Freeze Random repeats the same random choices every pattern cycle. Randomize (Random tab) rewrites settings within the enabled domains; it is always undoable. Reroll picks the next seed (unless Lock Seed is on).\n"
"Copy/Paste buttons on each tab copy sub-presets (chunk pattern, speed lane, gates, pan/phase, FX chain) to the clipboard.\n"
"User presets live in Documents/BackReverse/Presets. Presets never embed audio." },
{ "New in this beta",
"A/B Compare (usability): header A and B hold two complete settings; A->B copies the current one across.\n"
"Render to WAV (value): bounce the loaded file through the current settings, offline and deterministic. Rec records the live output.\n"
"Turntable platter (fun): a spinning record on the Scratch tab you can grab and scratch.\n"
"Tape Stop (random effect, Time tab): seeded per-chunk tape-stop / spin-up ramps." },
{ "Keyboard shortcuts",
"Space  play / pause (file)\nHome  return to start\nL  toggle loop\nB  bypass\nR  randomize, Shift+R reroll\n1-5  speed 1/4x, 1/2x, 1x, 2x, 3x\n"
"Ctrl+Z  undo, Ctrl+Y / Ctrl+Shift+Z  redo\nCtrl+O  open file\nCtrl+S  save preset\nF1  help\nTab / Shift+Tab move focus; arrows edit focused lanes, gates and the waveform." },
};
}
