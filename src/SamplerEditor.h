/*
  ==============================================================================

    SamplerEditor.h
    Created: 16 Jan 2026
    Author:  PC

    16-Button Square MIDI Sampler - Editor UI

  ==============================================================================
*/

#pragma once

#include "juce.h"
#include "SamplerPlugin.h"

// Debug logging to file (same directory as executable)
static File getEditorLogFile() { return File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getChildFile("debug.log"); }
static void editorLogMessage(const String& msg)
{
    File logFile = getEditorLogFile();
    logFile.appendText(Time::getCurrentTime().toString(true, true, true, true) + ": " + msg + "\n");
}
#define DEBUG_MIDI(msg) editorLogMessage(msg)

//==============================================================================
// Helper function for MIDI note names
static String getMidiNoteDisplayName(int midiNote)
{
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (midiNote / 12) - 1;
    int note = midiNote % 12;
    return String(noteNames[note]) + String(octave);
}

//==============================================================================
// MIDI Status Display Component
class MidiStatusDisplay : public Component,
                          public Timer
{
public:
    MidiStatusDisplay()
    {
        lastNoteText = "---";
        velocityText = "---";
        channelText = "---";
        startTimerHz(30);
    }

    void timerCallback() override
    {
        if (fadeCounter > 0)
        {
            fadeCounter--;
            repaint();
        }
    }

    void showMidiMessage(const MidiMessage& msg)
    {
        if (msg.isNoteOn())
        {
            lastNoteText = getMidiNoteDisplayName(msg.getNoteNumber());
            velocityText = String(int(msg.getVelocity()));  // Already 0-127
            channelText = String(msg.getChannel());
            fadeCounter = 30;  // 1 second display
            repaint();
        }
    }

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.fillAll(Colour(0xFF101010));
        g.setColour(Colours::grey);
        g.drawRect(bounds, 1);

        auto textColour = fadeCounter > 0 ? Colours::cyan : Colours::darkblue;
        g.setColour(textColour);
        g.setFont(Font(12.0f).withTypefaceStyle("Regular"));

        int h = bounds.getHeight();
        int y = (h - 14) / 2;  // Center vertically in the 24px bar
        int w = bounds.getWidth();

        // Center each field across the full width
        g.drawText("CH:" + channelText, 0, y, w / 3, 14, Justification::centred);
        g.drawText("NOTE:" + lastNoteText, w / 3, y, w / 3, 14, Justification::centred);
        g.drawText("VEL:" + velocityText, (w / 3) * 2, y, w / 3, 14, Justification::centred);
    }

private:
    String lastNoteText;
    String velocityText;
    String channelText;
    int fadeCounter = 0;
};

//==============================================================================
class SamplerButtonUI : public Component
{
public:
    SamplerButtonUI(int index, SamplerPlugin& plugin);
    ~SamplerButtonUI();

    void paint(Graphics& g) override;
    void resized() override;
    void mouseDown(const MouseEvent& e) override;
    void mouseDoubleClick(const MouseEvent& e) override;

    void setFileName(const String& name);
    void setLoaded(bool loaded) { isLoaded = loaded; }
    void setActive(bool active);
    bool getIsActive() const;
    void flash();
    void setVelocity(float vel) { velocity = vel; repaint(); }
    float getVelocity() const { return velocity; }

    int getButtonIndex() const { return buttonIndex; }

private:
    int buttonIndex;
    SamplerPlugin& sampler;
    String fileName;
    bool isActive;
    bool isLoaded;
    float velocity;  // Current velocity (0.0 to 1.0)
    Colour defaultColor;
    Colour activeColor;
    Colour loadedColor;
    Colour activeBorderColor;
    Rectangle<float> flashRect;
    float flashAlpha;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerButtonUI)
};

//==============================================================================
class SamplerEditor : public AudioProcessorEditor,
                      public Timer,
                      public Button::Listener
{
public:
    SamplerEditor(SamplerPlugin& plugin);
    ~SamplerEditor();

    void paint(Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void buttonClicked(Button* button) override;

    // MIDI note names for display
    static String getNoteName(int midiNote);

    void loadSampleForButton(int buttonIndex);

    // Handle MIDI message for learn mode
    void handleMidiMessage(const MidiMessage& msg);

    // Set the button index for MIDI learning
    void setLearningButtonIndex(int index)
    {
        learningButtonIndex = index;
        midiLearnLabel.setText("Listening... press a MIDI key", NotificationType::sendNotification);
        DEBUG_MIDI(String("setLearningButtonIndex: button=") + String(index));
    }

    // Timer for sending note-off after clicking a pad
    void startNoteOffTimer(int buttonIndex, int midiNote)
    {
        // Store the note-off info and start a short timer
        pendingNoteOffButton = buttonIndex;
        pendingNoteOffNote = midiNote;
        startTimerHz(100);  // 100Hz - check every 10ms
        noteOffStartTime = Time::getMillisecondCounter();
        DEBUG_MIDI(String("Started note-off timer for button ") + String(buttonIndex) + " note " + String(midiNote));
    }

    // Pending sample for MIDI learn assignment
    File pendingSampleFile;
    int sampleLearnButtonIndex = -1;  // Button index for sample learn

public:
    MidiStatusDisplay& getMidiStatus() { return midiStatus; }
    bool isMidiLearning = false;  // Made public for button access
    bool isSampleLearning = false;  // Sample learn mode active
    bool isOneShotMode = false;  // One-shot mode: play sample to completion regardless of note-off

    // Removed setOneShotMode/getOneShotMode from SamplerPlugin - now using OneShotMode namespace

    // Get the MIDI note mapping for a button
    int getNoteMapping(int buttonIndex) const { return sampler.getNoteMapping(buttonIndex); }

    // Get velocity for a mapped note (0.0 to 1.0, or 0 if not playing)
    float getNoteVelocity(int mappedNote) const
    {
        if (mappedNote >= 0 && mappedNote < noteVelocities.size())
            return noteVelocities[mappedNote];
        return 0.0f;
    }

private:
    SamplerPlugin& sampler;
    OwnedArray<SamplerButtonUI> buttons;
    TextButton midiLearnButton;
    TextButton sampleLearnButton;
    ToggleButton oneShotButton;  // One-shot mode toggle
    TextButton exportButton;  // Export settings to JSON
    TextButton importButton;  // Import settings from JSON
    Label midiLearnLabel;
    MidiStatusDisplay midiStatus;
    int learningButtonIndex;  // Button currently listening for MIDI note (-1 = none)
    Array<float> noteVelocities;  // Track velocity for each note (0-127), 0 = not playing
    Array<bool> notePlaying;  // Track which notes are currently playing (size 128) - kept for compatibility

    // File chooser for sample loading - kept alive to ensure callback executes
    std::unique_ptr<FileChooser> fileChooser;
    std::function<void(const FileChooser&)> fileChooserCallback;

    // File chooser for import/export
    std::unique_ptr<FileChooser> jsonFileChooser;
    std::function<void(const FileChooser&)> jsonFileChooserCallback;

    // Note-off timer for click-triggered samples
    int pendingNoteOffButton = -1;
    int pendingNoteOffNote = -1;
    uint32 noteOffStartTime = 0;
    static const int NOTE_OFF_DELAY_MS = 200;  // How long each sample plays when clicked

    // Import/export methods
    void exportAllSettings();
    void importAllSettings();
    bool loadAllSamplesFromJson(const File& jsonFile);

    // Auto-load last JSON file
    void saveLastJsonFile(const File& file);
    void loadLastJsonFileOnStartup();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerEditor)
};
