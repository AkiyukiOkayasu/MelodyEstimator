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
    
    essentia::init();
    essentia::standard::AlgorithmFactory& factory = essentia::standard::AlgorithmFactory::instance();
    
    //モノフォニック用
    //mel = factory.create("PitchMelodia", "minFrequency", (essentia::Real)220.0f, "maxFrequency", (essentia::Real)1760.0f, "voicingTolerance", -1.0f);
    //ポリフォニック用
    mel = factory.create("PredominantPitchMelodia", "minFrequency", (essentia::Real)220.0f, "maxFrequency", (essentia::Real)7040.0f, "voicingTolerance", -0.7f);
    //voicingToleranceパラメータは要調整 [-1.0~1.4] default:0.2
    //反応のしやすさ的なパラメータ
    
    pitchfilter = factory.create("PitchFilter");
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
    std::cout<<midiOut->getName()<<std::endl;
    midiOut->startBackgroundThread();
    
    setSize (400, 400);
    
    //オーディオインターフェースの設定の呼び出し
    PropertiesFile::Options options;
    options.applicationName     = "Juce Audio Plugin Host";
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Preferences";
    appProperties = new ApplicationProperties();
    appProperties->setStorageParameters (options);
    ScopedPointer<XmlElement> savedAudioState (appProperties->getUserSettings()->getXmlValue ("audioDeviceState"));
    
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
            //equalloudness->compute();
            mel->compute();
            mel->reset();//compute()あとにreset()は必ず呼ぶこと
            pitchfilter->compute();
            
            for (auto& el: freq)
            {
                int note = freqToMidi(el);
                
                if  (note > 0)
                {
                    note = note % 12 + 60;
                } else {
                    continue;
                }
                
                if (midiNote != note && rmsThreshold(essentiaAudioBuffer, 0.015f))
                {
                    midiNote = note;
                    sendOSC("/juce/notenumber", midiNote);
                    sendMIDI(midiNote);
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
                                                    0, 256,
                                                    0, 256,
                                                    true, true, true, false);
    
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
    
    ScopedPointer<XmlElement> audioState (deviceManager.createStateXml());
    appProperties->getUserSettings()->setValue ("audioDeviceState", audioState);
    appProperties->getUserSettings()->saveIfNeeded();
}

int MainContentComponent::freqToMidi(float freq)
{
    if (freq <= 20.0)//20Hz以下の時は-1を返す
    {
        return -1;
    }
    
    int notenumber = std::nearbyint(69.0 + 12.0 * log2(freq / 440.0));
    return notenumber;
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
    std::vector<float> pow2;
    for (auto& el: buf)
    {
        pow2.emplace_back(el * el);
    }
    
    float accum = std::accumulate(std::begin(pow2), std::end(pow2), 0.0f);
    float rmslevel = std::sqrt(accum / (float)pow2.size());
    return rmslevel > threshold;
}
