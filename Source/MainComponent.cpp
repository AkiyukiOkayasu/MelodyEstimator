#include "MainComponent.h"

MainContentComponent::MainContentComponent()
{
    setMacMainMenu(this);
    
    //GUI Noise gate
    addAndMakeVisible(noiseGateThreshold);
    noiseGateThreshold.setRange(-72.0, 0.0, 1.0);
    noiseGateThreshold.setTextValueSuffix("dB");
    noiseGateThreshold.setTextBoxStyle(Slider::TextBoxLeft, false, 100, noiseGateThreshold.getTextBoxHeight());
    noiseGateThreshold.addListener(this);
    
    addAndMakeVisible(noiseGateLabel);
    noiseGateLabel.setText("Noise Gate Threshold", NotificationType::dontSendNotification);
    noiseGateLabel.attachToComponent(&noiseGateThreshold, false);
    
    //Essentia
    essentia::init();
    essentia::standard::AlgorithmFactory& factory = essentia::standard::AlgorithmFactory::instance();
    melodyDetection = factory.create("PredominantPitchMelodia", "minFrequency", (essentia::Real)220.0f, "maxFrequency", (essentia::Real)7040.0f, "voicingTolerance", -0.7f);//voicingToleranceパラメータは要調整 [-1.0~1.4] default:0.2(反応のしやすさ的なパラメータ)
    pitchfilter = factory.create("PitchFilter", "confidenceThreshold", 36, "minChunkSize", 30);
    equalloudness = factory.create("EqualLoudness");
    std::cout<<"Essentia: algorithm created"<<std::endl;
    
    essentiaInput.reserve(lengthToDetectMelody_sample);
    essentiaInput.resize(lengthToDetectMelody_sample, 0.0f);
    essentiaPitch.reserve(200);
    essentiaPitchConfidence.reserve(200);
    essentiaFreq.reserve(200);
    equalloudness->input("signal").set(essentiaInput);
    equalloudness->output("signal").set(essentiaInput);
    melodyDetection->input("signal").set(essentiaInput);
    melodyDetection->output("pitch").set(essentiaPitch);
    melodyDetection->output("pitchConfidence").set(essentiaPitchConfidence);
    pitchfilter->input("pitch").set(essentiaPitch);
    pitchfilter->input("pitchConfidence").set(essentiaPitchConfidence);
    pitchfilter->output("pitchFiltered").set(essentiaFreq);
    std::cout<<"Essentia: algorithm connected"<<std::endl;
    
    if(! oscSender.connect(ip, port)) std::cout<<"OSC connection Error..."<<std::endl;
    
    midiOut = MidiOutput::createNewDevice(midiPortName);
    std::cout<<"MIDI port: "<<midiOut->getName()<<std::endl;
    midiOut->startBackgroundThread();
    
    setSize (600, 130);
    
    //保存したパラメータをXMLファイルから呼び出し
    PropertiesFile::Options options;
    options.applicationName     = ProjectInfo::projectName;
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Preferences";
    appProperties = new ApplicationProperties();
    appProperties->setStorageParameters (options);
    ScopedPointer<XmlElement> savedAudioState (appProperties->getUserSettings()->getXmlValue ("audioDeviceState"));//オーディオインターフェースの設定
    ScopedPointer<XmlElement> savedNoiseGateSettings (appProperties->getUserSettings()->getXmlValue("noiseGateSettings"));//ノイズゲートの設定
    const double thrsld = savedNoiseGateSettings ? savedNoiseGateSettings->getDoubleAttribute("threshold") : -24.0;
    noiseGateThreshold.setValue(thrsld, NotificationType::dontSendNotification);
    setAudioChannels (1, 0, savedAudioState);
}

MainContentComponent::~MainContentComponent()
{
    setMacMainMenu(nullptr);
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
}

void MainContentComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    const float* input = bufferToFill.buffer->getReadPointer(0);
    
    for (int samples = 0; samples < bufferToFill.buffer->getNumSamples(); ++samples)
    {
        essentiaInput.emplace_back((essentia::Real)input[samples]);
        
        if (essentiaInput.size() >= lengthToDetectMelody_sample)
        {
            if (!isLouder_RMS(essentiaInput, noiseGateThreshold.getValue())) continue;
            
            equalloudness->compute();
            melodyDetection->compute();
            melodyDetection->reset();//compute()あとにreset()は必ず呼ぶこと
            pitchfilter->compute();
            
            //周波数->MIDIノート変換
            auto freqToNote = [](float hz)->int{
                return hz >= 20.0 ? std::nearbyint(69.0 + 12.0 * log2(hz / 440.0)): -1;//20Hz以下の時は-1を返す
            };
            std::vector<int> noteArray(essentiaFreq.size(), -1);
            std::transform(essentiaFreq.begin(), essentiaFreq.end(), std::back_inserter(noteArray), freqToNote);
            
            const int numConsecutive = 10;
            for (int i = 0; i < noteArray.size() - numConsecutive; ++i)
            {
                const int target = noteArray[i];
                if  (target == -1 || target == lastNote) continue;
                
                bool isSame = std::all_of(noteArray.begin() + i, noteArray.begin() + i + numConsecutive, [target](int x){return x == target;});
                if (isSame)
                {
                    lastNote = target;
                    sendOSC(oscAddress_note, lastNote);
                    sendMIDI(lastNote);
                }
            }
            
            essentiaInput.clear();
        }
    }
}

void MainContentComponent::releaseResources()
{
}

//==============================================================================
void MainContentComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void MainContentComponent::resized()
{
    noiseGateThreshold.setBounds(10, 50, getWidth() - 20, 50);
}

void MainContentComponent::sliderValueChanged (Slider* slider)
{
    if (slider == &noiseGateThreshold)
    {
        //スライダーの値をXMLで保存
        String xmltag =  "noiseGateThreshold";
        ScopedPointer<XmlElement> noiseGateSettings = new XmlElement(xmltag);
        noiseGateSettings->setAttribute("threshold", slider->getValue());
        appProperties->getUserSettings()->setValue ("noiseGateSettings", noiseGateSettings);
        appProperties->getUserSettings()->saveIfNeeded();
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
    if (topLevelMenuIndex == 0)
    {
        menu.addItem(1, "Audio Settings");
    }
    return menu;
}

void MainContentComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    if (menuItemID == 1)
    {
        showAudioSettings();
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

float MainContentComponent::computeRMS(std::vector<float> &buffer)
{
    //入力のRMSレベルを算出する
    double rmsLevel = 0;
    std::for_each(std::begin(buffer), std::end(buffer), [&rmsLevel](float x) mutable {rmsLevel += x * x;});
    rmsLevel = rmsLevel / (double)buffer.size();
    rmsLevel = std::sqrt(rmsLevel);
    return Decibels::gainToDecibels(rmsLevel);
}
