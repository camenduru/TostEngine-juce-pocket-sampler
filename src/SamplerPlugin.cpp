/*
  ==============================================================================

    SamplerPlugin.cpp
    Created: 16 Jan 2026
    Author:  PC

    16-Button Square MIDI Sampler

  ==============================================================================
*/

#include "SamplerPlugin.h"
#include "SamplerEditor.h"

//==============================================================================
// One-shot mode namespace implementation
namespace OneShotMode
{
    bool isEnabled = false;

    void setEnabled(bool enable)
    {
        isEnabled = enable;
    }

    bool getEnabled()
    {
        return isEnabled;
    }
}

//==============================================================================
SamplerPlugin::SamplerPlugin()
{
    // Initialize 16 buttons
    buttons.clear();
    for (int i = 0; i < 16; i++)
    {
        ButtonSample button;
        buttons.add(button);
    }

    // Initialize note mapping (C2 to D3 = notes 36-51 for 16 buttons)
    noteMapping.clear();
    for (int i = 0; i < 16; i++)
    {
        noteMapping.add(36 + i); // Start from C2 (MIDI note 36)
    }

    // Initialize MIDI note samples array (128 notes)
    midiNoteSamples.clear();
    for (int i = 0; i < 128; i++)
    {
        ButtonSample sample;
        midiNoteSamples.add(sample);
    }

    // Register audio formats - use registerBasicFormats which registers all built-in formats
    formatManager.registerBasicFormats();

    // Add 16 voices to the synth (polyphonic)
    for (int i = 0; i < 16; i++)
    {
        synth.addVoice(new MidiSamplerVoice());
    }

    // Initialize default program
    setCurrentProgram(0);
}

SamplerPlugin::~SamplerPlugin()
{
    // Clear all buttons
    for (int i = 0; i < buttons.size(); i++)
    {
        buttons[i].clear();
    }
}

//==============================================================================
void SamplerPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    (void)samplesPerBlock;
    midiCollector.reset(sampleRate);
}

void SamplerPlugin::releaseResources()
{
}

//==============================================================================
void SamplerPlugin::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    // Get MIDI from the collector (Bluetooth devices, etc.)
    MidiBuffer collectorMidi;
    midiCollector.removeNextBlockOfMessages(collectorMidi, buffer.getNumSamples());

    // Combine incoming MIDI with collector MIDI
    MidiBuffer allMidi = midiMessages;
    allMidi.addEvents(collectorMidi, 0, -1, 0);

    // Debug: log if there's any MIDI to process
    int midiCount = allMidi.getNumEvents();
    // Process all MIDI through the synthesizer
    synth.renderNextBlock(buffer, allMidi, 0, buffer.getNumSamples());
}

//==============================================================================
AudioProcessorEditor* SamplerPlugin::createEditor()
{
    return new SamplerEditor(*this);
}

//==============================================================================
void SamplerPlugin::getStateInformation(MemoryBlock& destData)
{
    std::unique_ptr<XmlElement> xml(new XmlElement("SamplerState"));

    for (int i = 0; i < 16; i++)
    {
        XmlElement* buttonXml = xml->createNewChildElement("Button");
        buttonXml->setAttribute("index", i);
        buttonXml->setAttribute("filePath", buttons.getReference(i).filePath);
        buttonXml->setAttribute("noteMapping", noteMapping[i]);
    }

    String xmlString = xml->toString();
    destData.replaceAll(xmlString.toRawUTF8(), xmlString.length() + 1);
}

void SamplerPlugin::setStateInformation(const void* data, int sizeInBytes)
{
    String xmlString = String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
    std::unique_ptr<XmlElement> xml = XmlDocument::parse(xmlString);

    if (xml != nullptr && xml->hasTagName("SamplerState"))
    {
        for (int i = 0; i < 16; i++)
        {
            XmlElement* buttonXml = xml->getChildByName("Button");
            if (buttonXml != nullptr)
            {
                int index = buttonXml->getIntAttribute("index");
                if (index >= 0 && index < 16)
                {
                    ButtonSample& sample = buttons.getReference(index);
                    sample.clear();
                    sample.filePath = buttonXml->getStringAttribute("filePath");
                    noteMapping.set(index, buttonXml->getIntAttribute("noteMapping"));

                    if (sample.filePath.isNotEmpty())
                    {
                        File file(sample.filePath);
                        loadSample(index, file);
                    }
                }
            }
        }
    }
}

//==============================================================================
bool SamplerPlugin::loadSample(int buttonIndex, const File& file)
{
    if (buttonIndex < 0 || buttonIndex >= 16)
        return false;

    ButtonSample& sample = buttons.getReference(buttonIndex);
    clearSample(buttonIndex);

    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader != nullptr)
    {
        AudioSampleBuffer* buffer = new AudioSampleBuffer(
            reader->numChannels,
            static_cast<int>(reader->lengthInSamples)
        );

        reader->read(buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        sample.sampleBuffer = buffer;
        sample.filePath = file.getFullPathName();
        sample.sourceSampleRate = static_cast<float>(reader->sampleRate);
        sample.isLoaded = true;
        sample.rootNote = noteMapping[buttonIndex];

        // Add sound to synth
        auto* sound = new ButtonSampleSound(buttonIndex, buffer, sample.sourceSampleRate, sample.rootNote);
        synth.addSound(sound);

        return true;
    }

    return false;
}

void SamplerPlugin::clearSample(int buttonIndex)
{
    if (buttonIndex >= 0 && buttonIndex < 16)
    {
        buttons.getReference(buttonIndex).clear();

        // Remove associated sound from synth
        for (int i = synth.getNumSounds() - 1; i >= 0; --i)
        {
            if (auto* sound = dynamic_cast<ButtonSampleSound*>(synth.getSound(i).get()))
            {
                if (sound->buttonIndex == buttonIndex)
                {
                    synth.removeSound(i);
                }
            }
        }
    }
}

//==============================================================================
void SamplerPlugin::setNoteMapping(int buttonIndex, int midiNote)
{
    if (buttonIndex < 0 || buttonIndex >= 16)
        return;

    noteMapping.set(buttonIndex, midiNote);

    // Remove and re-add the sound so synth re-evaluates appliesToNote
    for (int i = synth.getNumSounds() - 1; i >= 0; --i)
    {
        if (auto* sound = dynamic_cast<ButtonSampleSound*>(synth.getSound(i).get()))
        {
            if (sound->buttonIndex == buttonIndex)
            {
                // Store the sound data before removing
                AudioSampleBuffer* buffer = sound->sampleBuffer;
                float sampleRate = sound->sourceSampleRate;

                // Remove old sound
                synth.removeSound(i);

                // Create new sound with updated rootNote
                auto* newSound = new ButtonSampleSound(buttonIndex, buffer, sampleRate, midiNote);
                synth.addSound(newSound);
                break;
            }
        }
    }
}

//==============================================================================
// MIDI Learn sample assignment - assign sample to a MIDI note
bool SamplerPlugin::loadSampleForMidiNote(int midiNote, const File& file)
{
    if (midiNote < 0 || midiNote >= 128)
        return false;

    ButtonSample& sample = midiNoteSamples.getReference(midiNote);
    clearMidiNoteSample(midiNote);

    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader != nullptr)
    {
        AudioSampleBuffer* buffer = new AudioSampleBuffer(
            reader->numChannels,
            static_cast<int>(reader->lengthInSamples)
        );

        reader->read(buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        sample.sampleBuffer = buffer;
        sample.filePath = file.getFullPathName();
        sample.sourceSampleRate = static_cast<float>(reader->sampleRate);
        sample.isLoaded = true;
        sample.rootNote = midiNote;

        // Add sound to synth with rootNote = midiNote so it responds to that note
        auto* sound = new ButtonSampleSound(-1, buffer, sample.sourceSampleRate, midiNote);
        synth.addSound(sound);

        return true;
    }

    return false;
}

void SamplerPlugin::clearMidiNoteSample(int midiNote)
{
    if (midiNote >= 0 && midiNote < 128)
    {
        midiNoteSamples.getReference(midiNote).clear();

        // Remove associated sound from synth
        for (int i = synth.getNumSounds() - 1; i >= 0; --i)
        {
            if (auto* sound = dynamic_cast<ButtonSampleSound*>(synth.getSound(i).get()))
            {
                // For MIDI note samples, buttonIndex is -1 and rootNote is the midiNote
                if (sound->buttonIndex == -1 && sound->rootNote == midiNote)
                {
                    synth.removeSound(i);
                }
            }
        }
    }
}

bool SamplerPlugin::isMidiNoteAssigned(int midiNote) const
{
    if (midiNote < 0 || midiNote >= midiNoteSamples.size())
        return false;
    return midiNoteSamples[midiNote].isLoaded;
}
