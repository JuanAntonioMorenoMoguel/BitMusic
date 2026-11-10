#include "BitMusic/Edit.h"

#include <algorithm>
#include <cmath>

namespace BitMusic {

namespace {

template <class Map> std::string UniqueKey(const Map& map, std::string_view base) {
    std::string name(base.empty() ? std::string_view("untitled") : base);
    if (!map.contains(name)) return name;
    for (int n = 2;; ++n) {
        std::string candidate = name + " " + std::to_string(n);
        if (!map.contains(candidate)) return candidate;
    }
}

bool IsKeep(std::string_view entry) {
    const auto parsed = NoteEntry::Parse(entry);
    return !parsed || parsed->Type == NoteEntry::Kind::Keep;
}

void Trim(SongTrack& track) {
    while (!track.Notes.empty() && track.Notes.back().empty()) track.Notes.pop_back();
}

// After removing entry `index` from Order (Loop follows the entry, or the one after it).
void LoopAfterRemove(Song& song, int index) {
    if (song.Loop < 0) return;
    if (index < song.Loop) --song.Loop;
    const int size = int(song.Order.size());
    if (song.Loop >= size) song.Loop = std::max(0, size - 1);
}

} // namespace

std::string UniqueInstrumentName(const Song& song, std::string_view base) { return UniqueKey(song.Instruments, base); }
std::string UniquePatternName(const Song& song, std::string_view base) { return UniqueKey(song.Patterns, base); }

int BarRows(const Song& song) { return std::max(1, song.Rows * song.Beats); }

// ---------------------------------------------------------------- channels

int Channels(const Song& song) {
    std::size_t channels = 0;
    for (const auto& [name, pattern] : song.Patterns) channels = std::max(channels, pattern.Tracks.size());
    return int(channels);
}

std::string ChannelInstrument(const Song& song, int channel) {
    if (channel < 0) return {};
    for (const auto& [name, pattern] : song.Patterns)
        if (std::size_t(channel) < pattern.Tracks.size() && !pattern.Tracks[std::size_t(channel)].Instrument.empty())
            return pattern.Tracks[std::size_t(channel)].Instrument;
    return {};
}

void EvenChannels(Song& song) {
    const int channels = Channels(song);
    std::vector<std::string> instruments;
    for (int c = 0; c < channels; ++c) instruments.push_back(ChannelInstrument(song, c));
    for (auto& [name, pattern] : song.Patterns)
        while (int(pattern.Tracks.size()) < channels)
            pattern.Tracks.push_back({.Instrument = instruments[pattern.Tracks.size()], .Notes = {}});
}

int AddChannel(Song& song, std::string_view instrument) {
    EvenChannels(song);
    const int channel = Channels(song);
    for (auto& [name, pattern] : song.Patterns) pattern.Tracks.push_back({.Instrument = std::string(instrument), .Notes = {}});
    return channel;
}

void RemoveChannel(Song& song, int channel) {
    if (channel < 0) return;
    for (auto& [name, pattern] : song.Patterns)
        if (std::size_t(channel) < pattern.Tracks.size()) pattern.Tracks.erase(pattern.Tracks.begin() + channel);
}

void MoveChannel(Song& song, int from, int to) {
    EvenChannels(song);
    const int channels = Channels(song);
    if (from < 0 || from >= channels || from == to) return;
    to = std::clamp(to, 0, channels - 1);
    for (auto& [name, pattern] : song.Patterns) {
        SongTrack track = std::move(pattern.Tracks[std::size_t(from)]);
        pattern.Tracks.erase(pattern.Tracks.begin() + from);
        pattern.Tracks.insert(pattern.Tracks.begin() + to, std::move(track));
    }
}

void SetChannelInstrument(Song& song, int channel, std::string_view instrument) {
    if (channel < 0) return;
    for (auto& [name, pattern] : song.Patterns) {
        if (std::size_t(channel) >= pattern.Tracks.size()) pattern.Tracks.resize(std::size_t(channel) + 1);
        pattern.Tracks[std::size_t(channel)].Instrument = std::string(instrument);
    }
    EvenChannels(song);
}

// ---------------------------------------------------------------- instruments

std::string AddInstrument(Song& song, std::string_view name, const Instrument& instrument) {
    std::string unique = UniqueInstrumentName(song, name);
    song.Instruments[unique] = instrument;
    return unique;
}

bool RenameInstrument(Song& song, const std::string& from, const std::string& to) {
    if (to.empty() || !song.Instruments.contains(from) || song.Instruments.contains(to)) return from == to;
    auto node = song.Instruments.extract(from);
    node.key() = to;
    song.Instruments.insert(std::move(node));
    for (auto& [name, pattern] : song.Patterns)
        for (SongTrack& track : pattern.Tracks)
            if (track.Instrument == from) track.Instrument = to;
    return true;
}

void RemoveInstrument(Song& song, const std::string& name) {
    song.Instruments.erase(name);
    for (auto& [key, pattern] : song.Patterns)
        for (SongTrack& track : pattern.Tracks)
            if (track.Instrument == name) track.Instrument.clear();
}

// ---------------------------------------------------------------- patterns

std::string AddPattern(Song& song, std::string_view name) {
    EvenChannels(song);
    Pattern pattern{.Length = BarRows(song), .Tracks = {}};
    for (int c = 0, channels = Channels(song); c < channels; ++c)
        pattern.Tracks.push_back({.Instrument = ChannelInstrument(song, c), .Notes = {}});
    std::string unique = UniquePatternName(song, name);
    song.Patterns[unique] = std::move(pattern);
    return unique;
}

std::string ClonePattern(Song& song, const std::string& name) {
    const auto found = song.Patterns.find(name);
    if (found == song.Patterns.end()) return {};
    Pattern copy = found->second;
    std::string unique = UniquePatternName(song, name);
    song.Patterns[unique] = std::move(copy);
    return unique;
}

bool RenamePattern(Song& song, const std::string& from, const std::string& to) {
    if (to.empty() || !song.Patterns.contains(from) || song.Patterns.contains(to)) return from == to;
    auto node = song.Patterns.extract(from);
    node.key() = to;
    song.Patterns.insert(std::move(node));
    for (std::string& entry : song.Order)
        if (entry == from) entry = to;
    return true;
}

void RemovePattern(Song& song, const std::string& name) {
    song.Patterns.erase(name);
    for (int i = int(song.Order.size()) - 1; i >= 0; --i)
        if (song.Order[std::size_t(i)] == name) RemoveOrder(song, i);
}

void SetLength(Pattern& pattern, int length) {
    pattern.Length = std::max(1, length);
    for (SongTrack& track : pattern.Tracks) {
        if (int(track.Notes.size()) > pattern.Length) track.Notes.resize(std::size_t(pattern.Length));
        Trim(track);
    }
}

// ---------------------------------------------------------------- notes

std::string GetNote(const Pattern& pattern, int channel, int row) {
    if (channel < 0 || row < 0 || std::size_t(channel) >= pattern.Tracks.size()) return {};
    const auto& notes = pattern.Tracks[std::size_t(channel)].Notes;
    return std::size_t(row) < notes.size() ? notes[std::size_t(row)] : std::string();
}

void SetNote(Pattern& pattern, int channel, int row, std::string_view entry) {
    if (channel < 0 || row < 0 || row >= pattern.Length) return;
    if (std::size_t(channel) >= pattern.Tracks.size()) {
        if (entry.empty()) return;
        pattern.Tracks.resize(std::size_t(channel) + 1);
    }
    SongTrack& track = pattern.Tracks[std::size_t(channel)];
    if (std::size_t(row) >= track.Notes.size()) {
        if (entry.empty()) return;
        track.Notes.resize(std::size_t(row) + 1);
    }
    track.Notes[std::size_t(row)] = std::string(entry);
    Trim(track);
}

void ToggleStep(Pattern& pattern, int channel, int row, std::string_view note) {
    SetNote(pattern, channel, row, IsKeep(GetNote(pattern, channel, row)) ? note : std::string_view());
}

void Transpose(Pattern& pattern, int channel, int first, int last, int semitones) {
    if (first > last) std::swap(first, last);
    for (int row = std::max(0, first); row <= last && row < pattern.Length; ++row) {
        auto entry = NoteEntry::Parse(GetNote(pattern, channel, row));
        if (!entry || entry->Type != NoteEntry::Kind::On) continue;
        entry->Note = std::clamp(entry->Note + float(semitones), 0.0f, 127.0f);
        SetNote(pattern, channel, row, entry->ToString());
    }
}

// ---------------------------------------------------------------- order

void InsertOrder(Song& song, int index, const std::string& pattern) {
    index = std::clamp(index, 0, int(song.Order.size()));
    song.Order.insert(song.Order.begin() + index, pattern);
    if (song.Loop >= 0 && index <= song.Loop && song.Order.size() > 1) ++song.Loop;
}

void RemoveOrder(Song& song, int index) {
    if (index < 0 || index >= int(song.Order.size())) return;
    song.Order.erase(song.Order.begin() + index);
    LoopAfterRemove(song, index);
}

void MoveOrder(Song& song, int from, int to) {
    const int size = int(song.Order.size());
    if (from < 0 || from >= size) return;
    to = std::clamp(to, 0, size - 1);
    if (from == to) return;
    std::string entry = std::move(song.Order[std::size_t(from)]);
    song.Order.erase(song.Order.begin() + from);
    song.Order.insert(song.Order.begin() + to, std::move(entry));
    if (song.Loop < 0) return;
    if (song.Loop == from) song.Loop = to;
    else if (from < song.Loop && to >= song.Loop) --song.Loop;
    else if (from > song.Loop && to <= song.Loop) ++song.Loop;
}

// ---------------------------------------------------------------- piano roll

std::vector<NoteBlock> Blocks(const Pattern& pattern, int channel) {
    std::vector<NoteBlock> blocks;
    for (int row = 0; row < pattern.Length; ++row) {
        const auto entry = NoteEntry::Parse(GetNote(pattern, channel, row));
        if (!entry || entry->Type == NoteEntry::Kind::Keep) {
            if (!blocks.empty() && blocks.back().Row + blocks.back().Length == row) ++blocks.back().Length;
            continue;
        }
        if (entry->Type == NoteEntry::Kind::On)
            blocks.push_back({.Row = row, .Length = 1, .Note = entry->Note, .Volume = entry->Volume});
    }
    return blocks;
}

void PlaceBlock(Pattern& pattern, int channel, const NoteBlock& block) {
    const int start = std::max(0, block.Row);
    const int end = std::min(pattern.Length, block.Row + std::max(1, block.Length));
    if (start >= end) return;
    NoteEntry note{.Type = NoteEntry::Kind::On, .Note = block.Note, .Volume = block.Volume};
    SetNote(pattern, channel, start, note.ToString());
    for (int row = start + 1; row < end; ++row) SetNote(pattern, channel, row, "");
    if (end < pattern.Length && IsKeep(GetNote(pattern, channel, end))) SetNote(pattern, channel, end, "off");
}

bool EraseBlock(Pattern& pattern, int channel, int row) {
    const std::vector<NoteBlock> blocks = Blocks(pattern, channel);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const NoteBlock& block = blocks[i];
        if (row < block.Row || row >= block.Row + block.Length) continue;
        const bool previousRunsIn = i > 0 && blocks[i - 1].Row + blocks[i - 1].Length == block.Row;
        SetNote(pattern, channel, block.Row, previousRunsIn ? "off" : "");
        const int end = block.Row + block.Length;
        const auto after = NoteEntry::Parse(GetNote(pattern, channel, end));
        if (after && after->Type == NoteEntry::Kind::Off) SetNote(pattern, channel, end, "");
        return true;
    }
    return false;
}

} // namespace BitMusic
