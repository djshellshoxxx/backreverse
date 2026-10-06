# BackReverse — UI/UX Specification

## 1. Main interface goals

The interface MUST make the temporal structure visible. A user should be able to understand:
- what source is playing
- current playhead position
- chunk boundaries
- current chunk
- chunk playback direction
- chunk playback rate/stretch
- gate states
- effects assigned to chunks/gates
- current buffer latency

The interface SHOULD remain usable at common plug-in window sizes and scalable on high-DPI displays.

## 2. Main layout

Recommended primary regions:

### Header
- preset browser
- undo/redo
- input/source selector
- transport sync
- latency indicator
- global bypass
- output meter

### Waveform / Timeline
- waveform
- chunk boundary overlays
- playhead
- buffer/capture progress
- selected chunk highlight
- drag-to-scrub interaction
- zoom
- horizontal scroll

### Chunk Controls
- chunk length
- units
- host-sync
- reverse mode
- chunk order
- variation pattern
- rate/stretch controls
- randomization

### Gate Lane
- visual gate cells
- per-gate enable
- shape
- gap
- direction
- assignments

### Pan/Phase Lane
- pan pattern
- polarity pattern
- phase pattern
- timing relationship controls

### FX Rack
- Stutter
- Delay
- Echo
- drag-reorder chain
- per-effect assignment scope

## 3. Waveform interaction

Loaded-file mode MUST render a waveform overview.

Required gestures:
- click = move playhead
- drag = scrub
- Shift+drag or equivalent = fine scrub
- mouse wheel/pinch = zoom
- drag selection = select one or more chunks where applicable

Chunk boundaries MUST update visually when chunk duration changes.

Current active chunk MUST be visually distinct.

## 4. Live-input visualization

When processing live input:
- show capture/write head
- show playback/read head
- show amount of current chunk buffered
- show unavoidable reverse latency
- show when a complete chunk becomes available

The UI MUST not imply zero latency for live reverse processing.

## 5. Chunk inspector

Selecting a chunk MUST expose:
- start/end
- duration
- direction
- rate
- stretch
- pitch preservation
- pan
- polarity/phase
- gate preset
- Stutter assignment
- Delay assignment
- Echo assignment
- probability and randomization status

Multi-select MUST permit batch editing.

## 6. Rate/stretch UI

Provide quick buttons:
- 1/4x
- 1/2x
- 1x
- 2x
- 3x

Also provide:
- continuous ratio control
- typed numeric entry
- Rate / Time-Stretch selector
- Preserve Pitch switch
- algorithm/quality selector

Pattern lane SHOULD show rate values as labels or blocks, e.g.:
1x | 1/2x | 2x | 1/4x | 3x

## 7. Pattern editor

Pattern editor MUST allow:
- add/remove step
- reorder step
- duplicate
- randomize
- seed entry
- freeze random result
- clear
- save pattern
- load pattern

Pattern steps SHOULD be editable by click/drag rather than text only.

Advanced users MAY enter pattern text directly.

## 8. Gate editor

Gate cells MUST be clickable.

At minimum:
- left-click toggles
- drag paints on/off
- right-click/context action opens gate properties
- visible forward/reverse state
- visible assigned FX
- visible custom shape

Gate shape editing MUST be graphical.

## 9. Scratching UI

The waveform playhead MUST be grab-capable.

Vinyl mode SHOULD visually communicate:
- movement speed
- direction
- return target if spring return is active

Optional large platter-style control MAY be provided, but waveform scrubbing remains mandatory.

## 10. Randomization controls

Randomization MUST never be destructive to the current state without undo.

Provide:
- Randomize
- Amount
- Seed
- Lock/Frozen seed
- Reroll
- Undo

Randomization domains MAY include:
- chunk order
- chunk duration
- speed/stretch
- pan
- phase/polarity
- gates
- FX assignments

Master randomize MUST support excluding domains.

## 11. Tooltips and help

Every non-obvious control MUST have a tooltip.

Tooltips SHOULD state:
- what it changes
- its unit/range
- whether it adds latency
- whether it is host automatable

The standalone build MUST include a Help/About area with:
- quick-start
- reversal explanation
- latency explanation
- chunk examples
- Rate vs Time-Stretch explanation
- polarity vs phase explanation
- gate tutorial
- scratch tutorial
- effect-routing tutorial
- keyboard shortcuts

Plug-in builds SHOULD expose the same help content in a compact help panel.

## 12. Undo/redo

Standalone MUST support multi-level undo/redo for editable state.

Plug-ins SHOULD provide internal undo for pattern editing, while host automation/state remains authoritative.

Undoable operations include:
- pattern edits
- randomization
- gate edits
- effect chain changes
- chunk assignments
- preset load if feasible

## 13. Accessibility

- scalable UI
- keyboard navigation for major controls
- visible focus
- labels not dependent on color alone
- numerical values available for graphical controls
- avoid critical states conveyed only by animation
