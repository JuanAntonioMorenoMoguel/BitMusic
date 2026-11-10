cmake_minimum_required(VERSION 3.24)
project(BitMusic VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Product information: the single source for the window title and the version (see cmake/AppInfo.h.in).
# ---------------------------------------------------------------------------
set(BM_APP_NAME "BitMusic")
set(BM_APP_IDENTIFIER "io.github.juanantoniomorenomoguel.BitMusic")
set(BM_APP_DESCRIPTION "A music editor for games: songs for BitMotor's synthesizer.")
set(BM_APP_AUTHOR "Juan Antonio Moreno Moguel")
set(BM_APP_COPYRIGHT "Copyright (C) 2026 Juan Antonio Moreno Moguel. All rights reserved.")
set(BM_APP_URL "https://github.com/JuanAntonioMorenoMoguel/BitMusic")
configure_file("${CMAKE_SOURCE_DIR}/cmake/AppInfo.h.in" "${CMAKE_BINARY_DIR}/generated/AppInfo.h" @ONLY)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Engine: BitMotor (window, UI::JFxStyle, the synthesizer and the .bitmusic format), by default in the
# sibling folder ../BitMotor. Its dependencies come from the vcpkg manifest (vcpkg.json).
# ---------------------------------------------------------------------------
set(BITMOTOR_PATH "${CMAKE_SOURCE_DIR}/../BitMotor" CACHE PATH "BitMotor folder")
add_subdirectory(${BITMOTOR_PATH} BitMotor)

# ---------------------------------------------------------------------------
# BitMusicCore: editing songs, undo, WAV export and the command line. It opens no window and plays no
# sound, so the tests and the command line use it on their own. Headers in include/BitMusic/.
# ---------------------------------------------------------------------------
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/core/*.cpp"
    "${CMAKE_SOURCE_DIR}/include/BitMusic/*.h"
)
add_library(BitMusicCore STATIC ${CORE_SOURCES})
target_include_directories(BitMusicCore PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(BitMusicCore PUBLIC BitMotor::BitMotor)

# ---------------------------------------------------------------------------
# The application: the editor window, or a command when there are arguments (bitmusic export ...).
# ---------------------------------------------------------------------------
file(GLOB EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/Main.cpp"
    "${CMAKE_SOURCE_DIR}/src/editor/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/editor/*.h"
)
add_executable(BitMusic ${EDITOR_SOURCES})
target_link_libraries(BitMusic PRIVATE BitMusicCore)
# assets/ and lang/ next to the executable; while developing they are read from the source folder.
BitMotor_Prepare(BitMusic COPY assets lang)

# ---------------------------------------------------------------------------
# Unit tests (no framework needed: see tests/TestMain.h). Run with
#   ctest --test-dir build -C Debug --output-on-failure
# ---------------------------------------------------------------------------
enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
add_executable(BitMusicTests ${TEST_SOURCES})
target_link_libraries(BitMusicTests PRIVATE BitMusicCore)
# The tests read assets/ and lang/ from the source folder.
target_compile_definitions(BitMusicTests PRIVATE BM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME BitMusicTests COMMAND BitMusicTests)

foreach(TARGET_NAME BitMusicCore BitMusic BitMusicTests)
    if(WIN32)
        target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(MSVC)
        # /utf-8: the texts are UTF-8 (accents, Ñ, ¿, ¡).
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
    endif()
endforeach()
