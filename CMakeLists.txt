cmake_minimum_required(VERSION 3.23.1)

# General setup
set(PROJECT_NAME "MIDInerd")
set(CURRENT_VERSION "0.0.1")
set(PRODUCT_NAME "MIDInerd")
set(COMPANY_NAME "YourCompany")
set(BUNDLE_ID "com.yourcompany.midinerd")
set(FORMATS Standalone VST3)

project(MIDInerd VERSION 0.0.1)

add_subdirectory(modules/JUCE)

# Project configuration
set(CMAKE_CXX_STANDARD 17)

if (UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.1)
  include_directories(${WEBKIT2_INCLUDE_DIRS})
  link_directories(${WEBKIT2_LIBRARY_DIRS})
  add_definitions(${WEBKIT2_CFLAGS_OTHER})
endif()

# Disable all warnings
add_definitions(-w)

# Append custom module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/modules/cmake-includes")

# Define the plugin
juce_add_plugin("${PROJECT_NAME}"
        COMPANY_NAME "${COMPANY_NAME}"
        BUNDLE_ID "${BUNDLE_ID}"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Mypl
        PLUGIN_CODE Mp01
        FORMATS ${FORMATS}
        PRODUCT_NAME "${PRODUCT_NAME}"
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT TRUE
        IS_SYNTH FALSE
)

add_compile_definitions(JUCE_PROJECT_NAME="${PROJECT_NAME}")

# Define SharedCode as an INTERFACE library
add_library(SharedCode INTERFACE)

# Set compile features for SharedCode
target_compile_features(SharedCode INTERFACE cxx_std_20)

# Include directories for SharedCode
target_include_directories(SharedCode INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/source"
        "${CMAKE_CURRENT_SOURCE_DIR}/modules/JUCE/modules"
)

# Set JUCE flags for SharedCode
target_compile_definitions(SharedCode INTERFACE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
        VERSION="${CURRENT_VERSION}"
        JUCE_DISPLAY_SPLASH_SCREEN=0
        PRODUCT_NAME_WITHOUT_VERSION="MIDInerd"
        JUCE_ENABLE_REPAINT_DEBUGGING=0
)

# Add sources to the main project
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
)

target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

# Link libraries to main project
target_link_libraries("${PROJECT_NAME}" PRIVATE
        SharedCode
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_product_unlocking
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_include_directories("${PROJECT_NAME}" PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/source"
)
