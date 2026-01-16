/*
  ==============================================================================

    Main.cpp
    Created: 16 Jan 2026
    Author:  PC

    16-Button Square MIDI Sampler - Based on Vivo JUCE framework

  ==============================================================================
*/

#include "juce.h"
#include "SamplerPlugin.h"
#include "SamplerEditor.h"

// Debug logging to file (same directory as executable)
static File getLogFile() { return File::getSpecialLocation(File::currentExecutableFile).getParentDirectory().getChildFile("debug.log"); }
static void logMessage(const String& msg)
{
    File logFile = getLogFile();
    logFile.appendText(Time::getCurrentTime().toString(true, true, true, true) + ": " + msg + "\n");
}

#define DEBUG_MIDI(msg) logMessage(msg)

//==============================================================================
String getMidiNoteName(int noteNumber)
{
    static const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = noteNumber / 12 - 1;
    int note = noteNumber % 12;
    return String(notes[note]) + String(octave);
}

//==============================================================================
class SettingsDialog : public DialogWindow
{
public:
    SettingsDialog(AudioDeviceManager& dm)
        : DialogWindow("Audio & MIDI Settings", Colours::darkgrey, true, true)
    {
        setContentOwned(new AudioDeviceSelectorComponent(
            dm,
            0, 2,
            0, 2,
            true,
            true,
            true,
            true
        ), true);

        setResizable(true, false);
        setSize(500, 450);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};

//==============================================================================
class SamplerApp : public JUCEApplication
{
public:
    SamplerApp() {}

    const String getApplicationName() override       { return "TostEngineJucePocketSampler"; }
    const String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override       { return true; }

    void initialise(const String&) override
    {
        DEBUG_MIDI("SamplerApp::initialise - starting");
        deviceManager.initialiseWithDefaultDevices(0, 2);

        // Check if audio device is actually open
        String audioDevice = deviceManager.getCurrentAudioDevice() ? deviceManager.getCurrentAudioDevice()->getName() : "NONE";
        DEBUG_MIDI(String("Audio device after init: ") + audioDevice);

        mainWindow.reset(new MainWindow(getApplicationName(), deviceManager));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        deviceManager.closeAudioDevice();
        LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->closeButtonPressed();
        else
            quit();
    }

    void anotherInstanceStarted(const String&) override {}

    class MainWindow : public DocumentWindow,
                       public MenuBarModel,
                       public MidiInputCallback
    {
    public:
        MainWindow(String name, AudioDeviceManager& dm)
            : DocumentWindow(name,
                Desktop::getInstance().getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                allButtons),
              deviceManager(dm)
        {
            DEBUG_MIDI("MainWindow::constructor starting");

            setMenuBar(this);

            plugin = std::make_unique<SamplerPlugin>();
            AudioProcessorEditor* editorPtr = plugin->createEditorIfNeeded();

            if (editorPtr != nullptr)
            {
                editor = std::unique_ptr<AudioProcessorEditor>(editorPtr);
                setContentOwned(editor.get(), true);
                editor->setSize(500, 560);
            }

            // Prepare the plugin for playback (44100 Hz sample rate, 512 samples per block)
            plugin->prepareToPlay(44100.0, 512);
            DEBUG_MIDI("MainWindow: Plugin prepared for playback");

            // Connect the plugin to the audio device for playback
            processorPlayer.setProcessor(plugin.get());
            deviceManager.addAudioCallback(&processorPlayer);
            DEBUG_MIDI("MainWindow: Audio processor player connected");

            // Open all available MIDI input devices
            openMidiInputs();

            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            DEBUG_MIDI("MainWindow::constructor complete");
        }

        ~MainWindow()
        {
            setMenuBar(nullptr);
            closeAllDevices();
            closeSettings();
        }

        void openMidiInputs()
        {
            DEBUG_MIDI("MainWindow::openMidiInputs - starting");

            // Close existing inputs
            for (auto* input : midiInputs)
            {
                if (input != nullptr)
                    input->stop();
            }
            midiInputs.clear();

            // List all available MIDI devices
            auto midiDevices = MidiInput::getAvailableDevices();
            DEBUG_MIDI("Found " + String(midiDevices.size()) + " MIDI input devices");

            for (int i = 0; i < midiDevices.size(); ++i)
            {
                const auto& device = midiDevices[i];
                DEBUG_MIDI("MIDI device " + String(i) + ": " + device.name + " (id: " + device.identifier + ")");

                auto midiInput = MidiInput::openDevice(device.identifier, this);
                if (midiInput != nullptr)
                {
                    DEBUG_MIDI("Opened MIDI device: " + device.name);
                    midiInputs.add(std::move(midiInput));
                    midiInputs.getLast()->start();
                    DEBUG_MIDI("Started MIDI device: " + device.name);
                }
                else
                {
                    DEBUG_MIDI("Failed to open MIDI device: " + device.name);
                }
            }

            DEBUG_MIDI("MainWindow::openMidiInputs - complete, opened " + String(midiInputs.size()) + " devices");
        }

        void closeSettings()
        {
            settingsDialog.reset();
        }

        void closeButtonPressed() override
        {
            closeAllDevices();
            closeSettings();
            editor.reset();
            plugin.reset();
            JUCEApplication::quit();
        }

        // MidiInputCallback
        void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& message) override
        {
            DEBUG_MIDI("MainWindow::handleIncomingMidiMessage - received MIDI from: " + (source ? source->getName() : "unknown"));

            if (message.isNoteOn())
            {
                DEBUG_MIDI("NOTE ON: ch=" + String(message.getChannel()) +
                          " note=" + String(message.getNoteNumber()) +
                          " (" + getMidiNoteName(message.getNoteNumber()) + ")" +
                          " vel=" + String(int(message.getVelocity() * 127)));
            }
            else if (message.isNoteOff())
            {
                DEBUG_MIDI("NOTE OFF: ch=" + String(message.getChannel()) +
                          " note=" + String(message.getNoteNumber()));
            }
            else if (message.isController())
            {
                DEBUG_MIDI("CONTROLLER: ch=" + String(message.getChannel()) +
                          " ctrl=" + String(message.getControllerNumber()) +
                          " val=" + String(message.getControllerValue()));
            }
            else if (message.isAftertouch())
            {
                DEBUG_MIDI("AFTERTOUCH: ch=" + String(message.getChannel()));
            }
            else
            {
                DEBUG_MIDI("OTHER MIDI: " + String(message.getRawDataSize()) + " bytes");
            }

            if (plugin != nullptr)
            {
                plugin->getMidiCollector().addMessageToQueue(message);

                // Update MIDI status display in editor
                if (editor != nullptr)
                {
                    auto* samplerEditor = dynamic_cast<SamplerEditor*>(editor.get());
                    if (samplerEditor != nullptr)
                    {
                        samplerEditor->getMidiStatus().showMidiMessage(message);
                        samplerEditor->handleMidiMessage(message);
                    }
                }

                if (midiOutput != nullptr)
                {
                    midiOutput->sendMessageNow(message);
                }
            }
        }

        StringArray getMenuBarNames() override
        {
            return { "Settings" };
        }

        PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName) override
        {
            PopupMenu menu;

            if (topLevelMenuIndex == 0)
            {
                PopupMenu::Item settingsItem("Audio & MIDI Settings...");
                settingsItem.action = [this]() { showSettings(); };
                menu.addItem(settingsItem);

                PopupMenu::Item githubItem("GitHub Repository...");
                githubItem.action = [this]() { URL("https://github.com/camenduru/TostEngine-juce-pocket-sampler").launchInDefaultBrowser(); };
                menu.addItem(githubItem);
            }

            return menu;
        }

        void menuItemSelected(int menuItemID, int topLevelMenuIndex) override
        {
            (void)menuItemID;
            (void)topLevelMenuIndex;
        }

    private:
        void showSettings()
        {
            settingsDialog = std::make_unique<SettingsDialog>(deviceManager);
            settingsDialog->setVisible(true);
        }

        void closeAllDevices()
        {
            // Disconnect the audio processor player first
            deviceManager.removeAudioCallback(&processorPlayer);
            processorPlayer.setProcessor(nullptr);

            for (auto* input : midiInputs)
            {
                if (input != nullptr)
                    input->stop();
            }
            midiInputs.clear();

            midiOutput.reset();
            midiOutputDeviceId = {};
        }

        std::unique_ptr<SamplerPlugin> plugin;
        std::unique_ptr<AudioProcessorEditor> editor;
        std::unique_ptr<SettingsDialog> settingsDialog;
        OwnedArray<MidiInput> midiInputs;
        std::unique_ptr<MidiOutput> midiOutput;
        AudioProcessorPlayer processorPlayer;  // Routes plugin audio to output
        AudioDeviceManager& deviceManager;
        String midiOutputDeviceId;
        int selectedMidiOutputIndex = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    AudioDeviceManager deviceManager;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SamplerApp)
