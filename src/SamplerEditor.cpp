/*
  ==============================================================================

    SamplerEditor.cpp
    Created: 16 Jan 2026
    Author:  PC

    16-Button Square MIDI Sampler - Editor UI

  ==============================================================================
*/

#include "SamplerEditor.h"

//==============================================================================
String SamplerEditor::getNoteName(int midiNote)
{
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (midiNote / 12) - 1;
    int note = midiNote % 12;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%s%d", noteNames[note], octave);
    return String(buffer);
}

//==============================================================================
SamplerButtonUI::SamplerButtonUI(int index, SamplerPlugin& plugin)
    : buttonIndex(index), sampler(plugin), isActive(false), isLoaded(false),
      velocity(0.0f), flashAlpha(0.0f)
{
    DEBUG_MIDI("SamplerButtonUI::constructor for button " + String(index));
    defaultColor = Colour(0xFF404040);
    activeColor = Colour(0xFF00AA00);
    loadedColor = Colour(0xFF606060);
    activeBorderColor = Colour(0xFF00FF00);
}

SamplerButtonUI::~SamplerButtonUI()
{
}

void SamplerButtonUI::paint(Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Calculate color based on active state and velocity
    Colour bgColour;
    if (isActive)
    {
        // Brightness increases with velocity (0.0 = dim green, 1.0 = bright green)
        bgColour = Colour::fromRGB(
            static_cast<uint8>(0 + velocity * 50),      // R: 0-50
            static_cast<uint8>(100 + velocity * 155),   // G: 100-255
            static_cast<uint8>(0 + velocity * 50)       // B: 0-50
        );
    }
    else
    {
        bgColour = isLoaded ? loadedColor : defaultColor;
    }

    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Draw border - also affected by velocity
    if (isActive)
    {
        Colour borderColour = Colour::fromRGB(
            static_cast<uint8>(0 + velocity * 100),
            static_cast<uint8>(200 + velocity * 55),
            static_cast<uint8>(0 + velocity * 100)
        );
        g.setColour(borderColour);
    }
    else
    {
        g.setColour(Colours::darkgrey);
    }
    g.drawRoundedRectangle(bounds.reduced(2.0f), 6.0f, 2.0f);

    // Draw flash effect
    if (flashAlpha > 0.0f)
    {
        g.setColour(Colours::white.withAlpha(flashAlpha));
        g.fillRoundedRectangle(bounds, 8.0f);
        flashAlpha -= 0.1f;
        if (flashAlpha < 0.0f) flashAlpha = 0.0f;
    }

    // Draw file name or button number
    g.setColour(Colours::white);
    g.setFont(12.0f);

    String displayText;
    if (isLoaded && fileName.isNotEmpty())
    {
        // Truncate long file names
        if (fileName.length() > 12)
        {
            displayText = fileName.substring(0, 10) + "..";
        }
        else
        {
            displayText = fileName;
        }
    }
    else
    {
        displayText = String(buttonIndex + 1);
    }

    // Draw centered text
    g.drawText(displayText, bounds, Justification::centred, true);

    // Draw MIDI note
    g.setFont(9.0f);
    g.setColour(Colours::silver);
    String noteName = SamplerEditor::getNoteName(sampler.getNoteMapping(buttonIndex));
    g.drawText(noteName, bounds.removeFromBottom(14), Justification::centred, true);
}

void SamplerButtonUI::resized()
{
    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    flashRect = bounds;
}

void SamplerButtonUI::mouseDown(const MouseEvent& e)
{
    DEBUG_MIDI("mouseDown: button=" + String(buttonIndex) + " rightBtn=" + String(e.mods.isRightButtonDown() ? 1 : 0));

    if (e.mods.isRightButtonDown())
    {
        DEBUG_MIDI("Right click - clearing sample");
        // Right click to clear sample
        sampler.clearSample(buttonIndex);
        fileName = "";
        isLoaded = false;
        repaint();
        return;
    }

    // Find the editor parent
    Component* parent = getParentComponent();
    if (parent == nullptr)
    {
        DEBUG_MIDI("ERROR: getParentComponent() is nullptr");
        return;
    }

    // Traverse up to find SamplerEditor
    Component* editorParent = parent;
    while (editorParent != nullptr && dynamic_cast<SamplerEditor*>(editorParent) == nullptr)
    {
        editorParent = editorParent->getParentComponent();
    }

    if (editorParent == nullptr)
    {
        DEBUG_MIDI("ERROR: Could not find SamplerEditor in parent hierarchy");
        return;
    }

    auto* editor = dynamic_cast<SamplerEditor*>(editorParent);
    if (editor == nullptr)
    {
        DEBUG_MIDI("ERROR: dynamic_cast to SamplerEditor failed");
        return;
    }

    DEBUG_MIDI("mouseDown: isMidiLearning=" + String(editor->isMidiLearning ? 1 : 0) + " isSampleLearning=" + String(editor->isSampleLearning ? 1 : 0));

    if (editor->isMidiLearning)
    {
        DEBUG_MIDI("MIDI Learn mode active - setting learning button");
        // Enter listening mode for this button
        editor->setLearningButtonIndex(buttonIndex);
        return;
    }

    // Check if we're in Sample Learn mode
    if (editor->isSampleLearning)
    {
        DEBUG_MIDI(String("Sample Learn mode ACTIVE - calling loadSampleForButton for button ") + String(buttonIndex));
        // Open file chooser to load sample
        editor->loadSampleForButton(buttonIndex);
        return;
    }

    // Left click - trigger the sample (flash effect and sound)
    DEBUG_MIDI("Normal click - triggering sample flash");
    setActive(true);
    flash();
    repaint();

    // Also trigger the sound directly for this button via MIDI
    int mappedNote = editor->getNoteMapping(buttonIndex);
    DEBUG_MIDI("Click triggering sound for mapped note " + String(mappedNote));

    // Send a note-on message to the synth to play the sample
    MidiMessage noteOn = MidiMessage::noteOn(1, mappedNote, uint8(100));
    sampler.getMidiCollector().addMessageToQueue(noteOn);

    // Schedule note-off after a short duration (for one-shot samples)
    // Use a timer to send note-off
    editor->startNoteOffTimer(buttonIndex, mappedNote);
}

void SamplerButtonUI::mouseDoubleClick(const MouseEvent& e)
{
    (void)e;
    // Double click to load a sample - call parent's method
    if (getParentComponent() != nullptr)
    {
        // Find the SamplerEditor parent
        Component* parent = getParentComponent();
        while (parent != nullptr && dynamic_cast<SamplerEditor*>(parent) == nullptr)
        {
            parent = parent->getParentComponent();
        }
        if (auto* editor = dynamic_cast<SamplerEditor*>(parent))
        {
            editor->loadSampleForButton(buttonIndex);
        }
    }
}

void SamplerButtonUI::setFileName(const String& name)
{
    fileName = name;
    isLoaded = name.isNotEmpty();
    repaint();
}

void SamplerButtonUI::setActive(bool active)
{
    isActive = active;
    repaint();
}

bool SamplerButtonUI::getIsActive() const
{
    return isActive;
}

void SamplerButtonUI::flash()
{
    flashAlpha = 0.5f;
}

//==============================================================================
SamplerEditor::SamplerEditor(SamplerPlugin& plugin)
    : AudioProcessorEditor(plugin), sampler(plugin), isMidiLearning(false), learningButtonIndex(-1)
{
    DEBUG_MIDI("SamplerEditor::constructor ENTRY");

    // Initialize note tracking array
    notePlaying.resize(128);
    notePlaying.fill(false);

    // Initialize velocity tracking array
    noteVelocities.resize(128);
    noteVelocities.fill(0.0f);

    setSize(500, 545);
    DEBUG_MIDI("SamplerEditor: setSize to 500x545");

    // MIDI Learn button at top
    midiLearnButton.setButtonText("MIDI Learn");
    midiLearnButton.addListener(this);
    midiLearnButton.setBounds(15, 10, 90, 22);
    addAndMakeVisible(&midiLearnButton);
    DEBUG_MIDI("SamplerEditor: added MIDI Learn button");

    // Sample Learn button (next to MIDI Learn)
    sampleLearnButton.setButtonText("Sample Learn");
    sampleLearnButton.addListener(this);
    sampleLearnButton.setBounds(115, 10, 100, 22);
    addAndMakeVisible(&sampleLearnButton);
    DEBUG_MIDI("SamplerEditor: added Sample Learn button");

    // One-Shot Mode toggle (after Sample Learn button)
    oneShotButton.setButtonText("One-Shot");
    oneShotButton.addListener(this);
    oneShotButton.setBounds(225, 10, 80, 22);
    oneShotButton.setToggleState(true, NotificationType::dontSendNotification);  // Default ON
    isOneShotMode = true;
    OneShotMode::setEnabled(true);  // Enable one-shot mode by default
    addAndMakeVisible(&oneShotButton);
    DEBUG_MIDI("SamplerEditor: added One-Shot button (default ON)");

    // Export button (next to One-Shot)
    exportButton.setButtonText("Export");
    exportButton.addListener(this);
    exportButton.setBounds(315, 10, 70, 22);
    addAndMakeVisible(&exportButton);
    DEBUG_MIDI("SamplerEditor: added Export button");

    // Import button (next to Export)
    importButton.setButtonText("Import");
    importButton.addListener(this);
    importButton.setBounds(395, 10, 70, 22);
    addAndMakeVisible(&importButton);
    DEBUG_MIDI("SamplerEditor: added Import button");

    // MIDI Learn status label
    midiLearnLabel.setText("One-Shot: ON - samples play to end", NotificationType::dontSendNotification);
    midiLearnLabel.setColour(Label::textColourId, Colours::yellow);
    midiLearnLabel.setBounds(15, 515, 470, 22);  // Moved below status bar
    addAndMakeVisible(&midiLearnLabel);
    DEBUG_MIDI("SamplerEditor: added MIDI Learn label");

    // Create 16 buttons in a 4x4 grid
    // Button 1 at bottom-left, Button 16 at top-right
    int buttonSize = 110;
    int margin = 10;
    int startX = 15;
    int startY = 40;

    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            // Invert row so button 1 is at bottom-left
            int index = (3 - row) * 4 + col;
            SamplerButtonUI* button = new SamplerButtonUI(index, sampler);
            button->setBounds(startX + col * (buttonSize + margin),
                            startY + row * (buttonSize + margin),
                            buttonSize, buttonSize);
            addAndMakeVisible(button);
            buttons.add(button);
            DEBUG_MIDI("SamplerEditor: added button " + String(index) + " at (" + String(startX + col * (buttonSize + margin)) + "," + String(startY + row * (buttonSize + margin)) + ")");
        }
    }
    DEBUG_MIDI("SamplerEditor: created " + String(buttons.size()) + " buttons");

    // Add MIDI status display at bottom (below pads)
    // Pads end at Y=510, so status starts at 515
    midiStatus.setBounds(15, 515, 470, 24);
    addAndMakeVisible(midiStatus);
    DEBUG_MIDI("SamplerEditor: added MIDI status display");

    // Start timer for button state updates
    startTimer(50);
    DEBUG_MIDI("SamplerEditor: timer started");

    // Update button states with loaded file names
    // Must iterate through buttons array and match by buttonIndex, not array index
    for (int i = 0; i < buttons.size(); i++)
    {
        int btnIdx = buttons[i]->getButtonIndex();
        if (sampler.getButton(btnIdx).isLoaded)
        {
            File file(sampler.getButton(btnIdx).filePath);
            buttons[i]->setFileName(file.getFileName());
            buttons[i]->setLoaded(true);  // Also set the loaded flag
            String dbgMsg = "Restored: buttonIndex " + String(btnIdx) + " (array pos " + String(i) + ") -> " + file.getFileName();
            DEBUG_MIDI(dbgMsg);
        }
    }

    // Auto-load last JSON file if exists
    loadLastJsonFileOnStartup();

    DEBUG_MIDI("SamplerEditor::constructor EXIT");
}

SamplerEditor::~SamplerEditor()
{
    stopTimer();
}

void SamplerEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF202020));
}

void SamplerEditor::resized()
{
    // Layout is fixed for now
}

void SamplerEditor::timerCallback()
{
    // Handle note-off timer for click-triggered samples
    if (pendingNoteOffButton >= 0 && pendingNoteOffNote >= 0)
    {
        uint32 currentTime = Time::getMillisecondCounter();
        if (currentTime - noteOffStartTime >= NOTE_OFF_DELAY_MS)
        {
            // Send note-off message
            MidiMessage noteOff = MidiMessage::noteOff(1, pendingNoteOffNote, uint8(0));
            sampler.getMidiCollector().addMessageToQueue(noteOff);
            DEBUG_MIDI(String("Sent note-off for note ") + String(pendingNoteOffNote));

            // Reset the pending note-off
            pendingNoteOffButton = -1;
            pendingNoteOffNote = -1;

            // If we don't have any notes playing, stop the high-frequency timer
            // and restart the normal 50ms timer
            bool anyNotesPlaying = false;
            for (int i = 0; i < notePlaying.size() && i < 128; i++)
            {
                if (notePlaying[i])
                {
                    anyNotesPlaying = true;
                    break;
                }
            }
            if (!anyNotesPlaying)
            {
                stopTimer();
                startTimer(50);
                DEBUG_MIDI("Reverted to normal 50ms timer");
            }
        }
        // Continue at high frequency if note-off is still pending
        return;
    }

    // Update active state based on tracked MIDI notes
    for (int i = 0; i < 16; i++)
    {
        bool wasActive = buttons[i]->getIsActive();
        int buttonIdx = buttons[i]->getButtonIndex();  // Get the actual button index stored in the button
        int mappedNote = sampler.getNoteMapping(buttonIdx);

        // Check if this mapped note is currently playing
        bool shouldBeActive = false;
        float velocity = 0.0f;
        if (mappedNote >= 0 && mappedNote < 128 && notePlaying.size() > mappedNote)
        {
            shouldBeActive = notePlaying[mappedNote];
            velocity = noteVelocities.size() > mappedNote ? noteVelocities[mappedNote] : 0.0f;
        }

        if (wasActive != shouldBeActive)
        {
            buttons[i]->setActive(shouldBeActive);
            buttons[i]->setVelocity(velocity);
            if (shouldBeActive)
            {
                String msg = "timerCallback: button " + String(buttonIdx + 1) + " ACTIVE (note " + String(mappedNote) + ")";
                DEBUG_MIDI(msg);
            }
            else
            {
                DEBUG_MIDI(String("timerCallback: button ") + String(buttonIdx + 1) + " INACTIVE");
            }
        }
        else if (shouldBeActive && buttons[i]->getVelocity() != velocity)
        {
            // Update velocity even if active state hasn't changed
            buttons[i]->setVelocity(velocity);
        }
    }
}

void SamplerEditor::buttonClicked(Button* button)
{
    DEBUG_MIDI(String("buttonClicked: ") + button->getButtonText());

    if (button == &midiLearnButton)
    {
        // Toggle MIDI learn mode - user selects a button, then presses MIDI key
        isMidiLearning = !isMidiLearning;
        DEBUG_MIDI("MIDI Learn button clicked: isMidiLearning=" + String(isMidiLearning ? 1 : 0));
        if (isMidiLearning)
        {
            midiLearnButton.setButtonText("Cancel Learn");
            midiLearnLabel.setText("Click a button, then press MIDI key...", NotificationType::sendNotification);
            learningButtonIndex = -1;  // No button selected yet
            // Exit sample learn mode if active
            if (isSampleLearning)
            {
                isSampleLearning = false;
                sampleLearnButton.setButtonText("Sample Learn");
                DEBUG_MIDI("Exited sample learn mode due to MIDI learn");
            }
        }
        else
        {
            midiLearnButton.setButtonText("MIDI Learn");
            midiLearnLabel.setText("Click Sample Learn, then click a pad to assign...", NotificationType::dontSendNotification);
            learningButtonIndex = -1;
        }
    }
    else if (button == &sampleLearnButton)
    {
        // Toggle Sample Learn mode
        isSampleLearning = !isSampleLearning;
        DEBUG_MIDI("Sample Learn button toggled: isSampleLearning=" + String(isSampleLearning ? 1 : 0));
        if (isSampleLearning)
        {
            sampleLearnButton.setButtonText("Cancel Sample");
            midiLearnLabel.setText("Click a pad to select audio file...", NotificationType::sendNotification);
            sampleLearnButtonIndex = -1;
            // Exit MIDI learn mode if active
            if (isMidiLearning)
            {
                isMidiLearning = false;
                midiLearnButton.setButtonText("MIDI Learn");
                learningButtonIndex = -1;
                DEBUG_MIDI("Exited MIDI learn mode due to sample learn");
            }
        }
        else
        {
            sampleLearnButton.setButtonText("Sample Learn");
            midiLearnLabel.setText("Click Sample Learn, then click a pad to assign...", NotificationType::dontSendNotification);
            sampleLearnButtonIndex = -1;
        }
    }
    else if (button == &oneShotButton)
    {
        // Toggle One-Shot mode
        isOneShotMode = oneShotButton.getToggleState();
        OneShotMode::setEnabled(isOneShotMode);
        DEBUG_MIDI("One-Shot mode toggled: " + String(isOneShotMode ? "ON" : "OFF"));
        if (isOneShotMode)
        {
            midiLearnLabel.setText("One-Shot: ON - samples play to end", NotificationType::sendNotification);
        }
        else
        {
            midiLearnLabel.setText("One-Shot: OFF - normal playback", NotificationType::sendNotification);
        }
    }
    else if (button == &exportButton)
    {
        // Export all settings to JSON
        DEBUG_MIDI("Export button clicked");
        exportAllSettings();
    }
    else if (button == &importButton)
    {
        // Import all settings from JSON
        DEBUG_MIDI("Import button clicked");
        importAllSettings();
    }
}

void SamplerEditor::loadSampleForButton(int buttonIndex)
{
    DEBUG_MIDI(String("loadSampleForButton ENTRY for button ") + String(buttonIndex));
    DEBUG_MIDI(String("  buttons array size = ") + String(buttons.size()));

    // Find the actual button in the buttons array by matching buttonIndex
    SamplerButtonUI* targetButton = nullptr;
    for (int i = 0; i < buttons.size(); i++)
    {
        if (buttons[i]->getButtonIndex() == buttonIndex)
        {
            targetButton = buttons[i];
            DEBUG_MIDI(String("Found button at array position ") + String(i) + " with buttonIndex=" + String(buttonIndex));
            break;
        }
    }

    if (targetButton == nullptr)
    {
        DEBUG_MIDI("ERROR: Could not find button with buttonIndex=" + String(buttonIndex));
        return;
    }

    // Use home directory as starting point
    File initialDir = File::getSpecialLocation(File::userHomeDirectory);

    DEBUG_MIDI("About to create FileChooser");

    // Create FileChooser as a member variable so it stays alive for the callback
    fileChooser = std::make_unique<FileChooser>("Select Audio Sample", initialDir, "*.*");

    DEBUG_MIDI("FileChooser created, launching async...");

    // Use a std::function to store the callback, then call launchAsync
    // This keeps the chooser alive until callback executes
    fileChooserCallback = [this, buttonIndex, targetButton](const FileChooser& fc)
    {
        DEBUG_MIDI(String("File chooser callback triggered for buttonIndex=") + String(buttonIndex));
        File file = fc.getResult();
        if (file.exists())
        {
            DEBUG_MIDI(String("File selected: ") + file.getFullPathName());
            // Get the MIDI note mapped to this button
            int midiNote = sampler.getNoteMapping(buttonIndex);
            DEBUG_MIDI(String("Button ") + String(buttonIndex) + " mapped to MIDI note " + String(midiNote));

            // Load sample for the MIDI note
            if (sampler.loadSampleForMidiNote(midiNote, file))
            {
                midiLearnLabel.setText("Sample assigned to pad " + String(buttonIndex + 1),
                                       NotificationType::sendNotification);
                DEBUG_MIDI(String("Sample assigned to note ") + String(midiNote) + " (pad " + String(buttonIndex + 1) + ")");

                // Update button UI using the found button pointer
                DEBUG_MIDI(String("Updating button with buttonIndex=") + String(buttonIndex) + " with fileName");
                targetButton->setFileName(file.getFileName());
                // Trigger the button visuals
                targetButton->setActive(true);
                targetButton->flash();
                targetButton->repaint();
                DEBUG_MIDI(String("Button ") + String(buttonIndex) + " UI updated successfully");

                // Play the sample immediately
                DEBUG_MIDI("Playing sample immediately after assignment");
                MidiMessage noteOn = MidiMessage::noteOn(1, midiNote, uint8(100));
                sampler.getMidiCollector().addMessageToQueue(noteOn);

                // Schedule note-off
                startNoteOffTimer(buttonIndex, midiNote);
            }
            else
            {
                midiLearnLabel.setText("Failed to load sample", NotificationType::sendNotification);
                DEBUG_MIDI("Failed to load sample");
            }
        }
        else
        {
            DEBUG_MIDI("No file selected or file doesn't exist");
        }

        // Exit sample learn mode
        isSampleLearning = false;
        sampleLearnButtonIndex = -1;
        sampleLearnButton.setButtonText("Sample Learn");
        DEBUG_MIDI("Sample learn mode exited");
    };

    // Launch the file browser - fileChooser stays alive as a member variable
    fileChooser->launchAsync(FileBrowserComponent::openMode, fileChooserCallback);

    DEBUG_MIDI("loadSampleForButton EXIT");
}

void SamplerEditor::handleMidiMessage(const MidiMessage& msg)
{
    int note = msg.getNoteNumber();

    // Track which notes are playing (for pad visualization)
    if (msg.isNoteOn() && note >= 0 && note < 128)
    {
        if (notePlaying.size() <= note)
            notePlaying.resize(128);
        if (noteVelocities.size() <= note)
            noteVelocities.resize(128);

        float velocity = msg.getVelocity() / 127.0f;  // Normalize to 0-1
        notePlaying.set(note, true);
        noteVelocities.set(note, velocity);
        DEBUG_MIDI(String("NOTE ON: note ") + String(note) + " vel=" + String(int(velocity * 127)));
    }
    else if (msg.isNoteOff() && note >= 0 && note < 128)
    {
        if (notePlaying.size() <= note)
            notePlaying.resize(128);
        if (noteVelocities.size() <= note)
            noteVelocities.resize(128);

        notePlaying.set(note, false);
        noteVelocities.set(note, 0.0f);
        DEBUG_MIDI(String("NOTE OFF: note ") + String(note));
    }

    // Handle MIDI learn - assign note to selected button
    if (isMidiLearning && learningButtonIndex >= 0 && msg.isNoteOn())
    {
        int buttonToRepaint = learningButtonIndex;  // Save for repaint

        DEBUG_MIDI(String("MIDI LEARN: assigning note ") + String(note) + " to button " + String(learningButtonIndex));

        sampler.setNoteMapping(learningButtonIndex, note);

        String buttonNum = String(learningButtonIndex + 1);
        midiLearnLabel.setText("Button " + buttonNum + " -> " + getNoteName(note),
                               NotificationType::sendNotification);

        // Exit learn mode
        isMidiLearning = false;
        learningButtonIndex = -1;
        midiLearnButton.setButtonText("MIDI Learn");

        DEBUG_MIDI(String("MIDI LEARN complete: button ") + buttonNum + " now mapped to note " + getNoteName(note));

        // Refresh button display to show new note name
        if (buttonToRepaint >= 0 && buttonToRepaint < buttons.size())
            buttons[buttonToRepaint]->repaint();
    }
}

//==============================================================================
// Export all settings to JSON file
void SamplerEditor::exportAllSettings()
{
    File initialDir = File::getSpecialLocation(File::userHomeDirectory);
    jsonFileChooser = std::make_unique<FileChooser>("Export Settings", initialDir, "*.json");

    jsonFileChooser->launchAsync(FileBrowserComponent::saveMode, [this](const FileChooser& fc)
    {
        File file = fc.getResult();
        // Add .json extension if not present
        File targetFile = file.getFileExtension().isEmpty() ? File(file.getFullPathName() + ".json") : file;

        // Build JSON structure using DynamicObject
        DynamicObject* jsonObj = new DynamicObject();
        jsonObj->setProperty("version", "1.0");
        jsonObj->setProperty("oneShotMode", isOneShotMode);

        // Export button mappings
        // NOTE: Samples are stored in midiNoteSamples, not buttons array
        // So we need to get the file path from midiNoteSamples based on the button's MIDI note mapping
        Array<var> buttonsArray;
        for (int i = 0; i < 16; i++)
        {
            DynamicObject* buttonObj = new DynamicObject();
            buttonObj->setProperty("index", i);
            buttonObj->setProperty("midiNote", sampler.getNoteMapping(i));

            // Get the MIDI note this button is mapped to, then check midiNoteSamples
            int mappedNote = sampler.getNoteMapping(i);
            if (sampler.hasSampleForMidiNote(mappedNote))
            {
                buttonObj->setProperty("filePath", sampler.getMidiNoteSample(mappedNote).filePath);
            }
            else
            {
                buttonObj->setProperty("filePath", "");
            }

            buttonsArray.add(var(buttonObj));
        }
        jsonObj->setProperty("buttons", var(buttonsArray));

        // Export MIDI note samples (128 notes)
        Array<var> midiNotesArray;
        for (int i = 0; i < 128; i++)
        {
            DynamicObject* noteObj = new DynamicObject();
            noteObj->setProperty("midiNote", i);

            if (sampler.getMidiNoteSample(i).isLoaded)
            {
                noteObj->setProperty("filePath", sampler.getMidiNoteSample(i).filePath);
            }
            else
            {
                noteObj->setProperty("filePath", "");
            }

            midiNotesArray.add(var(noteObj));
        }
        jsonObj->setProperty("midiNotes", var(midiNotesArray));

        // Write JSON to file
        std::unique_ptr<FileOutputStream> outputStream(targetFile.createOutputStream());
        if (outputStream != nullptr)
        {
            JSON::writeToStream(*outputStream, var(jsonObj));
            outputStream->flush();  // Make sure data is written
            outputStream.reset();  // Close and release
            DEBUG_MIDI("Exported settings to: " + targetFile.getFullPathName());
            midiLearnLabel.setText("Exported to: " + targetFile.getFileName(), NotificationType::sendNotification);
            saveLastJsonFile(targetFile);  // Save as last used file
        }
        else
        {
            DEBUG_MIDI("Error: Could not create output stream for export");
            midiLearnLabel.setText("Export failed!", NotificationType::sendNotification);
        }
    });
}

//==============================================================================
// Import all settings from JSON file
void SamplerEditor::importAllSettings()
{
    File initialDir = File::getSpecialLocation(File::userHomeDirectory);
    jsonFileChooser = std::make_unique<FileChooser>("Import Settings", initialDir, "*.json");

    jsonFileChooserCallback = [this](const FileChooser& fc)
    {
        File file = fc.getResult();
        if (file.exists())
        {
            bool success = loadAllSamplesFromJson(file);
            if (success)
            {
                midiLearnLabel.setText("Imported: " + file.getFileName(), NotificationType::sendNotification);
                DEBUG_MIDI(String("Import successful: ") + file.getFileName());
                saveLastJsonFile(file);  // Save as last used file
            }
            else
            {
                midiLearnLabel.setText("Import failed!", NotificationType::sendNotification);
                DEBUG_MIDI(String("Import failed for file: ") + file.getFullPathName());
            }
        }
        else
        {
            DEBUG_MIDI("Import cancelled - no file selected");
        }
    };

    jsonFileChooser->launchAsync(FileBrowserComponent::openMode, jsonFileChooserCallback);
}

//==============================================================================
// Load all samples from JSON file
bool SamplerEditor::loadAllSamplesFromJson(const File& jsonFile)
{
    DEBUG_MIDI(String("loadAllSamplesFromJson ENTRY: ") + jsonFile.getFullPathName());

    var json = JSON::parse(jsonFile);

    if (!json.isObject())
    {
        DEBUG_MIDI("ERROR: JSON is not an object");
        return false;
    }

    DynamicObject* jsonObj = json.getDynamicObject();
    if (jsonObj == nullptr)
    {
        DEBUG_MIDI("ERROR: Could not get DynamicObject from JSON");
        return false;
    }

    // Debug: show what properties exist in the JSON
    DEBUG_MIDI(String("JSON has ") + String(jsonObj->getProperties().size()) + " properties");
    NamedValueSet props = jsonObj->getProperties();
    for (int i = 0; i < props.size(); i++)
    {
        String propName = props.getName(i).toString();
        DEBUG_MIDI(String("  Property: ") + propName);
    }

    int loadedCount = 0;
    int failedCount = 0;

    // Load one-shot mode
    if (jsonObj->hasProperty("oneShotMode"))
    {
        DEBUG_MIDI("Found oneShotMode property");
        isOneShotMode = jsonObj->getProperty("oneShotMode");
        oneShotButton.setToggleState(isOneShotMode, NotificationType::dontSendNotification);
        OneShotMode::setEnabled(isOneShotMode);
    }
    else
    {
        DEBUG_MIDI("oneShotMode property NOT found");
    }

    // Load button mappings only - samples are loaded from midiNotes section
    if (jsonObj->hasProperty("buttons"))
    {
        DEBUG_MIDI("Found buttons property");
        var buttonsArray = jsonObj->getProperty("buttons");
        DEBUG_MIDI(String("buttons array size: ") + String(buttonsArray.size()));
        if (buttonsArray.isArray())
        {
            for (int i = 0; i < buttonsArray.size(); i++)
            {
                var buttonObj = buttonsArray[i];
                if (buttonObj.isObject())
                {
                    DynamicObject* btnObj = buttonObj.getDynamicObject();
                    if (btnObj)
                    {
                        int index = btnObj->getProperty("index");
                        int midiNote = btnObj->getProperty("midiNote");
                        String filePath = btnObj->getProperty("filePath").toString();

                        DEBUG_MIDI(String("Button ") + String(index) + ": midiNote=" + String(midiNote) + " filePath=\"" + filePath + "\"");

                        // Set note mapping only - samples are loaded from midiNotes section
                        if (index >= 0 && index < 16)
                        {
                            sampler.setNoteMapping(index, midiNote);
                            // Update the button UI - find the button with this buttonIndex
                            for (int j = 0; j < buttons.size(); j++)
                            {
                                if (buttons[j]->getButtonIndex() == index)
                                {
                                    if (filePath.isNotEmpty())
                                    {
                                        // Will be updated when midiNotes section loads the sample
                                        String msg = "Button " + String(index) + " (array pos " + String(j) + ") will be updated when sample loads";
                                        DEBUG_MIDI(msg);
                                    }
                                    else
                                    {
                                        // Clear the button display for buttons without samples
                                        buttons[j]->setFileName("");
                                        buttons[j]->setLoaded(false);
                                        buttons[j]->repaint();
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Load MIDI note samples
    if (jsonObj->hasProperty("midiNotes"))
    {
        DEBUG_MIDI("Found midiNotes property");
        var midiNotesArray = jsonObj->getProperty("midiNotes");
        DEBUG_MIDI(String("midiNotes array size: ") + String(midiNotesArray.size()));
        if (midiNotesArray.isArray())
        {
            for (int i = 0; i < midiNotesArray.size(); i++)
            {
                var noteObj = midiNotesArray[i];
                if (noteObj.isObject())
                {
                    DynamicObject* nObj = noteObj.getDynamicObject();
                    if (nObj)
                    {
                        int midiNote = nObj->getProperty("midiNote");
                        String filePath = nObj->getProperty("filePath").toString();

                        DEBUG_MIDI(String("MIDI Note ") + String(midiNote) + ": filePath=\"" + filePath + "\"");

                        if (midiNote >= 0 && midiNote < 128 && filePath.isNotEmpty())
                        {
                            File sampleFile(filePath);
                            if (sampleFile.exists())
                            {
                                if (sampler.loadSampleForMidiNote(midiNote, sampleFile))
                                {
                                    // Also update the button UI for any button mapped to this MIDI note
                                    for (int j = 0; j < buttons.size(); j++)
                                    {
                                        // Get the actual button number for this array position
                                        int btnNum = buttons[j]->getButtonIndex();
                                        // Check if this button is mapped to the current MIDI note
                                        if (sampler.getNoteMapping(btnNum) == midiNote)
                                        {
                                            buttons[j]->setFileName(sampleFile.getFileName());
                                            buttons[j]->setLoaded(true);
                                            buttons[j]->repaint();
                                            String msg = "Updated button " + String(btnNum) + " (array pos " + String(j) + ", mapped to note " + String(midiNote) + ") with file: " + sampleFile.getFileName();
                                            DEBUG_MIDI(msg);
                                            break;
                                        }
                                    }
                                    loadedCount++;
                                }
                                else
                                {
                                    failedCount++;
                                }
                            }
                            else
                            {
                                failedCount++;
                            }
                        }
                    }
                }
            }
        }
    }

    DEBUG_MIDI(String("Import summary: loaded=") + String(loadedCount) + " failed=" + String(failedCount));
    midiLearnLabel.setText("Imported: " + String(loadedCount) + " samples, " + String(failedCount) + " failed",
                          NotificationType::sendNotification);

    // Force repaint of all buttons to show loaded state
    for (int j = 0; j < buttons.size(); j++)
    {
        buttons[j]->repaint();
    }

    DEBUG_MIDI(String("loadAllSamplesFromJson EXIT - returning true"));
    return true;
}

//==============================================================================
// Save the last JSON file path to application settings
void SamplerEditor::saveLastJsonFile(const File& file)
{
    File settingsFile = File::getSpecialLocation(File::userApplicationDataDirectory)
        .getChildFile("TostEngineJucePocketSampler/settings.txt");

    // Create parent directory if it doesn't exist
    settingsFile.getParentDirectory().createDirectory();

    // Write the file path to settings
    std::unique_ptr<FileOutputStream> outputStream(settingsFile.createOutputStream());
    if (outputStream != nullptr)
    {
        outputStream->write(file.getFullPathName().toRawUTF8(), file.getFullPathName().length());
        outputStream->flush();
        outputStream.reset();
        DEBUG_MIDI(String("Saved last JSON file: ") + file.getFullPathName());
    }
    else
    {
        DEBUG_MIDI(String("Error: Could not save last JSON file path"));
    }
}

//==============================================================================
// Load the last JSON file on startup if it exists
void SamplerEditor::loadLastJsonFileOnStartup()
{
    File settingsFile = File::getSpecialLocation(File::userApplicationDataDirectory)
        .getChildFile("TostEngineJucePocketSampler/settings.txt");

    if (!settingsFile.exists())
    {
        DEBUG_MIDI("No last JSON file settings found");
        return;
    }

    std::unique_ptr<FileInputStream> inputStream(settingsFile.createInputStream());
    if (inputStream != nullptr)
    {
        String filePath = inputStream->readEntireStreamAsString();
        if (filePath.isNotEmpty())
        {
            File jsonFile(filePath);
            if (jsonFile.exists())
            {
                DEBUG_MIDI(String("Auto-loading last JSON file: ") + filePath);
                loadAllSamplesFromJson(jsonFile);
            }
            else
            {
                DEBUG_MIDI(String("Last JSON file not found: ") + filePath);
            }
        }
    }
}
