// The explorer (instruments and patterns, on the left) and the playlist (the song's Order, on top).
#include "Editor.h"

#include <algorithm>
#include <cmath>

namespace BitMusic {

using namespace BitMotor;

namespace {

// A row of small buttons under a list; returns which one was pressed (-1: none).
int ButtonRow(Rect area, std::initializer_list<std::pair<std::string, bool>> buttons) {
    const float gap = 4, width = std::floor((area.Width - gap * float(buttons.size() - 1)) / float(buttons.size()));
    int index = 0, pressed = -1;
    for (const auto& [label, enabled] : buttons) {
        if (Fx::Button({area.X + float(index) * (width + gap), area.Y, width, area.Height}, label, enabled)) pressed = index;
        ++index;
    }
    return pressed;
}

bool InUse(const Song& song, const std::string& instrument) {
    for (const auto& [name, pattern] : song.Patterns)
        for (const SongTrack& track : pattern.Tracks)
            if (track.Instrument == instrument) return true;
    return false;
}

} // namespace

void Editor::StartRename(std::string what) {
    renaming = std::move(what);
    field = renaming.substr(renaming.find(':') + 1);
    dialog = Dialog::Rename;
}

void Editor::FinishRename() {
    dialog = Dialog::None;
    const std::size_t colon = renaming.find(':');
    const std::string kind = renaming.substr(0, colon), from = renaming.substr(colon + 1);
    if (from == field) return;
    const bool done = kind == "instrument" ? RenameInstrument(song, from, field) : RenamePattern(song, from, field);
    if (!done) {
        Say(Tr("status.name.taken", field));
        return;
    }
    if (kind == "instrument" && instrument == from) instrument = field;
    if (kind == "pattern" && pattern == from) pattern = field;
    Touch();
}

void Editor::DeleteInstrument(const std::string& name, bool ask) {
    if (ask && InUse(song, name)) {
        renaming = name;
        dialog = Dialog::Delete;
        return;
    }
    RemoveInstrument(song, name);
    Touch();
}

// ---------------------------------------------------------------- explorer

void Editor::DrawExplorer(Rect area) {
    const Rect box = area.Expand(-6);
    const float half = std::floor((box.Height - 6) / 2);

    // Instruments: the list, and drag one onto a channel of the steps or the tracker.
    const Rect top = Fx::Panel({box.X, box.Y, box.Width, half}, Tr("explorer.instruments"));
    const std::vector<std::string> instruments = Names(song, false);
    int selected = IndexOf(instruments, instrument);
    const Rect list{top.X, top.Y, top.Width, top.Height - 34};
    if (Fx::ListView(list, selected, instruments, instrumentListScroll) && selected >= 0) {
        instrument = instruments[std::size_t(selected)];
        sideTab = 0;
    }
    if (selected >= 0) Fx::DragSource(list, "instrument:" + instrument, instrument);
    if (Fx::BeginContextMenu(list)) {
        if (Fx::MenuItem(Tr("menu.instrument.new"))) {
            instrument = AddInstrument(song, Tr("instrument.default"));
            Touch();
        }
        if (Fx::MenuItem(Tr("menu.rename"), {}, selected >= 0)) StartRename("instrument:" + instrument);
        if (Fx::MenuItem(Tr("menu.delete"), {}, selected >= 0)) DeleteInstrument(instrument, true);
        Fx::EndMenu();
    }
    switch (ButtonRow({top.X, top.Bottom() - 28, top.Width, 28},
                      {{"+", true}, {Tr("explorer.copy"), selected >= 0}, {Tr("explorer.rename"), selected >= 0},
                       {"-", selected >= 0}})) {
    case 0: instrument = AddInstrument(song, Tr("instrument.default")); Touch(); break;
    case 1: instrument = AddInstrument(song, instrument, song.Instruments[instrument]); Touch(); break;
    case 2: StartRename("instrument:" + instrument); break;
    case 3: DeleteInstrument(instrument, true); break;
    default: break;
    }

    // Patterns: choosing one edits it below; drag one onto the playlist.
    const Rect bottom = Fx::Panel({box.X, box.Y + half + 6, box.Width, box.Height - half - 6}, Tr("explorer.patterns"));
    const std::vector<std::string> patterns = Names(song, true);
    int chosen = IndexOf(patterns, pattern);
    const Rect rows{bottom.X, bottom.Y, bottom.Width, bottom.Height - 34};
    if (Fx::ListView(rows, chosen, patterns, patternListScroll) && chosen >= 0) pattern = patterns[std::size_t(chosen)];
    // A colour mark at the left of each visible pattern (the rows are as tall as JFxStyle's list rows).
    const Fx::Palette& palette = Fx::GetPalette();
    const float rowHeight = std::round(Text::Measure("H", palette.Text).Y + palette.Padding * 1.75f);
    for (int i = patternListScroll; i < int(patterns.size()); ++i) {
        const float y = rows.Y + 1 + float(i - patternListScroll) * rowHeight;
        if (y + rowHeight > rows.Bottom()) break;
        UI::Rect({rows.X + 2, y + 3, 3, rowHeight - 6}, PatternColor(song, patterns[std::size_t(i)]));
    }
    if (chosen >= 0) Fx::DragSource(rows, "pattern:" + pattern, pattern);
    switch (ButtonRow({bottom.X, bottom.Bottom() - 28, bottom.Width, 28},
                      {{"+", true}, {Tr("explorer.clone"), chosen >= 0}, {Tr("explorer.rename"), chosen >= 0},
                       {"-", chosen >= 0 && patterns.size() > 1}})) {
    case 0: pattern = AddPattern(song, std::to_string(song.Patterns.size() + 1)); Touch(); break;
    case 1: pattern = ClonePattern(song, pattern); Touch(); break;
    case 2: StartRename("pattern:" + pattern); break;
    case 3:
        RemovePattern(song, pattern);
        pattern = song.Patterns.begin()->first;
        Touch();
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- playlist

void Editor::DrawPlaylist(Rect area) {
    const Fx::Palette& palette = Fx::GetPalette();
    const Rect inside = Fx::Panel(area.Expand(-6), Tr("playlist.title"));
    const float ruler = 18, blockHeight = std::max(30.0f, inside.Height - ruler - 18);

    // The width of each entry of Order: its pattern's length times the zoom.
    std::vector<float> starts;
    float total = 0;
    for (const std::string& name : song.Order) {
        starts.push_back(total);
        const auto found = song.Patterns.find(name);
        total += float(found == song.Patterns.end() ? BarRows(song) : found->second.Length) * playlistZoom;
    }
    starts.push_back(total);
    if (UI::Hovered(inside) && CtrlDown() && Input::MouseWheel() != 0)
        playlistZoom = std::clamp(playlistZoom * (Input::MouseWheel() > 0 ? 1.25f : 0.8f), 1.0f, 24.0f);

    const Vec2 at = Fx::BeginScrollArea(inside, {total + 200, ruler + blockHeight}, playlistScroll);
    const Rect shown = Fx::ScrollView();
    // The ruler: a tick every bar, numbered.
    UI::Rect({shown.X, at.Y, shown.Width, ruler}, Fx::Derive(palette.Background, -0.2f));
    const float bar = float(BarRows(song)) * playlistZoom;
    for (int b = 0; float(b) * bar < total + 200; ++b) {
        const float x = at.X + float(b) * bar;
        if (x < shown.X - bar || x > shown.Right()) continue;
        UI::Rect({x, at.Y + 10, 1, ruler - 10}, palette.TextColor.WithAlpha(0.5f));
        if (bar >= 24 || b % 4 == 0) Text::Draw(std::to_string(b + 1), {x + 3, at.Y + 2}, Mono(palette.TextColor.WithAlpha(0.75f), 1));
    }

    // The blocks.
    int dropAt = int(song.Order.size());
    for (std::size_t i = 0; i < song.Order.size(); ++i) {
        const std::string& name = song.Order[i];
        const Rect block{at.X + starts[i] + 1, at.Y + ruler + 2, starts[i + 1] - starts[i] - 2, blockHeight - 4};
        const auto found = song.Patterns.find(name);
        const bool selected = int(i) == orderCursor;
        if (found == song.Patterns.end()) {
            // An entry the synthesizer skips: red stripes.
            Fx::RoundedRect(block, 3, Color::Hex(0x5A2020));
            for (float x = block.X; x < block.Right(); x += 8) UI::Line({x, block.Bottom()}, {std::min(block.Right(), x + 8), block.Y}, Color::Hex(0xC04040));
        } else {
            const Color color = PatternColor(song, name);
            Fx::RoundedRect(block, 3, color.Lerp(palette.Background, 0.55f), color.Lerp(palette.Background, 0.7f));
            // A miniature of its notes.
            const Pattern& p = found->second;
            for (const SongTrack& track : p.Tracks)
                for (std::size_t r = 0; r < track.Notes.size() && int(r) < p.Length; ++r) {
                    const auto entry = NoteEntry::Parse(track.Notes[r]);
                    if (!entry || entry->Type != NoteEntry::Kind::On) continue;
                    const float y = block.Y + 18 + (block.Height - 22) * (1 - std::clamp((entry->Note - 24) / 84, 0.0f, 1.0f));
                    UI::Rect({block.X + float(r) * playlistZoom, y, std::max(1.0f, playlistZoom - 1), 2}, color);
                }
        }
        if (selected) Fx::RoundedOutline(block, 3, palette.Accent, 2);
        Fx::Label({block.X + 4, block.Y, block.Width - 6, 16}, name);
        if (int(i) == song.Loop) {
            // The loop flag on the ruler.
            UI::Triangle({block.X, at.Y}, {block.X, at.Y + 12}, {block.X + 10, at.Y + 6}, Color::Hex(0xE0B040));
        }
        if (UI::Clicked(block)) {
            orderCursor = int(i);
            if (found != song.Patterns.end()) pattern = name;
        }
        Fx::DragSource(block, "order:" + std::to_string(i), name);
        if (Fx::BeginContextMenu(block)) {
            orderCursor = int(i);
            if (Fx::MenuItem(Tr("playlist.duplicate"))) {
                InsertOrder(song, int(i) + 1, name);
                Touch();
            }
            if (Fx::MenuItem(Tr("playlist.loop.here"))) {
                song.Loop = int(i);
                Touch();
            }
            if (Fx::MenuItem(Tr("playlist.loop.none"), {}, song.Loop >= 0)) {
                song.Loop = -1;
                Touch();
            }
            if (Fx::MenuItem(Tr("playlist.play.here"))) {
                StopPlaying();
                songMode = true;
                Music::Play(song, 0, true, int(i), 0);
                playing = true;
                ApplyMutes();
            }
            Fx::MenuSeparator();
            if (Fx::MenuItem(Tr("playlist.remove"))) {
                RemoveOrder(song, int(i));
                Touch();
            }
            Fx::EndMenu();
        }
        if (Input::Mouse().X < block.Center().X && dropAt == int(song.Order.size())) dropAt = int(i);
    }

    // The playhead.
    if (playing && songMode && position.Order >= 0 && position.Order < int(song.Order.size())) {
        const float x = at.X + starts[std::size_t(position.Order)] + float(position.Row) * playlistZoom;
        UI::Rect({x, at.Y, 2, ruler + blockHeight}, Color::Hex(0x7FD0FF));
    }
    if (song.Order.empty()) Fx::Label({shown.X + 10, at.Y + ruler, shown.Width - 20, 30}, Tr("playlist.empty"));
    Fx::EndScrollArea();

    // Patterns dropped from the explorer go in where they fall; entries dragged along the playlist move.
    if (std::string payload; Fx::DropTarget(inside, payload)) {
        if (payload.starts_with("pattern:")) {
            InsertOrder(song, dropAt, payload.substr(8));
            orderCursor = dropAt;
            Touch();
        } else if (payload.starts_with("order:")) {
            const int from = std::stoi(payload.substr(6));
            const int to = dropAt > from ? dropAt - 1 : dropAt;
            if (CtrlDown()) InsertOrder(song, dropAt, song.Order[std::size_t(from)]);
            else MoveOrder(song, from, to);
            orderCursor = CtrlDown() ? dropAt : to;
            Touch();
        }
    }
}

} // namespace BitMusic
