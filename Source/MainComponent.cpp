/*
 ==============================================================================
 
 This file was auto-generated!
 
 ==============================================================================
 */

//==============================================================================
/*
 This component lives inside our window, and this is where you should put all
 your controls and content.
 */

//==============================================================================
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
    
    //モノフォニック用
    //mel = factory.create("PitchMelodia", "minFrequency", (essentia::Real)220.0f, "maxFrequency", (essentia::Real)1760.0f, "voicingTolerance", -1.0f);
    //ポリフォニック用
    mel = factory.create("PredominantPitchMelodia", "minFrequency", (essentia::Real)220.0f, "maxFrequency", (essentia::Real)7040.0f, "voicingTolerance", -0.7f);
    //voicingToleranceパラメータは要調整 [-1.0~1.4] default:0.2
    //反応のしやすさ的なパラメータ
    
    pitchfilter = factory.create("PitchFilter", "confidenceThreshold", 36, "minChunkSize", 30);
    equalloudness = factory.create("EqualLoudness");
    std::cout<<"Essentia: algorithm created"<<std::endl;
    
    essentiaAudioBuffer.reserve(melFrameSize);
    essentiaAudioBuffer.resize(melFrameSize);
    std::iota(std::begin(essentiaAudioBuffer), std::end(essentiaAudioBuffer), 0.0f);
    pitch.reserve(200);
    pitchConfidence.reserve(200);
    freq.reserve(200);
    
    equalloudness->input("signal").set(essentiaAudioBuffer);
    equalloudness->output("signal").set(essentiaAudioBuffer);
    mel->input("signal").set(essentiaAudioBuffer);
    mel->output("pitch").set(pitch);
    mel->output("pitchConfidence").set(pitchConfidence);
    pitchfilter->input("pitch").set(pitch);
    pitchfilter->input("pitchConfidence").set(pitchConfidence);
    pitchfilter->output("pitchFiltered").set(freq);
    std::cout<<"Essentia: algorithm connected"<<std::endl;
    
    if(! sender.connect(ip, portnumber)) std::cout<<"OSC connection Error..."<<std::endl;
    
    midiOut = MidiOutput::createNewDevice("JUCE");
    std::cout<<"MIDI port: "<<midiOut->getName()<<std::endl;
    midiOut->startBackgroundThread();
    
    setSize (600, 130);
    
    
    //保存したパラメータをXMLファイルから呼び出し
    PropertiesFile::Options options;
    options.applicationName     = "Juce Audio Plugin Host";
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Preferences";
    appProperties = new ApplicationProperties();
    appProperties->setStorageParameters (options);
    ScopedPointer<XmlElement> savedAudioState (appProperties->getUserSettings()->getXmlValue ("audioDeviceState"));//オーディオインターフェースの設定
    ScopedPointer<XmlElement> savedNoiseGateSettings (appProperties->getUserSettings()->getXmlValue("noiseGateSettings"));//ノイズゲートの設定
    
    if (savedNoiseGateSettings != nullptr)
    {
        const double thre = savedNoiseGateSettings->getDoubleAttribute("threshold");
        noiseGateThreshold.setValue(thre, NotificationType::dontSendNotification);
    }
    else
    {
        noiseGateThreshold.setValue(-24.0, NotificationType::dontSendNotification);
    }
    
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
        essentiaAudioBuffer.emplace_back((essentia::Real)input[samples]);
        
        if (essentiaAudioBuffer.size() >= melFrameSize)
        {
            if (rmsThreshold(essentiaAudioBuffer, noiseGateThreshold.getValue()))
            {
                equalloudness->compute();
                mel->compute();
                mel->reset();//compute()あとにreset()は必ず呼ぶこと
                pitchfilter->compute();
                
                //周波数->MIDIノート変換(1オクターブ内にまとめる処理も行なっているので0~11にマッピングされる)
                auto freqToMIDI = [](float hz)->int{
                    return hz >= 20.0 ? (int)std::nearbyint(69.0 + 12.0 * log2(hz / 440.0)) % (int)12: -1;//20Hz以下の時は-1を返す
                };
                
                std::vector<int> noteArray;
                noteArray.resize(freq.size(), -1);
                std::transform(freq.begin(), freq.end(), std::back_inserter(noteArray), freqToMIDI);
                
                const int n = 10;
                for (int i = 0; i < noteArray.size() - n; ++i)
                {
                    const int target = noteArray.at(i);
                    if  (target == -1 || target == midiNote) continue;
                    
                    bool isSame = std::all_of(noteArray.begin() + i, noteArray.begin() + i + n, [target](int x){return x == target;});
                    if (isSame)
                    {
                        jassert(0 <= target || target < 12);
                        midiNote = target;
                        sendOSC("/juce/notenumber", midiNote + 60);
                        sendMIDI(midiNote + 60);
                    }
                }
            }
            
            essentiaAudioBuffer.clear();
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
    sender.send(oscAddress, value);
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

bool MainContentComponent::rmsThreshold(std::vector<float> &buf, float threshold)
{
    //入力のRMSレベルが閾値を上回っているかを判定する
    //buf:入力音声AudioSampleBufferなど
    //threshold:閾値(dB)
    std::vector<float> pow2;
    for (auto& el: buf)
    {
        pow2.emplace_back(el * el);
    }
    
    float accum = std::accumulate(std::begin(pow2), std::end(pow2), 0.0f);
    float rmslevel = std::sqrt(accum / (float)pow2.size());
    return rmslevel > Decibels::decibelsToGain(threshold);
}
