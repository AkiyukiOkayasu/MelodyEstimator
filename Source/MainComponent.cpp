#include "MainComponent.h"

MainContentComponent::MainContentComponent()
{
    setMacMainMenu(this);
    setLookAndFeel(&lookAndFeel);
    
    //GUI Noise gate
    addAndMakeVisible(sl_noiseGateThreshold);
    sl_noiseGateThreshold.setRange(-72.0, 0.0, 1.0);
    sl_noiseGateThreshold.setTextValueSuffix("dB");
    sl_noiseGateThreshold.setSliderStyle (Slider::LinearBar);
    sl_noiseGateThreshold.setTextBoxStyle (Slider::TextBoxLeft, false, 80, sl_noiseGateThreshold.getTextBoxHeight());
    sl_noiseGateThreshold.addListener(this);
    addAndMakeVisible(lbl_noiseGate);
    lbl_noiseGate.setText("Noise Gate", dontSendNotification);
    lbl_noiseGate.setFont(Font (Font::getDefaultMonospacedFontName(), 14.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_noiseGate.setJustificationType (Justification::centredLeft);
    lbl_noiseGate.setEditable (false, false, false);
    lbl_noiseGate.attachToComponent(&sl_noiseGateThreshold, false);
    
    //GUI Highpass filter
    addAndMakeVisible(sl_hpf);
    sl_hpf.setRange(20.0, 120.0, 5.0);
    sl_hpf.setTextValueSuffix("Hz");
    sl_hpf.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    sl_hpf.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 15);
    sl_hpf.addListener(this);
    addAndMakeVisible(lbl_hpf);
    lbl_hpf.setText("HPF", dontSendNotification);
    lbl_hpf.setFont (Font (Font::getDefaultMonospacedFontName(), 14.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_hpf.setJustificationType (Justification::centredLeft);
    lbl_hpf.setEditable (false, false, false);
    lbl_hpf.attachToComponent(&sl_hpf, true);
    addAndMakeVisible(tgl_hpf);
    tgl_hpf.addListener(this);
    
    //GUI Lowpass filter
    addAndMakeVisible(sl_lpf);
    sl_lpf.setRange(3200.0, 20000.0, 100.0);
    sl_lpf.setTextValueSuffix("Hz");
    sl_lpf.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    sl_lpf.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 15);
    sl_lpf.addListener(this);
    addAndMakeVisible(lbl_lpf);
    lbl_lpf.setText("LPF", dontSendNotification);
    lbl_lpf.setFont (Font (Font::getDefaultMonospacedFontName(), 14.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_lpf.setJustificationType (Justification::centredLeft);
    lbl_lpf.setEditable (false, false, false);
    lbl_lpf.attachToComponent(&sl_lpf, true);
    addAndMakeVisible(tgl_lpf);
    tgl_lpf.addListener(this);
    
    addAndMakeVisible (lbl_appName);
    lbl_appName.setText("Melody Estimator", dontSendNotification);
    lbl_appName.setFont (Font (Font::getDefaultMonospacedFontName(), 21.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_appName.setJustificationType (Justification::centredLeft);
    lbl_appName.setEditable (false, false, false);
    addAndMakeVisible (lbl_version);
    std::string version = "ver" + std::string(ProjectInfo::versionString);
    lbl_version.setText(version, dontSendNotification);
    lbl_version.setFont (Font (Font::getDefaultMonospacedFontName(), 14.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_version.setJustificationType (Justification::centredLeft);
    lbl_version.setEditable (false, false, false);
    
    //Essentia
    preApplyEssentia.buffer.setSize(1, lengthToEstimateMelody_sample);
    essentia::init();
    essentia::standard::AlgorithmFactory& factory = essentia::standard::AlgorithmFactory::instance();
    const float minFreq = MidiMessage::getMidiNoteInHertz(64 - (12 * overSampleFactor), 440.0) * 0.98;
    const float maxFreq = MidiMessage::getMidiNoteInHertz(95 - (12 * overSampleFactor), 440.0);
    melodyEstimate = factory.create("PredominantPitchMelodia", "minFrequency", minFreq, "maxFrequency", maxFreq, "voicingTolerance", -0.9f);//voicingToleranceパラメータは要調整 [-1.0~1.4] default:0.2(反応のしやすさ的なパラメータ)
    pitchfilter = factory.create("PitchFilter", "confidenceThreshold", 70, "minChunkSize", 35);
    
    essentiaInput.reserve(lengthToEstimateMelody_sample);
    essentiaInput.resize(lengthToEstimateMelody_sample, 0.0f);
    essentiaPitch.reserve(200);
    essentiaPitchConfidence.reserve(200);
    essentiaFreq.reserve(200);
    melodyEstimate->input("signal").set(essentiaInput);
    melodyEstimate->output("pitch").set(essentiaPitch);
    melodyEstimate->output("pitchConfidence").set(essentiaPitchConfidence);
    pitchfilter->input("pitch").set(essentiaPitch);
    pitchfilter->input("pitchConfidence").set(essentiaPitchConfidence);
    pitchfilter->output("pitchFiltered").set(essentiaFreq);
    
    if(!oscSender.connect(ip, port)) std::cout<<"OSC connection Error..."<<std::endl;
    midiOut = MidiOutput::createNewDevice(midiPortName);
    if(midiOut != nullptr) {
        std::cout<<"MIDI port: "<<midiOut->getName()<<std::endl;
        midiOut->startBackgroundThread();
    }
    
    setSize (420, 190);
    
    //保存したパラメータをXMLファイルから呼び出し
    PropertiesFile::Options options;
    options.applicationName     = ProjectInfo::projectName;
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Preferences";
    appProperties = new ApplicationProperties();
    appProperties->setStorageParameters (options);
    auto userSettings = appProperties->getUserSettings();
    savedAudioState = std::unique_ptr<XmlElement>(userSettings->getXmlValue (XMLKEYAUDIOSETTINGS));//オーディオIOの設定
    auto* xml_NoiseGate = userSettings->containsKey(XMLKEYNOISEGATE) ? userSettings->getXmlValue(XMLKEYNOISEGATE) : new XmlElement(XMLKEYNOISEGATE);
    noiseGateSettings = std::unique_ptr<XmlElement>(xml_NoiseGate);//ノイズゲートの設定
    auto* xml_highpass = userSettings->containsKey(XMLKEYHIGHPASS) ? userSettings->getXmlValue(XMLKEYHIGHPASS) : new XmlElement(XMLKEYHIGHPASS);
    highpassSettings = std::unique_ptr<XmlElement>(xml_highpass);//ハイパスの設定
    auto* xml_lowpass = userSettings->containsKey(XMLKEYLOWPASS) ? userSettings->getXmlValue(XMLKEYLOWPASS) : new XmlElement(XMLKEYLOWPASS);
    lowpassSettings = std::unique_ptr<XmlElement>(xml_lowpass);//ローパスの設定
    const double thrsld = noiseGateSettings != nullptr ? noiseGateSettings->getDoubleAttribute("threshold") : -24.0;
    const double hpfFreq = highpassSettings != nullptr ? highpassSettings->getDoubleAttribute("freq") : 20.0;
    const bool hpfEnable = highpassSettings != nullptr ? highpassSettings->getBoolAttribute("enable") : false;
    const double lpfFreq = lowpassSettings != nullptr ? lowpassSettings->getDoubleAttribute("freq") : 20000.0;
    const bool lpfEnable = lowpassSettings != nullptr ? lowpassSettings->getBoolAttribute("enable") : false;
    sl_hpf.setEnabled(hpfEnable);
    sl_hpf.setValue(hpfFreq, dontSendNotification);
    tgl_hpf.setToggleState(hpfEnable, dontSendNotification);
    sl_lpf.setEnabled(lpfEnable);
    sl_lpf.setValue(lpfFreq, dontSendNotification);
    tgl_lpf.setToggleState(lpfEnable, dontSendNotification);
    sl_noiseGateThreshold.setValue(thrsld, dontSendNotification);
    
    setAudioChannels (1, 0, savedAudioState.get());
    startTimerHz(30);
}

MainContentComponent::~MainContentComponent()
{
    setLookAndFeel(nullptr);
    setMacMainMenu(nullptr);
    midiOut->clearAllPendingMessages();
    midiOut->stopBackgroundThread();
    shutdownAudio();
    essentia::shutdown();
}

//==============================================================================
void MainContentComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    std::cout<<"Sample Rate: "<<sampleRate<<std::endl;
    std::cout<<"Buffer Size: "<<samplesPerBlockExpected<<std::endl;
    oversampling = new dsp::Oversampling<float>(1, overSampleFactor, dsp::Oversampling<float>::FilterType::filterHalfBandFIREquiripple, false);
    auto channels = static_cast<uint32>(1);//モノラル入力のみ対応
    dsp::ProcessSpec spec {sampleRate, static_cast<uint32>(samplesPerBlockExpected), channels};
    highpass.prepare(spec);
    lowpass.prepare(spec);
    updateHighpassCoefficient(sl_hpf.getValue(), sampleRate);
    updateLowpassCoefficient(sl_lpf.getValue(), sampleRate);
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlockExpected));
    oversampling->reset();
}

void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    dsp::AudioBlock<float> block(*bufferToFill.buffer);
    dsp::ProcessContextReplacing<float> context(block);
    if(tgl_hpf.getToggleState()) highpass.process(context);
    if(tgl_lpf.getToggleState()) lowpass.process(context);
    dsp::AudioBlock<float> overSampledBlock;
    overSampledBlock = oversampling->processSamplesUp(context.getInputBlock());
    
    if(preApplyEssentia.index + overSampledBlock.getNumSamples() < lengthToEstimateMelody_sample)
    {
        preApplyEssentia.buffer.copyFrom(0, preApplyEssentia.index, overSampledBlock.getChannelPointer(0), (int)overSampledBlock.getNumSamples());
        preApplyEssentia.index += overSampledBlock.getNumSamples();
    }
    else
    {
        const int remains = lengthToEstimateMelody_sample - preApplyEssentia.index;
        preApplyEssentia.buffer.copyFrom(0, preApplyEssentia.index, overSampledBlock.getChannelPointer(0), remains);
        RMSlevel_dB = Decibels::gainToDecibels(preApplyEssentia.buffer.getRMSLevel(0, 0, preApplyEssentia.buffer.getNumSamples()));
        if(RMSlevel_dB > sl_noiseGateThreshold.getValue())
        {
            const float* preBuffer = preApplyEssentia.buffer.getReadPointer(0);
            for(int i = 0; i < lengthToEstimateMelody_sample; ++i) {
                essentiaInput.emplace_back(preBuffer[i]);
            }
            estimateMelody();
            essentiaInput.clear();
        }
        
        preApplyEssentia.index = 0;
        const int n = (int)overSampledBlock.getNumSamples() - remains;
        if(n > 0)
        {
            dsp::AudioBlock<float> subBlock = overSampledBlock.getSubBlock(remains);
            preApplyEssentia.buffer.copyFrom(0, preApplyEssentia.index, subBlock.getChannelPointer(0), n);
            preApplyEssentia.index += n;
        }
    }
}

void MainContentComponent::releaseResources()
{
    highpass.reset();
    lowpass.reset();
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    float level = RMSlevel_dB.load();
    level = jmax<float>(level, sl_noiseGateThreshold.getMinimum());
    auto range = sl_noiseGateThreshold.getRange();
    const float gain = (level - range.getStart()) / range.getLength();
    g.fillAll(Colour::Colour(0xFFF9F9F4));
    auto meterArea = Rectangle<int>(8, 80, 400, 10);
    meterArea.removeFromRight(meterArea.getWidth() * (1.0 - gain));
    g.setColour(Colour::Colour(0xFFA9FDAC));
    g.fillRoundedRectangle (meterArea.toFloat(), 0.0);
}

void MainContentComponent::resized()
{
    lbl_appName.setBounds (4, 8, 170, 21);
    lbl_version.setBounds (173, 8, 70, 24);
    sl_noiseGateThreshold.setBounds(8, 60, 400, 20);
    sl_hpf.setBounds(95, 97, 80, 85);
    tgl_hpf.setBounds(87, 140, 30, 30);
    sl_lpf.setBounds(260, 97, 80, 85);
    tgl_lpf.setBounds(252, 140, 30, 30);
}

void MainContentComponent::sliderValueChanged (Slider* slider)
{
    if (slider == &sl_noiseGateThreshold)
    {
        noiseGateSettings->setAttribute("threshold", slider->getValue());
        appProperties->getUserSettings()->setValue (XMLKEYNOISEGATE, noiseGateSettings.get());
        appProperties->getUserSettings()->save();
    }
    else if(slider == &sl_hpf)
    {
        const double freq = slider->getValue();
        updateHighpassCoefficient(freq, deviceManager.getCurrentAudioDevice()->getCurrentSampleRate());
        highpassSettings->setAttribute("freq", freq);
        appProperties->getUserSettings()->setValue (XMLKEYHIGHPASS, highpassSettings.get());
        appProperties->getUserSettings()->save();
    }
    else if(slider == &sl_lpf)
    {
        const double freq = slider->getValue();
        updateLowpassCoefficient(freq, deviceManager.getCurrentAudioDevice()->getCurrentSampleRate());
        lowpassSettings->setAttribute("freq", freq);
        appProperties->getUserSettings()->setValue (XMLKEYLOWPASS, lowpassSettings.get());
        appProperties->getUserSettings()->save();
    }
}

StringArray MainContentComponent::getMenuBarNames()
{
    const char* const names[] = { "Settings", nullptr };
    return StringArray (names);
}

PopupMenu MainContentComponent::getMenuForIndex (int topLevelMenuIndex, const String& /*menuName*/)
{
    PopupMenu menu;
    if (topLevelMenuIndex == 0) menu.addItem(1, "Audio Settings");
    return menu;
}

void MainContentComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    if (menuItemID == 1)
    {
        showAudioSettings();
    }
}

void MainContentComponent::buttonClicked (Button* button)
{
    
    if(button == &tgl_hpf)
    {
        bool hpfEnable = button->getToggleState();
        sl_hpf.setEnabled(hpfEnable);
        highpassSettings->setAttribute("enable", hpfEnable);
        appProperties->getUserSettings()->setValue (XMLKEYHIGHPASS, highpassSettings.get());
        appProperties->getUserSettings()->save();
    }
    else if(button == &tgl_lpf)
    {
        bool lpfEnable = button->getToggleState();
        sl_lpf.setEnabled(lpfEnable);
        lowpassSettings->setAttribute("enable", lpfEnable);
        appProperties->getUserSettings()->setValue (XMLKEYLOWPASS, lowpassSettings.get());
        appProperties->getUserSettings()->save();
    }
}

void MainContentComponent::showAudioSettings()
{
    AudioDeviceSelectorComponent audioSettingsComp (deviceManager,
                                                    1, 1,//Audio input channels(min/max)
                                                    0, 0,//Audio output channels(min/max)
                                                    false,//Show MIDI input
                                                    false, //Show MIDI output
                                                    false,//Stereo pair
                                                    false);//Show advanced options
    
    audioSettingsComp.setSize (500, 450);
    
    DialogWindow::LaunchOptions o;
    o.content.setNonOwned (&audioSettingsComp);
    o.dialogTitle                   = "Audio Settings";
    o.componentToCentreAround       = this;
    o.dialogBackgroundColour        = Colours::black;
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = false;
    o.runModal();
    
    //設定をXMLファイルで保存
    ScopedPointer<XmlElement> audioState (deviceManager.createStateXml());
    appProperties->getUserSettings()->setValue (XMLKEYAUDIOSETTINGS, audioState);
    appProperties->getUserSettings()->saveIfNeeded();
}

void MainContentComponent::timerCallback()
{
    repaint();
}


void MainContentComponent::sendOSC(String oscAddress, int value)
{
    oscSender.send(oscAddress, value);
}

void MainContentComponent::sendMIDI(int noteNumber)
{
    if (midiMessage.isNoteOn())
    {
        midiMessage = MidiMessage::noteOff(midiChannel, midiMessage.getNoteNumber(), (uint8)0);
        midiOut->sendMessageNow(midiMessage);
    }
    
    midiMessage = MidiMessage::noteOn(midiChannel, noteNumber, (uint8)127);
    midiOut->sendMessageNow(midiMessage);
}

void MainContentComponent::estimateMelody()
{
    melodyEstimate->compute();
    melodyEstimate->reset();//compute()あとにreset()は必ず呼ぶこと
    pitchfilter->compute();
    
    //周波数->MIDIノート変換
    auto freqToNote = [](float hz)->int{
        return hz >= 20.0 ? std::nearbyint(69.0 + 12.0 * log2(hz / 440.0)): -1;//20Hz以下の時は-1を返す
    };
    std::vector<int> noteArray(essentiaFreq.size(), -1);
    std::transform(essentiaFreq.begin(), essentiaFreq.end(), std::back_inserter(noteArray), freqToNote);
    
    const int numConsecutive = 12;
    const int noteOffset = 12 * overSampleFactor;//オーバーサンプリングの影響でオクターブ下に推定されてしまっている
    for (int i = 0; i < noteArray.size() - numConsecutive; ++i)
    {
        const int target = noteArray[i];
        if  (target != -1 && target != lastNote)
        {
            bool isEnoughConsecutive = std::all_of(noteArray.begin() + i, noteArray.begin() + i + numConsecutive, [target](int x){return x == target;});
            if (isEnoughConsecutive)
            {
                lastNote = target;
                sendOSC(oscAddress_note, lastNote + noteOffset);
                sendMIDI(lastNote + noteOffset);
            }
        }
    }
}

void MainContentComponent::updateHighpassCoefficient(const double cutoffFreq, const double sampleRate)
{
    auto firstHighpass = dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoffFreq);
    *highpass.get<0>().state = *firstHighpass;
    auto secondHighpass = dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoffFreq);
    *highpass.get<1>().state = *secondHighpass;
}

void MainContentComponent::updateLowpassCoefficient(const double cutoffFreq, const double sampleRate)
{
    *lowpass.state = *dsp::FilterDesign<float>::designFIRLowpassKaiserMethod(cutoffFreq, sampleRate, 0.2f, -24.0f);
}

