// Factory presets (spec 05 s7) and user preset files.
#include "PluginProcessor.h"

using namespace juce;
using P = BackReverseProcessor;

namespace {
struct Factory { const char* name; std::function<void (P&, br::Patterns&)> apply; };

const std::vector<Factory>& factory()
{
    using namespace brp;
    static const std::vector<Factory> f {
        { "Init", [] (P&, br::Patterns&) {} },
        { "Basic 5-second reverse", [] (P& p, br::Patterns&) { p.set (chunkMs, 5000); } },
        { "Whole-track reverse", [] (P& p, br::Patterns&) { p.set (source, 1); p.set (revMode, 2); } },
        { "Half-speed reversed", [] (P& p, br::Patterns&) { p.set (chunkMs, 2000); p.set (ratio, 0.5f); } },
        { "Quarter-speed surreal", [] (P& p, br::Patterns&) { p.set (chunkMs, 3000); p.set (ratio, 0.25f); p.set (timeMode, 1); p.set (algo, 3); p.set (echoOn, 1); p.set (echoChar, 2); } },
        { "Triple-speed fragments", [] (P& p, br::Patterns&) { p.set (chunkMs, 250); p.set (ratio, 3); p.set (revMode, 1); p.set (order, 3); p.set (patLen, 8); } },
        { "Vocal stretch", [] (P& p, br::Patterns&) { p.set (chunkMs, 1500); p.set (timeMode, 1); p.set (algo, 2); p.set (ratio, 0.5f); p.set (xfade, 2); p.set (xfadeMs, 20); } },
        { "Random cut-up", [] (P& p, br::Patterns& q) { p.set (chunkMs, 400); p.set (revMode, 1); p.set (order, 2); p.set (patLen, 8); p.set (sizeMode, 3); p.set (revScope, 2); p.set (seed, 1977);
              q.sizeCount = 4; const float m[4] = { 0.5f, 1, 0.25f, 2 }; for (int i = 0; i < 4; ++i) q.sizeMult[i] = m[i]; } },
        { "Alternating forward/reverse gates", [] (P& p, br::Patterns& q) { p.set (chunkMs, 1000); p.set (gateOn, 1); p.set (gateSteps, 2); p.set (gateWidth, 95);
              for (int i = 0; i < br::kMaxSteps; ++i) q.gate[i].dir = (uint8_t) ((i % 2) ? 2 : 0); } },
        { "Pan mirror chunks", [] (P& p, br::Patterns&) { p.set (chunkMs, 750); p.set (panMode, 3); p.set (panDepth, 80); p.set (swap, 2); } },
        { "Phase/polarity pattern", [] (P& p, br::Patterns& q) { p.set (chunkMs, 600); p.set (polMode, 5); p.set (phaseMode, 2); p.set (phaseDeg, 90); q.polLen = 4; } },
        { "Stutter chain", [] (P& p, br::Patterns& q) { p.set (chunkMs, 1000); p.set (gateOn, 1); p.set (gateSteps, 2); p.set (gateWidth, 100); p.set (stutOn, 1); p.set (stutScope, 4); p.set (stutRetrig, 2); p.set (stutLen, 80); p.set (stutRepeats, 6);
              for (int i = 0; i < br::kMaxSteps; ++i) q.gate[i].fx = (uint8_t) ((i % 4 == 3) ? 1 : 0); } },
        { "Delay chain", [] (P& p, br::Patterns&) { p.set (chunkMs, 1000); p.set (dlyOn, 1); p.set (dlySync, 14); p.set (dlyPing, 1); p.set (dlyFb, 55); p.set (dlyMix, 40); p.set (dlyScope, 1); } },
        { "Echo chain", [] (P& p, br::Patterns&) { p.set (chunkMs, 1500); p.set (echoOn, 1); p.set (echoChar, 1); p.set (echoFb, 60); p.set (echoWow, 50); p.set (echoMix, 45); } },
        { "Scratch/vinyl", [] (P& p, br::Patterns&) { p.set (source, 1); p.set (revMode, 4); p.set (scrMode, 1); p.set (scrRelease, 2); p.set (scrInertia, 35); } },
        { "Extreme surreal", [] (P& p, br::Patterns& q) { p.set (chunkMs, 800); p.set (revMode, 1); p.set (order, 4); p.set (patLen, 6); p.set (timeMode, 1); p.set (algo, 3); p.set (tempoAssign, 4);
              p.set (stutOn, 1); p.set (stutScope, 3); p.set (stutProb, 40); p.set (echoOn, 1); p.set (echoChar, 4); p.set (echoFb, 75); p.set (dlyOn, 1); p.set (dlyFb, 60); p.set (chainOrder, 4); p.set (panMode, 2); p.set (phaseMode, 1);
              const float r[6] = { 0.25f, 3, 0.5f, 2, 1, 0.25f }; q.rateLen = 6; for (int i = 0; i < 6; ++i) q.rate[i].ratio = r[i]; } },
        { "Tape-stop chunks", [] (P& p, br::Patterns&) { p.set (chunkMs, 1200); p.set (tapeOn, 1); p.set (tapeMode, 0); p.set (tapeLen, 45); p.set (tapeProb, 60); p.set (seed, 33); } },
        { "Cycle lane 1x 1/2 2x 1/4 3x", [] (P& p, br::Patterns&) { p.set (chunkMs, 500); p.set (tempoAssign, 2); } },
    };
    return f;
}
} // namespace

File BackReverseProcessor::presetDir()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("BackReverse").getChildFile ("Presets");
}

StringArray BackReverseProcessor::presetNames() const
{
    StringArray s;
    for (auto& f : factory()) s.add (f.name);
    auto files = presetDir().findChildFiles (File::findFiles, false, "*.brpreset");
    files.sort();
    for (auto& f : files) s.add ("User: " + f.getFileNameWithoutExtension());
    return s;
}

int BackReverseProcessor::numFactoryPresets() { return (int) factory().size(); }

void BackReverseProcessor::resetToDefaults()
{
    for (int i = 0; i < brp::kNum; ++i)
        if (i != brp::source) set (i, brp::kDefs[i].def);
    applyPatterns (br::Patterns());
    setUserPattern ("0,1,0,2");
}

void BackReverseProcessor::loadPreset (int index)
{
    undo.beginNewTransaction ("Load preset");
    const auto& f = factory();
    if (isPositiveAndBelow (index, (int) f.size()))
    {
        const float src = get (brp::source);
        resetToDefaults();
        set (brp::source, src);
        br::Patterns q;
        f[(size_t) index].apply (*this, q);
        applyPatterns (q);
        currentPreset = index;
        return;
    }
    auto files = presetDir().findChildFiles (File::findFiles, false, "*.brpreset");
    files.sort();
    const int u = index - (int) f.size();
    if (isPositiveAndBelow (u, files.size()))
        if (auto xml = parseXML (files[u]))
        {
            applyStateTree (ValueTree::fromXml (*xml), false);
            currentPreset = index;
        }
}

bool BackReverseProcessor::saveUserPreset (const String& name)
{
    auto dir = presetDir(); dir.createDirectory();
    auto tree = stateTree();
    tree.setProperty ("presetName", name, nullptr);
    if (auto xml = tree.createXml())
        return xml->writeTo (dir.getChildFile (File::createLegalFileName (name) + ".brpreset"));
    return false;
}
