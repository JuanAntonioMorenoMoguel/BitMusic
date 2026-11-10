#pragma once
#include <BitMotor/Audio/Music.h>

#include <string>
#include <string_view>
#include <vector>

// Editing a song: small functions on BitMotor's Song, each one keeping the song valid for BitMotor (the names
// in Order and in the tracks exist, every pattern has the same channels, Loop points into Order). The editor
// calls them; they open no window and play nothing, so the tests and the command line use them too.
//     Song song;
//     AddInstrument(song, "bass");
//     std::string intro = AddPattern(song, "intro");
//     SetChannelInstrument(song, 0, "bass");
//     SetNote(song.Patterns[intro], 0, 0, "C2");
//     InsertOrder(song, 0, intro);
namespace BitMusic {

using BitMotor::Instrument;
using BitMotor::NoteEntry;
using BitMotor::Pattern;
using BitMotor::Song;
using BitMotor::SongTrack;

// `base` if no key of the song uses it, otherwise "base 2", "base 3"...
std::string UniqueInstrumentName(const Song& song, std::string_view base);
std::string UniquePatternName(const Song& song, std::string_view base);

// The rows of a bar (Rows * Beats), at least 1: the length of a new pattern and the steps of a bar.
int BarRows(const Song& song);

// ---------------------------------------------------------------- channels

// How many channels the song has: the most tracks of any pattern.
int Channels(const Song& song);
// Gives every pattern the same number of tracks (the song's channels), filling with empty tracks that use
// the instrument of that channel in other patterns.
void EvenChannels(Song& song);
// A new channel at the end of every pattern, with `instrument`. Returns its number.
int AddChannel(Song& song, std::string_view instrument = {});
// Removes channel `channel` from every pattern.
void RemoveChannel(Song& song, int channel);
// Moves a channel to another place (the others shift), in every pattern.
void MoveChannel(Song& song, int from, int to);
// The instrument of a channel: that of its track in the first pattern that has one.
std::string ChannelInstrument(const Song& song, int channel);
// Sets a channel's instrument in every pattern.
void SetChannelInstrument(Song& song, int channel, std::string_view instrument);

// ---------------------------------------------------------------- instruments

// A new instrument with BitMotor's defaults (or `instrument`). Returns its name (made unique).
std::string AddInstrument(Song& song, std::string_view name, const Instrument& instrument = {});
// Renames it in the song and in every track that uses it. false if `from` is missing or `to` is taken.
bool RenameInstrument(Song& song, const std::string& from, const std::string& to);
// Removes it; the tracks that used it are left without an instrument (they stay silent).
void RemoveInstrument(Song& song, const std::string& name);

// ---------------------------------------------------------------- patterns

// A new pattern of a bar (BarRows), with the song's channels and their instruments. Returns its name.
std::string AddPattern(Song& song, std::string_view name);
// A copy of a pattern under a new name. Returns it (empty if `name` is missing).
std::string ClonePattern(Song& song, const std::string& name);
// Renames it in the song and in Order. false if `from` is missing or `to` is taken.
bool RenamePattern(Song& song, const std::string& from, const std::string& to);
// Removes it and its entries in Order (Loop follows the entry it pointed at, or the one after).
void RemovePattern(Song& song, const std::string& name);
// Changes the length (at least 1): the notes after it are cut, a longer pattern ends in silence ("").
void SetLength(Pattern& pattern, int length);

// ---------------------------------------------------------------- notes

// The entry at a channel and row ("" past the end of the track, or for a missing track).
std::string GetNote(const Pattern& pattern, int channel, int row);
// Writes an entry, adding tracks and rows as needed and trimming the empty ones at the end of the track.
// Rows past the pattern's Length are ignored.
void SetNote(Pattern& pattern, int channel, int row, std::string_view entry);
// A step sequencer's click: a row with a note becomes "", an empty one gets `note`.
void ToggleStep(Pattern& pattern, int channel, int row, std::string_view note);
// Moves the notes of rows [first, last] of a channel by `semitones` (keeping their volumes; MIDI 0..127).
void Transpose(Pattern& pattern, int channel, int first, int last, int semitones);

// ---------------------------------------------------------------- order

// Puts a pattern into Order at `index` (clamped; Loop keeps pointing at the same entry).
void InsertOrder(Song& song, int index, const std::string& pattern);
void RemoveOrder(Song& song, int index);
void MoveOrder(Song& song, int from, int to);

// ---------------------------------------------------------------- piano roll

// A note as a block on a piano roll: from its row until the next entry that is not "" (or the end of the
// pattern). Only a track's notes; "off" and the empty rows are what is between blocks.
struct NoteBlock {
    int Row = 0;
    int Length = 1;
    float Note = 60;
    float Volume = 1;
    bool operator==(const NoteBlock&) const = default;
};
std::vector<NoteBlock> Blocks(const Pattern& pattern, int channel);
// Writes a block: its note at Row, "" over the rest of it and "off" just after it (unless another note
// starts there or the pattern ends). What it covers is replaced; a block that started earlier ends where
// this one starts.
void PlaceBlock(Pattern& pattern, int channel, const NoteBlock& block);
// Removes the block that covers `row` (and its "off"); a block just before it ends where it was. false if no
// block covers the row.
bool EraseBlock(Pattern& pattern, int channel, int row);

} // namespace BitMusic
