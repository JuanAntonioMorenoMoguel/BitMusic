#pragma once
#include <BitMotor/Audio/Music.h>

#include <cstddef>
#include <vector>

// Undo and redo as whole copies of the song: songs are small (a few KB), so a copy per change is simple and
// never goes out of step with the editing functions.
//     Song before = song;
//     SetNote(song.Patterns["intro"], 0, 4, "C4");
//     Record(history, before);
//     ...
//     Undo(history, song);   // song is as `before` again
namespace BitMusic {

struct History {
    std::vector<BitMotor::Song> Undo;
    std::vector<BitMotor::Song> Redo;
    // How many undo steps are kept (the oldest go first). 0: no limit.
    std::size_t Limit = 0;
};

// Remembers the song as it was before a change (and forgets what could be redone).
void Record(History& history, const BitMotor::Song& before);
// Goes back one change: `song` becomes the last recorded one and the current one can be redone. false if
// there is nothing to undo.
bool Undo(History& history, BitMotor::Song& song);
bool Redo(History& history, BitMotor::Song& song);
void Clear(History& history);

} // namespace BitMusic
