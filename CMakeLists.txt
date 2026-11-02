cmake_minimum_required(VERSION 3.24)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(OpenRbPcf VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

find_program(CARGO_EXECUTABLE cargo HINTS "$ENV{HOME}/.cargo/bin")
if(NOT CARGO_EXECUTABLE)
    message(FATAL_ERROR "cargo not found; install Rust from https://rustup.rs")
endif()

set(CARGO_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo")
set(ORB_PCF_LIBNAME "${CMAKE_STATIC_LIBRARY_PREFIX}orb_pcf${CMAKE_STATIC_LIBRARY_SUFFIX}")

file(GLOB_RECURSE ORB_PCF_RUST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.rs"
    "${CMAKE_SOURCE_DIR}/third_party/openrb-338/crates/*/*.rs"
    "${CMAKE_SOURCE_DIR}/third_party/openrb-338/crates/*/src/*.rs"
    "${CMAKE_SOURCE_DIR}/third_party/openrb-338/crates/*/Cargo.toml"
)

if(APPLE)
    find_program(RUSTUP_EXECUTABLE rustup HINTS "$ENV{HOME}/.cargo/bin")
    if(NOT RUSTUP_EXECUTABLE)
        message(FATAL_ERROR "rustup not found; install Rust from https://rustup.rs")
    endif()

    set(ORB_PCF_RUST_TRIPLES x86_64-apple-darwin aarch64-apple-darwin)
    set(ORB_PCF_STATICLIB "${CARGO_TARGET_DIR}/universal/${ORB_PCF_LIBNAME}")
    set(ORB_PCF_ARCH_LIBS)
    foreach(triple IN LISTS ORB_PCF_RUST_TRIPLES)
        list(APPEND ORB_PCF_ARCH_LIBS
            "${CARGO_TARGET_DIR}/${triple}/release/${ORB_PCF_LIBNAME}")
    endforeach()

    add_custom_command(
        OUTPUT "${ORB_PCF_STATICLIB}"
        COMMAND "${RUSTUP_EXECUTABLE}" target add ${ORB_PCF_RUST_TRIPLES}
        COMMAND "${CMAKE_COMMAND}" -E env
            "CARGO_TARGET_DIR=${CARGO_TARGET_DIR}"
            "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
            "${CARGO_EXECUTABLE}" build --release --target x86_64-apple-darwin
            --manifest-path "${CMAKE_SOURCE_DIR}/Cargo.toml"
        COMMAND "${CMAKE_COMMAND}" -E env
            "CARGO_TARGET_DIR=${CARGO_TARGET_DIR}"
            "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
            "${CARGO_EXECUTABLE}" build --release --target aarch64-apple-darwin
            --manifest-path "${CMAKE_SOURCE_DIR}/Cargo.toml"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CARGO_TARGET_DIR}/universal"
        COMMAND /usr/bin/lipo -create ${ORB_PCF_ARCH_LIBS} -output "${ORB_PCF_STATICLIB}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/Cargo.toml"
            "${CMAKE_SOURCE_DIR}/Cargo.lock"
            "${CMAKE_SOURCE_DIR}/include/orb_pcf.h"
            ${ORB_PCF_RUST_SOURCES}
        BYPRODUCTS ${ORB_PCF_ARCH_LIBS}
        COMMENT "Building universal orb_pcf staticlib"
        VERBATIM
    )
else()
    set(ORB_PCF_STATICLIB "${CARGO_TARGET_DIR}/release/${ORB_PCF_LIBNAME}")
    add_custom_command(
        OUTPUT "${ORB_PCF_STATICLIB}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "CARGO_TARGET_DIR=${CARGO_TARGET_DIR}"
            "${CARGO_EXECUTABLE}" build --release --manifest-path "${CMAKE_SOURCE_DIR}/Cargo.toml"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/Cargo.toml"
            "${CMAKE_SOURCE_DIR}/Cargo.lock"
            "${CMAKE_SOURCE_DIR}/include/orb_pcf.h"
            ${ORB_PCF_RUST_SOURCES}
        BYPRODUCTS "${ORB_PCF_STATICLIB}"
        COMMENT "Building orb_pcf staticlib"
        VERBATIM
    )
endif()
add_custom_target(orb_pcf_rust DEPENDS "${ORB_PCF_STATICLIB}")

add_library(orb_pcf_lib STATIC IMPORTED GLOBAL)
set_target_properties(orb_pcf_lib PROPERTIES IMPORTED_LOCATION "${ORB_PCF_STATICLIB}")
add_dependencies(orb_pcf_lib orb_pcf_rust)

set(ORB_PCF_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/output")
file(MAKE_DIRECTORY
    "${ORB_PCF_OUTPUT_DIR}/VST3"
    "${ORB_PCF_OUTPUT_DIR}/AU"
    "${ORB_PCF_OUTPUT_DIR}/Standalone"
)

juce_add_plugin(OpenRbPcf
    VERSION 0.2.0
    COMPANY_NAME "OpenRB"
    BUNDLE_ID com.openrb.pcf
    PLUGIN_MANUFACTURER_CODE OpnR
    PLUGIN_CODE OPcf
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "OpenRB PCF"
    PLUGIN_NAME "OpenRB PCF"
    DESCRIPTION "Pattern Controlled Filter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "${ORB_PCF_OUTPUT_DIR}/VST3"
    AU_COPY_DIR "${ORB_PCF_OUTPUT_DIR}/AU"
    VST3_CATEGORIES Fx Filter
    AU_MAIN_TYPE kAudioUnitType_Effect
)

juce_generate_juce_header(OpenRbPcf)

target_sources(OpenRbPcf
    PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginProcessor.h
        plugin/PluginEditor.cpp
        plugin/PluginEditor.h
)

target_include_directories(OpenRbPcf PRIVATE include)

target_compile_definitions(OpenRbPcf
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(OpenRbPcf
    PRIVATE
        orb_pcf_lib
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# JUCE's copy step skips Standalone (it is an app, not a plugin format folder).
if(APPLE)
    set(ORB_PCF_STANDALONE_DEST "${ORB_PCF_OUTPUT_DIR}/Standalone/OpenRB PCF.app")
    add_custom_command(TARGET OpenRbPcf_Standalone POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ORB_PCF_STANDALONE_DEST}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "$<TARGET_BUNDLE_DIR:OpenRbPcf_Standalone>"
            "${ORB_PCF_STANDALONE_DEST}"
        COMMENT "Copying standalone app into output/Standalone"
        VERBATIM
    )
endif()
