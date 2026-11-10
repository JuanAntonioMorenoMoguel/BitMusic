#include "TestMain.h"

#include <BitMusic/Edit.h>
#include <BitMusic/Wav.h>

#include <cstring>

using namespace BitMusic;

namespace {

std::uint32_t Read32(const std::vector<std::uint8_t>& b, std::size_t at) {
    return std::uint32_t(b[at]) | std::uint32_t(b[at + 1]) << 8 | std::uint32_t(b[at + 2]) << 16 |
           std::uint32_t(b[at + 3]) << 24;
}

std::int16_t Read16(const std::vector<std::uint8_t>& b, std::size_t at) {
    return std::int16_t(std::uint16_t(b[at]) | std::uint16_t(b[at + 1]) << 8);
}

Song Tune() {
    Song song;
    AddInstrument(song, "lead");
    const std::string a = AddPattern(song, "a");
    AddChannel(song, "lead");
    SetNote(song.Patterns[a], 0, 0, "C4");
    SetNote(song.Patterns[a], 0, 4, "E4:0.5");
    SetNote(song.Patterns[a], 0, 8, "off");
    InsertOrder(song, 0, a);
    song.Loop = -1;
    return song;
}

} // namespace

TEST(WavHeaderAndSamples) {
    const std::vector<float> samples = {0, 0, 1, -1, 2, -0.5f};
    const auto bytes = EncodeWav(samples);
    REQUIRE(bytes.size() == 44 + 12);
    CHECK(std::memcmp(bytes.data(), "RIFF", 4) == 0);
    CHECK(Read32(bytes, 4) == 36 + 12);
    CHECK(std::memcmp(bytes.data() + 8, "WAVEfmt ", 8) == 0);
    CHECK(Read32(bytes, 24) == 48000);
    CHECK(std::memcmp(bytes.data() + 36, "data", 4) == 0);
    CHECK(Read32(bytes, 40) == 12);
    CHECK(Read16(bytes, 44 + 4) == 32767);
    CHECK(Read16(bytes, 44 + 6) == -32767);
    CHECK(Read16(bytes, 44 + 8) == 32767); // clipped
    CHECK(Read16(bytes, 44 + 10) == -16384);
}

TEST(SameSongSameBytes) {
    const Song song = Tune();
    const auto first = EncodeWav(song.Render());
    const auto second = EncodeWav(Tune().Render());
    CHECK(first.size() > 44 + 48000); // a few seconds, stereo, 16 bits
    CHECK(first == second);
    bool sound = false;
    for (std::size_t i = 44; i + 1 < first.size() && !sound; i += 2) sound = Read16(first, i) != 0;
    CHECK(sound);
}
