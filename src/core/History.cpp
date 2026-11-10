#include "BitMusic/History.h"

#include <utility>

namespace BitMusic {

void Record(History& history, const BitMotor::Song& before) {
    history.Undo.push_back(before);
    history.Redo.clear();
    if (history.Limit > 0 && history.Undo.size() > history.Limit)
        history.Undo.erase(history.Undo.begin(), history.Undo.end() - std::ptrdiff_t(history.Limit));
}

bool Undo(History& history, BitMotor::Song& song) {
    if (history.Undo.empty()) return false;
    history.Redo.push_back(std::move(song));
    song = std::move(history.Undo.back());
    history.Undo.pop_back();
    return true;
}

bool Redo(History& history, BitMotor::Song& song) {
    if (history.Redo.empty()) return false;
    history.Undo.push_back(std::move(song));
    song = std::move(history.Redo.back());
    history.Redo.pop_back();
    return true;
}

void Clear(History& history) {
    history.Undo.clear();
    history.Redo.clear();
}

} // namespace BitMusic
