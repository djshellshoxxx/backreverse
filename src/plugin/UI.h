// BackReverse editor widgets.
#pragma once
#include "PluginProcessor.h"

namespace ui {
using namespace juce;

namespace col {
const Colour bg { 0xff14161b }, panel { 0xff1d2028 }, panel2 { 0xff252935 }, line { 0xff343a48 }, text { 0xffe6eaf2 }, dim { 0xff8a93a3 },
             accent { 0xff4fd1c5 }, rev { 0xfff6a04d }, fwd { 0xff63b3ed }, red { 0xfff56565 }, green { 0xff68d391 }, purple { 0xffb794f4 }, yellow { 0xfff6e05e };
}

class Look : public LookAndFeel_V4
{
public:
    Look()
    {
        setColourScheme ({ col::bg, col::panel, col::panel2, col::line, col::text, col::panel2, col::text, col::accent, col::text });
        setColour (Slider::rotarySliderFillColourId, col::accent); setColour (Slider::thumbColourId, col::accent);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack); setColour (ComboBox::backgroundColourId, col::panel2);
        setColour (TextButton::buttonColourId, col::panel2); setColour (TextButton::buttonOnColourId, col::accent.darker (0.3f));
        setColour (ToggleButton::tickColourId, col::accent); setColour (TooltipWindow::backgroundColourId, Colour (0xf0101216));
        setColour (TabbedButtonBar::tabOutlineColourId, col::line); setColour (TabbedComponent::outlineColourId, col::line);
        setColour (TextEditor::backgroundColourId, col::panel2); setColour (TextEditor::outlineColourId, col::line);
        setColour (PopupMenu::backgroundColourId, col::panel); setColour (PopupMenu::highlightedBackgroundColourId, col::accent.darker (0.4f));
    }
    // Tabs: flat, with a teal underline on the active page and a subtle hover lift.
    void drawTabButton (TabBarButton& b, Graphics& g, bool isMouseOver, bool) override
    {
        const bool on = b.isFrontTab();
        auto r = b.getLocalBounds().toFloat().reduced (3.0f, 2.0f);
        auto underline = r.removeFromBottom (3.0f).reduced (8.0f, 0.0f);
        g.setColour (on ? col::panel2 : (isMouseOver ? col::panel : col::bg));
        g.fillRoundedRectangle (r, 6.0f);
        if (on) { g.setColour (col::accent); g.fillRoundedRectangle (underline, 1.5f); }
        g.setColour (on ? col::text : col::dim);
        g.setFont (FontOptions (13.0f, on ? Font::bold : Font::plain));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), Justification::centred, 1);
    }
    void drawTabbedButtonBarBackground (TabbedButtonBar& bar, Graphics& g) override
    {
        g.fillAll (col::bg);
        g.setColour (col::line); g.fillRect (0, bar.getHeight() - 1, bar.getWidth(), 1);
    }
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f; const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        Path bgArc; bgArc.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (bgArc, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        Path fg; fg.addCentredArc (c.x, c.y, r, r, 0, a0, ang, true);
        g.setColour (s.isEnabled() ? col::accent : col::dim); g.strokePath (fg, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col::text); g.drawLine (Line<float> (c, c.getPointOnCircumference (r * 0.75f, ang)), 2.0f);
        if (s.hasKeyboardFocus (true)) { g.setColour (col::yellow); g.drawEllipse (b.withSizeKeepingCentre (r * 2 + 4, r * 2 + 4), 1.0f); }
    }
};

inline String tooltipFor (int idx)
{
    const auto& d = brp::kDefs[idx];
    String t (d.tip);
    if (d.kind == brp::F || d.kind == brp::I) t << "\nRange: " << String (d.min, d.kind == brp::I ? 0 : 2) << " to " << String (d.max, d.kind == brp::I ? 0 : 2) << " " << d.unit;
    t << "\nHost automatable.";
    if (t.contains ("Adds latency")) t << " Changing it changes the reverse delay.";
    return t;
}

// ---------------------------------------------------------------- parameter widgets
struct Knob : Component
{
    Slider s; Label l; std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> a;
    Knob (BackReverseProcessor& p, int idx)
    {
        const auto& d = brp::kDefs[idx];
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
        s.setTextBoxIsEditable (true); s.setTooltip (tooltipFor (idx)); s.setTitle (d.name); s.setDescription (d.tip);
        s.setColour (Slider::textBoxTextColourId, col::text);
        l.setText (d.name, dontSendNotification); l.setJustificationType (Justification::centred); l.setFont (FontOptions (11.5f)); l.setColour (Label::textColourId, col::dim);
        l.setMinimumHorizontalScale (0.72f); // long names scale down instead of truncating ("Reverse Probability")
        l.setTooltip (s.getTooltip()); l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (s); addAndMakeVisible (l);
        a = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, d.id, s);
        s.setTextValueSuffix (String (d.unit).isNotEmpty() ? " " + String (d.unit) : String());
    }
    void resized() override { auto b = getLocalBounds(); l.setBounds (b.removeFromTop (14)); s.setBounds (b); }
};

struct Choice : Component
{
    ComboBox c; Label l; std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> a;
    Choice (BackReverseProcessor& p, int idx)
    {
        const auto& d = brp::kDefs[idx];
        c.addItemList (StringArray::fromTokens (d.choices, "|", ""), 1); c.setTooltip (tooltipFor (idx)); c.setTitle (d.name);
        l.setText (d.name, dontSendNotification); l.setFont (FontOptions (11.5f)); l.setColour (Label::textColourId, col::dim); l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (c); addAndMakeVisible (l);
        a = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, d.id, c);
    }
    void resized() override { auto b = getLocalBounds(); l.setBounds (b.removeFromTop (14)); c.setBounds (b.removeFromTop (24)); }
};

struct Toggle : Component
{
    ToggleButton b; std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> a;
    Toggle (BackReverseProcessor& p, int idx)
    {
        const auto& d = brp::kDefs[idx];
        b.setButtonText (d.name); b.setTooltip (tooltipFor (idx)); b.setTitle (d.name);
        addAndMakeVisible (b);
        a = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, d.id, b);
    }
    void resized() override { b.setBounds (getLocalBounds()); }
};

// Titled panel with a flow layout of controls.
struct Panel : Component
{
    String title; std::vector<std::unique_ptr<Component>> owned; std::vector<std::pair<Component*, Point<int>>> items;
    explicit Panel (String t = {}) : title (std::move (t)) {}
    Component* add (BackReverseProcessor& p, int idx)
    {
        const auto k = brp::kDefs[idx].kind;
        std::unique_ptr<Component> c;
        Point<int> sz;
        if (k == brp::C) { c = std::make_unique<Choice> (p, idx); sz = { 136, 40 }; }
        else if (k == brp::B) { c = std::make_unique<Toggle> (p, idx); sz = { 132, 24 }; }
        else { c = std::make_unique<Knob> (p, idx); sz = { 72, 78 }; }
        auto* raw = c.get(); addAndMakeVisible (*raw); items.push_back ({ raw, sz }); owned.push_back (std::move (c));
        return raw;
    }
    void addAll (BackReverseProcessor& p, std::initializer_list<int> ids) { for (int i : ids) add (p, i); }
    template <class C> C* own (std::unique_ptr<C> c, int w, int h) { auto* r = c.get(); addAndMakeVisible (*r); items.push_back ({ r, { w, h } }); owned.push_back (std::move (c)); return r; }
    void addExternal (Component& c, int w, int h) { addAndMakeVisible (c); items.push_back ({ &c, { w, h } }); }
    void paint (Graphics& g) override
    {
        g.setColour (col::panel); g.fillRoundedRectangle (getLocalBounds().toFloat(), 6);
        g.setColour (col::line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6, 1);
        if (title.isNotEmpty()) { g.setColour (col::accent); g.setFont (FontOptions (13.0f, Font::bold)); g.drawText (title, 10, 4, getWidth() - 20, 16, Justification::centredLeft); }
    }
    void resized() override
    {
        int x = 8, y = title.isNotEmpty() ? 24 : 8, rowH = 0;
        for (auto& [c, s] : items)
        {
            if (x + s.x > getWidth() - 6 && x > 8) { x = 8; y += rowH + 6; rowH = 0; }
            const int dy = (s.y < 40 && rowH > 40) ? (rowH - s.y) / 2 : 0;
            c->setBounds (x, y + dy, s.x, s.y); x += s.x + 6; rowH = jmax (rowH, s.y);
        }
    }
};

inline std::unique_ptr<TextButton> button (const String& text, const String& tip, std::function<void()> fn)
{
    auto b = std::make_unique<TextButton> (text); b->setTooltip (tip); b->onClick = std::move (fn); return b;
}

// ---------------------------------------------------------------- generic step lane
class StepLane : public Component
{
public:
    String title;
    std::function<int()> count = [] { return 8; };
    std::function<float (int)> get = [] (int) { return 0.0f; };       // 0..1
    std::function<void (int, float)> set = [] (int, float) {};
    std::function<String (int)> label;
    std::function<Colour (int)> colour;
    std::function<void (int)> rightClick;
    std::function<void()> begin;
    std::function<int()> highlight = [] { return -1; };
    bool toggles = false, bipolar = false; int quantize = 0; int focusStep = 0; int titleWidth = 0;

    StepLane() { setWantsKeyboardFocus (true); }
    Rectangle<int> area() const { return titleWidth > 0 ? getLocalBounds().withTrimmedLeft (titleWidth) : getLocalBounds().withTrimmedTop (title.isNotEmpty() ? 16 : 0); }
    int stepAt (int x) const { const int n = jmax (1, count()); return jlimit (0, n - 1, (x - area().getX()) * n / jmax (1, area().getWidth())); }
    void paint (Graphics& g) override
    {
        if (title.isNotEmpty()) { g.setColour (col::dim); g.setFont (FontOptions (11.5f)); if (titleWidth > 0) g.drawFittedText (title, 0, 0, titleWidth - 6, getHeight(), Justification::centredLeft, 2); else g.drawText (title, 0, 0, getWidth(), 14, Justification::centredLeft); }
        auto a = area().toFloat(); const int n = jmax (1, count()); const float w = a.getWidth() / n;
        g.setColour (col::panel2); g.fillRoundedRectangle (a, 4);
        const int hl = highlight();
        for (int i = 0; i < n; ++i)
        {
            auto cell = Rectangle<float> (a.getX() + i * w, a.getY(), w, a.getHeight()).reduced (1.5f);
            const float v = jlimit (0.0f, 1.0f, get (i));
            const Colour c = colour ? colour (i) : col::accent;
            if (i == hl) { g.setColour (col::yellow.withAlpha (0.18f)); g.fillRect (cell); }
            if (toggles) { g.setColour (v > 0.5f ? c : col::line); g.fillRoundedRectangle (cell.reduced (2), 3); }
            else if (bipolar)
            {
                const float mid = cell.getCentreY(), y = cell.getBottom() - v * cell.getHeight();
                g.setColour (c); g.fillRect (Rectangle<float>::leftTopRightBottom (cell.getX() + 2, jmin (mid, y), cell.getRight() - 2, jmax (mid, y)));
                g.setColour (col::line); g.drawHorizontalLine ((int) mid, cell.getX(), cell.getRight());
            }
            else { g.setColour (c.withAlpha (0.85f)); g.fillRect (cell.withTop (cell.getBottom() - v * cell.getHeight()).reduced (2, 0)); }
            if (label) { g.setColour (col::text); g.setFont (FontOptions (jmin (12.0f, w * 0.38f))); g.drawFittedText (label (i), cell.toNearestInt(), Justification::centredTop, 1); }
            if (hasKeyboardFocus (false) && i == focusStep) { g.setColour (col::yellow); g.drawRect (cell, 1.0f); }
        }
    }
    float valueAt (int y) const { auto a = area(); return jlimit (0.0f, 1.0f, 1.0f - (float) (y - a.getY()) / jmax (1, a.getHeight())); }
    float paintValue = 0;
    void apply (int step, float v) { if (quantize > 1) v = std::round (v * (quantize - 1)) / (float) (quantize - 1); set (step, v); repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const int s = stepAt (e.x); focusStep = s;
        if (e.mods.isPopupMenu()) { if (rightClick) rightClick (s); return; }
        if (begin) begin();
        paintValue = toggles ? (get (s) > 0.5f ? 0.0f : 1.0f) : valueAt (e.y);
        apply (s, paintValue);
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const int s = stepAt (e.x); focusStep = s;
        apply (s, toggles ? paintValue : valueAt (e.y));
    }
    bool keyPressed (const KeyPress& k) override
    {
        const int n = jmax (1, count());
        if (k == KeyPress::leftKey) { focusStep = (focusStep + n - 1) % n; repaint(); return true; }
        if (k == KeyPress::rightKey) { focusStep = (focusStep + 1) % n; repaint(); return true; }
        const float step = quantize > 1 ? 1.0f / (quantize - 1) : 0.05f;
        if (begin && (k == KeyPress::upKey || k == KeyPress::downKey || k == KeyPress::spaceKey)) begin();
        if (k == KeyPress::upKey) { apply (focusStep, jmin (1.0f, get (focusStep) + step)); return true; }
        if (k == KeyPress::downKey) { apply (focusStep, jmax (0.0f, get (focusStep) - step)); return true; }
        if (k == KeyPress::spaceKey && toggles) { apply (focusStep, get (focusStep) > 0.5f ? 0.0f : 1.0f); return true; }
        if ((k.isKeyCode (KeyPress::returnKey) || (k.getModifiers().isShiftDown() && k.isKeyCode (KeyPress::F10Key))) && rightClick) { rightClick (focusStep); return true; }
        return false;
    }
};

inline String ratioLabel (float r)
{
    if (std::abs (r - 0.25f) < 1e-3f) return "1/4x";
    if (std::abs (r - 0.5f) < 1e-3f) return "1/2x";
    if (std::abs (r - 1.0f / 3.0f) < 1e-3f) return "1/3x";
    return String (r, r < 1 ? 2 : (std::abs (r - std::round (r)) < 1e-3f ? 0 : 2)) + "x";
}
inline float ratioToNorm (float r) { return (std::log (jlimit (0.05f, 8.0f, r)) - std::log (0.05f)) / (std::log (8.0f) - std::log (0.05f)); }
inline float normToRatio (float v)
{
    float r = std::exp (std::log (0.05f) + v * (std::log (8.0f) - std::log (0.05f)));
    for (float s : { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f }) if (std::abs (std::log (r / s)) < 0.06f) return s; // magnetic quick values
    return r;
}

// ---------------------------------------------------------------- gate editor
class GateEditor : public Component, public SettableTooltipClient
{
public:
    explicit GateEditor (BackReverseProcessor& pr) : p (pr) { setWantsKeyboardFocus (true); setTitle ("Gate cells"); setTooltip ("Click toggles a gate, drag paints. Right-click (or Enter) opens properties: direction, shape, width, depth, fades, FX. Arrows move, Space toggles."); }
    int steps() const { return 2 << (int) p.get (brp::gateSteps); }
    int cellAt (int x) const { return jlimit (0, steps() - 1, x * steps() / jmax (1, getWidth())); }
    void paint (Graphics& g) override
    {
        const int n = steps(); const float w = (float) getWidth() / n, h = (float) getHeight();
        const auto& pt = p.pats; const auto P = p.snapshot();
        g.setColour (col::panel2); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4);
        const int playing = p.engine.tel.gateCell.load();
        for (int i = 0; i < n; ++i)
        {
            const auto& st = pt.gate[i];
            auto cell = Rectangle<float> (i * w, 0, w, h).reduced (1.5f, 2);
            if (i == playing && P.gateOn) { g.setColour (col::yellow.withAlpha (0.2f)); g.fillRect (cell); }
            const float width = jlimit (0.01f, 1.0f, st.width > 0 ? st.width : P.gateWidth);
            const float depth = st.depth >= 0 ? st.depth : P.gateDepth;
            const br::Shape sh = st.shape >= 0 ? (br::Shape) st.shape : P.gateShape;
            auto body = cell.withTrimmedTop (16).withTrimmedBottom (16);
            const Colour c = st.dir == 2 ? col::fwd : st.dir == 1 ? col::rev : col::accent;
            if (st.on)
            {
                Path env; const float aw = body.getWidth() * width;
                env.startNewSubPath (body.getX(), body.getBottom());
                for (int k = 0; k <= 24; ++k)
                {
                    const float x = (float) k / 24.0f;
                    float e = br::Engine::shapeEnv (sh, x, pt);
                    if (st.fadeIn > 0) e *= jmin (1.0f, x / st.fadeIn);
                    if (st.fadeOut > 0) e *= jmin (1.0f, (1 - x) / st.fadeOut);
                    e = 1.0f - depth * (1.0f - e);
                    env.lineTo (body.getX() + x * aw, body.getBottom() - e * body.getHeight());
                }
                env.lineTo (body.getX() + aw, body.getBottom()); env.closeSubPath();
                g.setColour (c.withAlpha (0.75f)); g.fillPath (env);
                g.setColour (col::bg.withAlpha (0.5f)); g.fillRect (body.withTrimmedLeft (aw)); // gap
            }
            else { g.setColour (col::line); g.drawLine (body.getX(), body.getBottom(), body.getRight(), body.getY(), 1.0f); }
            g.setColour (col::text); g.setFont (FontOptions (11.0f));
            g.drawText (st.dir == 1 ? "REV" : st.dir == 2 ? "FWD" : (st.on ? "on" : "off"), cell.withHeight (14).toNearestInt(), Justification::centred);
            String fx; if (st.fx & 1) fx << "S"; if (st.fx & 2) fx << "D"; if (st.fx & 4) fx << "E";
            if (st.shape >= 0) fx << (fx.isEmpty() ? "" : " ") << "~";
            g.setColour (col::purple); g.drawText (fx, cell.withTop (cell.getBottom() - 14).toNearestInt(), Justification::centred);
            if (hasKeyboardFocus (false) && i == focus) { g.setColour (col::yellow); g.drawRect (cell, 1.5f); }
        }
    }
    void edit (int i, std::function<void (br::GateStep&)> fn) { br::Patterns q = p.pats; fn (q.gate[i]); p.applyPatterns (q); repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const int i = cellAt (e.x); focus = i;
        if (e.mods.isPopupMenu()) { props (i); return; }
        p.undo.beginNewTransaction ("Gate edit");
        paintOn = p.pats.gate[i].on ? 0 : 1; edit (i, [&] (br::GateStep& s) { s.on = (uint8_t) paintOn; });
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const int i = cellAt (e.x);
        if (p.pats.gate[i].on != paintOn) edit (i, [&] (br::GateStep& s) { s.on = (uint8_t) paintOn; });
    }
    bool keyPressed (const KeyPress& k) override
    {
        const int n = steps();
        if (k == KeyPress::leftKey) { focus = (focus + n - 1) % n; repaint(); return true; }
        if (k == KeyPress::rightKey) { focus = (focus + 1) % n; repaint(); return true; }
        if (k == KeyPress::spaceKey) { p.undo.beginNewTransaction ("Gate edit"); edit (focus, [] (br::GateStep& s) { s.on = (uint8_t) ! s.on; }); return true; }
        if (k == KeyPress::returnKey) { props (focus); return true; }
        return false;
    }
    void props (int i)
    {
        const auto st = p.pats.gate[i];
        PopupMenu m, dir, shp, wid, dep, fin, fout;
        m.addSectionHeader ("Gate " + String (i + 1));
        m.addItem (1, "Enabled", true, st.on);
        dir.addItem (10, "Inherit chunk", true, st.dir == 0); dir.addItem (11, "Force reverse", true, st.dir == 1); dir.addItem (12, "Force forward", true, st.dir == 2);
        m.addSubMenu ("Direction", dir);
        shp.addItem (20, "Inherit global", true, st.shape < 0);
        auto names = StringArray::fromTokens (brp::kDefs[brp::gateShape].choices, "|", "");
        for (int s = 0; s < names.size(); ++s) shp.addItem (21 + s, names[s], true, st.shape == s);
        m.addSubMenu ("Shape", shp);
        wid.addItem (40, "Inherit global", true, st.width <= 0);
        for (int v = 1; v <= 10; ++v) wid.addItem (40 + v, String (v * 10) + "%", true, std::abs (st.width - v * 0.1f) < 1e-3f);
        m.addSubMenu ("Width (sustain)", wid);
        dep.addItem (60, "Inherit global", true, st.depth < 0);
        for (int v = 0; v <= 10; ++v) dep.addItem (61 + v, String (v * 10) + "%", true, std::abs (st.depth - v * 0.1f) < 1e-3f);
        m.addSubMenu ("Depth", dep);
        for (int v = 0; v <= 5; ++v) { fin.addItem (80 + v, String (v * 10) + "%", true, std::abs (st.fadeIn - v * 0.1f) < 1e-3f); fout.addItem (90 + v, String (v * 10) + "%", true, std::abs (st.fadeOut - v * 0.1f) < 1e-3f); }
        m.addSubMenu ("Attack / entry fade", fin); m.addSubMenu ("Release / exit fade", fout);
        m.addSeparator();
        m.addItem (100, "Stutter on this gate", true, st.fx & 1); m.addItem (101, "Delay on this gate", true, st.fx & 2); m.addItem (102, "Echo on this gate", true, st.fx & 4);
        m.addSeparator(); m.addItem (110, "Apply to all gates");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, i] (int r)
        {
            if (r <= 0) return;
            p.undo.beginNewTransaction ("Gate properties");
            if (r == 110) { br::Patterns q = p.pats; for (auto& g : q.gate) g = q.gate[i]; p.applyPatterns (q); repaint(); return; }
            edit (i, [r] (br::GateStep& s)
            {
                if (r == 1) s.on = (uint8_t) ! s.on;
                else if (r >= 10 && r <= 12) s.dir = (uint8_t) (r - 10);
                else if (r >= 20 && r < 40) s.shape = (int8_t) (r - 21);
                else if (r >= 40 && r < 60) s.width = r == 40 ? -1.0f : (r - 40) * 0.1f;
                else if (r >= 60 && r < 80) s.depth = r == 60 ? -1.0f : (r - 61) * 0.1f;
                else if (r >= 80 && r < 90) s.fadeIn = (r - 80) * 0.1f;
                else if (r >= 90 && r < 100) s.fadeOut = (r - 90) * 0.1f;
                else if (r >= 100 && r <= 102) s.fx = (uint8_t) (s.fx ^ (1 << (r - 100)));
            });
        });
    }
private:
    BackReverseProcessor& p; int paintOn = 1, focus = 0;
};

// ---------------------------------------------------------------- custom gate curve editor (draggable points)
class CurveEditor : public Component, public SettableTooltipClient
{
public:
    explicit CurveEditor (BackReverseProcessor& pr) : p (pr) { setTooltip ("Custom gate curve: drag points, double-click to add, right-click to delete. Used when Gate Shape = Custom Curve."); }
    Point<float> toPx (float x, float y) const { auto b = getLocalBounds().toFloat().reduced (8); return { b.getX() + x * b.getWidth(), b.getBottom() - y * b.getHeight() }; }
    Point<float> fromPx (Point<float> q) const { auto b = getLocalBounds().toFloat().reduced (8); return { jlimit (0.0f, 1.0f, (q.x - b.getX()) / b.getWidth()), jlimit (0.0f, 1.0f, (b.getBottom() - q.y) / b.getHeight()) }; }
    void paint (Graphics& g) override
    {
        const auto& q = p.pats;
        g.setColour (col::panel2); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4);
        const auto sh = (br::Shape) (int) p.get (brp::gateShape);
        Path cur, pre;
        for (int k = 0; k <= 64; ++k)
        {
            const float x = k / 64.0f;
            auto a = toPx (x, br::Engine::shapeEnv (br::Shape::Custom, x, q)), b = toPx (x, br::Engine::shapeEnv (sh, x, q));
            if (k == 0) { cur.startNewSubPath (a); pre.startNewSubPath (b); } else { cur.lineTo (a); pre.lineTo (b); }
        }
        g.setColour (col::dim.withAlpha (0.5f)); g.strokePath (pre, PathStrokeType (1.0f));
        g.setColour (col::purple); g.strokePath (cur, PathStrokeType (2.0f));
        for (int i = 0; i < q.curveCount; ++i) { auto c = toPx (q.curveX[i], q.curveY[i]); g.setColour (i == drag ? col::yellow : col::text); g.fillEllipse (c.x - 4, c.y - 4, 8, 8); }
        g.setColour (col::dim); g.setFont (FontOptions (11.0f)); g.drawText ("Custom curve (grey = active shape)", 10, getHeight() - 16, getWidth() - 20, 14, Justification::centredLeft);
    }
    int nearest (Point<float> m) const { const auto& q = p.pats; int best = -1; float bd = 12; for (int i = 0; i < q.curveCount; ++i) { const float d = toPx (q.curveX[i], q.curveY[i]).getDistanceFrom (m); if (d < bd) { bd = d; best = i; } } return best; }
    void commit (br::Patterns q)
    {
        std::vector<std::pair<float, float>> v; for (int i = 0; i < q.curveCount; ++i) v.push_back ({ q.curveX[i], q.curveY[i] });
        std::sort (v.begin(), v.end());
        for (int i = 0; i < q.curveCount; ++i) { q.curveX[i] = v[(size_t) i].first; q.curveY[i] = v[(size_t) i].second; }
        p.applyPatterns (q); repaint();
    }
    void mouseDown (const MouseEvent& e) override
    {
        p.undo.beginNewTransaction ("Curve edit");
        drag = nearest (e.position);
        if (e.mods.isPopupMenu() && drag >= 0 && p.pats.curveCount > 2)
        {
            br::Patterns q = p.pats;
            for (int i = drag; i < q.curveCount - 1; ++i) { q.curveX[i] = q.curveX[i + 1]; q.curveY[i] = q.curveY[i + 1]; }
            --q.curveCount; drag = -1; commit (q);
        }
    }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        br::Patterns q = p.pats;
        if (q.curveCount >= br::kCurvePts) return;
        auto v = fromPx (e.position); q.curveX[q.curveCount] = v.x; q.curveY[q.curveCount] = v.y; ++q.curveCount; commit (q);
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (drag < 0) return;
        br::Patterns q = p.pats; auto v = fromPx (e.position);
        q.curveX[drag] = v.x; q.curveY[drag] = v.y;
        if (drag == 0) q.curveX[0] = 0; // endpoints pinned in time
        commit (q);
        for (int i = 0; i < q.curveCount; ++i) if (std::abs (p.pats.curveX[i] - v.x) < 1e-6f && std::abs (p.pats.curveY[i] - v.y) < 1e-6f) drag = i;
    }
    void mouseUp (const MouseEvent&) override { drag = -1; repaint(); }
private:
    BackReverseProcessor& p; int drag = -1;
};

// ---------------------------------------------------------------- FX chain strip (drag to reorder)
class ChainStrip : public Component, public SettableTooltipClient
{
public:
    explicit ChainStrip (BackReverseProcessor& pr) : p (pr) { setTooltip ("Drag blocks to reorder the effect chain; click a block to enable/disable it."); setWantsKeyboardFocus (true); }
    static constexpr int perm[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
    void paint (Graphics& g) override
    {
        const int o = (int) p.get (brp::chainOrder); const char* names[3] = { "STUTTER", "DELAY", "ECHO" }; const int onIds[3] = { brp::stutOn, brp::dlyOn, brp::echoOn };
        const int scopes[3] = { brp::stutScope, brp::dlyScope, brp::echoScope };
        const float w = getWidth() / 3.0f;
        for (int k = 0; k < 3; ++k)
        {
            const int fx = perm[o][k];
            auto r = Rectangle<float> (k * w, 0, w, (float) getHeight()).reduced (8, 4);
            if (fx == dragging) r = r.translated ((float) dragOffset, 0);
            const bool on = p.get (onIds[fx]) > 0.5f;
            g.setColour (on ? col::accent.withAlpha (0.8f) : col::panel2); g.fillRoundedRectangle (r, 6);
            g.setColour (col::line); g.drawRoundedRectangle (r, 6, 1);
            g.setColour (on ? col::bg : col::dim); g.setFont (FontOptions (14.0f, Font::bold));
            g.drawText (String (names[fx]) + (on ? "" : " (off)"), r.withTrimmedBottom (r.getHeight() * 0.4f).toNearestInt(), Justification::centred);
            g.setFont (FontOptions (11.0f));
            g.drawText (StringArray::fromTokens (BR_SCOPES, "|", "")[(int) p.get (scopes[fx])], r.withTrimmedTop (r.getHeight() * 0.55f).toNearestInt(), Justification::centred);
            if (k < 2) { g.setColour (col::text); g.drawArrow (Line<float> (r.getRight() + 1, r.getCentreY(), r.getRight() + 14, r.getCentreY()), 2, 8, 8); }
        }
    }
    int slotAt (int x) const { return jlimit (0, 2, x * 3 / jmax (1, getWidth())); }
    void mouseDown (const MouseEvent& e) override { p.undo.beginNewTransaction ("Chain"); dragging = perm[(int) p.get (brp::chainOrder)][slotAt (e.x)]; dragOffset = 0; }
    void mouseDrag (const MouseEvent& e) override { dragOffset = e.getDistanceFromDragStartX(); repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        const int o = (int) p.get (brp::chainOrder);
        if (std::abs (dragOffset) < 6)
        {
            const int onIds[3] = { brp::stutOn, brp::dlyOn, brp::echoOn };
            const int id = onIds[dragging]; p.set (id, p.get (id) > 0.5f ? 0.0f : 1.0f);
        }
        else
        {
            std::vector<int> cur { perm[o][0], perm[o][1], perm[o][2] };
            cur.erase (std::find (cur.begin(), cur.end(), dragging));
            cur.insert (cur.begin() + slotAt (e.x), dragging);
            for (int i = 0; i < 6; ++i) if (perm[i][0] == cur[0] && perm[i][1] == cur[1]) p.set (brp::chainOrder, (float) i);
        }
        dragging = -1; dragOffset = 0; repaint();
    }
    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::rightKey || k == KeyPress::leftKey) { p.set (brp::chainOrder, (float) (((int) p.get (brp::chainOrder) + (k == KeyPress::rightKey ? 1 : 5)) % 6)); repaint(); return true; }
        return false;
    }
private:
    BackReverseProcessor& p; int dragging = -1, dragOffset = 0;
};

// ---------------------------------------------------------------- turntable platter (fun feature)
class Platter : public Component, public SettableTooltipClient
{
public:
    explicit Platter (BackReverseProcessor& pr) : p (pr) { setTooltip ("Grab the record to scratch. One turn = 1.8 s of audio (33 1/3 rpm). In Tape Shuttle mode drag left/right from the centre to shuttle."); }
    void tick()
    {
        const auto& t = p.engine.tel;
        const double v = t.scrubbing.load() ? t.scrubVel.load() : (t.playing.load() ? (t.reversed.load() ? -1.0 : 1.0) * t.speed.load() : 0.0);
        speed = (float) v;
        if (! held) angle += (float) (v * MathConstants<double>::twoPi / (1.8 * 30.0));
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (10); const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f; const auto c = b.getCentre();
        g.setColour (Colour (0xff0b0c0f)); g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        for (float k = 0.95f; k > 0.4f; k -= 0.06f) { g.setColour (Colour (0xff22252d)); g.drawEllipse (c.x - r * k, c.y - r * k, 2 * r * k, 2 * r * k, 1.0f); }
        g.setColour (speed < 0 ? col::rev : col::fwd); g.fillEllipse (c.x - r * 0.33f, c.y - r * 0.33f, r * 0.66f, r * 0.66f);
        g.setColour (col::bg); g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
        g.setColour (col::text); g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.36f, angle), c.getPointOnCircumference (r * 0.95f, angle)), 3.0f);
        g.setColour (col::bg); g.setFont (FontOptions (12.0f, Font::bold));
        g.drawText (speed < 0 ? "REV" : "FWD", Rectangle<float> (c.x - 30, c.y + 6, 60, 14).toNearestInt(), Justification::centred);
        g.setColour (col::text); g.setFont (FontOptions (13.0f));
        g.drawText (String (std::abs (speed), 2) + "x " + (speed < -0.01f ? "<<" : speed > 0.01f ? ">>" : "||"), getLocalBounds().removeFromBottom (18), Justification::centred);
        if (! held && p.engine.tel.scrubbing.load()) { g.setColour (col::yellow); g.drawText ("returning to sequence", getLocalBounds().removeFromTop (18), Justification::centred); }
        if (held) { g.setColour (col::yellow.withAlpha (0.3f)); g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 3.0f); }
    }
    float angleOf (Point<float> m) const { const auto c = getLocalBounds().toFloat().getCentre(); return std::atan2 (m.y - c.y, m.x - c.x); }
    void mouseDown (const MouseEvent& e) override
    {
        held = true; lastA = angleOf (e.position); accum = 0; base = p.engine.tel.posNorm.load();
        p.engine.ctl.scrubNorm.store (base); p.engine.ctl.shuttle.store (0); p.engine.ctl.scrubHeld.store (true);
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if ((int) p.get (brp::scrMode) == (int) br::ScrMode::Shuttle)
        { p.engine.ctl.shuttle.store (jlimit (-1.0, 1.0, e.getDistanceFromDragStartX() / (getWidth() * 0.4))); return; }
        float a = angleOf (e.position), d = a - lastA;
        if (d > MathConstants<float>::pi) d -= MathConstants<float>::twoPi; if (d < -MathConstants<float>::pi) d += MathConstants<float>::twoPi;
        lastA = a; accum += d; angle += d;
        auto src = p.source();
        const double total = p.get (brp::source) > 0.5f && src ? (double) src->sb.length / jmax (1.0, src->sb.sampleRate)
                                                             : (double) (p.engine.tel.writePos.load() - p.engine.tel.validFrom.load()) / jmax (1.0, p.engine.sampleRate());
        p.engine.ctl.scrubNorm.store (jlimit (0.0, 1.0, base + accum / MathConstants<double>::twoPi * 1.8 / jmax (0.1, total)));
    }
    void mouseUp (const MouseEvent&) override { held = false; p.engine.ctl.scrubHeld.store (false); p.engine.ctl.shuttle.store (0); }
private:
    BackReverseProcessor& p; bool held = false; float angle = 0, lastA = 0, speed = 0; double accum = 0, base = 0;
};

// ---------------------------------------------------------------- waveform / timeline
class Waveform : public Component, public SettableTooltipClient
{
public:
    std::function<void (int64, int64)> onSelect; // first/last chunk index
    explicit Waveform (BackReverseProcessor& pr) : p (pr)
    {
        setTitle ("Waveform timeline"); setWantsKeyboardFocus (true);
        setTooltip ("Click: move playhead. Drag: scrub/scratch (Shift = fine). Ctrl/Alt-drag: select chunks. Wheel: zoom, Shift+wheel: scroll. Right-click: loop/selection menu. Double-click: zoom to fit.");
    }
    void rebuildPeaks()
    {
        auto s = p.source(); gen = p.sourceGeneration; mn.clear(); mx.clear();
        if (! s) return;
        const int64 n = s->sb.length; const int bins = (int) ((n + kRes - 1) / kRes);
        mn.resize ((size_t) bins); mx.resize ((size_t) bins);
        for (int b = 0; b < bins; ++b)
        {
            float a = 1, z = -1;
            for (int64 i = (int64) b * kRes; i < jmin (n, (int64) (b + 1) * kRes); ++i) { const float v = 0.5f * (s->sb.L[i] + s->sb.R[i]); a = jmin (a, v); z = jmax (z, v); }
            mn[(size_t) b] = a; mx[(size_t) b] = z;
        }
    }
    bool fileMode() const { return p.get (brp::source) > 0.5f; }
    double xToNorm (float x) const { return jlimit (0.0, 1.0, v0 + (double) x / jmax (1, getWidth()) * vlen); }
    float normToX (double n) const { return (float) ((n - v0) / vlen * getWidth()); }
    void paint (Graphics& g) override
    {
        if (gen != p.sourceGeneration) rebuildPeaks();
        auto bnd = getLocalBounds().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (bnd, 6);
        const float mid = bnd.getCentreY(), hh = bnd.getHeight() * 0.42f;
        const auto& t = p.engine.tel; const auto P = p.snapshot();
        auto src = p.source();
        if (fileMode() && src)
        {
            const double sr = jmax (1.0, src->sb.sampleRate); const int64 n = src->sb.length;
            // loop region
            if (P.loop) { const float a = normToX ((double) p.ui (lk::loopA, 0.0)), b = normToX ((double) p.ui (lk::loopB, 1.0)); g.setColour (col::green.withAlpha (0.07f)); g.fillRect (a, 0.0f, b - a, bnd.getHeight()); }
            // chunk overlays
            const double base = P.revMode == br::RevMode::WholeSource ? (double) n : br::Engine::baseChunkFrames (P, sr, p.engine.tel.bpm.load(), 4.0);
            const double anchor = P.loop ? (double) p.ui (lk::loopA, 0.0) * n : 0.0;
            double s = anchor; const double vs = v0 * n, ve = (v0 + vlen) * n;
            const bool lane = P.tempoAssign == br::TempoAssign::CycleLane;
            for (int64 m = 0; m < 200000 && s < ve && s < n; ++m)
            {
                const double L = base * (P.revMode == br::RevMode::WholeSource ? 1.0 : br::Engine::sizeMultiplier (P, p.pats, m));
                if (s + L >= vs)
                {
                    const float x0 = normToX (s / n), x1 = normToX (jmin ((double) n, s + L) / n);
                    const bool rv = P.revMode == br::RevMode::WholeSource || br::Engine::chunkReversed (P, p.pats, m);
                    const bool sel = m >= selFirst && m <= selLast;
                    g.setColour ((rv ? col::rev : col::fwd).withAlpha (sel ? 0.28f : ((m & 1) ? 0.07f : 0.12f))); g.fillRect (x0, 0.0f, x1 - x0, bnd.getHeight());
                    if (x1 - x0 > 2) { g.setColour (col::line); g.drawVerticalLine ((int) x0, 0, bnd.getHeight()); }
                    if (x1 - x0 > 46)
                    {
                        g.setColour (rv ? col::rev : col::fwd); g.setFont (FontOptions (11.0f));
                        String lbl = String (rv ? "<< " : ">> ") + String (m);
                        if (lane) lbl << "  " << ratioLabel (p.pats.rate[m % jmax (1, p.pats.rateLen)].ratio);
                        else if (P.ratio != 1.0f) lbl << "  " << ratioLabel (P.ratio);
                        g.drawText (lbl, Rectangle<float> (x0 + 3, 2, x1 - x0 - 6, 14), Justification::centredLeft);
                    }
                }
                s += L;
                if (P.revMode == br::RevMode::WholeSource) break;
            }
            // waveform
            g.setColour (col::text.withAlpha (0.75f));
            for (int x = 0; x < getWidth(); ++x)
            {
                const int b0 = (int) (xToNorm ((float) x) * mn.size()), b1 = jmax (b0 + 1, (int) (xToNorm ((float) x + 1) * mn.size()));
                float a = 0, z = 0;
                for (int b = b0; b < b1 && b < (int) mn.size(); ++b) { a = jmin (a, mn[(size_t) b]); z = jmax (z, mx[(size_t) b]); }
                g.drawVerticalLine (x, mid - z * hh, mid - a * hh + 1);
            }
            // active chunk + playhead
            const float ax = normToX (t.chunkStartNorm.load()), aw = (float) (t.chunkLenNorm.load() / vlen * getWidth());
            g.setColour (col::yellow.withAlpha (0.6f)); g.drawRect (ax, 1.0f, jmax (2.0f, aw), bnd.getHeight() - 2, 2.0f);
            const float px = normToX (t.posNorm.load());
            g.setColour (t.scrubbing.load() ? col::red : col::yellow); g.fillRect (px - 1, 0.0f, 2.0f, bnd.getHeight());
            g.setColour (col::text); g.setFont (FontOptions (12.0f));
            g.drawText (src->name + "   " + timeStr (t.posNorm.load() * n / sr) + " / " + timeStr (n / sr) + (t.ended.load() ? "   (end)" : ""), getLocalBounds().reduced (8, 4), Justification::bottomLeft);
        }
        else if (fileMode())
        {
            g.setColour (col::dim); g.setFont (FontOptions (15.0f));
            g.drawText ("Drop an audio file here or press Open (WAV, AIFF, FLAC, MP3, OGG) - or Capture live input", getLocalBounds(), Justification::centred);
        }
        else
        {
            // live: capture history (newest at right), read head and buffer progress
            const int N = br::Engine::kPeakHist, wi = p.engine.peakWrite.load();
            g.setColour (col::text.withAlpha (0.7f));
            for (int x = 0; x < getWidth(); ++x)
            {
                const int i0 = (int) ((double) x / getWidth() * N), i1 = jmax (i0 + 1, (int) ((double) (x + 1) / getWidth() * N));
                float z = 0; for (int i = i0; i < i1; ++i) z = jmax (z, p.engine.peakHist[(size_t) ((wi + i) % N)].load());
                g.drawVerticalLine (x, mid - z * hh, mid + z * hh + 1);
            }
            const double ringFrames = (double) br::Engine::kPeakHist * p.engine.peakHop(); const double sr = jmax (1.0, p.engine.sampleRate());
            const double lat = t.latency.load();
            const float rx = (float) (getWidth() * (1.0 - lat / ringFrames)) ;
            g.setColour (col::rev.withAlpha (0.15f)); g.fillRect (rx, 0.0f, (float) getWidth() - rx, bnd.getHeight());
            g.setColour (col::red); g.fillRect ((float) getWidth() - 3, 0.0f, 3.0f, bnd.getHeight());
            const double wpos = (double) t.writePos.load(), vfrom = (double) t.validFrom.load();
            const float readX = (float) (getWidth() * (1.0 - (1.0 - t.posNorm.load()) * (wpos - vfrom) / ringFrames));
            g.setColour (t.scrubbing.load() ? col::red : col::yellow); g.fillRect (readX - 1, 0.0f, 2.0f, bnd.getHeight());
            auto bar = getLocalBounds().removeFromBottom (8).reduced (8, 1).toFloat();
            g.setColour (col::panel2); g.fillRect (bar); g.setColour (t.liveFill.load() > 0.98f ? col::green : col::rev); g.fillRect (bar.withWidth (bar.getWidth() * t.liveFill.load()));
            g.setColour (col::text); g.setFont (FontOptions (12.0f));
            g.drawText ("LIVE  write head (red) | read head (yellow) | orange = reverse buffer: " + String (lat / sr * 1000.0, 1)
                        + " ms delay. Live reverse can never be zero-latency; each chunk plays once fully captured.", getLocalBounds().reduced (8, 10), Justification::topLeft);
        }
        if (hasKeyboardFocus (false)) { g.setColour (col::yellow); g.drawRoundedRectangle (bnd.reduced (1), 6, 1); }
    }
    static String timeStr (double s) { const int m = (int) (s / 60); return String (m) + ":" + String (s - m * 60, 2).paddedLeft ('0', 5); }
    void mouseDown (const MouseEvent& e) override
    {
        down = xToNorm (e.position.x); scrubbing = false;
        if (e.mods.isPopupMenu()) { menu(); return; }
        selecting = e.mods.isCtrlDown() || e.mods.isAltDown() || e.mods.isCommandDown();
        if (selecting) { selA = selB = down; updateSel(); return; }
        if (fileMode()) p.engine.ctl.seekNorm.store (down);
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const double n = xToNorm (e.position.x);
        if (selecting) { selB = n; updateSel(); repaint(); return; }
        if (! scrubbing && e.getDistanceFromDragStart() > 3) { scrubbing = true; anchor = fileMode() ? down : p.engine.tel.posNorm.load(); p.engine.ctl.scrubNorm.store (anchor); p.engine.ctl.scrubHeld.store (true); }
        if (scrubbing) p.engine.ctl.scrubNorm.store (jlimit (0.0, 1.0, e.mods.isShiftDown() ? anchor + (n - down) * 0.1 : (fileMode() ? n : anchor + (n - down))));
    }
    void mouseUp (const MouseEvent&) override { if (scrubbing) p.engine.ctl.scrubHeld.store (false); scrubbing = selecting = false; }
    void mouseDoubleClick (const MouseEvent&) override { v0 = 0; vlen = 1; repaint(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) { v0 = jlimit (0.0, 1.0 - vlen, v0 - (w.deltaX + (e.mods.isShiftDown() ? w.deltaY : 0)) * vlen * 0.5); }
        else
        {
            const double at = xToNorm (e.position.x), z = w.deltaY > 0 ? 0.8 : 1.25;
            vlen = jlimit (0.0005, 1.0, vlen * z); v0 = jlimit (0.0, 1.0 - vlen, at - (at - v0) * z);
        }
        repaint();
    }
    void updateSel()
    {
        auto src = p.source(); if (! src || ! fileMode()) return;
        const auto P = p.snapshot(); const double n = (double) src->sb.length;
        const double base = br::Engine::baseChunkFrames (P, src->sb.sampleRate, p.engine.tel.bpm.load(), 4.0);
        const double a = jmin (selA, selB) * n, b = jmax (selA, selB) * n; double s = P.loop ? (double) p.ui (lk::loopA, 0.0) * n : 0.0;
        selFirst = selLast = -1;
        for (int64 m = 0; m < 200000 && s < n; ++m) { const double L = base * br::Engine::sizeMultiplier (P, p.pats, m); if (s + L > a && s <= b) { if (selFirst < 0) selFirst = m; selLast = m; } s += L; }
        if (onSelect) onSelect (selFirst, selLast);
    }
    void menu()
    {
        PopupMenu m;
        m.addItem (1, "Loop selection", selFirst >= 0); m.addItem (2, "Clear loop region"); m.addItem (3, "Zoom to fit"); m.addItem (4, "Clear chunk selection");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r)
        {
            if (r == 1) { p.setUi (lk::loopA, jmin (selA, selB)); p.setUi (lk::loopB, jmax (selA, selB)); p.set (brp::loop, 1); }
            if (r == 2) { p.setUi (lk::loopA, 0.0); p.setUi (lk::loopB, 1.0); }
            if (r == 3) { v0 = 0; vlen = 1; }
            if (r == 4) { selFirst = selLast = -1; if (onSelect) onSelect (-1, -1); }
            repaint();
        });
    }
    bool keyPressed (const KeyPress& k) override
    {
        if (! fileMode()) return false;
        const double step = k.getModifiers().isShiftDown() ? 0.001 : 0.01;
        if (k == KeyPress::leftKey) { p.engine.ctl.seekNorm.store (jmax (0.0, p.engine.tel.posNorm.load() - step)); return true; }
        if (k == KeyPress::rightKey) { p.engine.ctl.seekNorm.store (jmin (1.0, p.engine.tel.posNorm.load() + step)); return true; }
        return false;
    }
    int64 selFirst = -1, selLast = -1;
private:
    static constexpr int kRes = 256;
    BackReverseProcessor& p; std::vector<float> mn, mx; int gen = -1;
    double v0 = 0, vlen = 1, down = 0, anchor = 0, selA = 0, selB = 0; bool scrubbing = false, selecting = false;
};

// ---------------------------------------------------------------- meter
struct Meter : Component, SettableTooltipClient
{
    float l = 0, r = 0;
    void set (float a, float b) { l = jmax (a, l * 0.85f); r = jmax (b, r * 0.85f); repaint(); }
    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        for (int ch = 0; ch < 2; ++ch)
        {
            auto bar = b.removeFromTop (b.getHeight() * (ch == 0 ? 0.5f : 1.0f)).reduced (0, 1);
            g.setColour (col::panel2); g.fillRect (bar);
            const float v = ch ? r : l, db = Decibels::gainToDecibels (v, -60.0f), f = jlimit (0.0f, 1.0f, (db + 60) / 66);
            g.setColour (v > 1.0f ? col::red : v > 0.7f ? col::yellow : col::green); g.fillRect (bar.withWidth (bar.getWidth() * f));
        }
    }
};

} // namespace ui
