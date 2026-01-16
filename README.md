# 🍞 TostEngine Juce Pocket Sampler

A 16-Button MIDI Sampler built with JUCE framework. Trigger samples using MIDI input or by clicking buttons in the UI.

![Screenshot 2026-01-16 162614](https://github.com/user-attachments/assets/b42c8562-1314-4ae8-8d8f-2a423e29a30a)

## Features

### Core Functionality
- **16 Sample Pads** - Click buttons or use MIDI to trigger samples
- **MIDI Learn** - Assign MIDI notes to buttons by clicking "MIDI Learn" then pressing a key on your controller
- **Sample Learn** - Assign sample files to MIDI notes by clicking "Sample Learn" then pressing a key
- **One-Shot Mode** - Toggle to play samples to completion without requiring note-off
- **MIDI Status Display** - Shows last received MIDI note, velocity, and channel

### Import/Export
- **Export** - Save all button mappings and sample paths to a JSON file
- **Import** - Load button mappings and sample paths from a JSON file
- **Auto-Load** - Automatically loads the last imported JSON file on startup

### Supported Formats
- WAV, AIFF, FLAC, and other formats supported by JUCE audio formats

## Settings

Access settings via the **Settings** menu in the application title bar:

- **Audio & MIDI Settings...** - Configure audio device and MIDI input/output devices
- **GitHub Repository...** - Open the project page on GitHub

## How to Build

### Prerequisites
- Windows 10/11
- Visual Studio 2022 (Community Edition works)
- CMake 3.22 or later
- JUCE framework (included as submodule)

### Build Steps

1. **Generate Visual Studio solution:**
   ```powershell
   cd C:\Users\PC\Documents\content\midi\Sampler
   mkdir -Force build
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64
   ```

2. **Build the project:**
   ```powershell
   cmake --build . --config Release
   ```

   Or open `TostEngineJucePocketSampler.sln` in Visual Studio and build.

3. **Run the application:**
   ```powershell
   Start-Process -FilePath "C:\Users\PC\Documents\content\midi\Sampler\build\TostEngineJucePocketSampler_artefacts\Release\TostEngineJucePocketSampler.exe"
   ```

### Quick Build Script
```powershell
cd C:\Users\PC\Documents\content\midi\Sampler
.\build_and_run.ps1
```

## Usage

### Loading Samples
1. Click "Sample Learn" button
2. Press a key on your MIDI controller
3. Select a sample file when prompted
4. Repeat for other keys

### Mapping Buttons to MIDI Notes
1. Click "MIDI Learn" button
2. Press a key on your MIDI controller (the button will light up)
3. The button is now mapped to that MIDI note

### Exporting Settings
1. Click "Export" button
2. Choose a location to save the JSON file
3. This saves all button mappings and sample file paths

### Importing Settings
1. Click "Import" button
2. Select a previously exported JSON file
3. All samples will be loaded automatically

## File Locations

- **Executable:** `build/TostEngineJucePocketSampler_artefacts/Release/TostEngineJucePocketSampler.exe`
- **Debug Log:** Same directory as executable (`debug.log`)
- **Settings:** `%APPDATA%/TostEngineJucePocketSampler/settings.txt`

## MIDI Input

The application automatically opens all available MIDI input devices on startup. MIDI messages are processed and passed to the sampler engine.

### MIDI Messages Supported
- **Note On** - Trigger sample with velocity
- **Note Off** - Stop sample (unless One-Shot mode is enabled)

## License

MIT License

