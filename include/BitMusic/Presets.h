#pragma once
#include <BitMotor/Audio/Music.h>

#include <span>
#include <string_view>

// BitMusic's example instruments: a starting point for drums, basses and melodies on BitMotor's synthesizer.
//     for (const Preset& preset : Presets()) AddInstrument(song, preset.Name, preset.Instrument);
namespace BitMusic {

struct Preset {
    // The id: "kick", "snare"... (the editor shows Tr("preset.<id>")).
    std::string_view Name;
    BitMotor::Instrument Instrument;
};

std::span<const Preset> Presets();
// The preset called `name`, or nullptr.
const Preset* FindPreset(std::string_view name);

// The song a new file starts as: a lead, a bass, a kick and a hi-hat on four channels of one empty pattern.
BitMotor::Song NewSong();

} // namespace BitMusic
