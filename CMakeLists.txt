cmake_minimum_required(VERSION 3.22)
project(TostEngineJucePocketSampler VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE path
set(JUCE_PATH "C:/Users/PC/Documents/JUCE")

# Add JUCE as subdirectory
add_subdirectory("${JUCE_PATH}" juce_framework)

# Create the GUI application
juce_add_gui_app(TostEngineJucePocketSampler
    PRODUCT_NAME "TostEngineJucePocketSampler"
)

# Add source files
target_sources(TostEngineJucePocketSampler
    PRIVATE
        "src/Main.cpp"
        "src/SamplerPlugin.cpp"
        "src/SamplerEditor.cpp"
)

# Set preprocessor definitions
target_compile_definitions(TostEngineJucePocketSampler PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
)

# Link required modules
target_link_libraries(TostEngineJucePocketSampler
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
