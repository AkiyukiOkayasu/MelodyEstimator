#include "MainComponent.h"

MainContentComponent::MainContentComponent()
{
    setMacMainMenu(this);
    setLookAndFeel(&lookAndFeel);
    
    //GUI Noise gate
    addAndMakeVisible(sl_noiseGateThreshold);
    sl_noiseGateThreshold.setRange(-72.0, 0.0, 1.0);
    sl_noiseGateThreshold.setSliderStyle (Slider::LinearBarVertical);
    sl_noiseGateThreshold.setTextValueSuffix("dB");
    sl_noiseGateThreshold.setTextBoxStyle (Slider::TextBoxBelow, true, 45, 18);
    sl_noiseGateThreshold.addListener(this);
    addAndMakeVisible(lbl_noiseGate);
    lbl_noiseGate.setText("Gate", dontSendNotification);
    lbl_noiseGate.setFont(Font (Font::getDefaultSansSerifFontName(), 14.0f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_noiseGate.setJustificationType (Justification::centredLeft);
    lbl_noiseGate.setEditable (false, false, false);
    
    //GUI Highpass filter
    addAndMakeVisible(sl_hpf);
    sl_hpf.setRange(18.0, 120.0, 1.0);
    sl_hpf.setTextValueSuffix("Hz");
    sl_hpf.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    sl_hpf.setTextBoxStyle(Slider::TextBoxBelow, true, 45, 18);
    sl_hpf.addListener(this);
    addAndMakeVisible(lbl_hpf);
    lbl_hpf.setFont (Font (Font::getDefaultSansSerifFontName(), 14.0f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_hpf.setText("HPF", dontSendNotification);
    lbl_hpf.setEditable (false, false, false);
    
    addAndMakeVisible (lbl_version);
    std::string version = "ver" + std::string(ProjectInfo::versionString);
    lbl_version.setText(version, dontSendNotification);
    lbl_version.setFont (Font (Font::getDefaultSansSerifFontName(), 12.0f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_version.setJustificationType (Justification::centredLeft);
    lbl_version.setEditable (false, false, false);
    
    addAndMakeVisible(lbl_pitch);
    lbl_pitch.setText("", dontSendNotification);
    lbl_pitch.setFont(Font (Font::getDefaultSansSerifFontName(), 96.0f, Font::plain).withTypefaceStyle ("Regular"));
    lbl_pitch.setJustificationType(Justification::centred);
    lbl_pitch.setEditable(false, false, false);
    
    //Essentia
    preApplyEssentia.buffer.setSize(1, lengthToEstimateMelody_sample);
    essentia::init();
    essentia::standard::AlgorithmFactory& factory = essentia::standard::AlgorithmFactory::instance();
    const float minFreq = MidiMessage::getMidiNoteInHertz((minNoteToEstimate - 5) - (12 * overSampleFactor), standardPitch);//minNoteToEstimateの完全四度下まで処理する
    const float maxFreq = MidiMessage::getMidiNoteInHertz(maxNoteToEstimate - (12 * overSampleFactor), standardPitch);
    melodyEstimate = factory.create("PredominantPitchMelodia",
                                    "minFrequency", minFreq, "maxFrequency", maxFreq,
                                    "voicingTolerance", -0.9f);//voicingToleranceパラメータは要調整 [-1.0~1.4] default:0.2(反応のしやすさ的なパラメータ)
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
    
    midiOut = MidiOutput::createNewDevice(midiPortName);
    if(midiOut != nullptr) {
        std::cout<<"MIDI port: "<<midiOut->getName()<<std::endl;
        midiOut->startBackgroundThread();
    }
    
    setSize (305, 277);
    
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
    const double thrsld = noiseGateSettings != nullptr ? noiseGateSettings->getDoubleAttribute("threshold") : -24.0;
    const double hpfFreq = highpassSettings != nullptr ? highpassSettings->getDoubleAttribute("freq") : 20.0;
    sl_hpf.setValue(hpfFreq, dontSendNotification);
    sl_noiseGateThreshold.setValue(thrsld, dontSendNotification);
    
    setAudioChannels (1, 0, savedAudioState.get());
    startTimerHz(60);
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
    updateHighpassCoefficient(sl_hpf.getValue(), sampleRate);
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlockExpected));
    oversampling->reset();
}

void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    dsp::AudioBlock<float> block(*bufferToFill.buffer);
    dsp::ProcessContextReplacing<float> context(block);
    highpass.process(context);
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
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    float level = RMSlevel_dB.load();
    level = jmax<float>(level, sl_noiseGateThreshold.getMinimum());
    auto range = sl_noiseGateThreshold.getRange();
    const float gain = (level - range.getStart()) / range.getLength();
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    auto meterArea = sl_noiseGateThreshold.getBounds();
    meterArea.removeFromTop(meterArea.getHeight() * (1.0 - gain));
    g.setColour(lookAndFeel.me_green);
    g.fillRect(meterArea.toFloat());
    
    std::string nt;
    int chrm = lastNote.load() % 12;
    switch (chrm)
    {
        case 0:
            nt = "C";
            break;
        case 1:
            nt = "C#";
            break;
        case 2:
            nt = "D";
            break;
        case 3:
            nt = "D#";
            break;
        case 4:
            nt = "E";
            break;
        case 5:
            nt = "F";
            break;
        case 6:
            nt = "F#";
            break;
        case 7:
            nt = "G";
            break;
        case 8:
            nt = "G#";
            break;
        case 9:
            nt = "A";
            break;
        case 10:
            nt = "A#";
            break;
        case 11:
            nt = "B";
            break;
        default:
            break;
    }
    lbl_pitch.setText(nt, dontSendNotification);
}

void MainContentComponent::resized()
{
    sl_hpf.setBounds(42, 12, 61, 60);
    lbl_hpf.setBounds(8, 30, 35, 15);
    sl_noiseGateThreshold.setBounds(47, 80, 52, 183);
    lbl_noiseGate.setBounds(6, 164, 42, 15);
    lbl_version.setBounds(241, 248, 46, 15);
    lbl_pitch.setBounds(143, 82, 130, 130);
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
        return hz >= 20.0 ? std::nearbyint(69.0 + 12.0 * log2(hz / standardPitch)): -1;//20Hz以下の時は-1を返す
    };
    std::vector<int> noteArray(essentiaFreq.size(), -1);
    std::transform(essentiaFreq.begin(), essentiaFreq.end(), std::back_inserter(noteArray), freqToNote);
    
    const int numConsecutive = 12;
    const int noteOffset = 12 * overSampleFactor;//オーバーサンプリングの影響でオクターブ下に推定されてしまっているのを補正
    for (int i = 0; i < noteArray.size() - numConsecutive; ++i)
    {
        const int target = noteArray[i] != -1 ? noteArray[i] + noteOffset : -1;
        if  (target != -1 && target != lastNote.load() && minNoteToEstimate <= target && target <= maxNoteToEstimate)
        {
            bool isEnoughConsecutive = std::all_of(noteArray.begin() + i, noteArray.begin() + i + numConsecutive,
                                                   [target](int x){return x + noteOffset == target;});
            if (isEnoughConsecutive)
            {
                lastNote.store(target);
                sendMIDI(lastNote);
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

