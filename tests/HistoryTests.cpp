#include "TestMain.h"

#include <BitMusic/Edit.h>
#include <BitMusic/History.h>

using namespace BitMusic;

TEST(UndoAndRedoGoBackAndForth) {
    History history;
    Song song;
    Song before = song;
    AddInstrument(song, "bass");
    Record(history, before);
    before = song;
    song.Bpm = 90;
    Record(history, before);

    CHECK(Undo(history, song));
    CHECK(song.Bpm == 120 && song.Instruments.size() == 1);
    CHECK(Undo(history, song));
    CHECK(song.Instruments.empty());
    CHECK(!Undo(history, song));
    CHECK(Redo(history, song));
    CHECK(Redo(history, song));
    CHECK(song.Bpm == 90);
    CHECK(!Redo(history, song));

    // A new change forgets what could be redone.
    Undo(history, song);
    before = song;
    song.Rows = 3;
    Record(history, before);
    CHECK(history.Redo.empty());
}

TEST(HistoryKeepsItsLimit) {
    History history{.Undo = {}, .Redo = {}, .Limit = 2};
    Song song;
    for (int i = 0; i < 5; ++i) {
        Song before = song;
        song.Bpm = float(100 + i);
        Record(history, before);
    }
    CHECK(history.Undo.size() == 2);
    Undo(history, song);
    Undo(history, song);
    CHECK(song.Bpm == 102);
}
