#include "TestMain.h"

#include <BitMusic/Edit.h>
#include <BitMusic/Presets.h>

using namespace BitMusic;

namespace {

Song TwoPatterns() {
    Song song;
    AddInstrument(song, "bass");
    AddInstrument(song, "lead");
    AddPattern(song, "a");
    AddChannel(song, "bass");
    AddChannel(song, "lead");
    AddPattern(song, "b");
    InsertOrder(song, 0, "a");
    InsertOrder(song, 1, "b");
    InsertOrder(song, 2, "a");
    return song;
}

} // namespace

TEST(UniqueNamesCount) {
    Song song;
    CHECK(AddInstrument(song, "bass") == "bass");
    CHECK(AddInstrument(song, "bass") == "bass 2");
    CHECK(AddInstrument(song, "bass") == "bass 3");
    CHECK(AddPattern(song, "") == "untitled");
}

TEST(NewPatternsAreABarWithTheSongsChannels) {
    Song song = TwoPatterns();
    CHECK(song.Patterns["a"].Length == 16);
    REQUIRE(song.Patterns["b"].Tracks.size() == 2);
    CHECK(song.Patterns["b"].Tracks[0].Instrument == "bass");
    CHECK(song.Patterns["b"].Tracks[1].Instrument == "lead");
    song.Beats = 3;
    CHECK(song.Patterns[AddPattern(song, "c")].Length == 12);
}

TEST(ChannelsChangeInEveryPattern) {
    Song song = TwoPatterns();
    SetNote(song.Patterns["a"], 1, 0, "C5");
    MoveChannel(song, 1, 0);
    CHECK(song.Patterns["a"].Tracks[0].Instrument == "lead");
    CHECK(song.Patterns["b"].Tracks[0].Instrument == "lead");
    CHECK(GetNote(song.Patterns["a"], 0, 0) == "C5");
    RemoveChannel(song, 0);
    CHECK(Channels(song) == 1);
    CHECK(song.Patterns["b"].Tracks[0].Instrument == "bass");
    SetChannelInstrument(song, 0, "lead");
    CHECK(ChannelInstrument(song, 0) == "lead");
    CHECK(song.Patterns["a"].Tracks[0].Instrument == "lead");
}

TEST(RenamingKeepsReferences) {
    Song song = TwoPatterns();
    CHECK(RenameInstrument(song, "bass", "sub"));
    CHECK(!RenameInstrument(song, "sub", "lead"));
    CHECK(song.Instruments.contains("sub") && !song.Instruments.contains("bass"));
    CHECK(song.Patterns["b"].Tracks[0].Instrument == "sub");
    CHECK(RenamePattern(song, "a", "intro"));
    CHECK(song.Order == std::vector<std::string>({"intro", "b", "intro"}));
    RemoveInstrument(song, "sub");
    CHECK(song.Patterns["b"].Tracks[0].Instrument.empty());
}

TEST(RemovingAPatternFixesOrderAndLoop) {
    Song song = TwoPatterns();
    song.Loop = 1; // "b"
    RemovePattern(song, "a");
    CHECK(song.Order == std::vector<std::string>({"b"}));
    CHECK(song.Loop == 0);
    RemovePattern(song, "b");
    CHECK(song.Order.empty());
    CHECK(song.Loop == 0);
}

TEST(OrderMovesKeepTheLoopOnItsEntry) {
    Song song = TwoPatterns(); // a b a, Loop 0
    song.Loop = 1;
    InsertOrder(song, 0, "b"); // b a b a
    CHECK(song.Loop == 2);
    MoveOrder(song, 2, 0); // b b a a
    CHECK(song.Loop == 0);
    MoveOrder(song, 3, 0); // a b b a
    CHECK(song.Loop == 1);
    RemoveOrder(song, 0);
    CHECK(song.Loop == 0);
    song.Loop = -1;
    RemoveOrder(song, 0);
    CHECK(song.Loop == -1);
}

TEST(NotesPadAndTrim) {
    Pattern pattern{.Length = 8, .Tracks = {}};
    SetNote(pattern, 1, 3, "C4");
    REQUIRE(pattern.Tracks.size() == 2);
    CHECK(pattern.Tracks[1].Notes.size() == 4);
    SetNote(pattern, 1, 9, "D4"); // past Length
    CHECK(pattern.Tracks[1].Notes.size() == 4);
    SetNote(pattern, 1, 3, "");
    CHECK(pattern.Tracks[1].Notes.empty());
    ToggleStep(pattern, 0, 2, "C2");
    CHECK(GetNote(pattern, 0, 2) == "C2");
    ToggleStep(pattern, 0, 2, "C2");
    CHECK(GetNote(pattern, 0, 2).empty());
    SetLength(pattern, 2);
    CHECK(pattern.Length == 2);
}

TEST(TransposeKeepsVolumes) {
    Pattern pattern{.Length = 4, .Tracks = {}};
    SetNote(pattern, 0, 0, "C4:0.5");
    SetNote(pattern, 0, 1, "off");
    SetNote(pattern, 0, 2, "B4");
    Transpose(pattern, 0, 0, 3, 1);
    CHECK(GetNote(pattern, 0, 0) == "C#4:0.5");
    CHECK(GetNote(pattern, 0, 1) == "off");
    CHECK(GetNote(pattern, 0, 2) == "C5");
}

TEST(PianoRollBlocks) {
    Pattern pattern{.Length = 16, .Tracks = {}};
    PlaceBlock(pattern, 0, {.Row = 0, .Length = 4, .Note = 60, .Volume = 1});
    PlaceBlock(pattern, 0, {.Row = 8, .Length = 2, .Note = 67, .Volume = 0.5f});
    CHECK(GetNote(pattern, 0, 0) == "C4");
    CHECK(GetNote(pattern, 0, 4) == "off");
    CHECK(GetNote(pattern, 0, 8) == "G4:0.5");
    CHECK(GetNote(pattern, 0, 10) == "off");
    auto blocks = Blocks(pattern, 0);
    REQUIRE(blocks.size() == 2);
    CHECK((blocks[0] == NoteBlock{.Row = 0, .Length = 4, .Note = 60, .Volume = 1}));
    CHECK(blocks[1].Row == 8 && blocks[1].Length == 2);

    // A block inside another one cuts it.
    PlaceBlock(pattern, 0, {.Row = 2, .Length = 1, .Note = 62, .Volume = 1});
    blocks = Blocks(pattern, 0);
    REQUIRE(blocks.size() == 3);
    CHECK(blocks[0].Length == 2);
    CHECK(blocks[1].Row == 2 && blocks[1].Length == 1);

    // Erasing a block that the one before runs into ends that one where it was.
    CHECK(EraseBlock(pattern, 0, 2));
    blocks = Blocks(pattern, 0);
    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0].Length == 2);
    CHECK(EraseBlock(pattern, 0, 9));
    CHECK(!EraseBlock(pattern, 0, 9));
    CHECK(Blocks(pattern, 0).size() == 1);
    CHECK(GetNote(pattern, 0, 10).empty());

    // A block that reaches the end of the pattern has no "off".
    PlaceBlock(pattern, 0, {.Row = 12, .Length = 8, .Note = 48, .Volume = 1});
    blocks = Blocks(pattern, 0);
    CHECK(blocks.back().Row == 12 && blocks.back().Length == 4);
}

TEST(NewSongIsReadyToWrite) {
    const Song song = NewSong();
    CHECK(song.Instruments.size() == 4);
    REQUIRE(song.Patterns.size() == 1);
    CHECK(Channels(song) == 4);
    CHECK(song.Order.size() == 1 && song.Patterns.contains(song.Order[0]));
    CHECK(ChannelInstrument(song, 2) == "kick");
    for (const Preset& preset : Presets()) CHECK(FindPreset(preset.Name) == &preset);
}
