#include "PluginEditor.h"
#include "HelpText.h"

using namespace juce;
using namespace ui;

namespace {
// ---------------------------------------------------------------- small helpers
struct LenSpin : Component
{
    Label l; TextButton minus { "-" }, plus { "+" }; std::function<int()> get; std::function<void (int)> set; int lo = 1, hi = 16;
    LenSpin (String name, int mn, int mx) : lo (mn), hi (mx)
    {
        l.setText (name, dontSendNotification); l.setColour (Label::textColourId, col::dim); l.setFont (FontOptions (11.5f));
        for (auto* c : { (Component*) &l, (Component*) &minus, (Component*) &plus }) addAndMakeVisible (c);
        minus.setTooltip ("Remove a step"); plus.setTooltip ("Add a step");
        minus.onClick = [this] { if (get && set) set (jlimit (lo, hi, get() - 1)); };
        plus.onClick = [this] { if (get && set) set (jlimit (lo, hi, get() + 1)); };
    }
    void resized() override { auto b = getLocalBounds(); plus.setBounds (b.removeFromRight (24)); minus.setBounds (b.removeFromRight (24)); l.setBounds (b); }
    void refresh() { if (get) l.setText (l.getText().upToFirstOccurrenceOf (":", false, false) + ": " + String (get()), dontSendNotification); }
};

struct CopyPaste : Component
{
    TextButton c { "Copy" }, v { "Paste" };
    CopyPaste (BackReverseProcessor& p, String section, String what)
    {
        c.setTooltip ("Copy the " + what + " to the clipboard as a reusable sub-preset."); v.setTooltip ("Paste a copied " + what + " (undoable).");
        c.onClick = [&p, section] { p.copySection (section); };
        v.onClick = [&p] { p.pasteSection (SystemClipboard::getTextFromClipboard()); };
        addAndMakeVisible (c); addAndMakeVisible (v);
    }
    void resized() override { auto b = getLocalBounds(); c.setBounds (b.removeFromLeft (b.getWidth() / 2).reduced (2, 0)); v.setBounds (b.reduced (2, 0)); }
};

struct Page : Component
{
    std::vector<std::unique_ptr<Component>> owned;
    template <class C> C* make (std::unique_ptr<C> c) { auto* r = c.get(); addAndMakeVisible (*r); owned.push_back (std::move (c)); return r; }
    Panel* panel (const String& t) { return make (std::make_unique<Panel> (t)); }
    std::function<void()> onResize;
    void resized() override { if (onResize) onResize(); }
    void paint (Graphics& g) override { g.fillAll (col::bg); }
};

// ---------------------------------------------------------------- order pattern editor
class PatternEditor : public Component, public SettableTooltipClient, private Timer
{
public:
    explicit PatternEditor (BackReverseProcessor& pr) : p (pr)
    {
        setWantsKeyboardFocus (true);
        setTooltip ("Order pattern steps (used when Chunk Order = User Pattern). Click/drag sets the chunk each step plays; right-click a step for rest, random, relative, probability, duplicate or delete.");
        text.setTooltip ("Pattern text: numbers = chunk in group, +n/-n relative, REST, ? random, n%50 probability, n*3 repeat. Press Enter to apply.");
        text.onReturnKey = [this] { apply (StringArray::fromTokens (text.getText(), ",", "")); };
        text.onFocusLost = text.onReturnKey;
        text.setText (p.userPattern(), false);
        addAndMakeVisible (text);
        const char* names[] = { "Add", "Remove", "Duplicate", "<", ">", "Randomize", "Clear", "Save...", "Load..." };
        const char* tips[] = { "Add a step", "Remove the selected step", "Duplicate the whole pattern", "Move selected step left", "Move selected step right", "Randomize the pattern (seeded, undoable)", "Reset to 0,1,2,3...", "Save pattern to a file", "Load a pattern file" };
        for (int i = 0; i < 9; ++i) { auto* b = btn.add (new TextButton (names[i])); b->setTooltip (tips[i]); addAndMakeVisible (b); b->onClick = [this, i] { action (i); }; }
        startTimerHz (10);
    }
    StringArray toks() const { auto t = StringArray::fromTokens (p.userPattern(), ",", ""); t.trim(); t.removeEmptyStrings(); if (t.isEmpty()) t.add ("0"); return t; }
    void apply (StringArray t) { t.trim(); t.removeEmptyStrings(); p.undo.beginNewTransaction ("Pattern"); p.setUserPattern (t.joinIntoString (",")); repaint(); }
    int G() const { return jmax (1, (int) p.get (brp::patLen)); }
    Rectangle<int> bars() const { return getLocalBounds().withTrimmedTop (16).withTrimmedBottom (64); }
    void paint (Graphics& g) override
    {
        g.setColour (col::dim); g.setFont (FontOptions (11.5f)); g.drawText ("Order pattern (chunk index within a group of Pattern Length)", 0, 0, getWidth(), 14, Justification::centredLeft);
        auto a = bars().toFloat(); g.setColour (col::panel2); g.fillRoundedRectangle (a, 4);
        const auto t = toks(); const float w = a.getWidth() / t.size();
        for (int i = 0; i < t.size(); ++i)
        {
            auto c = Rectangle<float> (a.getX() + i * w, a.getY(), w, a.getHeight()).reduced (2);
            String s = t[i].toUpperCase(); const float prob = s.containsChar ('%') ? s.fromFirstOccurrenceOf ("%", false, false).getFloatValue() / 100.0f : 1.0f;
            const String core = s.upToFirstOccurrenceOf ("%", false, false).upToFirstOccurrenceOf ("*", false, false);
            if (core == "REST" || core == "R" || core == "-" || core == "_") { g.setColour (col::line); for (float y = c.getY(); y < c.getBottom(); y += 6) g.drawLine (c.getX(), y, c.getRight(), y + 6); }
            else if (core == "?") { g.setColour (col::purple.withAlpha (0.6f)); g.fillRect (c); }
            else if (core.startsWithChar ('+') || (core.startsWithChar ('-') && core.length() > 1)) { g.setColour (col::fwd.withAlpha (0.6f)); g.fillRect (c.withTrimmedTop (c.getHeight() * 0.5f)); }
            else { const float v = (float) ((core.getIntValue() % G()) + 1) / G(); g.setColour (col::accent.withAlpha (0.35f + 0.6f * prob)); g.fillRect (c.withTop (c.getBottom() - v * c.getHeight())); }
            g.setColour (i == sel ? col::yellow : col::text); g.setFont (FontOptions (12.0f));
            g.drawText (t[i], c.toNearestInt(), Justification::centredTop);
            if (i == sel) g.drawRect (c, 1.0f);
        }
    }
    void resized() override
    {
        auto b = getLocalBounds(); auto row = b.removeFromBottom (26); text.setBounds (b.removeFromBottom (30).reduced (0, 3));
        const int w = row.getWidth() / btn.size(); for (auto* x : btn) x->setBounds (row.removeFromLeft (w).reduced (2, 0));
    }
    int stepAt (int x) const { return jlimit (0, toks().size() - 1, (x - bars().getX()) * toks().size() / jmax (1, bars().getWidth())); }
    void setStep (int i, int y)
    {
        auto t = toks(); const String s = t[i]; const String suffix = s.containsChar ('%') ? "%" + s.fromFirstOccurrenceOf ("%", false, false) : String();
        const int v = jlimit (0, G() - 1, (int) ((1.0f - (float) (y - bars().getY()) / bars().getHeight()) * G()));
        t.set (i, String (v) + suffix);
        p.setUserPattern (t.joinIntoString (",")); repaint();
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (! bars().contains (e.getPosition())) return;
        sel = stepAt (e.x);
        if (e.mods.isPopupMenu())
        {
            PopupMenu m; m.addItem (1, "Rest"); m.addItem (2, "Random (?)"); m.addItem (3, "Relative +1"); m.addItem (4, "Relative -1");
            m.addItem (5, "50% probability"); m.addItem (6, "Always (100%)"); m.addItem (7, "Duplicate step"); m.addItem (8, "Delete step");
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r)
            {
                auto t = toks(); if (r <= 0 || sel >= t.size()) return;
                const String core = t[sel].upToFirstOccurrenceOf ("%", false, false);
                if (r == 1) t.set (sel, "REST"); if (r == 2) t.set (sel, "?"); if (r == 3) t.set (sel, "+1"); if (r == 4) t.set (sel, "-1");
                if (r == 5) t.set (sel, core + "%50"); if (r == 6) t.set (sel, core);
                if (r == 7) t.insert (sel, t[sel]); if (r == 8 && t.size() > 1) t.remove (sel);
                apply (t);
            });
            return;
        }
        p.undo.beginNewTransaction ("Pattern"); setStep (sel, e.y);
    }
    void mouseDrag (const MouseEvent& e) override { if (! e.mods.isPopupMenu() && bars().contains (e.getPosition())) { sel = stepAt (e.x); setStep (sel, e.y); } }
    bool keyPressed (const KeyPress& k) override
    {
        auto t = toks();
        if (k == KeyPress::leftKey) { sel = (sel + t.size() - 1) % t.size(); repaint(); return true; }
        if (k == KeyPress::rightKey) { sel = (sel + 1) % t.size(); repaint(); return true; }
        if (k == KeyPress::upKey || k == KeyPress::downKey) { t.set (sel, String (jlimit (0, G() - 1, t[sel].getIntValue() + (k == KeyPress::upKey ? 1 : -1)))); apply (t); return true; }
        return false;
    }
    void action (int i)
    {
        auto t = toks(); sel = jlimit (0, t.size() - 1, sel);
        switch (i)
        {
            case 0: t.add (String (t.size() % G())); break;
            case 1: if (t.size() > 1) t.remove (sel); break;
            case 2: t.addArray (StringArray (t)); break;
            case 3: if (sel > 0) { t.move (sel, sel - 1); --sel; } break;
            case 4: if (sel < t.size() - 1) { t.move (sel, sel + 1); ++sel; } break;
            case 5: { br::Rng r ((uint64_t) p.get (brp::seed) * 31u + 7u + (uint64_t) t.size()); for (auto& s : t) s = r.next() < 0.1 ? "REST" : String (r.below (G())); break; }
            case 6: t.clear(); for (int k = 0; k < G(); ++k) t.add (String (k)); break;
            case 7:
                fc = std::make_unique<FileChooser> ("Save pattern", BackReverseProcessor::presetDir().getChildFile ("pattern.brpattern"), "*.brpattern");
                fc->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& c) { if (c.getResult() != File()) c.getResult().replaceWithText (p.userPattern()); });
                return;
            case 8:
                fc = std::make_unique<FileChooser> ("Load pattern", BackReverseProcessor::presetDir(), "*.brpattern;*.txt");
                fc->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& c) { if (c.getResult().existsAsFile()) apply (StringArray::fromTokens (c.getResult().loadFileAsString(), ",", "")); });
                return;
            default: break;
        }
        apply (t);
    }
private:
    void timerCallback() override { if (! text.hasKeyboardFocus (true) && text.getText() != p.userPattern()) { text.setText (p.userPattern(), false); repaint(); } }
    BackReverseProcessor& p; TextEditor text; OwnedArray<TextButton> btn; int sel = 0; std::unique_ptr<FileChooser> fc;
};

// ---------------------------------------------------------------- chunk inspector (batch editing of selected chunks)
class Inspector : public Component
{
public:
    explicit Inspector (BackReverseProcessor& pr) : p (pr)
    {
        info.setJustificationType (Justification::topLeft); info.setFont (FontOptions (12.5f)); info.setColour (Label::textColourId, col::text);
        addAndMakeVisible (info);
        const char* n[] = { "Reverse", "Stutter", "Delay", "Echo" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = flags.add (new TextButton (String (n[i]) + " +/-")); addAndMakeVisible (b);
            b->setTooltip ("Toggle " + String (n[i]) + " for the selected chunks (Chunk Lane steps they map to). Used when the scope is Chunk Lane.");
            b->onClick = [this, i] { toggleBit (i); };
        }
        rate.addItemList ({ "1/4x", "1/2x", "1x", "2x", "3x" }, 1); rate.setTextWhenNothingSelected ("Speed..."); rate.setTooltip ("Set the Speed Lane step of the selected chunks (Cycle Lane assignment).");
        rate.onChange = [this] { const float r[5] = { 0.25f, 0.5f, 1, 2, 3 }; if (rate.getSelectedId() > 0) edit ([&] (br::Patterns& q, int64 m) { q.rate[m % jmax (1, q.rateLen)].ratio = r[rate.getSelectedId() - 1]; }); rate.setSelectedId (0, dontSendNotification); };
        pol.addItemList ({ "Polarity: none", "Invert L", "Invert R", "Invert both" }, 1); pol.setTextWhenNothingSelected ("Polarity..."); pol.setTooltip ("Set the Polarity Lane step of the selected chunks.");
        pol.onChange = [this] { if (pol.getSelectedId() > 0) edit ([&] (br::Patterns& q, int64 m) { q.pol[m % jmax (1, q.polLen)] = (uint8_t) (pol.getSelectedId() - 1); }); pol.setSelectedId (0, dontSendNotification); };
        pan.setSliderStyle (Slider::LinearHorizontal); pan.setRange (-100, 100, 1); pan.setTextBoxStyle (Slider::TextBoxRight, false, 44, 18); pan.setTooltip ("Set the Pan Lane step of the selected chunks.");
        pan.onDragStart = [this] { p.undo.beginNewTransaction ("Pan"); };
        pan.onValueChange = [this] { if (first >= 0) edit ([&] (br::Patterns& q, int64 m) { q.pan[m % jmax (1, q.panLen)] = (float) pan.getValue() * 0.01f; }, false); };
        for (auto* c : { (Component*) &rate, (Component*) &pol, (Component*) &pan }) addAndMakeVisible (c);
        refresh();
    }
    void setSelection (int64 a, int64 b) { first = a; last = b; refresh(); }
    void edit (std::function<void (br::Patterns&, int64)> fn, bool newTx = true)
    {
        if (first < 0) return;
        if (newTx) p.undo.beginNewTransaction ("Chunk edit");
        br::Patterns q = p.pats; for (int64 m = first; m <= last && m < first + 64; ++m) fn (q, m); p.applyPatterns (q); refresh();
    }
    void toggleBit (int bit)
    {
        bool all = true; for (int64 m = first; m <= last && m < first + 64; ++m) all = all && (p.pats.chunkFlags[m % jmax (1, p.pats.laneLen)] & (1 << bit));
        edit ([&] (br::Patterns& q, int64 m) { auto& f = q.chunkFlags[m % jmax (1, q.laneLen)]; f = (uint8_t) (all ? (f & ~(1 << bit)) : (f | (1 << bit))); });
    }
    void refresh()
    {
        auto src = p.source();
        for (auto* b : flags) b->setEnabled (first >= 0);
        rate.setEnabled (first >= 0); pol.setEnabled (first >= 0); pan.setEnabled (first >= 0);
        if (first < 0 || ! src) { info.setText ("Chunk inspector\nCtrl/Alt-drag across the waveform (file mode) to select one or more chunks, then edit them here as a batch.", dontSendNotification); return; }
        const auto P = p.snapshot(); const auto& q = p.pats; const double sr = src->sb.sampleRate;
        const double base = br::Engine::baseChunkFrames (P, sr, p.engine.tel.bpm.load(), 4.0);
        double s = P.loop ? (double) p.ui (lk::loopA, 0.0) * src->sb.length : 0.0;
        for (int64 m = 0; m < first; ++m) s += base * br::Engine::sizeMultiplier (P, q, m);
        double e = s; for (int64 m = first; m <= last; ++m) e += base * br::Engine::sizeMultiplier (P, q, m);
        e = jmin (e, (double) src->sb.length);
        const int64 m = first; const auto& rs = q.rate[m % jmax (1, q.rateLen)];
        const bool laneRate = P.tempoAssign == br::TempoAssign::CycleLane;
        const float r = laneRate ? rs.ratio : P.ratio; const int mode = laneRate ? rs.stretch : 0;
        const bool stretch = mode == 2 || (mode == 0 && P.timeMode == br::TimeMode::Stretch);
        auto scope = [&] (int sid, int bit) -> String
        {
            const auto sc = (br::Scope) (int) p.get (sid);
            switch (sc) { case br::Scope::Global: return "all"; case br::Scope::Alternating: return (m & 1) ? "yes (odd)" : "no (even)";
                          case br::Scope::ChunkLane: return (q.chunkFlags[m % jmax (1, q.laneLen)] & (1 << bit)) ? "yes (lane)" : "no (lane)";
                          case br::Scope::Random: return "random"; case br::Scope::GateLane: return "per gate"; }
            return {};
        };
        int gOn = 0; const int steps = P.gateSteps; for (int i = 0; i < steps; ++i) gOn += q.gate[i].on;
        String t;
        t << (last > first ? "Chunks " + String (first) + "-" + String (last) : "Chunk " + String (first)) << "   start " << String (s / sr, 3) << " s   end " << String (e / sr, 3)
          << " s   duration " << String ((e - s) / sr, 3) << " s\n"
          << "Direction: " << (br::Engine::chunkReversed (P, q, m) || P.revMode == br::RevMode::WholeSource ? "reversed" : "forward")
          << "   Speed: " << ratioLabel (r) << (stretch ? "  Time-Stretch" : "  Rate") << (stretch ? (P.preserve ? " (pitch preserved)" : " (pitch follows)") : " (pitch follows)") << "\n"
          << "Pan: " << (P.panMode == br::PanMode::Lane ? String (q.pan[m % jmax (1, q.panLen)] * 100, 0) + "% (lane)" : StringArray::fromTokens (brp::kDefs[brp::panMode].choices, "|", "")[(int) P.panMode])
          << "   Polarity: " << StringArray ({ "none", "invert L", "invert R", "invert both" })[q.pol[m % jmax (1, q.polLen)] & 3] << (P.polMode == br::PolMode::Lane ? " (lane)" : " (lane, inactive)")
          << "   Phase: " << StringArray::fromTokens (brp::kDefs[brp::phaseMode].choices, "|", "")[(int) P.phaseMode] << "\n"
          << "Gate: " << (P.gateOn ? String (gOn) + "/" + String (steps) + " cells on" : String ("off"))
          << "   Stutter: " << scope (brp::stutScope, 1) << "   Delay: " << scope (brp::dlyScope, 2) << "   Echo: " << scope (brp::echoScope, 3) << "\n"
          << "Randomization: seed " << P.seed << (P.freeze ? " (frozen)" : " (evolving)") << (P.revScope == br::RevScope::Random ? ", random direction" : "")
          << (P.sizeMode == br::SizeMode::RandomList ? ", random size" : "") << (P.tempoAssign == br::TempoAssign::RandomSet || P.tempoAssign == br::TempoAssign::Weighted ? ", random speed" : "");
        info.setText (t, dontSendNotification);
    }
    void paint (Graphics& g) override { g.setColour (col::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (8); auto row = b.removeFromBottom (26);
        const int w = row.getWidth() / 7;
        for (auto* f : flags) f->setBounds (row.removeFromLeft (w).reduced (2, 0));
        rate.setBounds (row.removeFromLeft (w).reduced (2, 0)); pol.setBounds (row.removeFromLeft (w).reduced (2, 0)); pan.setBounds (row.reduced (2, 0));
        info.setBounds (b);
    }
private:
    BackReverseProcessor& p; Label info; OwnedArray<TextButton> flags; ComboBox rate, pol; Slider pan; int64 first = -1, last = -1;
};

std::unique_ptr<StepLane> laneBits (BackReverseProcessor& p, const String& title, int bit, Colour c)
{
    auto l = std::make_unique<StepLane>(); l->title = title; l->toggles = true; l->titleWidth = 250;
    l->count = [&p] { return p.pats.laneLen; };
    l->get = [&p, bit] (int i) { return (p.pats.chunkFlags[i] & (1 << bit)) ? 1.0f : 0.0f; };
    l->set = [&p, bit] (int i, float v) { br::Patterns q = p.pats; q.chunkFlags[i] = (uint8_t) (v > 0.5f ? q.chunkFlags[i] | (1 << bit) : q.chunkFlags[i] & ~(1 << bit)); p.applyPatterns (q); };
    l->begin = [&p] { p.undo.beginNewTransaction ("Chunk lane"); };
    l->colour = [c] (int) { return c; };
    l->highlight = [&p] { return (int) (p.engine.tel.chunkIdx.load() % jmax (1, p.pats.laneLen)); };
    return l;
}
} // namespace

// ================================================================ editor content
struct BackReverseEditor::Content : public Component
{
    BackReverseProcessor& p;
    // header
    Label title, latL, timeL; ComboBox presets; Meter meter;
    std::unique_ptr<TextButton> prevP, nextP, saveP, undoB, redoB, aB, bB, abCopy, helpB, openB, capB, playB, pauseB, stopB, rtzB, restartB, renderB, recB;
    std::unique_ptr<Toggle> bypass, loop, hostSync; std::unique_ptr<Choice> source;
    Waveform wave { p };
    TabbedComponent tabs { TabbedButtonBar::TabsAtTop };
    // pages & widgets needing refresh
    Inspector* inspector = nullptr; GateEditor* gates = nullptr; Platter* platter = nullptr; ChainStrip* chain = nullptr;
    std::vector<StepLane*> lanesToRepaint; std::vector<LenSpin*> spins; TextEditor* helpText = nullptr; Slider* randAmt = nullptr;
    OwnedArray<ToggleButton> randDomains; ToggleButton* randLock = nullptr;
    std::unique_ptr<FileChooser> fc; int lastPresetCount = -1;

    explicit Content (BackReverseProcessor& pr) : p (pr)
    {
        title.setText ("BackReverse", dontSendNotification); title.setFont (FontOptions (22.0f, Font::bold)); title.setColour (Label::textColourId, col::accent);
        title.setTooltip ("BackReverse 0.0.1 beta by Circuit Drift Labs");
        addAndMakeVisible (title);
        presets.setTooltip ("Factory and user presets. Loading is undoable."); presets.onChange = [this] { if (presets.getSelectedItemIndex() >= 0) p.loadPreset (presets.getSelectedItemIndex()); };
        addAndMakeVisible (presets); refreshPresets();
        auto mk = [this] (const String& t, const String& tip, std::function<void()> fn) { auto b = button (t, tip, std::move (fn)); addAndMakeVisible (*b); return b; };
        prevP = mk ("<", "Previous preset", [this] { step (-1); });
        nextP = mk (">", "Next preset", [this] { step (1); });
        saveP = mk ("Save", "Save a user preset (Ctrl+S). Presets never embed audio.", [this] { savePreset(); });
        undoB = mk ("Undo", "Undo (Ctrl+Z): pattern, gate, chain, randomize, preset and parameter edits.", [this] { p.undo.undo(); });
        redoB = mk ("Redo", "Redo (Ctrl+Y)", [this] { p.undo.redo(); });
        aB = mk ("A", "A/B compare: switch to setting A", [this] { p.selectAB (0); });
        bB = mk ("B", "A/B compare: switch to setting B", [this] { p.selectAB (1); });
        abCopy = mk ("A>B", "Copy the active A/B slot into the other one", [this] { p.copyABToOther(); });
        helpB = mk ("?", "Help (F1)", [this] { tabs.setCurrentTabIndex (tabs.getNumTabs() - 1); });
        openB = mk ("Open", "Open an audio file (Ctrl+O). WAV, AIFF, FLAC, MP3, OGG. You can also drag & drop.", [this] { openFile(); });
        capB = mk ("Capture", "Record live input into a finite capture buffer (up to 60 s). Stop it to use the capture as a file source, enabling Whole Source reverse.", [this] { if (p.capturing()) p.stopCapture(); else p.startCapture(); });
        playB = mk ("Play", "Play file (Space)", [this] { p.engine.ctl.transport.store (1); });
        pauseB = mk ("Pause", "Pause file (Space)", [this] { p.engine.ctl.transport.store (2); });
        stopB = mk ("Stop", "Stop and return to start", [this] { p.engine.ctl.transport.store (3); });
        rtzB = mk ("|<", "Return to start (Home)", [this] { p.engine.ctl.transport.store (4); });
        restartB = mk ("Restart", "Restart patterns now (manual trigger)", [this] { p.engine.ctl.restart.store (true); });
        renderB = mk ("Render...", "Render the loaded file through the current settings to a WAV file (offline, deterministic).", [this] { render(); });
        recB = mk ("Rec", "Record the processed output to a WAV file in Documents/BackReverse/Recordings.", [this] { record(); });
        bypass = std::make_unique<Toggle> (p, brp::bypass); loop = std::make_unique<Toggle> (p, brp::loop); hostSync = std::make_unique<Toggle> (p, brp::fileHostSync);
        source = std::make_unique<Choice> (p, brp::source);
        for (auto* c : { (Component*) bypass.get(), (Component*) loop.get(), (Component*) hostSync.get(), (Component*) source.get(), (Component*) &meter, (Component*) &latL, (Component*) &timeL }) addAndMakeVisible (c);
        latL.setFont (FontOptions (12.0f)); latL.setColour (Label::textColourId, col::text); latL.setTooltip ("Chunk length, the unavoidable reverse buffering delay and the latency reported to the host.");
        timeL.setFont (FontOptions (12.0f)); timeL.setColour (Label::textColourId, col::dim);
        meter.setTooltip ("Output level");
        addAndMakeVisible (wave);
        wave.onSelect = [this] (int64 a, int64 b) { if (inspector) inspector->setSelection (a, b); };
        tabs.setTabBarDepth (28); tabs.setOutline (0);
        buildPages();
        tabs.setCurrentTabIndex (jlimit (0, tabs.getNumTabs() - 1, (int) p.ui (lk::uiTab, 0)));
        addAndMakeVisible (tabs);
    }

    void refreshPresets()
    {
        auto names = p.presetNames();
        if (names.size() == lastPresetCount) { presets.setSelectedItemIndex (p.currentPreset, dontSendNotification); return; }
        lastPresetCount = names.size(); presets.clear (dontSendNotification); presets.addItemList (names, 1); presets.setSelectedItemIndex (p.currentPreset, dontSendNotification);
    }
    void step (int d) { const int n = presets.getNumItems(); if (n > 0) presets.setSelectedItemIndex ((p.currentPreset + d + n) % n, sendNotification); }
    void savePreset()
    {
        auto* w = new AlertWindow ("Save preset", "Preset name:", MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", "My preset"); w->addButton ("Save", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int r) { if (r == 1) { p.saveUserPreset (w->getTextEditorContents ("name")); refreshPresets(); } }), true);
    }
    void openFile()
    {
        fc = std::make_unique<FileChooser> ("Open audio file", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        fc->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& c) { if (c.getResult().existsAsFile()) load (c.getResult()); });
    }
    void load (const File& f)
    {
        String err;
        if (! p.loadFile (f, &err) || err.isNotEmpty()) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "BackReverse", err);
        wave.repaint();
    }
    void render()
    {
        if (! p.source()) { AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Render", "Load or capture a file first."); return; }
        auto def = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("BackReverse").getChildFile ("Renders");
        def.createDirectory();
        fc = std::make_unique<FileChooser> ("Render to WAV", def.getChildFile (File::createLegalFileName (p.source()->name.upToLastOccurrenceOf (".", false, false)) + " - BackReverse.wav"), "*.wav");
        fc->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& c)
        { auto f = c.getResult(); if (f != File()) { if (! p.startRender (f.withFileExtension ("wav"))) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Render", "A render is already running."); } });
    }
    void record()
    {
        if (p.isRecording()) { p.stopRecording(); return; }
        auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("BackReverse").getChildFile ("Recordings"); dir.createDirectory();
        if (! p.startRecording (dir.getChildFile ("BackReverse " + Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S") + ".wav")))
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Record", "Could not create the recording file.");
    }

    // ------------------------------------------------------------ pages
    StepLane* lane (Page* pg, std::unique_ptr<StepLane> l) { auto* r = pg->make (std::move (l)); lanesToRepaint.push_back (r); return r; }
    LenSpin* spin (Page* pg, const String& name, int lo, int hi, std::function<int()> g, std::function<void (int)> s)
    { auto* r = pg->make (std::make_unique<LenSpin> (name, lo, hi)); r->get = std::move (g); r->set = std::move (s); spins.push_back (r); return r; }

    void buildPages()
    {
        auto addTab = [this] (const String& name, Page* pg) { tabs.addTab (name, col::bg, pg, true); };
        auto withPats = [this] (std::function<void (br::Patterns&)> fn) { p.undo.beginNewTransaction ("Lane"); br::Patterns q = p.pats; fn (q); p.applyPatterns (q); };

        // ---- Chunks
        {
            auto* pg = new Page();
            auto* rev = pg->panel ("Reverse & chunks");
            rev->addAll (p, { brp::revMode, brp::revScope, brp::revProb, brp::chunkMs, brp::chunkSync, brp::chunkUnit, brp::xfade, brp::xfadeMs });
            rev->own (button ("Whole File", "Shortcut: reverse the whole loaded file end-to-start (Whole Source mode).", [this] { p.set (brp::source, 1); p.set (brp::revMode, 2); }), 100, 26);
            rev->own (button ("Flush Live", "Discard buffered live audio and restart capture (also happens on host seek).", [this] { p.engine.ctl.flush.store (true); }), 100, 26);
            auto* ord = pg->panel ("Order & randomness");
            ord->addAll (p, { brp::order, brp::patLen, brp::patStart, brp::repeatProb, brp::skipProb, brp::maxRepeat, brp::seed, brp::restart, brp::patLoop, brp::freeze });
            auto* l1 = lane (pg, laneBits (p, "Chunk Lane: reverse-select\n(used when Reverse Scope = Chunk Lane)", 0, col::rev));
            auto* l2 = lane (pg, laneBits (p, "Chunk Lane: Stutter (scope Chunk Lane)", 1, col::purple));
            auto* l3 = lane (pg, laneBits (p, "Chunk Lane: Delay (scope Chunk Lane)", 2, col::fwd));
            auto* l4 = lane (pg, laneBits (p, "Chunk Lane: Echo (scope Chunk Lane)", 3, col::green));
            auto* sp = spin (pg, "Chunk lane steps", 1, br::kLane, [this] { return p.pats.laneLen; }, [this, withPats] (int v) { withPats ([v] (br::Patterns& q) { q.laneLen = v; }); });
            auto* cp = pg->make (std::make_unique<CopyPaste> (p, "chunk", "chunk pattern (order, sizes, chunk lane)"));
            inspector = pg->make (std::make_unique<Inspector> (p));
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); auto top = b.removeFromTop (224);
                rev->setBounds (top.removeFromLeft (560)); top.removeFromLeft (6); ord->setBounds (top);
                b.removeFromTop (6); inspector->setBounds (b.removeFromBottom (104)); b.removeFromBottom (4);
                auto hdr = b.removeFromTop (24); sp->setBounds (hdr.removeFromLeft (220)); cp->setBounds (hdr.removeFromRight (160));
                const int h = b.getHeight() / 4; l1->setBounds (b.removeFromTop (h)); l2->setBounds (b.removeFromTop (h)); l3->setBounds (b.removeFromTop (h)); l4->setBounds (b); };
            addTab ("Chunks", pg);
        }
        // ---- Patterns
        {
            auto* pg = new Page();
            auto* pe = pg->make (std::make_unique<PatternEditor> (p));
            auto* sz = pg->panel ("Chunk size variation"); sz->addAll (p, { brp::sizeMode, brp::variation });
            auto* mult = lane (pg, std::make_unique<StepLane>());
            mult->title = "Size list: multiplier of Chunk Length (x1/8 .. x8)"; mult->count = [this] { return p.pats.sizeCount; };
            mult->get = [this] (int i) { return (std::log2 (p.pats.sizeMult[i]) + 3.0f) / 6.0f; };
            mult->set = [this] (int i, float v) { br::Patterns q = p.pats; q.sizeMult[i] = std::exp2 (std::round ((v * 6 - 3) * 4) / 4); p.applyPatterns (q); };
            mult->label = [this] (int i) { return "x" + String (p.pats.sizeMult[i], p.pats.sizeMult[i] < 1 ? 3 : 2); };
            mult->begin = [this] { p.undo.beginNewTransaction ("Sizes"); };
            auto* wt = lane (pg, std::make_unique<StepLane>());
            wt->title = "Size weights (Random From List)"; wt->count = mult->count; wt->colour = [] (int) { return col::purple; };
            wt->get = [this] (int i) { return p.pats.sizeWeight[i] / 4.0f; }; wt->set = [this] (int i, float v) { br::Patterns q = p.pats; q.sizeWeight[i] = v * 4; p.applyPatterns (q); };
            wt->label = [this] (int i) { return String (p.pats.sizeWeight[i], 2); }; wt->begin = mult->begin;
            auto* sp = spin (pg, "Size list entries", 1, br::kSizes, [this] { return p.pats.sizeCount; }, [withPats] (int v) { withPats ([v] (br::Patterns& q) { q.sizeCount = v; }); });
            auto* cp = pg->make (std::make_unique<CopyPaste> (p, "chunk", "chunk pattern (order, sizes, chunk lane)"));
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); pe->setBounds (b.removeFromTop (230)); b.removeFromTop (8);
                auto left = b.removeFromLeft (330); sz->setBounds (left.removeFromTop (90)); left.removeFromTop (6); sp->setBounds (left.removeFromTop (24)); left.removeFromTop (6); cp->setBounds (left.removeFromTop (26).withWidth (160));
                b.removeFromLeft (8); mult->setBounds (b.removeFromTop (b.getHeight() * 6 / 10)); b.removeFromTop (6); wt->setBounds (b); };
            addTab ("Patterns", pg);
        }
        // ---- Time
        {
            auto* pg = new Page();
            auto* sp = pg->panel ("Speed / time");
            sp->addAll (p, { brp::timeMode, brp::ratio });
            const float qv[5] = { 0.25f, 0.5f, 1, 2, 3 }; const char* qn[5] = { "1/4x", "1/2x", "1x", "2x", "3x" };
            for (int i = 0; i < 5; ++i) { const float v = qv[i]; sp->own (button (qn[i], String ("Set speed to ") + qn[i] + " (key " + String (i + 1) + ")", [this, v] { p.set (brp::ratio, v); }), 50, 26); }
            sp->addAll (p, { brp::tempoAssign, brp::quality, brp::algo, brp::preserve, brp::stretchReset });
            auto* tape = pg->panel ("Tape Stop (random effect)"); tape->addAll (p, { brp::tapeOn, brp::tapeMode, brp::tapeLen, brp::tapeProb });
            auto* rl = lane (pg, std::make_unique<StepLane>());
            rl->title = "Speed Lane (click/drag; magnetic 1/4, 1/2, 1, 2, 3x; right-click for presets & mode)"; rl->count = [this] { return p.pats.rateLen; };
            rl->get = [this] (int i) { return ratioToNorm (p.pats.rate[i].ratio); };
            rl->set = [this] (int i, float v) { br::Patterns q = p.pats; q.rate[i].ratio = normToRatio (v); p.applyPatterns (q); };
            rl->label = [this] (int i) { const auto& r = p.pats.rate[i]; return ratioLabel (r.ratio) + (r.stretch == 1 ? " R" : r.stretch == 2 ? " S" : ""); };
            rl->colour = [this] (int i) { return p.pats.rate[i].stretch == 2 ? col::purple : p.pats.rate[i].stretch == 1 ? col::fwd : col::accent; };
            rl->begin = [this] { p.undo.beginNewTransaction ("Speed lane"); };
            rl->highlight = [this] { return (int) (p.engine.tel.chunkIdx.load() % jmax (1, p.pats.rateLen)); };
            rl->rightClick = [this, rl, withPats] (int i)
            {
                PopupMenu m; const float qv2[5] = { 0.25f, 0.5f, 1, 2, 3 };
                for (int k = 0; k < 5; ++k) m.addItem (1 + k, ratioLabel (qv2[k]));
                m.addSeparator(); m.addItem (10, "Mode: follow global", true, p.pats.rate[i].stretch == 0); m.addItem (11, "Mode: Rate", true, p.pats.rate[i].stretch == 1); m.addItem (12, "Mode: Time-Stretch", true, p.pats.rate[i].stretch == 2);
                m.showMenuAsync (PopupMenu::Options().withTargetComponent (rl), [this, i, withPats] (int r)
                { if (r <= 0) return; withPats ([&] (br::Patterns& q) { const float qv3[5] = { 0.25f, 0.5f, 1, 2, 3 }; if (r <= 5) q.rate[i].ratio = qv3[r - 1]; else q.rate[i].stretch = (uint8_t) (r - 10); }); });
            };
            auto* pl = lane (pg, std::make_unique<StepLane>());
            pl->title = "Step probability (else 1x)"; pl->count = rl->count; pl->colour = [] (int) { return col::yellow; };
            pl->get = [this] (int i) { return p.pats.rate[i].prob; }; pl->set = [this] (int i, float v) { br::Patterns q = p.pats; q.rate[i].prob = v; p.applyPatterns (q); };
            pl->label = [this] (int i) { return String (roundToInt (p.pats.rate[i].prob * 100)) + "%"; }; pl->begin = rl->begin;
            auto* wl = lane (pg, std::make_unique<StepLane>());
            wl->title = "Weight (Weighted Random)"; wl->count = rl->count; wl->colour = [] (int) { return col::purple; };
            wl->get = [this] (int i) { return p.pats.rate[i].weight / 4.0f; }; wl->set = [this] (int i, float v) { br::Patterns q = p.pats; q.rate[i].weight = v * 4; p.applyPatterns (q); };
            wl->label = [this] (int i) { return String (p.pats.rate[i].weight, 2); }; wl->begin = rl->begin;
            auto* s = spin (pg, "Speed lane steps", 1, br::kLane, [this] { return p.pats.rateLen; }, [withPats] (int v) { withPats ([v] (br::Patterns& q) { q.rateLen = v; }); });
            auto* cp = pg->make (std::make_unique<CopyPaste> (p, "rate", "speed/stretch pattern"));
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); auto top = b.removeFromTop (196);
                tape->setBounds (top.removeFromRight (330)); top.removeFromRight (6); sp->setBounds (top);
                b.removeFromTop (6); auto hdr = b.removeFromTop (24); s->setBounds (hdr.removeFromLeft (220)); cp->setBounds (hdr.removeFromRight (160));
                rl->setBounds (b.removeFromTop (b.getHeight() / 2)); b.removeFromTop (4); pl->setBounds (b.removeFromLeft (b.getWidth() / 2 - 3)); b.removeFromLeft (6); wl->setBounds (b); };
            addTab ("Time", pg);
        }
        // ---- Gates
        {
            auto* pg = new Page();
            auto* gp = pg->panel ("Gate sequencer");
            gp->addAll (p, { brp::gateOn, brp::gateSteps, brp::gateTiming, brp::gateMs, brp::gateNote, brp::gateHz, brp::gateShape, brp::gateWidth, brp::gateDepth, brp::gapMode });
            gp->own (button ("All On", "Enable every gate cell", [this, withPats] { withPats ([] (br::Patterns& q) { for (auto& g : q.gate) g.on = 1; }); }), 80, 26);
            gp->own (button ("Alt FWD", "Alternate inherit / force-forward cells (forward slices in reversed chunks)", [this, withPats] { withPats ([] (br::Patterns& q) { for (int i = 0; i < br::kMaxSteps; ++i) q.gate[i].dir = (uint8_t) ((i & 1) ? 2 : 0); }); }), 80, 26);
            gp->own (button ("Reset", "Reset all cell properties", [this, withPats] { withPats ([] (br::Patterns& q) { for (auto& g : q.gate) g = br::GateStep(); }); }), 80, 26);
            gates = pg->make (std::make_unique<GateEditor> (p));
            auto* cv = pg->make (std::make_unique<CurveEditor> (p));
            auto* cp = pg->make (std::make_unique<CopyPaste> (p, "gate", "gate pattern"));
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); auto top = b.removeFromTop (196);
                cv->setBounds (top.removeFromRight (300)); top.removeFromRight (6); gp->setBounds (top);
                b.removeFromTop (6); cp->setBounds (b.removeFromTop (24).removeFromRight (160)); b.removeFromTop (4); gates->setBounds (b); };
            addTab ("Gates", pg);
        }
        // ---- Pan / phase
        {
            auto* pg = new Page();
            auto* pp = pg->panel ("Pan & stereo"); pp->addAll (p, { brp::panMode, brp::pan, brp::panDepth, brp::panSmooth, brp::swap, brp::panClock, brp::panClockMs, brp::panOffset, brp::panMirror });
            auto* ph = pg->panel ("Polarity & phase"); ph->addAll (p, { brp::polMode, brp::phaseMode, brp::phaseDeg });
            auto* pnl = lane (pg, std::make_unique<StepLane>());
            pnl->title = "Pan Lane (Pan Mode = Pan Lane)"; pnl->bipolar = true; pnl->count = [this] { return p.pats.panLen; };
            pnl->get = [this] (int i) { return (p.pats.pan[i] + 1) * 0.5f; }; pnl->set = [this] (int i, float v) { br::Patterns q = p.pats; q.pan[i] = v * 2 - 1; p.applyPatterns (q); };
            pnl->label = [this] (int i) { const float v = p.pats.pan[i]; return std::abs (v) < 0.02f ? String ("C") : (v < 0 ? "L" : "R") + String (roundToInt (std::abs (v) * 100)); };
            pnl->begin = [this] { p.undo.beginNewTransaction ("Pan lane"); };
            auto* pol = lane (pg, std::make_unique<StepLane>());
            pol->title = "Polarity Lane (Polarity = Polarity Lane): none / L / R / both"; pol->quantize = 4; pol->count = [this] { return p.pats.polLen; };
            pol->get = [this] (int i) { return p.pats.pol[i] / 3.0f; }; pol->set = [this] (int i, float v) { br::Patterns q = p.pats; q.pol[i] = (uint8_t) roundToInt (v * 3); p.applyPatterns (q); };
            pol->label = [this] (int i) { return StringArray ({ "-", "L", "R", "LR" })[p.pats.pol[i] & 3]; }; pol->colour = [] (int) { return col::rev; };
            pol->begin = [this] { p.undo.beginNewTransaction ("Polarity lane"); };
            auto* s1 = spin (pg, "Pan lane steps", 1, br::kLane, [this] { return p.pats.panLen; }, [withPats] (int v) { withPats ([v] (br::Patterns& q) { q.panLen = v; }); });
            auto* s2 = spin (pg, "Polarity lane steps", 1, br::kLane, [this] { return p.pats.polLen; }, [withPats] (int v) { withPats ([v] (br::Patterns& q) { q.polLen = v; }); });
            auto* cp = pg->make (std::make_unique<CopyPaste> (p, "panphase", "pan/phase pattern"));
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); auto top = b.removeFromTop (196);
                ph->setBounds (top.removeFromRight (360)); top.removeFromRight (6); pp->setBounds (top);
                b.removeFromTop (6); auto hdr = b.removeFromTop (24); cp->setBounds (hdr.removeFromRight (160)); s1->setBounds (hdr.removeFromLeft (220)); hdr.removeFromLeft (20); s2->setBounds (hdr.removeFromLeft (240));
                pnl->setBounds (b.removeFromTop (b.getHeight() / 2)); b.removeFromTop (6); pol->setBounds (b); };
            addTab ("Pan/Phase", pg);
        }
        // ---- FX rack
        {
            auto* pg = new Page();
            chain = pg->make (std::make_unique<ChainStrip> (p));
            auto* cm = pg->panel (""); cm->addAll (p, { brp::chainMix });
            auto* st = pg->panel ("Stutter"); st->addAll (p, { brp::stutOn, brp::stutScope, brp::stutDir, brp::stutRetrig, brp::stutSync, brp::stutPeriod, brp::stutLen, brp::stutRepeats, brp::stutDecay, brp::stutDrift, brp::stutProb, brp::stutWet, brp::stutDry });
            auto* dl = pg->panel ("Delay"); dl->addAll (p, { brp::dlyOn, brp::dlyScope, brp::dlySync, brp::dlyLink, brp::dlyPing, brp::dlyFreeze, brp::dlyTimeL, brp::dlyTimeR, brp::dlyFb, brp::dlyXfb, brp::dlyLP, brp::dlyHP, brp::dlyModRate, brp::dlyModDepth, brp::dlyProb, brp::dlyMix });
            auto* ec = pg->panel ("Echo"); ec->addAll (p, { brp::echoOn, brp::echoScope, brp::echoChar, brp::echoSync, brp::echoTime, brp::echoFb, brp::echoDecay, brp::echoTone, brp::echoSpread, brp::echoDrift, brp::echoWow, brp::echoProb, brp::echoMix });
            auto* cp = pg->make (std::make_unique<CopyPaste> (p, "fx", "FX chain"));
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); auto top = b.removeFromTop (70);
                cp->setBounds (top.removeFromRight (160).withSizeKeepingCentre (160, 26)); cm->setBounds (top.removeFromRight (96)); chain->setBounds (top);
                b.removeFromTop (6); const int w = (b.getWidth() - 12) / 3; st->setBounds (b.removeFromLeft (w)); b.removeFromLeft (6); dl->setBounds (b.removeFromLeft (w)); b.removeFromLeft (6); ec->setBounds (b); };
            addTab ("FX Rack", pg);
        }
        // ---- Scratch
        {
            auto* pg = new Page();
            platter = pg->make (std::make_unique<Platter> (p));
            auto* sc = pg->panel ("Scrub / scratch");
            sc->addAll (p, { brp::scrMode, brp::scrRelease, brp::scrInertia, brp::scrFriction, brp::scrAccel, brp::scrMaxRate, brp::scrMotor, brp::scrRamp, brp::scrPos, brp::scrRevLock, brp::scrHold, brp::scrActive });
            sc->own (button ("Free Scrub Mode", "Make the playhead the transport: motor plays backwards at Speed until you grab it.", [this] { p.set (brp::revMode, 4); }), 140, 26);
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); platter->setBounds (b.removeFromLeft (b.getHeight())); b.removeFromLeft (8); sc->setBounds (b); };
            addTab ("Scratch", pg);
        }
        // ---- Random
        {
            auto* pg = new Page();
            auto* rp = pg->panel ("Randomize (never destructive: always undoable)");
            rp->own (button ("Randomize", "Randomize the enabled domains using the seed (R)", [this] { p.randomize (false); }), 120, 30);
            rp->own (button ("Reroll", "Next seed, then randomize (Shift+R)", [this] { p.randomize (true); }), 100, 30);
            rp->own (button ("Undo", "Undo the last randomize", [this] { p.undo.undo(); }), 80, 30);
            auto amt = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::TextBoxRight); amt->setRange (0, 100, 1); amt->setValue ((double) p.ui (lk::randAmt, 0.6) * 100, dontSendNotification);
            amt->setTooltip ("Randomize amount: chance each enabled setting is changed."); amt->setTextValueSuffix ("% amount");
            amt->onValueChange = [this] { p.setUi (lk::randAmt, randAmt->getValue() * 0.01); };
            randAmt = rp->own (std::move (amt), 300, 26);
            auto lock = std::make_unique<ToggleButton> ("Lock seed"); lock->setTooltip ("When locked, Reroll keeps the current seed so results stay reproducible.");
            lock->setToggleState ((bool) p.ui (lk::randLock, false), dontSendNotification); lock->onClick = [this] { p.setUi (lk::randLock, randLock->getToggleState()); };
            randLock = rp->own (std::move (lock), 120, 26);
            rp->addAll (p, { brp::seed, brp::freeze });
            auto* dm = pg->panel ("Domains included in Randomize");
            const char* dn[] = { "Chunk order", "Chunk duration", "Speed / stretch", "Pan", "Phase / polarity", "Gates", "FX assignments" };
            for (int i = 0; i < 7; ++i)
            {
                auto* t = randDomains.add (new ToggleButton (dn[i])); t->setToggleState (((int) p.ui (lk::randEx, 0) & (1 << i)) == 0, dontSendNotification);
                t->setTooltip ("Include " + String (dn[i]) + " in Randomize");
                t->onClick = [this] { int ex = 0; for (int k = 0; k < randDomains.size(); ++k) if (! randDomains[k]->getToggleState()) ex |= 1 << k; p.setUi (lk::randEx, ex); };
                dm->addExternal (*t, 170, 26);
            }
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); rp->setBounds (b.removeFromTop (190)); b.removeFromTop (6); dm->setBounds (b.removeFromTop (110)); };
            addTab ("Random", pg);
        }
        // ---- Global
        {
            auto* pg = new Page();
            auto* gl = pg->panel ("Global"); gl->addAll (p, { brp::inGain, brp::outGain, brp::mix, brp::bpm, brp::maxBuffer, brp::dryAlign, brp::latPolicy, brp::bypass, brp::loop, brp::fileHostSync });
            auto* about = pg->make (std::make_unique<Label>());
            about->setJustificationType (Justification::topLeft); about->setColour (Label::textColourId, col::dim); about->setFont (FontOptions (13.0f));
            about->setText (String ("BackReverse ") + BR_VERSION_STRING + " beta - Circuit Drift Labs\n"
                            "Copyright (c) 2026 Sheldon Davidson. MIT licensed source. BackReverse(TM) and Circuit Drift Labs(TM) are unregistered trademarks.\n\n"
                            "Latency policy: Dynamic reports the current reverse delay and updates the host when it changes (at the next chunk boundary). "
                            "Fixed Maximum always reports Max Buffer so chunk sizes can change without host re-compensation.\n"
                            "Formats: VST3, CLAP and Standalone share one DSP engine and one parameter set. In the standalone app use Options (top-left) for audio device, input/output channels, sample rate and buffer size.", dontSendNotification);
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); gl->setBounds (b.removeFromTop (200)); b.removeFromTop (10); about->setBounds (b.reduced (6)); };
            addTab ("Global", pg);
        }
        // ---- Help
        {
            auto* pg = new Page();
            auto te = std::make_unique<TextEditor>(); te->setMultiLine (true); te->setReadOnly (true); te->setFont (FontOptions (15.0f)); te->setCaretVisible (false);
            helpText = pg->make (std::move (te));
            auto* list = pg->make (std::make_unique<Component>());
            OwnedArray<TextButton>* btns = new OwnedArray<TextButton>();
            for (int i = 0; i < (int) std::size (help::topics); ++i)
            {
                auto* b = btns->add (new TextButton (help::topics[i].title)); list->addAndMakeVisible (b);
                b->onClick = [this, i] { helpText->setText (String (help::topics[i].title) + "\n\n" + help::topics[i].body); };
            }
            helpText->setText (String (help::topics[0].title) + "\n\n" + help::topics[0].body);
            helpButtons.reset (btns);
            pg->onResize = [=] { auto b = pg->getLocalBounds().reduced (6); list->setBounds (b.removeFromLeft (220)); b.removeFromLeft (6); helpText->setBounds (b);
                auto lb = list->getLocalBounds(); for (auto* x : *btns) x->setBounds (lb.removeFromTop (34).reduced (0, 2)); };
            addTab ("Help", pg);
        }
    }
    std::unique_ptr<OwnedArray<TextButton>> helpButtons;

    void resized() override
    {
        auto b = getLocalBounds().reduced (8, 6);
        auto r1 = b.removeFromTop (30);
        title.setBounds (r1.removeFromLeft (150));
        presets.setBounds (r1.removeFromLeft (260).reduced (2)); prevP->setBounds (r1.removeFromLeft (28).reduced (2)); nextP->setBounds (r1.removeFromLeft (28).reduced (2)); saveP->setBounds (r1.removeFromLeft (56).reduced (2));
        r1.removeFromLeft (10); undoB->setBounds (r1.removeFromLeft (56).reduced (2)); redoB->setBounds (r1.removeFromLeft (56).reduced (2));
        r1.removeFromLeft (10); aB->setBounds (r1.removeFromLeft (32).reduced (2)); bB->setBounds (r1.removeFromLeft (32).reduced (2)); abCopy->setBounds (r1.removeFromLeft (48).reduced (2));
        helpB->setBounds (r1.removeFromRight (32).reduced (2)); meter.setBounds (r1.removeFromRight (150).reduced (4, 6)); bypass->setBounds (r1.removeFromRight (90).reduced (2));
        b.removeFromTop (4);
        auto r2 = b.removeFromTop (40);
        source->setBounds (r2.removeFromLeft (150)); r2.removeFromLeft (6);
        auto r2b = r2.withTrimmedTop (14);
        for (auto* c : { openB.get(), capB.get() }) c->setBounds (r2b.removeFromLeft (70).reduced (2, 0));
        r2b.removeFromLeft (6);
        for (auto* c : { playB.get(), pauseB.get(), stopB.get(), rtzB.get() }) c->setBounds (r2b.removeFromLeft (52).reduced (2, 0));
        loop->setBounds (r2b.removeFromLeft (70)); hostSync->setBounds (r2b.removeFromLeft (170)); restartB->setBounds (r2b.removeFromLeft (70).reduced (2, 0));
        recB->setBounds (r2b.removeFromRight (50).reduced (2, 0)); renderB->setBounds (r2b.removeFromRight (80).reduced (2, 0));
        latL.setBounds (r2.removeFromRight (r2.getWidth() - (r2b.getX() - r2.getX())).withTrimmedRight (136).withHeight (14)); timeL.setBounds (r2b.withTrimmedRight (4));
        b.removeFromTop (6);
        wave.setBounds (b.removeFromTop (176)); b.removeFromTop (6);
        tabs.setBounds (b);
    }
    void paint (Graphics& g) override { g.fillAll (col::bg); }

    void tick()
    {
        const auto& t = p.engine.tel; const auto P = p.snapshot();
        wave.repaint(); if (gates) gates->repaint(); if (platter) platter->tick(); if (chain) chain->repaint();
        for (auto* l : lanesToRepaint) l->repaint();
        for (auto* s : spins) s->refresh();
        meter.set (t.peakL.load(), t.peakR.load());
        undoB->setEnabled (p.undo.canUndo()); redoB->setEnabled (p.undo.canRedo());
        aB->setToggleState (p.abSlot == 0, dontSendNotification); bB->setToggleState (p.abSlot == 1, dontSendNotification);
        aB->setColour (TextButton::buttonColourId, p.abSlot == 0 ? col::accent.darker (0.3f) : col::panel2); bB->setColour (TextButton::buttonColourId, p.abSlot == 1 ? col::accent.darker (0.3f) : col::panel2);
        capB->setButtonText (p.capturing() ? "Stop Cap" : "Capture"); capB->setColour (TextButton::buttonColourId, p.capturing() ? col::red : col::panel2);
        recB->setColour (TextButton::buttonColourId, p.isRecording() ? col::red : col::panel2);
        const float rp = p.renderProgress.load(); renderB->setButtonText (rp >= 0 ? String (roundToInt (rp * 100)) + "%" : String ("Render..."));
        const bool file = P.source == br::Source::File;
        for (auto* c : { playB.get(), pauseB.get(), stopB.get(), rtzB.get(), renderB.get() }) c->setEnabled (file);
        playB->setColour (TextButton::buttonColourId, file && t.playing.load() ? col::green.darker (0.4f) : col::panel2);
        // chunk length readout in the chosen unit + latency
        const double sr = jmax (1.0, p.engine.sampleRate());
        const double cf = br::Engine::baseChunkFrames (P, sr, p.engine.tel.bpm.load(), 4.0);
        String chunk;
        switch ((int) p.get (brp::chunkUnit))
        {
            case 1: chunk = String (cf / sr, 3) + " s"; break;
            case 2: chunk = String ((int64) cf) + " smp"; break;
            case 3: chunk = String (cf / sr * t.bpm.load() / 60.0, 3) + " beats"; break;
            default: chunk = String (cf / sr * 1000.0, cf / sr < 1 ? 2 : 0) + " ms"; break;
        }
        if (P.revMode == br::RevMode::WholeSource && file) chunk = "whole file";
        const int lat = t.latency.load();
        latL.setText ("Chunk " + chunk + "  |  reverse delay " + (file ? String ("none (file)") : String (lat / sr * 1000.0, 1) + " ms") + "  |  host latency " + String (p.getLatencySamples()) + " smp ("
                      + (P.latPolicy == br::LatPolicy::Dynamic ? "Dynamic" : "Fixed Max") + ")" + (t.clamped.load() ? "  CLAMPED to live buffer" : ""), dontSendNotification);
        latL.setColour (Label::textColourId, t.clamped.load() ? col::red : col::text);
        String tl;
        if (p.isRecording()) tl << "REC " << p.recordingFile.getFileName() << "   ";
        if (p.capturing()) tl << "CAPTURING " << String (t.captureLen.load() / sr, 1) << " s   ";
        if (! p.isRecording() && ! p.capturing() && p.lastRender != File() && rp < 0) tl << "Rendered: " << p.lastRender.getFileName();
        timeL.setText (tl, dontSendNotification);
        refreshPresets();
        p.setUi (lk::uiTab, tabs.getCurrentTabIndex());
    }
};

// ================================================================ editor
BackReverseEditor::BackReverseEditor (BackReverseProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    content = std::make_unique<Content> (p);
    addAndMakeVisible (*content);
    content->setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (W * 6 / 10, H * 6 / 10, W * 2, H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setWantsKeyboardFocus (true);
    addMouseListener (this, true);
    setSize (W, H);
    startTimerHz (30);
}

BackReverseEditor::~BackReverseEditor() { stopTimer(); removeMouseListener (this); content.reset(); setLookAndFeel (nullptr); }
void BackReverseEditor::paint (Graphics& g) { g.fillAll (col::bg); }
void BackReverseEditor::resized() { content->setTransform (AffineTransform::scale ((float) getWidth() / W)); }
void BackReverseEditor::timerCallback() { content->tick(); }
int BackReverseEditor::numTabs() const { return content->tabs.getNumTabs(); }
void BackReverseEditor::selectTab (int i) { content->tabs.setCurrentTabIndex (i); content->tick(); }
void BackReverseEditor::mouseDown (const MouseEvent&) { proc.undo.beginNewTransaction(); }
void BackReverseEditor::filesDropped (const StringArray& files, int, int) { if (! files.isEmpty()) content->load (File (files[0])); }

bool BackReverseEditor::keyPressed (const KeyPress& k)
{
    const auto m = k.getModifiers(); const int c = k.getKeyCode();
    if (m.isCommandDown() && (c == 'Z' || c == 'z')) { if (m.isShiftDown()) proc.undo.redo(); else proc.undo.undo(); return true; }
    if (m.isCommandDown() && (c == 'Y' || c == 'y')) { proc.undo.redo(); return true; }
    if (m.isCommandDown() && (c == 'O' || c == 'o')) { content->openFile(); return true; }
    if (m.isCommandDown() && (c == 'S' || c == 's')) { content->savePreset(); return true; }
    if (k == KeyPress::F1Key) { content->tabs.setCurrentTabIndex (content->tabs.getNumTabs() - 1); return true; }
    if (m.isAnyModifierKeyDown() && ! m.isShiftDown()) return false;
    if (k == KeyPress::spaceKey) { proc.engine.ctl.transport.store (proc.engine.isFilePlaying() ? 2 : 1); return true; }
    if (k == KeyPress::homeKey) { proc.engine.ctl.transport.store (4); return true; }
    if (c == 'L' || c == 'l') { proc.set (brp::loop, proc.get (brp::loop) > 0.5f ? 0.0f : 1.0f); return true; }
    if (c == 'B' || c == 'b') { proc.set (brp::bypass, proc.get (brp::bypass) > 0.5f ? 0.0f : 1.0f); return true; }
    if (c == 'R' || c == 'r') { proc.randomize (m.isShiftDown()); return true; }
    const float q[5] = { 0.25f, 0.5f, 1, 2, 3 };
    if (c >= '1' && c <= '5') { proc.set (brp::ratio, q[c - '1']); return true; }
    return false;
}

juce::AudioProcessorEditor* BackReverseProcessor::createEditor() { return new BackReverseEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BackReverseProcessor(); }
