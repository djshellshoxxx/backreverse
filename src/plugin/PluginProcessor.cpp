#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// Float parameter that stores its normalised value exactly, so host save/restore is bit-identical even for skewed ranges.
class ExactFloatParam : public RangedAudioParameter
{
public:
    ExactFloatParam (const ParameterID& id, const String& name, NormalisableRange<float> r, float def, const String& unit)
        : RangedAudioParameter (id, name, AudioProcessorParameterWithIDAttributes().withLabel (unit)), range (r), defNorm (r.convertTo0to1 (def)), norm (defNorm) {}
    float getValue() const override { return norm.load(); }
    void setValue (float v) override { norm.store (jlimit (0.0f, 1.0f, v)); }
    float getDefaultValue() const override { return defNorm; }
    String getText (float v, int) const override { const float x = range.convertFrom0to1 (v), a = std::abs (x); return String (x, a < 10 ? 2 : a < 100 ? 1 : 0); }
    float getValueForText (const String& t) const override { return range.convertTo0to1 (jlimit (range.start, range.end, t.getFloatValue())); }
    const NormalisableRange<float>& getNormalisableRange() const override { return range; }
private:
    NormalisableRange<float> range; float defNorm; std::atomic<float> norm;
};

static AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& d : brp::kDefs)
    {
        const ParameterID id { d.id, 1 };
        switch (d.kind)
        {
            case brp::F:
            {
                NormalisableRange<float> r (d.min, d.max);
                if (d.centre > 0) r.setSkewForCentre (d.centre);
                layout.add (std::make_unique<ExactFloatParam> (id, d.name, r, d.def, d.unit));
                break;
            }
            case brp::I: layout.add (std::make_unique<AudioParameterInt> (id, d.name, (int) d.min, (int) d.max, (int) d.def)); break;
            case brp::B: layout.add (std::make_unique<AudioParameterBool> (id, d.name, d.def > 0.5f)); break;
            case brp::C: layout.add (std::make_unique<AudioParameterChoice> (id, d.name, StringArray::fromTokens (d.choices, "|", ""), (int) d.def)); break;
        }
    }
    return layout;
}

// ---------------------------------------------------------------- pattern <-> lanes text
static String joinF (const float* v, int n) { StringArray s; for (int i = 0; i < n; ++i) s.add (String (v[i], 4)); return s.joinIntoString (","); }
static std::vector<float> splitF (const String& t) { std::vector<float> v; for (auto& s : StringArray::fromTokens (t, ",;", "")) if (s.trim().isNotEmpty()) v.push_back (s.getFloatValue()); return v; }

static void patternsToTree (const br::Patterns& p, ValueTree& t, UndoManager* um)
{
    StringArray s;
    for (int i = 0; i < p.sizeCount; ++i) s.add (String (p.sizeMult[i], 4) + ":" + String (p.sizeWeight[i], 3));
    t.setProperty (lk::sizes, s.joinIntoString (","), um); s.clear();
    for (int i = 0; i < p.rateLen; ++i) s.add (String (p.rate[i].ratio, 4) + ":" + String ((int) p.rate[i].stretch) + ":" + String (p.rate[i].prob, 3) + ":" + String (p.rate[i].weight, 3));
    t.setProperty (lk::rates, s.joinIntoString (","), um); s.clear();
    t.setProperty (lk::laneLen, p.laneLen, um);
    for (int i = 0; i < br::kLane; ++i) s.add (String ((int) p.chunkFlags[i]));
    t.setProperty (lk::chunkFlags, s.joinIntoString (","), um); s.clear();
    t.setProperty (lk::panLen, p.panLen, um); t.setProperty (lk::pan, joinF (p.pan, br::kLane), um);
    t.setProperty (lk::polLen, p.polLen, um);
    for (int i = 0; i < br::kLane; ++i) s.add (String ((int) p.pol[i]));
    t.setProperty (lk::pol, s.joinIntoString (","), um); s.clear();
    for (auto& g : p.gate)
        s.add (String ((int) g.on) + ":" + String ((int) g.dir) + ":" + String ((int) g.fx) + ":" + String ((int) g.shape) + ":" + String (g.width, 3) + ":" + String (g.depth, 3) + ":" + String (g.fadeIn, 3) + ":" + String (g.fadeOut, 3));
    t.setProperty (lk::gates, s.joinIntoString (";"), um); s.clear();
    for (int i = 0; i < p.curveCount; ++i) s.add (String (p.curveX[i], 4) + ":" + String (p.curveY[i], 4));
    t.setProperty (lk::curve, s.joinIntoString (","), um);
}

static br::Patterns treeToPatterns (const ValueTree& t)
{
    br::Patterns p;
    auto items = [&] (const Identifier& k, const char* sep) { return StringArray::fromTokens (t[k].toString(), sep, ""); };
    if (t.hasProperty (lk::sizes))
    {
        auto it = items (lk::sizes, ","); p.sizeCount = jlimit (1, br::kSizes, it.size());
        for (int i = 0; i < p.sizeCount && i < it.size(); ++i) { auto f = splitF (it[i].replaceCharacter (':', ',')); p.sizeMult[i] = f.size() > 0 ? jlimit (0.01f, 64.0f, f[0]) : 1; p.sizeWeight[i] = f.size() > 1 ? f[1] : 1; }
    }
    if (t.hasProperty (lk::rates))
    {
        auto it = items (lk::rates, ","); p.rateLen = jlimit (1, br::kLane, it.size());
        for (int i = 0; i < p.rateLen && i < it.size(); ++i)
        {
            auto f = splitF (it[i].replaceCharacter (':', ','));
            p.rate[i].ratio = f.size() > 0 ? jlimit (0.05f, 8.0f, f[0]) : 1; p.rate[i].stretch = (uint8_t) (f.size() > 1 ? jlimit (0, 2, (int) f[1]) : 0);
            p.rate[i].prob = f.size() > 2 ? jlimit (0.0f, 1.0f, f[2]) : 1; p.rate[i].weight = f.size() > 3 ? jmax (0.0f, f[3]) : 1;
        }
    }
    p.laneLen = jlimit (1, br::kLane, (int) t.getProperty (lk::laneLen, p.laneLen));
    if (t.hasProperty (lk::chunkFlags)) { auto f = splitF (t[lk::chunkFlags]); for (int i = 0; i < br::kLane && i < (int) f.size(); ++i) p.chunkFlags[i] = (uint8_t) f[(size_t) i]; }
    p.panLen = jlimit (1, br::kLane, (int) t.getProperty (lk::panLen, p.panLen));
    if (t.hasProperty (lk::pan)) { auto f = splitF (t[lk::pan]); for (int i = 0; i < br::kLane && i < (int) f.size(); ++i) p.pan[i] = jlimit (-1.0f, 1.0f, f[(size_t) i]); }
    p.polLen = jlimit (1, br::kLane, (int) t.getProperty (lk::polLen, p.polLen));
    if (t.hasProperty (lk::pol)) { auto f = splitF (t[lk::pol]); for (int i = 0; i < br::kLane && i < (int) f.size(); ++i) p.pol[i] = (uint8_t) jlimit (0, 3, (int) f[(size_t) i]); }
    if (t.hasProperty (lk::gates))
    {
        auto it = items (lk::gates, ";");
        for (int i = 0; i < br::kMaxSteps && i < it.size(); ++i)
        {
            auto f = splitF (it[i].replaceCharacter (':', ','));
            if (f.size() < 8) continue;
            auto& g = p.gate[i];
            g.on = (uint8_t) (f[0] > 0.5f); g.dir = (uint8_t) jlimit (0, 2, (int) f[1]); g.fx = (uint8_t) jlimit (0, 7, (int) f[2]);
            g.shape = (int8_t) jlimit (-1, 8, (int) f[3]); g.width = f[4]; g.depth = f[5]; g.fadeIn = jlimit (0.0f, 1.0f, f[6]); g.fadeOut = jlimit (0.0f, 1.0f, f[7]);
        }
    }
    if (t.hasProperty (lk::curve))
    {
        auto it = items (lk::curve, ","); p.curveCount = jlimit (2, br::kCurvePts, it.size());
        for (int i = 0; i < p.curveCount && i < it.size(); ++i) { auto f = splitF (it[i].replaceCharacter (':', ',')); if (f.size() > 1) { p.curveX[i] = jlimit (0.0f, 1.0f, f[0]); p.curveY[i] = jlimit (0.0f, 1.0f, f[1]); } }
    }
    BackReverseProcessor::parseUserPattern (t[lk::userPat].toString(), p);
    return p;
}

void BackReverseProcessor::parseUserPattern (const String& text, br::Patterns& p)
{
    // tokens: n (absolute), +n/-n (relative), REST/-/_ (rest), ? (random), n%p (probability), n*k (repeat k times)
    int len = 0;
    for (auto tok : StringArray::fromTokens (text, ", \t\n", ""))
    {
        tok = tok.trim().toUpperCase();
        if (tok.isEmpty()) continue;
        int reps = 1;
        if (tok.containsChar ('*')) { reps = jlimit (1, br::kUserPat, tok.fromFirstOccurrenceOf ("*", false, false).getIntValue()); tok = tok.upToFirstOccurrenceOf ("*", false, false); }
        br::UserTok t;
        if (tok.containsChar ('%')) { t.prob = jlimit (0.0f, 1.0f, tok.fromFirstOccurrenceOf ("%", false, false).getFloatValue() * 0.01f); tok = tok.upToFirstOccurrenceOf ("%", false, false); }
        if (tok == "REST" || tok == "R" || tok == "-" || tok == "_") t.kind = 2;
        else if (tok == "?") t.kind = 3;
        else if (tok.startsWithChar ('+') || (tok.startsWithChar ('-') && tok.length() > 1)) { t.kind = 1; t.val = (int16_t) tok.getIntValue(); }
        else if (tok.containsOnly ("0123456789")) { t.kind = 0; t.val = (int16_t) tok.getIntValue(); }
        else continue;
        for (int r = 0; r < reps && len < br::kUserPat; ++r) p.user[len++] = t;
    }
    if (len == 0) { len = 1; p.user[0] = br::UserTok(); }
    p.userLen = len;
}

// ---------------------------------------------------------------- lifecycle
BackReverseProcessor::BackReverseProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMS", makeLayout())
{
    for (int i = 0; i < brp::kNum; ++i) raw[(size_t) i] = apvts.getRawParameterValue (brp::kDefs[i].id);
    formats.registerBasicFormats();
    patternsToTree (br::Patterns(), lanes, nullptr);
    lanes.setProperty (lk::userPat, "0,1,0,2", nullptr);
    lanes.setProperty (lk::loopA, 0.0, nullptr); lanes.setProperty (lk::loopB, 1.0, nullptr);
    lanes.setProperty (lk::randAmt, 0.6, nullptr); lanes.setProperty (lk::randEx, 0, nullptr);
    lanes.addListener (this);
    publishPatterns();
    engine.prepare (48000, 512);
    startTimerHz (30);
}

BackReverseProcessor::~BackReverseProcessor()
{
    stopTimer();
    renderCancel = true;
    if (renderThread.joinable()) renderThread.join();
    stopRecording();
    engine.setSource (nullptr);
}

bool BackReverseProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::stereo() || in == AudioChannelSet::mono() || in.isDisabled();
}

void BackReverseProcessor::prepareToPlay (double rate, int block)
{
    const bool rateChanged = std::abs (rate - sr) > 0.5;
    sr = rate;
    engine.prepare (rate, jmax (16, block));
    engine.ctl.loopA.store ((float) (double) ui (lk::loopA, 0.0)); engine.ctl.loopB.store ((float) (double) ui (lk::loopB, 1.0));
    setLatencySamples (0);
    if (rateChanged && activeSrc != nullptr)
    {
        // re-derive the source at the new device rate off the audio thread
        MessageManager::callAsync ([this, f = currentFile, src = activeSrc, old = activeSrc->sb.sampleRate]
        {
            if (f.existsAsFile()) loadFile (f);
            else { AudioBuffer<float> copy (src->buf); setSource (makeSource (std::move (copy), old, src->name)); }
        });
    }
    else if (activeSrc) engine.setSource (&activeSrc->sb);
}

void BackReverseProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    patBox.tryRead (audioPats, patSeq);
    br::EngineParams P; brp::read (raw, P);
    br::HostInfo h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            h.valid = true; h.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) h.bpm = *b;
            if (auto q = pos->getPpqPosition()) h.ppq = *q;
            if (auto ts = pos->getTimeSignature()) h.beatsPerBar = ts->numerator * 4.0 / jmax (1, ts->denominator);
            if (auto s = pos->getTimeInSamples()) h.samplePos = *s;
        }
    h.offline = isNonRealtime();
    for (int ch = 2; ch < outs; ++ch) buffer.clear (ch, 0, n);
    if (ins == 0) buffer.clear(); // no input bus: process silence in place (engine reads each sample before writing it)
    const float* inL = buffer.getReadPointer (0);
    const float* inR = ins > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = outs > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (inL, inR, outL, outR, n, P, audioPats, h);
    if (auto* w = writer.load (std::memory_order_acquire); w != nullptr && outs >= 2) w->write (buffer.getArrayOfReadPointers(), n);
    blockCounter.fetch_add (1, std::memory_order_release);
    lastBlockMs.store (Time::getMillisecondCounter(), std::memory_order_relaxed);
}

void BackReverseProcessor::timerCallback()
{
    const int lat = engine.latencyFrames();
    if (lat != getLatencySamples()) setLatencySamples (lat); // host compensation updated on the message thread
    const auto bc = blockCounter.load();
    const bool idle = Time::getMillisecondCounter() - lastBlockMs.load() > 500;
    retired.erase (std::remove_if (retired.begin(), retired.end(), [&] (auto& r) { return bc > r.second || idle; }), retired.end());
    if (captureStopAt >= 0 && (bc > captureStopAt + 1 || idle))
    {
        captureStopAt = -1;
        const int len = engine.tel.captureLen.load();
        if (len > 64)
        {
            AudioBuffer<float> b (2, len);
            b.copyFrom (0, 0, engine.captureData (0), len); b.copyFrom (1, 0, engine.captureData (1), len);
            currentFile = File(); setUi (lk::filePath, "");
            setSource (makeSource (std::move (b), engine.sampleRate(), "Live capture " + Time::getCurrentTime().toString (false, true)));
            set (brp::source, 1);
        }
    }
}

// ---------------------------------------------------------------- params / lanes
void BackReverseProcessor::set (int i, float plain)
{
    if (auto* p = param (i)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (plain)); p->endChangeGesture(); }
}

void BackReverseProcessor::publishPatterns()
{
    pats = treeToPatterns (lanes);
    patBox.write (pats);
}

void BackReverseProcessor::applyPatterns (const br::Patterns& p, bool undoable) { patternsToTree (p, lanes, undoable ? &undo : nullptr); }
void BackReverseProcessor::setUserPattern (const String& text, bool undoable) { lanes.setProperty (lk::userPat, text, undoable ? &undo : nullptr); }

void BackReverseProcessor::valueTreePropertyChanged (ValueTree&, const Identifier& k)
{
#define BR_IS(key) k == lk::key ||
    if (BR_LANE_KEYS (BR_IS) false) publishPatterns();
#undef BR_IS
    if (k == lk::loopA) engine.ctl.loopA.store ((float) (double) lanes[k]);
    if (k == lk::loopB) engine.ctl.loopB.store ((float) (double) lanes[k]);
}

// ---------------------------------------------------------------- sources
std::shared_ptr<LoadedSource> BackReverseProcessor::makeSource (AudioBuffer<float>&& in, double rate, const String& name)
{
    auto s = std::make_shared<LoadedSource>();
    s->name = name;
    const double target = sr > 0 ? sr : rate;
    if (std::abs (rate - target) > 0.5)
    {
        const double ratio = rate / target;
        const int outLen = (int) std::floor (in.getNumSamples() / ratio);
        s->buf.setSize (2, jmax (1, outLen));
        for (int ch = 0; ch < 2; ++ch)
        {
            WindowedSincInterpolator interp;
            interp.process (ratio, in.getReadPointer (jmin (ch, in.getNumChannels() - 1)), s->buf.getWritePointer (ch), outLen);
        }
    }
    else
    {
        s->buf.setSize (2, in.getNumSamples());
        for (int ch = 0; ch < 2; ++ch) s->buf.copyFrom (ch, 0, in, jmin (ch, in.getNumChannels() - 1), 0, in.getNumSamples());
    }
    s->sb.L = s->buf.getReadPointer (0); s->sb.R = s->buf.getReadPointer (1); s->sb.length = s->buf.getNumSamples(); s->sb.sampleRate = target;
    return s;
}

void BackReverseProcessor::setSource (std::shared_ptr<LoadedSource> s)
{
    engine.setSource (s ? &s->sb : nullptr);
    if (activeSrc) retired.push_back ({ activeSrc, blockCounter.load() + 2 });
    activeSrc = std::move (s);
    ++sourceGeneration;
    engine.ctl.transport.store (4);
}

bool BackReverseProcessor::loadFile (const File& f, String* error)
{
    std::unique_ptr<AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr) { if (error) *error = "Unsupported or unreadable audio file: " + f.getFileName(); return false; }
    const int64 maxLen = (int64) (r->sampleRate * 60.0 * 20.0);
    if (r->lengthInSamples > maxLen && error) *error = "File longer than 20 minutes was truncated.";
    const int len = (int) jmin (r->lengthInSamples, maxLen);
    AudioBuffer<float> b ((int) jlimit (1u, 2u, r->numChannels), jmax (1, len));
    r->read (&b, 0, len, 0, true, true);
    currentFile = f;
    setUi (lk::filePath, f.getFullPathName());
    setSource (makeSource (std::move (b), r->sampleRate, f.getFileName()));
    set (brp::source, 1);
    return true;
}

void BackReverseProcessor::startCapture() { engine.ctl.capture.store (true); }
void BackReverseProcessor::stopCapture() { if (engine.ctl.capture.exchange (false)) captureStopAt = blockCounter.load(); }

// ---------------------------------------------------------------- record / render
bool BackReverseProcessor::startRecording (const File& f)
{
    stopRecording();
    f.deleteFile();
    auto stream = f.createOutputStream();
    if (stream == nullptr) return false;
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (stream.release());
    auto* w = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0);
    if (w == nullptr) return false;
    os.release();
    writerThread.startThread();
    writer.store (new AudioFormatWriter::ThreadedWriter (w, writerThread, 65536));
    recordingFile = f;
    return true;
}

void BackReverseProcessor::stopRecording()
{
    auto* w = writer.exchange (nullptr);
    if (w == nullptr) return;
    const auto bc = blockCounter.load();
    for (int i = 0; i < 20 && blockCounter.load() <= bc + 1 && Time::getMillisecondCounter() - lastBlockMs.load() < 500; ++i) Thread::sleep (10);
    delete w;
}

bool BackReverseProcessor::startRender (const File& f)
{
    auto src = activeSrc;
    if (src == nullptr || renderProgress.load() >= 0) return false;
    if (renderThread.joinable()) renderThread.join();
    br::EngineParams P = snapshot();
    P.source = br::Source::File; P.loop = false; P.bypass = false; P.fileHostSync = false; P.scrActive = false;
    if (P.revMode == br::RevMode::FreeScrub) P.revMode = br::RevMode::Sequential;
    const br::Patterns pp = pats; const double rate = src->sb.sampleRate;
    renderProgress = 0; renderCancel = false; lastRender = f;
    renderThread = std::thread ([this, src, P, pp, rate, f]
    {
        auto e = std::make_unique<br::Engine>();
        e->prepare (rate, 1024, 1.0, 0.01);
        e->setSource (&src->sb); e->ctl.transport.store (1);
        f.deleteFile();
        std::unique_ptr<AudioFormatWriter> w;
        if (auto os = f.createOutputStream()) { WavAudioFormat wav; std::unique_ptr<OutputStream> o (os.release()); w.reset (wav.createWriterFor (o.get(), rate, 2, 24, {}, 0)); if (w) o.release(); }
        if (w)
        {
            AudioBuffer<float> out (2, 1024); std::vector<float> z (1024, 0.0f); br::HostInfo h;
            const double expect = (double) src->sb.length / jmax (0.05f, P.ratio);
            const int64 maxFrames = (int64) (src->sb.length * 25 + rate * 10);
            int64 done = 0, tail = -1;
            while (! renderCancel && done < maxFrames)
            {
                e->process (z.data(), z.data(), out.getWritePointer (0), out.getWritePointer (1), 1024, P, pp, h);
                w->writeFromAudioSampleBuffer (out, 0, 1024);
                done += 1024;
                if (tail < 0 && e->tel.ended.load()) tail = (int64) (rate * 3.0);
                if (tail >= 0 && (tail -= 1024) <= 0) break;
                renderProgress = (float) jmin (0.99, (double) done / jmax (1.0, expect));
            }
        }
        renderProgress = -1;
    });
    return true;
}

// ---------------------------------------------------------------- state
ValueTree BackReverseProcessor::stateTree()
{
    ValueTree root ("BackReverseState");
    root.setProperty ("formatVersion", 1, nullptr);
    root.setProperty ("productVersion", BR_VERSION_STRING, nullptr);
    root.setProperty ("schemaVersion", 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (lanes.createCopy(), nullptr);
    // exact normalised values so skewed parameters round-trip bit-identically (host state reproducibility)
    ValueTree norm ("NORM");
    for (int i = 0; i < brp::kNum; ++i) if (auto* q = param (i); q != nullptr && brp::kDefs[i].kind == brp::F) norm.setProperty (brp::kDefs[i].id, String ((double) q->getValue(), 17), nullptr);
    root.appendChild (norm, nullptr);
    return root;
}

void BackReverseProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = stateTree().createXml()) copyXmlToBinary (*xml, dest);
}

void BackReverseProcessor::applyStateTree (const ValueTree& rootIn, bool includeFile)
{
    ValueTree root = rootIn.createCopy();
    if (! root.hasType ("BackReverseState")) return;
    // migration hook: formatVersion/schemaVersion 1 is the first public schema; future versions upgrade here.
    const int schema = root.getProperty ("schemaVersion", 1);
    ignoreUnused (schema);
    auto p = root.getChildWithName (apvts.state.getType());
    if (p.isValid()) apvts.replaceState (p);
    auto norm = root.getChildWithName ("NORM");
    for (int i = 0; norm.isValid() && i < brp::kNum; ++i)
        if (norm.hasProperty (brp::kDefs[i].id))
            if (auto* q = param (i)) q->setValueNotifyingHost (jlimit (0.0f, 1.0f, (float) (double) norm[brp::kDefs[i].id]));
    auto l = root.getChildWithName ("LANES");
    if (l.isValid())
    {
#define BR_COPY(k) if (l.hasProperty (lk::k)) lanes.setProperty (lk::k, l[lk::k], nullptr);
        BR_LANE_KEYS (BR_COPY)
        if (includeFile) { BR_UI_KEYS (BR_COPY) }
#undef BR_COPY
    }
    publishPatterns();
    if (includeFile)
    {
        const File f (lanes[lk::filePath].toString());
        if (f.existsAsFile() && f != currentFile)
            MessageManager::callAsync ([this, f] { loadFile (f); });
    }
}

void BackReverseProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size)) applyStateTree (ValueTree::fromXml (*xml), true);
}

// ---------------------------------------------------------------- A/B compare (usability feature)
void BackReverseProcessor::selectAB (int slot)
{
    slot = jlimit (0, 1, slot);
    if (slot == abSlot) return;
    getStateInformation (ab[abSlot]); abValid[abSlot] = true;
    if (abValid[slot])
        if (auto xml = getXmlFromBinary (ab[slot].getData(), (int) ab[slot].getSize())) applyStateTree (ValueTree::fromXml (*xml), false);
    abSlot = slot;
}
void BackReverseProcessor::copyABToOther() { getStateInformation (ab[1 - abSlot]); abValid[1 - abSlot] = true; }

// ---------------------------------------------------------------- clipboard sub-presets
static const std::map<String, std::pair<StringArray, StringArray>>& sections()
{
    static const std::map<String, std::pair<StringArray, StringArray>> s {
        { "chunk", { { "order", "patLen", "patStart", "patLoop", "repeatProb", "skipProb", "maxRepeat", "sizeMode", "variation", "revScope", "revProb" }, { "userPat", "sizes", "laneLen", "chunkFlags" } } },
        { "rate", { { "timeMode", "ratio", "preserve", "quality", "algo", "stretchReset", "tempoAssign", "tapeOn", "tapeMode", "tapeLen", "tapeProb" }, { "rates" } } },
        { "gate", { { "gateOn", "gateSteps", "gateTiming", "gateMs", "gateNote", "gateHz", "gateShape", "gateWidth", "gateDepth", "gapMode" }, { "gates", "curve" } } },
        { "panphase", { { "panMode", "pan", "panDepth", "swap", "panMirror", "panClock", "panClockMs", "panOffset", "panSmooth", "polMode", "phaseMode", "phaseDeg" }, { "panLen", "pan", "polLen", "pol" } } },
        { "fx", { { "stutOn", "stutScope", "stutProb", "stutPeriod", "stutSync", "stutLen", "stutRepeats", "stutDecay", "stutDrift", "stutDir", "stutRetrig", "stutWet", "stutDry",
                    "dlyOn", "dlyScope", "dlyProb", "dlyTimeL", "dlyTimeR", "dlyLink", "dlySync", "dlyFb", "dlyXfb", "dlyLP", "dlyHP", "dlyPing", "dlyModRate", "dlyModDepth", "dlyFreeze", "dlyMix",
                    "echoOn", "echoScope", "echoProb", "echoTime", "echoSync", "echoFb", "echoDecay", "echoTone", "echoSpread", "echoDrift", "echoWow", "echoChar", "echoMix", "chainOrder", "chainMix" }, {} } } };
    return s;
}

String BackReverseProcessor::copySection (const String& section)
{
    auto it = sections().find (section);
    if (it == sections().end()) return {};
    XmlElement x ("BackReverseClip"); x.setAttribute ("type", section); x.setAttribute ("formatVersion", 1);
    for (auto& id : it->second.first) if (auto* p = apvts.getParameter (id)) { auto* e = x.createNewChildElement ("P"); e->setAttribute ("id", id); e->setAttribute ("v", p->convertFrom0to1 (p->getValue())); }
    for (auto& k : it->second.second) { auto* e = x.createNewChildElement ("L"); e->setAttribute ("k", k); e->setAttribute ("v", lanes[Identifier (k)].toString()); }
    const String s = x.toString();
    SystemClipboard::copyTextToClipboard (s);
    return s;
}

bool BackReverseProcessor::pasteSection (const String& text)
{
    auto x = parseXML (text);
    if (x == nullptr || ! x->hasTagName ("BackReverseClip")) return false;
    auto it = sections().find (x->getStringAttribute ("type"));
    if (it == sections().end()) return false;
    undo.beginNewTransaction ("Paste " + it->first);
    for (auto* e : x->getChildIterator())
    {
        if (e->hasTagName ("P") && it->second.first.contains (e->getStringAttribute ("id")))
            if (auto* p = apvts.getParameter (e->getStringAttribute ("id"))) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) e->getDoubleAttribute ("v"))); p->endChangeGesture(); }
        if (e->hasTagName ("L") && it->second.second.contains (e->getStringAttribute ("k")))
            lanes.setProperty (Identifier (e->getStringAttribute ("k")), e->getStringAttribute ("v"), &undo);
    }
    return true;
}

// ---------------------------------------------------------------- randomize (non-destructive: undoable)
void BackReverseProcessor::randomize (bool reroll)
{
    undo.beginNewTransaction ("Randomize");
    const bool locked = (bool) ui (lk::randLock, false);
    if (reroll && ! locked) set (brp::seed, (float) ((int) get (brp::seed) % 99999 + 1));
    const int ex = (int) ui (lk::randEx, 0);
    const float amt = (float) (double) ui (lk::randAmt, 0.6);
    br::Rng rng ((uint64_t) get (brp::seed) * 7777u + 99u);
    auto on = [&] (int bit) { return (ex & (1 << bit)) == 0; };
    auto maybe = [&] { return rng.next() < amt; };
    auto choice = [&] (int id, int n) { if (maybe()) set (id, (float) rng.below (n)); };
    br::Patterns p = pats;
    if (on (0))
    {
        if (maybe()) { set (brp::revMode, 1); choice (brp::order, 11); }
        if (maybe()) set (brp::patLen, (float) (2 << rng.below (3)));
        if (maybe())
        {
            StringArray t; const int len = 4 + rng.below (5);
            for (int i = 0; i < len; ++i) { const double r = rng.next(); t.add (r < 0.12 ? "REST" : r < 0.2 ? "?" : String (rng.below (4))); }
            setUserPattern (t.joinIntoString (","));
        }
    }
    if (on (1) && maybe())
    {
        set (brp::sizeMode, (float) (1 + rng.below (3)));
        const float m[5] = { 0.25f, 0.5f, 1, 2, 4 }; p.sizeCount = 2 + rng.below (4);
        for (int i = 0; i < p.sizeCount; ++i) { p.sizeMult[i] = m[rng.below (5)]; p.sizeWeight[i] = (float) (0.2 + rng.next()); }
    }
    if (on (2) && maybe())
    {
        set (brp::tempoAssign, (float) (2 + rng.below (3)));
        const float r[5] = { 0.25f, 0.5f, 1, 2, 3 }; p.rateLen = 2 + rng.below (7);
        for (int i = 0; i < p.rateLen; ++i) { p.rate[i].ratio = r[rng.below (5)]; p.rate[i].stretch = (uint8_t) rng.below (3); p.rate[i].prob = 1; p.rate[i].weight = (float) (0.2 + rng.next()); }
    }
    if (on (3) && maybe())
    {
        choice (brp::panMode, 6); p.panLen = 2 + rng.below (7);
        for (int i = 0; i < p.panLen; ++i) p.pan[i] = (float) (rng.next() * 2 - 1);
    }
    if (on (4) && maybe())
    {
        choice (brp::polMode, 7); choice (brp::phaseMode, 3); p.polLen = 2 + rng.below (7);
        for (int i = 0; i < p.polLen; ++i) p.pol[i] = (uint8_t) rng.below (4);
    }
    if (on (5) && maybe())
    {
        set (brp::gateOn, 1); choice (brp::gateSteps, 4); choice (brp::gateShape, 8);
        for (auto& g : p.gate) { if (maybe()) g.on = rng.next() > 0.3 ? 1 : 0; if (maybe()) g.dir = (uint8_t) (rng.next() < 0.7 ? 0 : 1 + rng.below (2)); }
    }
    if (on (6) && maybe())
    {
        set (brp::stutOn, rng.next() < 0.5f ? 1.0f : 0.0f); set (brp::dlyOn, rng.next() < 0.5f ? 1.0f : 0.0f); set (brp::echoOn, rng.next() < 0.5f ? 1.0f : 0.0f);
        choice (brp::stutScope, 5); choice (brp::dlyScope, 5); choice (brp::echoScope, 5); choice (brp::chainOrder, 6);
        for (int i = 0; i < br::kLane; ++i) p.chunkFlags[i] = (uint8_t) ((p.chunkFlags[i] & 1) | (rng.below (8) << 1));
        for (auto& g : p.gate) if (maybe()) g.fx = (uint8_t) rng.below (8);
    }
    applyPatterns (p);
}
