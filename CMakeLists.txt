cmake_minimum_required(VERSION 3.22)

project(Geminus VERSION 0.4.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# macOS: one universal binary for Intel and Apple Silicon, back to Catalina (JUCE 8's floor).
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
    endif()
endif()

add_subdirectory(third_party/JUCE)
add_subdirectory(third_party/clap-juce-extensions EXCLUDE_FROM_ALL)

# ── DSP core: plain C++20, no JUCE, shared by the plugin and the headless tests ──────────────
add_library(sg_core STATIC
    src/core/WaveTable.cpp
    src/core/Dds1.cpp
    src/core/Voice.cpp
    src/core/SuperVoice.cpp
    src/core/LayerEngine.cpp
    src/core/Performance.cpp
    src/core/ArpSeq.cpp
    src/core/Effects.cpp
    src/core/Decimator.cpp)
target_include_directories(sg_core PUBLIC src)
target_compile_features(sg_core PUBLIC cxx_std_20)
# Whole-program optimisation lets the per-sample oscillator / filter calls inline across files.
include(CheckIPOSupported)
check_ipo_supported(RESULT SG_IPO_OK OUTPUT SG_IPO_MSG)
if(SG_IPO_OK)
    set_property(TARGET sg_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
if(MSVC)
    target_compile_options(sg_core PRIVATE /W3 /utf-8 /fp:fast $<$<NOT:$<CONFIG:Debug>>:/O2 /Oi>)
else()
    target_compile_options(sg_core PRIVATE -Wall -O2 -ffast-math -fno-finite-math-only)
endif()

# ── Panel artwork (DD-74): photographed hardware parts, keyed and cut at load ─────────────
juce_add_binary_data(GeminusArt SOURCES
    resources/knobs.png
    resources/faders.png
    resources/buttons.png
    resources/buttons_dark.png
    resources/cheek.png
    resources/ribbon.png
    resources/glass.png
    resources/panel_surface.png
    resources/shading.png)
set_target_properties(GeminusArt PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ── Web UI (ui/) ────────────────────────────────────────────────────────────────────────────
# The interface is an HTML page rendered by juce::WebBrowserComponent. Two resource modes:
#   GEMINUS_DEV_UI=ON   the WebView loads ui/ from disk, so edits show on reload, no rebuild
#   GEMINUS_DEV_UI=OFF  every UI file is embedded and served by a resource provider (shipping)
option(GEMINUS_DEV_UI "Load the web UI from disk instead of embedding it" OFF)

# Recursive: the JUCE frontend library lives in ui/juce/, and binary data flattens to
# basenames anyway, which is what the resource provider matches on.
file(GLOB_RECURSE GEMINUS_UI_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.css"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.woff2")
# Own namespace: GeminusArt already claims the default BinaryData one.
juce_add_binary_data(GeminusWebUI NAMESPACE GeminusUiData SOURCES ${GEMINUS_UI_FILES})
# Both targets emit a header called BinaryData.h, so reaching the UI one by that name is
# ambiguous. Write a shim with a distinct name that points at it.
file(WRITE "${CMAKE_BINARY_DIR}/geminus_ui_include/GeminusUiData.h"
     "#pragma once\n#include \"${CMAKE_BINARY_DIR}/juce_binarydata_GeminusWebUI/JuceLibraryCode/BinaryData.h\"\n")
set_target_properties(GeminusWebUI PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
list(LENGTH GEMINUS_UI_FILES GEMINUS_UI_FILE_COUNT)
message(STATUS "Geminus web UI: ${GEMINUS_UI_FILE_COUNT} files (dev mode: ${GEMINUS_DEV_UI})")

# WebView2 SDK, fetched into third_party/webview2 (nupkg contents).
set(GEMINUS_WEBVIEW2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/build/native")

# ── Plugin ──────────────────────────────────────────────────────────────────────────────────
set(SG_FORMATS VST3 Standalone)
set(SG_COPY_ARGS "")
if(APPLE)
    list(APPEND SG_FORMATS AU)   # AU can only be built on macOS
else()
    # Windows: install straight into the shared VST3 folder. On macOS JUCE already uses
    # ~/Library/Audio/Plug-Ins, so naming a path here would point the copy at a Windows folder.
    set(SG_COPY_ARGS VST3_COPY_DIR "C:/Program Files/Common Files/VST3/002 By SPKR")
endif()

# Brand: vendor "SPKR", product "002" (renamed from Geminus on 2026-09-19; target name and IDs kept).
juce_add_plugin(Geminus
    COMPANY_NAME "SPKR"
    PRODUCT_NAME "002 by SPKR"          # file names (no | allowed in Windows file names)
    PLUGIN_NAME "002 | by SPKR"          # the name hosts show; the IDs below stay, so projects still find it
    ${SG_COPY_ARGS}                      # Windows: its own folder under Common Files
    BUNDLE_ID "com.spkr.geminus"
    PLUGIN_MANUFACTURER_CODE Spkr
    PLUGIN_CODE Gmns
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${SG_FORMATS})

set(SG_PLUGIN_SOURCES
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/WebEditor.cpp
    src/plugin/Parameters.cpp
    src/plugin/FxRack.cpp
    src/plugin/FxUnits.cpp
    src/plugin/MidiRouter.cpp
    src/ui/Theme.cpp
    src/ui/Artwork.cpp
    src/ui/Controls.cpp
    src/ui/Panel.cpp
    src/ui/Popovers.cpp)
target_sources(Geminus PRIVATE ${SG_PLUGIN_SOURCES})

target_include_directories(Geminus PRIVATE src "${CMAKE_BINARY_DIR}/geminus_ui_include")

target_compile_definitions(Geminus PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1       # DDS 1 CUSTOM accepts MP3 as well as WAV / AIFF / FLAC / OGG
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_definitions(Geminus PRIVATE
    GEMINUS_DEV_UI=$<BOOL:${GEMINUS_DEV_UI}>
    GEMINUS_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ui")

if(WIN32)
    # WebView2: static loader, so the shipped plugin has no extra DLL to place.
    target_compile_definitions(Geminus PUBLIC
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
    target_include_directories(Geminus PRIVATE "${GEMINUS_WEBVIEW2_DIR}/include")
    target_link_libraries(Geminus PRIVATE "${GEMINUS_WEBVIEW2_DIR}/x64/WebView2LoaderStatic.lib")
endif()

if(MSVC)
    target_compile_options(Geminus PRIVATE /utf-8)
endif()

target_link_libraries(Geminus
    PRIVATE
        GeminusArt
        GeminusWebUI
        sg_core
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

clap_juce_extensions_plugin(TARGET Geminus
    CLAP_ID "com.spkr.geminus"
    CLAP_FEATURES instrument synthesizer stereo)

# ── Parameter dump: the source of truth for the web UI's binding table ──────────────────────
add_executable(SGWavePreview tools/WavePreview.cpp)
target_link_libraries(SGWavePreview PRIVATE sg_core)

# Renders a patch from the DSP core to a WAV, for comparing against recordings of the hardware
add_executable(SGRender tools/Render.cpp)
target_link_libraries(SGRender PRIVATE sg_core)
if(MSVC)
    target_compile_options(SGRender PRIVATE /utf-8)
endif()

juce_add_console_app(SGDumpParams PRODUCT_NAME "SGDumpParams")
target_sources(SGDumpParams PRIVATE tools/DumpParams.cpp ${SG_PLUGIN_SOURCES})
target_include_directories(SGDumpParams PRIVATE src "${CMAKE_BINARY_DIR}/geminus_ui_include")
target_compile_definitions(SGDumpParams PRIVATE
    JucePlugin_Name="002" JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0
    GEMINUS_DEV_UI=$<BOOL:${GEMINUS_DEV_UI}>
    GEMINUS_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ui")
if(WIN32)
    target_compile_definitions(SGDumpParams PRIVATE JUCE_USE_WIN_WEBVIEW2=1 JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
    target_include_directories(SGDumpParams PRIVATE "${GEMINUS_WEBVIEW2_DIR}/include")
    target_link_libraries(SGDumpParams PRIVATE "${GEMINUS_WEBVIEW2_DIR}/x64/WebView2LoaderStatic.lib")
endif()
if(MSVC)
    target_compile_options(SGDumpParams PRIVATE /utf-8)
endif()
target_link_libraries(SGDumpParams
    PRIVATE GeminusArt GeminusWebUI sg_core juce::juce_audio_utils juce::juce_gui_extra juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# ── Host smoke test: loads the built VST3 like a DAW would ──────────────────────────────────
if(WIN32)   # drives the real WebView2 through open / close / reopen; Windows-only by nature
juce_add_console_app(GeminusLifecycle PRODUCT_NAME "GeminusLifecycle")
target_sources(GeminusLifecycle PRIVATE tools/WebLifecycle.cpp)
target_include_directories(GeminusLifecycle PRIVATE src third_party/JUCE/modules)
target_compile_definitions(GeminusLifecycle PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(GeminusLifecycle PRIVATE Geminus)
if(MSVC)
    target_compile_options(GeminusLifecycle PRIVATE /utf-8)
endif()
endif()

juce_add_console_app(SGPluginSmoke PRODUCT_NAME "SGPluginSmoke")
target_sources(SGPluginSmoke PRIVATE tools/PluginSmokeTest.cpp)
target_compile_definitions(SGPluginSmoke PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
if(MSVC)
    target_compile_options(SGPluginSmoke PRIVATE /utf-8)
endif()
target_link_libraries(SGPluginSmoke
    PRIVATE juce::juce_audio_processors juce::juce_events
    PUBLIC juce::juce_recommended_config_flags)

# ── UI check: renders the panel and its pop-overs to PNG, and fails if any host parameter has
#    no control that can reach it ────────────────────────────────────────────────────────────
juce_add_console_app(SGUiSnapshot PRODUCT_NAME "SGUiSnapshot")
target_sources(SGUiSnapshot PRIVATE tools/UiSnapshot.cpp ${SG_PLUGIN_SOURCES})
target_include_directories(SGUiSnapshot PRIVATE src)
target_compile_definitions(SGUiSnapshot PRIVATE JucePlugin_Name="002" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if(MSVC)
    target_compile_options(SGUiSnapshot PRIVATE /utf-8)
endif()
target_link_libraries(SGUiSnapshot
    PRIVATE GeminusArt sg_core juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# ── Headless tests (juce::UnitTest console runner) ──────────────────────────────────────────
juce_add_console_app(SGTests PRODUCT_NAME "SGTests")
target_sources(SGTests PRIVATE
    tests/TestMain.cpp
    tests/CoreTests.cpp
    tests/OscillatorTests.cpp
    tests/FilterTests.cpp
    tests/ModulationTests.cpp
    tests/VoiceTests.cpp
    tests/PerformanceTests.cpp
    tests/Phase4Tests.cpp)
target_include_directories(SGTests PRIVATE src tests)
target_compile_definitions(SGTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
if(MSVC)
    target_compile_options(SGTests PRIVATE /utf-8)
    target_link_options(SGTests PRIVATE /STACK:16777216)   # tests build whole PerformanceEngines on the stack
endif()
if(SG_IPO_OK)
    set_property(TARGET SGTests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
target_link_libraries(SGTests
    PRIVATE
        sg_core
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ── FX rack check: every effect at default / min / max settings (tools/FxCheck.cpp) ──────────
juce_add_console_app(SGFxCheck PRODUCT_NAME "SGFxCheck")
target_sources(SGFxCheck PRIVATE tools/FxCheck.cpp src/plugin/FxRack.cpp src/plugin/FxUnits.cpp)
target_include_directories(SGFxCheck PRIVATE src src/plugin)
target_compile_definitions(SGFxCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if(MSVC)
    target_compile_options(SGFxCheck PRIVATE /utf-8)
endif()
target_link_libraries(SGFxCheck
    PRIVATE sg_core juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
