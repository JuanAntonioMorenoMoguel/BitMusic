#pragma once
#include <BitMotor/Audio/Music.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

// Songs as WAV files: 16-bit PCM, stereo, at BitMotor's 48000 Hz. The same song always gives the same bytes.
//     ExportWav(song, "out/theme.wav", 2);   // Order, then once more from Loop
namespace BitMusic {

// A whole WAV file from interleaved stereo samples (-1..1; louder ones are clipped).
std::vector<std::uint8_t> EncodeWav(std::span<const float> stereo, int sampleRate = 48000);
// Song::Render(passes) written as a WAV file (relative paths: next to the working folder). false if it
// cannot be written (logged).
bool ExportWav(const BitMotor::Song& song, const std::filesystem::path& path, int passes = 1);

} // namespace BitMusic
