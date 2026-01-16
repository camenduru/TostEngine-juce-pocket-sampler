/*
  ==============================================================================

    SamplerPlugin.h
    Created: 16 Jan 2026
    Author:  PC

    16-Button Square MIDI Sampler

  ==============================================================================
*/

#pragma once

#include "juce.h"

//==============================================================================
// Forward declarations for one-shot mode access
namespace OneShotMode
{
    extern bool isEnabled;
    extern void setEnabled(bool enable);
    extern bool getEnabled();
}

//==============================================================================
class ButtonSample
{
public:
    ButtonSample() : sampleBuffer(nullptr), sourceSampleRate(0), isLoaded(false), rootNote(60) {}

    AudioSampleBuffer* sampleBuffer;
    String filePath;
    float sourceSampleRate;
    bool isLoaded;
    int rootNote;

    void clear()
    {
        if (sampleBuffer != nullptr)
        {
            delete sampleBuffer;
            sampleBuffer = nullptr;
        }
        filePath = "";
        sourceSampleRate = 0;
        isLoaded = false;
        rootNote = 60;
    }
};

//==============================================================================
// SynthesiserSound for each button sample
class ButtonSampleSound : public SynthesiserSound
{
public:
    ButtonSampleSound(int buttonIndex, AudioSampleBuffer* buffer, float sampleRate, int rootNote)
        : buttonIndex(buttonIndex), sampleBuffer(buffer), sourceSampleRate(sampleRate), rootNote(rootNote)
    {
    }

    bool appliesToNote(int midiNote) override { return midiNote == rootNote; }
    bool appliesToChannel(int midiChannel) override { (void)midiChannel; return true; }

    void setRootNote(int newRootNote) { rootNote = newRootNote; }
    int getRootNote() const { return rootNote; }

    int buttonIndex;
    AudioSampleBuffer* sampleBuffer;
    float sourceSampleRate;
    int rootNote;
};

//==============================================================================
class MidiSamplerVoice : public SynthesiserVoice
{
public:
    MidiSamplerVoice()
    {
    }

    bool canPlaySound(SynthesiserSound* sound) override
    {
        (void)sound;
        return true;
    }

    void startNote(int midiNoteNumber, float velocity, SynthesiserSound* sound, int currentPitchWheelPosition) override
    {
        (void)currentPitchWheelPosition;

        // Debug log
        File logFile = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getChildFile("debug.log");
        logFile.appendText(Time::getCurrentTime().toString(true, true, true, true) +
            ": MidiSamplerVoice::startNote note=" + String(midiNoteNumber) +
            " velocity=" + String(int(velocity * 127)) + "\n");

        this->midiNoteNumber = midiNoteNumber;
        this->velocity = velocity;
        this->isPlaying = true;
        this->position = 0;

        // Set up from sound
        if (auto* buttonSound = dynamic_cast<ButtonSampleSound*>(sound))
        {
            sampleBuffer = buttonSound->sampleBuffer;
            sourceSampleRate = buttonSound->sourceSampleRate;
            rootNote = buttonSound->rootNote;

            // Calculate pitch ratio for pitch shifting
            if (sourceSampleRate > 0)
            {
                pitchRatio = pow(2.0, (midiNoteNumber - rootNote) / 12.0);
            }
            else
            {
                pitchRatio = 1.0f;
            }
        }
        else
        {
            sampleBuffer = nullptr;
            pitchRatio = 1.0f;
        }
    }

    void stopNote(float velocity, bool allowTailOff) override
    {
        (void)velocity;

        // In one-shot mode, ignore note-off and let sample play to completion
        if (OneShotMode::getEnabled())
        {
            return;  // Don't stop - let sample play to completion
        }

        // Debug log
        File logFile = File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getChildFile("debug.log");
        logFile.appendText(Time::getCurrentTime().toString(true, true, true, true) +
            ": MidiSamplerVoice::stopNote note=" + String(midiNoteNumber) +
            " isPlaying=" + String(isPlaying ? "true" : "false") + "\n");

        isPlaying = false;
        if (!allowTailOff)
        {
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int newPitchWheelValue) override
    {
        (void)newPitchWheelValue;
    }

    void controllerMoved(int controllerNumber, int newControllerValue) override
    {
        (void)controllerNumber;
        (void)newControllerValue;
    }

    void renderNextBlock(AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (sampleBuffer == nullptr || !isPlaying)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            int outputIndex = startSample + i;

            // Read from sample
            float sample = 0.0f;
            for (int ch = 0; ch < sampleBuffer->getNumChannels(); ++ch)
            {
                float* channelData = sampleBuffer->getWritePointer(ch);
                int samplePos = static_cast<int>(position) % sampleBuffer->getNumSamples();
                sample += channelData[samplePos];
            }
            sample /= sampleBuffer->getNumChannels();

            // Apply pitch ratio
            position += pitchRatio;

            // Check if sample ended
            if (position >= sampleBuffer->getNumSamples())
            {
                isPlaying = false;
                clearCurrentNote();
                break;
            }

            // Write to output
            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            {
                outputBuffer.getWritePointer(ch)[outputIndex] += sample * velocity;
            }
        }
    }

    bool isPlayingNote() const { return isPlaying; }
    int getMidiNote() const { return midiNoteNumber; }

    void setSample(ButtonSampleSound* sound)
    {
        sampleBuffer = sound->sampleBuffer;
        sourceSampleRate = sound->sourceSampleRate;
        rootNote = sound->rootNote;
    }

private:
    bool isPlaying = false;
    float velocity = 0.0f;
    double position = 0.0;
    float pitchRatio = 1.0f;
    int midiNoteNumber = 60;
    int rootNote = 60;
    AudioSampleBuffer* sampleBuffer = nullptr;
    float sourceSampleRate = 44100.0;
};

//==============================================================================
class SamplerPlugin : public AudioProcessor
{
public:
    SamplerPlugin();
    ~SamplerPlugin();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return "16-Button MIDI Sampler"; }

    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { (void)index; }
    const String getProgramName(int index) override { (void)index; return "Default"; }
    void changeProgramName(int index, const String& newName) override { (void)index; (void)newName; }

    void getStateInformation(MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    Array<ButtonSample>& getButtons() { return buttons; }
    ButtonSample& getButton(int index) { return buttons.getReference(index); }

    void setNoteMapping(int buttonIndex, int midiNote);
    int getNoteMapping(int buttonIndex) const { return noteMapping[buttonIndex]; }

    bool loadSample(int buttonIndex, const File& file);
    void clearSample(int buttonIndex);

    // MIDI Learn sample assignment - assign sample to a MIDI note
    bool loadSampleForMidiNote(int midiNote, const File& file);
    void clearMidiNoteSample(int midiNote);
    ButtonSample& getMidiNoteSample(int midiNote) { return midiNoteSamples.getReference(midiNote); }
    bool hasSampleForMidiNote(int midiNote) const
    {
        return midiNote >= 0 && midiNote < midiNoteSamples.size() && midiNoteSamples[midiNote].isLoaded;
    }

    // Check if any sample is assigned to a MIDI note (for UI display)
    bool isMidiNoteAssigned(int midiNote) const;

    Synthesiser& getSynth() { return synth; }
    MidiMessageCollector& getMidiCollector() { return midiCollector; }

private:
    Synthesiser synth;
    Array<ButtonSample> buttons;
    Array<int> noteMapping;
    Array<ButtonSample> midiNoteSamples;  // Samples indexed by MIDI note (0-127)
    AudioFormatManager formatManager;
    MidiMessageCollector midiCollector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerPlugin)
};
