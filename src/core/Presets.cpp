#include "BitMusic/Presets.h"

#include "BitMusic/Edit.h"

#include <array>

namespace BitMusic {

namespace {

using BitMotor::Instrument;

Instrument Make(std::string_view wave, float volume, float attack, float decay, float sustain, float release) {
    Instrument instrument;
    instrument.Wave = std::string(wave);
    instrument.Volume = volume;
    instrument.Attack = attack;
    instrument.Decay = decay;
    instrument.Sustain = sustain;
    instrument.Release = release;
    return instrument;
}

const std::array<Preset, 9>& All() {
    static const std::array<Preset, 9> presets = [] {
        Instrument kick = Make("sine", 0.9f, 0.001f, 0.15f, 0, 0.05f);
        kick.Sweep = -24;
        kick.SweepTime = 0.08f;
        Instrument snare = Make("noise", 0.5f, 0.001f, 0.12f, 0, 0.08f);
        snare.LowPass = 0.6f;
        Instrument hat = Make("noise", 0.25f, 0.001f, 0.04f, 0, 0.02f);
        Instrument bass = Make("triangle", 0.6f, 0.005f, 0.2f, 0.5f, 0.1f);
        Instrument lead = Make("square", 0.35f, 0.01f, 0.1f, 0.6f, 0.15f);
        lead.Width = 0.25f;
        Instrument pad = Make("saw", 0.25f, 0.4f, 0.5f, 0.7f, 0.8f);
        pad.LowPass = 0.25f;
        Instrument pluck = Make("square", 0.35f, 0.001f, 0.2f, 0, 0.1f);
        pluck.Width = 0.125f;
        Instrument arp = Make("square", 0.3f, 0.002f, 0.08f, 0.3f, 0.05f);
        Instrument chip = Make("square", 0.35f, 0.005f, 0.1f, 0.7f, 0.1f);
        chip.Crush = 8;
        chip.Downsample = 2;
        return std::array<Preset, 9>{{{"lead", lead}, {"bass", bass}, {"kick", kick}, {"snare", snare}, {"hihat", hat},
                                      {"pad", pad}, {"pluck", pluck}, {"arp", arp}, {"chip", chip}}};
    }();
    return presets;
}

} // namespace

std::span<const Preset> Presets() { return All(); }

const Preset* FindPreset(std::string_view name) {
    for (const Preset& preset : All())
        if (preset.Name == name) return &preset;
    return nullptr;
}

BitMotor::Song NewSong() {
    BitMotor::Song song;
    song.Volume = 0.8f;
    const std::string first = AddPattern(song, "1");
    for (std::string_view name : {"lead", "bass", "kick", "hihat"})
        AddChannel(song, AddInstrument(song, name, FindPreset(name)->Instrument));
    InsertOrder(song, 0, first);
    return song;
}

} // namespace BitMusic
