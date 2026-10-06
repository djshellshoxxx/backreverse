#include "PluginEditor.h"
#include <chrono>

void GateGrid::paint(juce::Graphics& g){
    const int n=processor.activeGateCount();
    const int cols=std::min(16,n), rows=(n+cols-1)/cols;
    auto r=getLocalBounds().toFloat(); const float cw=r.getWidth()/cols,ch=r.getHeight()/rows;
    for(int i=0;i<n;++i){
        const int x=i%cols,y=i/cols;juce::Rectangle<float> cell(r.getX()+x*cw,r.getY()+y*ch,cw-2,ch-2);
        g.setColour(processor.gateEnabled(i)?juce::Colour(0xff3ed6c6):juce::Colour(0xff29313d));g.fillRoundedRectangle(cell,3);
        g.setFont(8.0f);float bx=cell.getX()+2;
        auto badge=[&](const char* s,bool on,juce::Colour colour){if(!on)return;g.setColour(colour);g.drawText(s,(int)bx,(int)cell.getY()+1,8,9,juce::Justification::centred);bx+=8;};
        badge("F",processor.gateForwardState(i),juce::Colour(0xffffc857));
        badge("S",processor.gateEffectState(i,br::EffectType::Stutter),juce::Colour(0xffff7a8a));
        badge("D",processor.gateEffectState(i,br::EffectType::Delay),juce::Colour(0xff8ab4ff));
        badge("E",processor.gateEffectState(i,br::EffectType::Echo),juce::Colour(0xffc49aff));
        g.setColour(juce::Colour(0xff0b0e13));g.setFont(10);g.drawText(juce::String(i+1),cell,juce::Justification::centred);
    }
}
void GateGrid::applyAt(juce::Point<int> p,const juce::ModifierKeys& mods){
    const int n=processor.activeGateCount();const int cols=std::min(16,n),rows=(n+cols-1)/cols;
    int col=juce::jlimit(0,cols-1,p.x*cols/std::max(1,getWidth()));int row=juce::jlimit(0,rows-1,p.y*rows/std::max(1,getHeight()));int idx=row*cols+col;if(idx>=n)return;
    if(idx==lastPaintedCell)return;
    lastPaintedCell=idx;
    if(mods.isShiftDown())processor.setGateEffect(idx,br::EffectType::Stutter,!processor.gateEffectState(idx,br::EffectType::Stutter));
    else if(mods.isCtrlDown()||mods.isCommandDown())processor.setGateEffect(idx,br::EffectType::Delay,!processor.gateEffectState(idx,br::EffectType::Delay));
    else if(mods.isAltDown())processor.setGateEffect(idx,br::EffectType::Echo,!processor.gateEffectState(idx,br::EffectType::Echo));
    else if(mods.isRightButtonDown())processor.setGateForward(idx,!processor.gateForwardState(idx));
    else processor.setGateEnabled(idx,!processor.gateEnabled(idx));
    repaint();
}
void GateGrid::mouseDown(const juce::MouseEvent& ev){lastPaintedCell=-1;applyAt(ev.getPosition(),ev.mods);}
void GateGrid::mouseDrag(const juce::MouseEvent& ev){applyAt(ev.getPosition(),ev.mods);}

void GateCurveEditor::paint(juce::Graphics& g){
    auto r=getLocalBounds().toFloat().reduced(4);g.setColour(juce::Colour(0xff080b10));g.fillRoundedRectangle(r,4);
    juce::Path p;for(int i=0;i<8;++i){float x=r.getX()+r.getWidth()*i/7.0f;float y=r.getBottom()-r.getHeight()*processor.gateCurvePoint(i);if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);}
    g.setColour(juce::Colour(0xffffc857));g.strokePath(p,juce::PathStrokeType(2));
    for(int i=0;i<8;++i){float x=r.getX()+r.getWidth()*i/7.0f;float y=r.getBottom()-r.getHeight()*processor.gateCurvePoint(i);g.fillEllipse(x-3,y-3,6,6);}
}
void GateCurveEditor::edit(const juce::MouseEvent& ev){
    auto r=getLocalBounds().toFloat().reduced(4);int i=juce::jlimit(0,7,(int)std::lround((ev.position.x-r.getX())/std::max(1.0f,r.getWidth())*7.0f));
    float v=1.0f-(ev.position.y-r.getY())/std::max(1.0f,r.getHeight());processor.setGateCurvePoint(i,juce::jlimit(0.0f,1.0f,v));repaint();
}
void GateCurveEditor::mouseDown(const juce::MouseEvent& ev){edit(ev);}
void GateCurveEditor::mouseDrag(const juce::MouseEvent& ev){edit(ev);}

void WaveformView::paint(juce::Graphics& g){
    auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff080b10));g.fillRoundedRectangle(r,5);
    auto peaks=processor.getWaveformPeaks(); if(!peaks.empty()){
        g.setColour(juce::Colour(0xff3ed6c6));juce::Path path;const float mid=r.getCentreY(),amp=r.getHeight()*0.44f;
        for(std::size_t i=0;i<peaks.size();++i){float x=r.getX()+r.getWidth()*(float)i/(float)std::max<std::size_t>(1,peaks.size()-1);float y=mid-peaks[i]*amp;if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
        for(std::size_t ii=peaks.size();ii-->0;){float x=r.getX()+r.getWidth()*(float)ii/(float)std::max<std::size_t>(1,peaks.size()-1);path.lineTo(x,mid+peaks[ii]*amp);}path.closeSubPath();g.fillPath(path);
    }
    const float px=r.getX()+r.getWidth()*(float)processor.filePlayheadNormalized();g.setColour(juce::Colour(0xffffc857));g.drawLine(px,r.getY(),px,r.getBottom(),2);
    const double len=processor.fileLengthSeconds(); const double chunk=*processor.state.getRawParameterValue("chunkSeconds");
    if(len>0&&chunk>0){g.setColour(juce::Colour(0x558b95a5));for(double t=chunk;t<len;t+=chunk){float x=r.getX()+r.getWidth()*(float)(t/len);g.drawVerticalLine((int)x,r.getY(),r.getBottom());}}
}
void WaveformView::seek(const juce::MouseEvent& ev,bool start){
    const double now=juce::Time::getMillisecondCounterHiRes();const double norm=juce::jlimit(0.0,1.0,(double)ev.x/std::max(1,getWidth()));
    if(start){lastNorm=norm;lastMs=now;processor.beginScratch(norm);}
    else{const double dt=std::max(1.0,now-lastMs)/1000.0;const double vel=(norm-lastNorm)/dt;processor.updateScratch(norm,vel);lastNorm=norm;lastMs=now;}
    repaint();
}
void WaveformView::mouseDown(const juce::MouseEvent& ev){seek(ev,true);}
void WaveformView::mouseDrag(const juce::MouseEvent& ev){seek(ev,false);}
void WaveformView::mouseUp(const juce::MouseEvent&){processor.endScratch();}

BackReverseAudioProcessorEditor::BackReverseAudioProcessorEditor(BackReverseAudioProcessor& proc)
:AudioProcessorEditor(&proc),p(proc),gateGrid(proc),gateCurve(proc),waveform(proc){
    setResizable(true,true); setResizeLimits(900,850,1700,1250); setSize(1180,1020);
    title.setText("BACKREVERSE",juce::dontSendNotification); title.setFont(juce::Font(28.0f,juce::Font::bold)); addAndMakeVisible(title);
    status.setText("reverse / cut / stretch / scratch",juce::dontSendNotification); addAndMakeVisible(status); addAndMakeVisible(latencyLabel);
    addAndMakeVisible(gateGrid);addAndMakeVisible(gateCurve);addAndMakeVisible(waveform);
    gateCurve.setTooltip("Custom gate envelope: drag the eight control points. Select Custom gate shape to use it.");
    gateGrid.setTooltip("Gate grid: click on/off, right-click forward, Shift=Stutter, Ctrl/Cmd=Delay, Alt=Echo.");

    auto addCombo=[this](juce::ComboBox& b,std::initializer_list<const char*> xs){int i=1;for(auto* x:xs)b.addItem(x,i++);addAndMakeVisible(b);};
    addCombo(reverseMode,{"Sequential","Whole Source","Reordered","Free Scrub","Hybrid"});
    addCombo(orderMode,{"Sequential","Reverse Order","Random","Shuffle","Ping Pong","Odds/Evens","Evens/Odds","Rotate Left","Rotate Right","User"});
    addCombo(timeMode,{"Rate","Time Stretch"}); addCombo(polarity,{"Normal","Invert L","Invert R","Invert Both"});
    addCombo(syncDivision,{"1/128","1/64","1/32","1/16T","1/16","1/8T","1/8","1/8.","1/4T","1/4","1/4.","1/2","1 bar","2 bars","4 bars","8 bars","16 bars","32 bars"});
    addCombo(gateSteps,{"2","4","8","16","32","64"});addCombo(gateShape,{"Hard","Linear In","Linear Out","Triangle","Equal Power","Sine","Exponential","Logarithmic","Custom"});
    addCombo(gapMode,{"Silence","Dry Through","Hold","Crossfade","FX Tail"});
    addCombo(fxOrder,{"Stutter > Delay > Echo","Stutter > Echo > Delay","Delay > Stutter > Echo","Delay > Echo > Stutter","Echo > Stutter > Delay","Echo > Delay > Stutter"});
    addCombo(preset,{"Basic 5s Reverse","Whole Track Reverse","Half Speed Reverse","Quarter Stretch Surreal","Triple Speed Fragments","Random Cut Up","Forward Gate Breaks","Stereo Mirror","Polarity Flicker","Stutter Delay Echo","Echo Into Stutter","Vinyl Slow Drag","Extreme Stretch"});
    addCombo(chunkUnit,{"seconds","milliseconds","samples"});chunkUnit.setSelectedId(1,juce::dontSendNotification);
    addCombo(scratchMode,{"Linear","Vinyl","Tape Shuttle","Fine"});addCombo(scratchRelease,{"Latch","Spring Return","Continue"});
    preset.onChange=[this]{if(preset.getSelectedId()>0)p.loadFactoryPreset(preset.getSelectedId()-1);};

    configureSlider(chunk,"Chunk length. Live buffer capacity is reserved when audio starts from this value; longer selections are capped at the prepared limit. Restart audio after changing the value to reserve a larger buffer.");
    chunk.setTextBoxStyle(juce::Slider::TextBoxBelow,false,104,20);
    chunk.textFromValueFunction=[this](double seconds){int unit=chunkUnit.getSelectedId();if(unit==2)return juce::String(seconds*1000.0,2)+" ms";if(unit==3)return juce::String((juce::int64)std::llround(seconds*std::max(1.0,p.getSampleRate())))+" smp";return juce::String(seconds,4)+" s";};
    chunk.valueFromTextFunction=[this](const juce::String& text){double v=text.getDoubleValue();int unit=chunkUnit.getSelectedId();if(unit==2)return v/1000.0;if(unit==3)return v/std::max(1.0,p.getSampleRate());return v;};
    chunkUnit.onChange=[this]{chunk.updateText();p.state.state.setProperty("chunkDisplayUnit",chunkUnit.getSelectedId(),&p.undoManager);};
    chunkUnit.setSelectedId((int)p.state.state.getProperty("chunkDisplayUnit",1),juce::dontSendNotification);configureSlider(ratio,"Rate/stretch ratio: 0.25 quarter, 0.5 half, 2 double, 3 triple.");
    configureSlider(pan,"Stereo pan");configureSlider(phase,"Frequency-dependent all-pass phase rotation");configureSlider(dry,"Dry level");configureSlider(wet,"Processed level");
    configureSlider(gateWidth,"Gate active width");configureSlider(gateGap,"Gate gap proportion");
    configureSlider(stutterMs,"Stutter period in milliseconds");configureSlider(stutterWet,"Stutter wet level");configureSlider(stutterDry,"Stutter dry level");configureSlider(stutterDecay,"Stutter repeat decay");configureSlider(stutterRepeats,"Stutter repeat count");
    configureSlider(delayMs,"Delay time in milliseconds");configureSlider(delayWet,"Delay wet level");configureSlider(delayDry,"Delay dry level");configureSlider(delayFeedback,"Delay feedback amount");configureSlider(delayLowpass,"Delay feedback low-pass cutoff");configureSlider(delayHighpass,"Delay feedback high-pass cutoff");
    configureSlider(scratchInertia,"Scratch Inertia");configureSlider(scratchFriction,"Scratch Friction");configureSlider(scratchMaxRate,"Scratch Max Rate");
    configureSlider(echoMs,"Echo time in milliseconds");configureSlider(echoWet,"Echo wet level");configureSlider(echoDry,"Echo dry level");configureSlider(echoFeedback,"Echo feedback amount");configureSlider(echoDamping,"Echo damping");configureSlider(echoSpread,"Echo stereo spread");configureSlider(echoDrift,"Echo timing drift");configureSlider(echoWow,"Echo wow and flutter");
    for(auto* b:{&swap,&hostSync,&stutter,&delay,&echo,&stutterAlternate,&delayPingPong,&scratchReverseOnly}) addAndMakeVisible(*b);
    for(auto* b:{&quarter,&half,&normal,&dbl,&triple})addAndMakeVisible(*b);
    auto rate=[this](float v){if(auto* q=p.state.getParameter("ratio"))q->setValueNotifyingHost(q->convertTo0to1(v));};
    quarter.onClick=[rate]{rate(0.25f);};half.onClick=[rate]{rate(0.5f);};normal.onClick=[rate]{rate(1.0f);};dbl.onClick=[rate]{rate(2.0f);};triple.onClick=[rate]{rate(3.0f);};

    addAndMakeVisible(load);addAndMakeVisible(play);addAndMakeVisible(random);addAndMakeVisible(help);addAndMakeVisible(undo);addAndMakeVisible(redo);
    undo.onClick=[this]{p.undoManager.undo();p.refreshGateCurveCache();gateGrid.repaint();gateCurve.repaint();};
    redo.onClick=[this]{p.undoManager.redo();p.refreshGateCurveCache();gateGrid.repaint();gateCurve.repaint();};
    patternText.setTextToShowWhenEmpty("User pattern: 0,+2,REST,1*2,4@50",juce::Colour(0xff687384));
    patternText.setText(p.userPatternText(),false);
    patternText.setTooltip("Advanced chunk pattern: absolute/relative references, REST, *repeat and @probability");
    patternText.onTextChange=[this]{p.setUserPatternText(patternText.getText());};
    addAndMakeVisible(patternText);
    load.onClick=[this]{auto chooser=std::make_shared<juce::FileChooser>("Load audio",juce::File{},"*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,chooser](const juce::FileChooser& fc){auto f=fc.getResult();if(f.existsAsFile())p.loadAudioFile(f);});};
    play.onClick=[this]{p.setFilePlaying(!p.isFilePlaying());play.setButtonText(p.isFilePlaying()?"Pause":"Play");};
    random.onClick=[this]{auto s=(std::uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();p.randomize(s);gateGrid.repaint();};
    help.onClick=[]{
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"BackReverse Help",
        "LIVE REVERSE\nThe chunk must be captured before it can play backwards, so chunk length is the fundamental live latency.\n\nRATE / STRETCH\nRate changes pitch like vinyl or tape. Time Stretch changes duration while retaining local pitch.\n\nGATES\nLeft-click a cell to enable/disable it. Right-click adds a forward slice inside a reversed chunk.\n\nWAVEFORM / SCRATCH\nDrag the waveform playhead to seek and scrub a loaded source.\n\nPHASE\nPolarity inversion and phase rotation are separate controls.\n\nFX\nStutter, Delay and Echo can be chained in six orders.");
    };

    auto& s=p.state;
    auto SAadd=[&](const char* id,juce::Slider& sl){sas.emplace_back(std::make_unique<SA>(s,id,sl));};
    SAadd("chunkSeconds",chunk);SAadd("ratio",ratio);SAadd("pan",pan);SAadd("phase",phase);SAadd("dry",dry);SAadd("wet",wet);SAadd("gateWidth",gateWidth);SAadd("gateGap",gateGap);
    SAadd("stutterMs",stutterMs);SAadd("stutterWet",stutterWet);SAadd("stutterDry",stutterDry);SAadd("stutterDecay",stutterDecay);SAadd("stutterRepeats",stutterRepeats);
    SAadd("delayMs",delayMs);SAadd("delayWet",delayWet);SAadd("delayDry",delayDry);SAadd("delayFeedback",delayFeedback);SAadd("delayLowpass",delayLowpass);SAadd("delayHighpass",delayHighpass);
    SAadd("scratchInertia",scratchInertia);SAadd("scratchFriction",scratchFriction);SAadd("scratchMaxRate",scratchMaxRate);
    SAadd("echoMs",echoMs);SAadd("echoWet",echoWet);SAadd("echoDry",echoDry);SAadd("echoFeedback",echoFeedback);SAadd("echoDamping",echoDamping);SAadd("echoSpread",echoSpread);SAadd("echoDrift",echoDrift);SAadd("echoWow",echoWow);
    auto BAadd=[&](const char* id,juce::Button& b){bas.emplace_back(std::make_unique<BA>(s,id,b));};
    BAadd("swapStereo",swap);BAadd("hostSync",hostSync);BAadd("stutterOn",stutter);BAadd("delayOn",delay);BAadd("echoOn",echo);BAadd("stutterAlternate",stutterAlternate);BAadd("delayPingPong",delayPingPong);BAadd("scratchReverseOnly",scratchReverseOnly);
    auto CAadd=[&](const char* id,juce::ComboBox& b){cas.emplace_back(std::make_unique<CA>(s,id,b));};
    CAadd("reverseMode",reverseMode);CAadd("orderMode",orderMode);CAadd("temporalMode",timeMode);CAadd("polarity",polarity);CAadd("syncDivision",syncDivision);CAadd("gateSteps",gateSteps);CAadd("gateShape",gateShape);CAadd("gapMode",gapMode);CAadd("fxOrder",fxOrder);CAadd("scratchMode",scratchMode);CAadd("scratchRelease",scratchRelease);
    startTimerHz(20);
}

void BackReverseAudioProcessorEditor::configureSlider(juce::Slider& s,const juce::String& tip){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,64,18);s.setTooltip(tip);addAndMakeVisible(s);}
void BackReverseAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff0b0e13));g.setColour(juce::Colour(0xff141b24));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12),12);
    g.setColour(juce::Colour(0xff3ed6c6));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(12),12,1.5f);
    g.setColour(juce::Colour(0xff8b95a5));g.setFont(11);
    auto label=[&](juce::Slider& s,const char* txt){auto b=s.getBounds();g.drawFittedText(txt,b.getX(),b.getY()-13,b.getWidth(),13,juce::Justification::centred,1);};
    label(stutterMs,"St ms");label(stutterWet,"St W");label(stutterDry,"St D");label(stutterDecay,"St Dec");label(stutterRepeats,"St Rep");
    label(delayMs,"Dl ms");label(delayWet,"Dl W");label(delayDry,"Dl D");label(delayFeedback,"Dl Fbk");label(delayLowpass,"Dl LP");label(delayHighpass,"Dl HP");
    label(scratchInertia,"Sc In");label(scratchFriction,"Sc Fr");label(scratchMaxRate,"Sc Max");
    label(echoMs,"Ec ms");label(echoWet,"Ec W");label(echoDry,"Ec D");label(echoFeedback,"Ec Fbk");label(echoDamping,"Ec Damp");label(echoSpread,"Ec Spr");label(echoDrift,"Ec Drift");label(echoWow,"Ec Wow");
    g.drawText("Gate: click on/off • right=F • Shift=S • Ctrl=Delay • Alt=Echo • waveform drag=scratch",20,getHeight()-25,getWidth()-40,16,juce::Justification::centred);
}
void BackReverseAudioProcessorEditor::resized(){
    auto r=getLocalBounds().reduced(22);auto top=r.removeFromTop(48);title.setBounds(top.removeFromLeft(230));status.setBounds(top.removeFromLeft(290));latencyLabel.setBounds(top.removeFromLeft(250));
    auto actions=r.removeFromTop(34);load.setBounds(actions.removeFromLeft(105));play.setBounds(actions.removeFromLeft(75));preset.setBounds(actions.removeFromLeft(165).reduced(2));random.setBounds(actions.removeFromLeft(100));undo.setBounds(actions.removeFromLeft(60));redo.setBounds(actions.removeFromLeft(60));help.setBounds(actions.removeFromLeft(65));hostSync.setBounds(actions.removeFromLeft(95));syncDivision.setBounds(actions.removeFromLeft(90));chunkUnit.setBounds(actions.removeFromLeft(100));
    r.removeFromTop(8);waveform.setBounds(r.removeFromTop(130));
    r.removeFromTop(5);patternText.setBounds(r.removeFromTop(30));
    auto rates=r.removeFromTop(31);for(auto* b:{&quarter,&half,&normal,&dbl,&triple})b->setBounds(rates.removeFromLeft(58).reduced(2));
    auto combos=r.removeFromTop(38);reverseMode.setBounds(combos.removeFromLeft(145).reduced(3));orderMode.setBounds(combos.removeFromLeft(145).reduced(3));timeMode.setBounds(combos.removeFromLeft(130).reduced(3));polarity.setBounds(combos.removeFromLeft(120).reduced(3));fxOrder.setBounds(combos.reduced(3));
    auto knobs=r.removeFromTop(138);const int kw=knobs.getWidth()/8;for(auto* s:{&chunk,&ratio,&pan,&phase,&dry,&wet,&gateWidth,&gateGap})s->setBounds(knobs.removeFromLeft(kw).reduced(4));
    auto gateCtl=r.removeFromTop(34);gateSteps.setBounds(gateCtl.removeFromLeft(90).reduced(2));gateShape.setBounds(gateCtl.removeFromLeft(130).reduced(2));gapMode.setBounds(gateCtl.removeFromLeft(115).reduced(2));swap.setBounds(gateCtl.removeFromLeft(105));stutter.setBounds(gateCtl.removeFromLeft(85));delay.setBounds(gateCtl.removeFromLeft(75));echo.setBounds(gateCtl.removeFromLeft(75));
    r.removeFromTop(6);
    auto fx1=r.removeFromTop(95);const int fxw=fx1.getWidth()/11;
    for(auto* s:{&stutterMs,&stutterWet,&stutterDry,&stutterDecay,&stutterRepeats,&delayMs,&delayWet,&delayDry,&delayFeedback,&delayLowpass,&delayHighpass})s->setBounds(fx1.removeFromLeft(fxw).reduced(2,8));
    auto scratchRow=r.removeFromTop(75);scratchMode.setBounds(scratchRow.removeFromLeft(130).reduced(2));scratchRelease.setBounds(scratchRow.removeFromLeft(130).reduced(2));scratchReverseOnly.setBounds(scratchRow.removeFromLeft(85));
    const int scw=scratchRow.getWidth()/3;for(auto* s:{&scratchInertia,&scratchFriction,&scratchMaxRate})s->setBounds(scratchRow.removeFromLeft(scw).reduced(3,4));
    auto fx2=r.removeFromTop(95);const int exw=fx2.getWidth()/10;
    for(auto* s:{&echoMs,&echoWet,&echoDry,&echoFeedback,&echoDamping,&echoSpread,&echoDrift,&echoWow})s->setBounds(fx2.removeFromLeft(exw).reduced(2,8));
    stutterAlternate.setBounds(fx2.removeFromLeft(90));delayPingPong.setBounds(fx2.removeFromLeft(90));
    auto gateArea=r.removeFromTop(190);gateGrid.setBounds(gateArea.removeFromTop(135));gateCurve.setBounds(gateArea.reduced(0,4));
}
void BackReverseAudioProcessorEditor::timerCallback(){
    const double sec=*p.state.getRawParameterValue("chunkSeconds");
    const double sr=std::max(1.0,p.getSampleRate());
    const auto limit=p.preparedBufferFrames();
    const double active=std::min(sec,static_cast<double>(limit)/sr);
    const bool capped=sec>active+0.0005;
    latencyLabel.setText("buffer "+juce::String(active,3)+" s / "+juce::String((int)std::round(active*sr))+" samples"+(capped?" (prepared limit)":""),juce::dontSendNotification);
    waveform.repaint();gateGrid.repaint();gateCurve.repaint();
}
