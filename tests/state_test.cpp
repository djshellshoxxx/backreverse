// Save/reload the full processor state and verify identical behaviour (spec 06 s16, spec 05 s5/s6).
#include "../src/plugin/PluginEditor.h"
#include <cstdio>

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { ++fails; std::printf ("FAIL %s\n", m); } else std::printf ("ok   %s\n", m); } while (0)

static std::vector<float> run (BackReverseProcessor& p)
{
    p.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer midi; br::Rng r (5); std::vector<float> out;
    for (int k = 0; k < 400; ++k)
    {
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) b.setSample (ch, i, (float) (r.next() * 2 - 1) * 0.5f);
        p.processBlock (b, midi);
        for (int ch = 0; ch < 2; ++ch) out.insert (out.end(), b.getReadPointer (ch), b.getReadPointer (ch) + 512);
    }
    return out;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    BackReverseProcessor a;
    a.set (brp::chunkMs, 37); a.set (brp::revMode, 1); a.set (brp::order, 2); a.set (brp::patLen, 6); a.set (brp::seed, 77); a.set (brp::freeze, 0);
    a.set (brp::sizeMode, 3); a.set (brp::gateOn, 1); a.set (brp::gateSteps, 3); a.set (brp::stutOn, 1); a.set (brp::stutScope, 3); a.set (brp::dlyOn, 1);
    a.set (brp::chainOrder, 3); a.set (brp::panMode, 5); a.set (brp::polMode, 6); a.set (brp::phaseMode, 2); a.set (brp::tempoAssign, 3); a.set (brp::timeMode, 1);
    br::Patterns q = a.pats;
    q.sizeCount = 3; q.sizeMult[1] = 0.5f; q.sizeMult[2] = 2; q.gate[1].dir = 2; q.gate[2].on = 0; q.gate[3].fx = 5; q.pan[2] = -0.4f; q.rateLen = 4; q.rate[2].ratio = 0.25f; q.rate[3].stretch = 1;
    a.applyPatterns (q, false); a.setUserPattern ("0,2,REST,1%50,?", false);
    juce::MemoryBlock st; a.getStateInformation (st);
    auto xml = juce::AudioProcessor::getXmlFromBinary (st.getData(), (int) st.getSize());
    CHECK (xml != nullptr && xml->getIntAttribute ("formatVersion") == 1 && xml->getIntAttribute ("schemaVersion") == 1 && xml->getStringAttribute ("productVersion").isNotEmpty(), "state carries format/product/schema versions");
    BackReverseProcessor b;
    b.setStateInformation (st.getData(), (int) st.getSize());
    bool same = true; for (int i = 0; i < brp::kNum; ++i) if (brp::kDefs[i].kind == brp::F ? (a.get (i) != b.get (i) || a.param (i)->getValue() != b.param (i)->getValue()) : std::lround (a.get (i)) != std::lround (b.get (i))) { same = false; std::printf ("  mismatch %s %.9g %.9g\n", brp::kDefs[i].id, a.get (i), b.get (i)); }
    CHECK (same, "all parameters restored");
    bool lanesSame = true;
#define BR_CMP(k) lanesSame = lanesSame && a.lanes[lk::k] == b.lanes[lk::k];
    BR_LANE_KEYS (BR_CMP)
#undef BR_CMP
    CHECK (lanesSame && b.pats.gate[1].dir == 2 && b.pats.rate[2].ratio == 0.25f && b.pats.userLen == 5, "pattern/lane state restored");
    CHECK (run (a) == run (b), "restored state renders identically (deterministic seed recall)");
    // unknown future fields are ignored safely
    xml->setAttribute ("formatVersion", 2); xml->createNewChildElement ("FutureThing")->setAttribute ("x", 1);
    juce::MemoryBlock st2; juce::AudioProcessor::copyXmlToBinary (*xml, st2);
    BackReverseProcessor c; c.setStateInformation (st2.getData(), (int) st2.getSize());
    CHECK (c.get (brp::seed) == 77.0f, "future/unknown fields ignored");
    // clipboard sub-preset round trip
    const auto clip = a.copySection ("gate");
    BackReverseProcessor d; CHECK (d.pasteSection (clip) && d.pats.gate[1].dir == 2 && d.get (brp::gateOn) > 0.5f, "gate pattern copy/paste");
    // factory presets load without crashing and differ from init
    for (int i = 0; i < BackReverseProcessor::numFactoryPresets(); ++i) d.loadPreset (i);
    CHECK (BackReverseProcessor::numFactoryPresets() >= 15, "factory presets present");
    d.loadPreset (1); CHECK (std::abs (d.get (brp::chunkMs) - 5000.0f) < 1.0f, "preset applies (5-second reverse)");
    // UI smoke test: the editor builds, every tab lays out, the editor scales; optional PNG snapshots
    {
        BackReverseProcessor e; e.prepareToPlay (48000, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (e.createEditor());
        auto* be = dynamic_cast<BackReverseEditor*> (ed.get());
        CHECK (be != nullptr && be->numTabs() >= 10, "editor constructs with all tabs");
        const auto dir = juce::SystemStats::getEnvironmentVariable ("BR_SNAPSHOT_DIR", {});
        for (int t = 0; be && t < be->numTabs(); ++t)
        {
            be->selectTab (t);
            auto img = be->createComponentSnapshot (be->getLocalBounds());
            if (dir.isNotEmpty()) { juce::FileOutputStream os (juce::File (dir).getChildFile ("tab" + juce::String (t) + ".png")); os.setPosition (0); os.truncate(); juce::PNGImageFormat().writeImageToStream (img, os); }
        }
        if (be) { be->setSize (1920, 1230); CHECK (be->getWidth() == 1920, "editor resizes/scales"); }
    }
    std::printf ("%s\n", fails ? "STATE TESTS FAILED" : "all state tests passed");
    return fails ? 1 : 0;
}
