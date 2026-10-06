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
    if(mods.isShiftDown())processor.setGateEffect(idx,br::EffectType::Stutter,!processor.gateEffectState(idx,br::EffectType::Stutter));
    else if(mods.isCtrlDown()||mods.isCommandDown())processor.setGateEffect(idx,br::EffectType::Delay,!processor.gateEffectState(idx,br::EffectType::Delay));
    else if(mods.isAltDown())processor.setGateEffect(idx,br::EffectType::Echo,!processor.gateEffectState(idx,br::EffectType::Echo));
    else if(mods.isRightButtonDown())processor.setGateForward(idx,!processor.gateForwardState(idx));
    else processor.setGateEnabled(idx,!processor.gateEnabled(idx));
    repaint();
}
void GateGrid::mouseDown(const juce::MouseEvent& ev){applyAt(ev.getPosition(),ev.mods);}
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
void WaveformView::seek(const juce::MouseEvent& e){processor.seekFile(juce::jlimit(0.0,1.0,(double)e.x/std::max(1,getWidth())));repaint();}
void WaveformView::mouseDown(const juce::MouseEvent& e){seek(e);}
void WaveformView::mouseDrag(const juce::MouseEvent& e){seek(e);}

BackReverseAudioProcessorEditor::BackReverseAudioProcessorEditor(BackReverseAudioProcessor& proc)
:AudioProcessorEditor(&proc),p(proc),gateGrid(proc),gateCurve(proc),waveform(proc){
    setResizable(true,true); setResizeLimits(820,620,1600,1100); setSize(1120,820);
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

    configureSlider(chunk,"Chunk length in seconds. Live reverse latency follows this value.");configureSlider(ratio,"Rate/stretch ratio: 0.25 quarter, 0.5 half, 2 double, 3 triple.");
    configureSlider(pan,"Stereo pan");configureSlider(phase,"Frequency-dependent all-pass phase rotation");configureSlider(dry,"Dry level");configureSlider(wet,"Processed level");
    configureSlider(gateWidth,"Gate active width");configureSlider(gateGap,"Gate gap proportion");
    for(auto* b:{&swap,&hostSync,&stutter,&delay,&echo}) addAndMakeVisible(*b);

    addAndMakeVisible(load);addAndMakeVisible(play);addAndMakeVisible(random);addAndMakeVisible(help);addAndMakeVisible(undo);addAndMakeVisible(redo);
    undo.onClick=[this]{p.undoManager.undo();gateGrid.repaint();gateCurve.repaint();};
    redo.onClick=[this]{p.undoManager.redo();gateGrid.repaint();gateCurve.repaint();};
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
    auto BAadd=[&](const char* id,juce::Button& b){bas.emplace_back(std::make_unique<BA>(s,id,b));};
    BAadd("swapStereo",swap);BAadd("hostSync",hostSync);BAadd("stutterOn",stutter);BAadd("delayOn",delay);BAadd("echoOn",echo);
    auto CAadd=[&](const char* id,juce::ComboBox& b){cas.emplace_back(std::make_unique<CA>(s,id,b));};
    CAadd("reverseMode",reverseMode);CAadd("orderMode",orderMode);CAadd("temporalMode",timeMode);CAadd("polarity",polarity);CAadd("syncDivision",syncDivision);CAadd("gateSteps",gateSteps);CAadd("gateShape",gateShape);CAadd("gapMode",gapMode);CAadd("fxOrder",fxOrder);
    startTimerHz(20);
}

void BackReverseAudioProcessorEditor::configureSlider(juce::Slider& s,const juce::String& tip){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,20);s.setTooltip(tip);addAndMakeVisible(s);}
void BackReverseAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff0b0e13));g.setColour(juce::Colour(0xff141b24));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12),12);
    g.setColour(juce::Colour(0xff3ed6c6));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(12),12,1.5f);
    g.setColour(juce::Colour(0xff8b95a5));g.setFont(12);g.drawText("Gate: click on/off • right=F • Shift=S • Ctrl=Delay • Alt=Echo • waveform drag=scratch",20,getHeight()-30,getWidth()-40,18,juce::Justification::centred);
}
void BackReverseAudioProcessorEditor::resized(){
    auto r=getLocalBounds().reduced(22);auto top=r.removeFromTop(48);title.setBounds(top.removeFromLeft(230));status.setBounds(top.removeFromLeft(290));latencyLabel.setBounds(top.removeFromLeft(250));
    auto actions=r.removeFromTop(34);load.setBounds(actions.removeFromLeft(105));play.setBounds(actions.removeFromLeft(75));random.setBounds(actions.removeFromLeft(105));undo.setBounds(actions.removeFromLeft(65));redo.setBounds(actions.removeFromLeft(65));help.setBounds(actions.removeFromLeft(70));hostSync.setBounds(actions.removeFromLeft(100));syncDivision.setBounds(actions.removeFromLeft(100));
    r.removeFromTop(8);waveform.setBounds(r.removeFromTop(130));
    r.removeFromTop(5);patternText.setBounds(r.removeFromTop(30));
    auto combos=r.removeFromTop(38);reverseMode.setBounds(combos.removeFromLeft(145).reduced(3));orderMode.setBounds(combos.removeFromLeft(145).reduced(3));timeMode.setBounds(combos.removeFromLeft(130).reduced(3));polarity.setBounds(combos.removeFromLeft(120).reduced(3));fxOrder.setBounds(combos.reduced(3));
    auto knobs=r.removeFromTop(138);const int kw=knobs.getWidth()/8;for(auto* s:{&chunk,&ratio,&pan,&phase,&dry,&wet,&gateWidth,&gateGap})s->setBounds(knobs.removeFromLeft(kw).reduced(4));
    auto gateCtl=r.removeFromTop(34);gateSteps.setBounds(gateCtl.removeFromLeft(90).reduced(2));gateShape.setBounds(gateCtl.removeFromLeft(130).reduced(2));gapMode.setBounds(gateCtl.removeFromLeft(115).reduced(2));swap.setBounds(gateCtl.removeFromLeft(105));stutter.setBounds(gateCtl.removeFromLeft(85));delay.setBounds(gateCtl.removeFromLeft(75));echo.setBounds(gateCtl.removeFromLeft(75));
    r.removeFromTop(6);auto gateArea=r.removeFromTop(190);gateGrid.setBounds(gateArea.removeFromTop(135));gateCurve.setBounds(gateArea.reduced(0,4));
}
void BackReverseAudioProcessorEditor::timerCallback(){
    const double sec=*p.state.getRawParameterValue("chunkSeconds");
    latencyLabel.setText("buffer "+juce::String(sec,3)+" s / "+juce::String((int)std::round(sec*p.getSampleRate()))+" samples",juce::dontSendNotification);
    waveform.repaint();gateGrid.repaint();gateCurve.repaint();
}
