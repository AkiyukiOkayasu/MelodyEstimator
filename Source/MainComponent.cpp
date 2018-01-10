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
    lbl_noiseGate.setText("Noise Gate Threshold", NotificationType::dontSendNotification);
    lbl_noiseGate.setFont(Font (Font::getDefaultMonospacedFontName(), 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_noiseGate.setJustificationType (Justification::centredLeft);
    lbl_noiseGate.setEditable (false, false, false);
    lbl_noiseGate.attachToComponent(&sl_noiseGateThreshold, true);
    
    //GUI Highpass filter
    addAndMakeVisible(sl_hpf);
    sl_hpf.setRange(20.0, 120.0, 1.0);
    sl_hpf.setTextValueSuffix("Hz");
    sl_hpf.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    sl_hpf.setTextBoxStyle(Slider::TextBoxBelow, false, 45, 15);
    sl_hpf.addListener(this);
    addAndMakeVisible(lbl_hpf);
    lbl_hpf.setText("High-pass Filter", NotificationType::dontSendNotification);
    lbl_hpf.setFont (Font (Font::getDefaultMonospacedFontName(), 15.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_hpf.setJustificationType (Justification::centredLeft);
    lbl_hpf.setEditable (false, false, false);
    lbl_hpf.attachToComponent(&sl_hpf, true);
    addAndMakeVisible(tgl_hpf);
    tgl_hpf.addListener(this);
    
    addAndMakeVisible (lbl_appName);
    lbl_appName.setText("Melody Estimator", NotificationType::dontSendNotification);
    lbl_appName.setFont (Font (Font::getDefaultMonospacedFontName(), 21.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_appName.setJustificationType (Justification::centredLeft);
    lbl_appName.setEditable (false, false, false);
    addAndMakeVisible (lbl_version);
    std::string version = "ver" + std::string(ProjectInfo::versionString);
    lbl_version.setText(version, NotificationType::dontSendNotification);
    lbl_version.setFont (Font (Font::getDefaultMonospacedFontName(), 14.00f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_version.setJustificationType (Justification::centredLeft);
    lbl_version.setEditable (false, false, false);
    
    preApplyEssentia.buffer.setSize(1, lengthToEstimateMelody_sample);
    
    //Essentia
    essentia::init();
    essentia::standard::AlgorithmFactory& factory = essentia::standard::AlgorithmFactory::instance();
    const float minFreq = MidiMessage::getMidiNoteInHertz(64 - (12 * overSampleFactor), 440.0) * 0.98;
    const float maxFreq = MidiMessage::getMidiNoteInHertz(88 - (12 * overSampleFactor), 440.0);
    std::cout<<"Estimate Freq Range: "<<minFreq<<"Hz ~ "<<maxFreq<<"Hz"<<std::endl;
    melodyEstimate = factory.create("PredominantPitchMelodia", "minFrequency", minFreq, "maxFrequency", maxFreq, "voicingTolerance", -0.9f);//voicingToleranceパラメータは要調整 [-1.0~1.4] default:0.2(反応のしやすさ的なパラメータ)
    pitchfilter = factory.create("PitchFilter", "confidenceThreshold", 70, "minChunkSize", 35);
    std::cout<<"Essentia: algorithm created"<<std::endl;
    
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
    std::cout<<"Essentia: algorithm connected"<<std::endl;
    
    if(! oscSender.connect(ip, port)) std::cout<<"OSC connection Error..."<<std::endl;
    
    midiOut = MidiOutput::createNewDevice(midiPortName);
    std::cout<<"MIDI port: "<<midiOut->getName()<<std::endl;
    midiOut->startBackgroundThread();
    
    setSize (600, 200);
    
    //保存したパラメータをXMLファイルから呼び出し
    PropertiesFile::Options options;
    options.applicationName     = ProjectInfo::projectName;
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Preferences";
    appProperties = new ApplicationProperties();
    appProperties->setStorageParameters (options);
    auto userSettings = appProperties->getUserSettings();
    ScopedPointer<XmlElement> savedAudioState (userSettings->getXmlValue ("audioDeviceState"));//オーディオインターフェースの設定
    ScopedPointer<XmlElement> savedNoiseGateSettings (userSettings->getXmlValue("noiseGateSettings"));//ノイズゲートの設定
    ScopedPointer<XmlElement> savedHighpassFilterSettings (userSettings->getXmlValue("highpassFilterSettings"));//ハイパスの設定
    const double thrsld = savedNoiseGateSettings ? savedNoiseGateSettings->getDoubleAttribute("threshold") : -24.0;
    const auto hpfIndex = savedHighpassFilterSettings ? savedHighpassFilterSettings->getIntAttribute("selectedItemIndex"): 0;
    cmb_hpf.setSelectedItemIndex(hpfIndex);
    sl_noiseGateThreshold.setValue(thrsld, NotificationType::dontSendNotification);
    setAudioChannels (1, 0, savedAudioState);
    const double hpfFreq = highpassSettings != nullptr ? highpassSettings->getDoubleAttribute("freq") : 20.0;
    const bool hpfEnable = highpassSettings != nullptr ? highpassSettings->getBoolAttribute("enable") : false;
    sl_hpf.setEnabled(hpfEnable);
    sl_hpf.setValue(hpfFreq, dontSendNotification);
    tgl_hpf.setToggleState(hpfEnable, dontSendNotification);
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
    std::cout<<"prepareToPlay()"<<std::endl;
    std::cout<<"Sample Rate: "<<sampleRate<<std::endl;
    std::cout<<"Buffer Size: "<<samplesPerBlockExpected<<std::endl;
    
    oversampling = new dsp::Oversampling<float>(1, overSampleFactor, dsp::Oversampling<float>::FilterType::filterHalfBandFIREquiripple, false);
    auto channels = static_cast<uint32>(1);//1ch
    dsp::ProcessSpec spec {sampleRate, static_cast<uint32>(samplesPerBlockExpected), channels};
    highpass.prepare(spec);
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlockExpected));
    oversampling->reset();
}

void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    dsp::AudioBlock<float> block(*bufferToFill.buffer);
    dsp::ProcessContextReplacing<float> context(block);
    if(tgl_hpf.getToggleState()) highpass.process(context);
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
            for(int i = 0; i < lengthToEstimateMelody_sample; ++i)
            {
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
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    float level = RMSlevel_dB.load();
    level = jmax<float>(level, sl_noiseGateThreshold.getMinimum());
    auto range = sl_noiseGateThreshold.getRange();
    const float gain = (level - range.getStart()) / range.getLength();
    g.fillAll(Colour::Colour(0xFF121258));
    auto meterArea = Rectangle<int>(175, 77, 400, 10);
    meterArea.removeFromRight(meterArea.getWidth() * (1.0 - gain));
    g.setColour(Colour::Colour(0xFF81fcad));
    g.fillRoundedRectangle (meterArea.toFloat(), 0.0);
}

void MainContentComponent::resized()
{
    lbl_appName.setBounds (8, 16, 170, 21);
    lbl_version.setBounds (173, 16, 70, 24);
    sl_noiseGateThreshold.setBounds(175, 57, 400, 20);
    sl_hpf.setBounds(175, 97, 80, 80);
    tgl_hpf.setBounds(160, 115, 30, 30);
}

void MainContentComponent::sliderValueChanged (Slider* slider)
{
    if (slider == &sl_noiseGateThreshold)
    {
        //スライダーの値をXMLで保存
        String xmltag =  "noiseGateThreshold";
        ScopedPointer<XmlElement> noiseGateSettings = new XmlElement(xmltag);
        noiseGateSettings->setAttribute("threshold", slider->getValue());
        appProperties->getUserSettings()->setValue ("noiseGateSettings", noiseGateSettings);
        appProperties->getUserSettings()->saveIfNeeded();
    }
    else if(slider == &sl_hpf)
    {
        const double freq = slider->getValue();
        computeHighpassCoefficient(freq, deviceManager.getCurrentAudioDevice()->getCurrentSampleRate());
        highpassSettings->setAttribute("freq", freq);
        appProperties->getUserSettings()->setValue (XMLKEYHIGHPASS, highpassSettings.get());
        appProperties->getUserSettings()->save();
    }
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
    appProperties->getUserSettings()->setValue ("audioDeviceState", audioState);
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

void MainContentComponent::computeHighpassCoefficient(const double cutoffFreq, const double sampleRate)
{
    auto firstHighpass = dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoffFreq);//Q =1/sqrt(2)
    *highpass.get<0>().state = *firstHighpass;
    auto secondHighpass = dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoffFreq);//Q =1/sqrt(2)
    *highpass.get<1>().state = *secondHighpass;
}

