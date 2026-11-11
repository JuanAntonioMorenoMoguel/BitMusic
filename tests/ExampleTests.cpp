#include "TestMain.h"

#include <BitMotor/Audio/Music.h>

#include <cmath>
#include <filesystem>
#include <string>

// The example songs in assets/examples/: they load, every note is valid, they sound without clipping, the same
// song renders to the same samples twice, and saving and loading one again keeps its sound.
namespace {

std::filesystem::path Examples() { return std::filesystem::path(BM_SOURCE_DIR) / "assets" / "examples"; }

} // namespace

TEST(ExampleSongsLoadAndSound) {
    int songs = 0;
    for (const auto& file : std::filesystem::directory_iterator(Examples())) {
        if (file.path().extension() != ".bitmusic") continue;
        ++songs;
        const std::string name = file.path().filename().string();
        const auto song = BitMotor::Song::Load(file.path());
        REQUIRE(song);
        CHECK(!song->Order.empty());
        for (const std::string& entry : song->Order) CHECK(song->Patterns.contains(entry));
        for (const auto& [key, pattern] : song->Patterns)
            for (const auto& track : pattern.Tracks) {
                CHECK(track.Instrument.empty() || song->Instruments.contains(track.Instrument));
                CHECK(int(track.Notes.size()) <= pattern.Length);
                for (const std::string& note : track.Notes)
                    if (!BitMotor::NoteEntry::Parse(note)) test::Fail(__FILE__, __LINE__, name + ": bad note " + note);
            }
        const std::vector<float> samples = song->Render();
        float peak = 0;
        for (float s : samples) peak = std::max(peak, std::abs(s));
        if (peak < 0.05f || peak > 1.0f) test::Fail(__FILE__, __LINE__, name + ": peak " + std::to_string(peak));
        CHECK(samples == song->Render());

        const auto copy = std::filesystem::temp_directory_path() / "BitMusicTests" / name;
        REQUIRE(song->Save(copy));
        const auto again = BitMotor::Song::Load(copy);
        REQUIRE(again);
        CHECK(again->Render() == samples);
        std::filesystem::remove_all(copy.parent_path());
    }
    CHECK(songs >= 6);
}
