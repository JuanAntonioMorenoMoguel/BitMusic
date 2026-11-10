#include "BitMusic/Wav.h"

#include <BitMotor/Core/Files.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace BitMusic {

namespace {

void Put(std::vector<std::uint8_t>& bytes, std::uint32_t value, int size) {
    for (int i = 0; i < size; ++i) bytes.push_back(std::uint8_t((value >> (8 * i)) & 0xFF));
}

void Put(std::vector<std::uint8_t>& bytes, std::string_view tag) { bytes.insert(bytes.end(), tag.begin(), tag.end()); }

} // namespace

std::vector<std::uint8_t> EncodeWav(std::span<const float> stereo, int sampleRate) {
    const std::uint32_t channels = 2, bits = 16;
    const std::uint32_t frames = std::uint32_t(stereo.size() / channels);
    const std::uint32_t data = frames * channels * (bits / 8);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(44 + data);
    Put(bytes, "RIFF");
    Put(bytes, 36 + data, 4);
    Put(bytes, "WAVE");
    Put(bytes, "fmt ");
    Put(bytes, 16, 4);
    Put(bytes, 1, 2); // PCM
    Put(bytes, channels, 2);
    Put(bytes, std::uint32_t(sampleRate), 4);
    Put(bytes, std::uint32_t(sampleRate) * channels * (bits / 8), 4);
    Put(bytes, channels * (bits / 8), 2);
    Put(bytes, bits, 2);
    Put(bytes, "data");
    Put(bytes, data, 4);
    for (std::size_t i = 0; i < std::size_t(frames) * channels; ++i) {
        const float sample = std::isfinite(stereo[i]) ? std::clamp(stereo[i], -1.0f, 1.0f) : 0.0f;
        const auto value = std::int16_t(std::lround(sample * 32767.0f));
        Put(bytes, std::uint16_t(value), 2);
    }
    return bytes;
}

bool ExportWav(const BitMotor::Song& song, const std::filesystem::path& path, int passes) {
    const std::vector<float> samples = song.Render(std::max(1, passes));
    return BitMotor::Files::WriteBytes(std::filesystem::absolute(path), EncodeWav(samples));
}

} // namespace BitMusic
