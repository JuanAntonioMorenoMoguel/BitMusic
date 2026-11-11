// The pattern being edited, in three tabs that edit the same Pattern: Steps, Piano roll and Tracker.
#include "Editor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>

namespace BitMusic {

using namespace BitMotor;

namespace {

// The note of a piano roll row (row 0 is the highest note).
float RollNote(int row) { return float(127 - row); }
int RollRow(float note) { return 127 - int(std::lround(note)); }

bool IsBlackKey(int note) {
    const int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

// Keeps `row` (of `size` pixels each) inside a view `height` tall that has scrolled `scroll`.
void KeepVisible(float& scroll, int row, float size, float height) {
    const float top = float(row) * size;
    if (top < scroll) scroll = top;
    else if (top + size > scroll + height) scroll = top + size - height;
}

// Zooms a grid's cells with Ctrl and the wheel over its area.
void Zoom(Rect area, Fx::Grid& grid, bool columns, bool rows) {
    const float wheel = Input::MouseWheel();
    if (wheel == 0 || !CtrlDown() || !UI::Hovered(area)) return;
    const float factor = wheel > 0 ? 1.2f : 1 / 1.2f;
    if (columns) grid.Cell.X = std::clamp(std::round(grid.Cell.X * factor), 8.0f, 96.0f);
    if (rows) grid.Cell.Y = std::clamp(std::round(grid.Cell.Y * factor), 8.0f, 48.0f);
}

} // namespace

Pattern* Editor::Current() {
    const auto found = song.Patterns.find(pattern);
    return found == song.Patterns.end() ? nullptr : &found->second;
}

int Editor::ChannelCount() {
    const Pattern* p = Current();
    return std::max(Channels(song), p ? int(p->Tracks.size()) : 0);
}

std::string Editor::StepNote(int track) {
    if (const auto found = stepNotes.find(track); found != stepNotes.end()) return found->second;
    if (const Pattern* p = Current()) {
        const std::vector<NoteBlock> blocks = Blocks(*p, track);
        if (!blocks.empty()) return NoteName(blocks.front().Note);
    }
    return "C4";
}

bool Editor::Sounding(int& sounding) {
    if (!playing || position.Order < 0) return false;
    if (songMode && (position.Order >= int(song.Order.size()) || song.Order[std::size_t(position.Order)] != pattern))
        return false;
    sounding = position.Row;
    return true;
}

void Editor::Copy(bool cut) {
    Pattern* p = Current();
    if (!p) return;
    const Fx::GridPoint a = selectA.Valid() ? selectA : Fx::GridPoint{channel, row};
    const Fx::GridPoint b = selectA.Valid() ? selectB : a;
    clipboard.clear();
    for (int c = std::min(a.Column, b.Column); c <= std::max(a.Column, b.Column); ++c) {
        std::vector<std::string>& column = clipboard.emplace_back();
        for (int r = std::min(a.Row, b.Row); r <= std::max(a.Row, b.Row); ++r) {
            column.push_back(GetNote(*p, c, r));
            if (cut) SetNote(*p, c, r, "");
        }
    }
    if (cut) Touch();
}

void Editor::Paste() {
    Pattern* p = Current();
    if (!p || clipboard.empty()) return;
    for (std::size_t c = 0; c < clipboard.size(); ++c)
        for (std::size_t r = 0; r < clipboard[c].size(); ++r)
            SetNote(*p, channel + int(c), row + int(r), clipboard[c][r]);
    Touch();
}

void Editor::TransposeSelection(int semitones) {
    Pattern* p = Current();
    if (!p) return;
    const Fx::GridPoint a = selectA.Valid() ? selectA : Fx::GridPoint{channel, row};
    const Fx::GridPoint b = selectA.Valid() ? selectB : a;
    for (int c = std::min(a.Column, b.Column); c <= std::max(a.Column, b.Column); ++c)
        Transpose(*p, c, a.Row, b.Row, semitones);
    Touch();
    Commit();
}

// ---------------------------------------------------------------- the tabs and their toolbar

void Editor::DrawPattern(Rect area) {
    const int channels = ChannelCount();
    if (int(muted.size()) < channels) ApplyMutes();
    int tab = int(view);
    const Rect page = Fx::TabPane(area.Expand(-6), tab, {Tr("tab.steps"), Tr("tab.piano"), Tr("tab.tracker")});
    view = View(tab);
    Pattern* p = Current();
    if (!p) {
        Fx::Label(page.Expand(-12), Tr("pattern.none"), Align::Center);
        return;
    }

    // The toolbar: the pattern's name and length, a new channel, and a hint.
    const Rect bar{page.X + 8, page.Y + 6, page.Width - 16, 28};
    float x = bar.X;
    UI::Rect({x, bar.Y + 4, 6, bar.Height - 8}, PatternColor(song, pattern));
    x += 12;
    Fx::Label({x, bar.Y, 120, bar.Height}, pattern);
    x += 124;
    Fx::Label({x, bar.Y, 60, bar.Height}, Tr("pattern.length"));
    x += 62;
    int length = p->Length;
    if (Fx::Spinner({x, bar.Y, 70, bar.Height}, length, 1, 256)) {
        SetLength(*p, length);
        Touch();
    }
    UI::Tooltip(Tr("pattern.length.tip"));
    x += 78;
    if (Fx::Button({x, bar.Y, 110, bar.Height}, Tr("pattern.add.channel"))) {
        AddChannel(song, instrument);
        Touch();
    }
    x += 118;
    Fx::Label({x, bar.Y, bar.Right() - x, bar.Height}, Tr(view == View::Tracker ? "pattern.hint.tracker" : "pattern.hint.zoom"));

    const Rect inside{page.X + 8, bar.Bottom() + 6, page.Width - 16, page.Bottom() - bar.Bottom() - 14};
    switch (view) {
    case View::Steps: DrawSteps(inside); break;
    case View::PianoRoll: DrawPianoRoll(inside); break;
    case View::Tracker: DrawTracker(inside); break;
    }
}

void Editor::DrawChannelMenu(Rect area, int track) {
    if (!Fx::BeginContextMenu(area)) return;
    const int channels = ChannelCount();
    std::string here;
    if (const Pattern* p = Current(); p && std::size_t(track) < p->Tracks.size()) here = p->Tracks[std::size_t(track)].Instrument;
    if (Fx::MenuItem(Tr("channel.everywhere"), {}, !here.empty())) {
        SetChannelInstrument(song, track, here);
        Touch();
    }
    Fx::MenuSeparator();
    if (Fx::MenuItem(Tr("channel.up"), {}, track > 0)) {
        MoveChannel(song, track, track - 1);
        Touch();
    }
    if (Fx::MenuItem(Tr("channel.down"), {}, track + 1 < channels)) {
        MoveChannel(song, track, track + 1);
        Touch();
    }
    if (Fx::MenuItem(Tr("pattern.add.channel"))) {
        AddChannel(song, instrument);
        Touch();
    }
    if (Fx::MenuItem(Tr("channel.remove"), {}, channels > 1)) {
        RemoveChannel(song, track);
        channel = std::min(channel, std::max(0, ChannelCount() - 1));
        Touch();
    }
    Fx::EndMenu();
}

// ---------------------------------------------------------------- steps

void Editor::DrawSteps(Rect area) {
    Pattern& p = *Current();
    const Fx::Palette& palette = Fx::GetPalette();
    const int channels = ChannelCount();
    if (int(muted.size()) < channels) ApplyMutes();
    const float rackWidth = 236, header = 18;
    const Rect rack{area.X, area.Y + header, rackWidth, area.Height - header};
    const Rect gridArea{area.X + rackWidth + 6, area.Y + header, area.Width - rackWidth - 6, area.Height - header};
    stepsGrid.Columns = p.Length;
    stepsGrid.Rows = channels;
    stepsGrid.MajorColumns = BarRows(song);
    stepsGrid.BandColumns = std::max(1, song.Rows);
    Zoom(gridArea, stepsGrid, true, true);
    const Vec2 cell = stepsGrid.Cell;

    // The step numbers over the grid, one per beat.
    Screen::PushClip({gridArea.X, area.Y, gridArea.Width, header});
    for (int c = 0; c < p.Length; c += std::max(1, song.Rows)) {
        const float x = gridArea.X + 1 + float(c) * cell.X - stepsGrid.Scroll.X;
        const bool bar = c % BarRows(song) == 0;
        Text::Draw(std::to_string(c / std::max(1, song.Rows) + 1), {x + 3, area.Y + 4},
                   Mono(bar ? palette.TextColor : palette.TextColor.WithAlpha(0.6f), 1));
    }
    Screen::PopClip();

    // The channel rack: number, instrument of this pattern's track, mute and solo.
    const std::vector<std::string> instruments = Names(song, false);
    Screen::PushClip(rack);
    for (int c = 0; c < channels; ++c) {
        const float y = rack.Y + 1 + float(c) * cell.Y - stepsGrid.Scroll.Y;
        if (y + cell.Y < rack.Y || y > rack.Bottom()) continue;
        const Rect line{rack.X, y, rack.Width, cell.Y};
        const float h = std::min(26.0f, cell.Y - 4), top = y + std::floor((cell.Y - h) / 2);
        if (c == channel) Fx::RoundedRect(line.Expand(-1), 3, palette.Accent.WithAlpha(0.18f));
        Fx::Label({rack.X + 4, top, 24, h}, std::to_string(c + 1));
        std::string here = std::size_t(c) < p.Tracks.size() ? p.Tracks[std::size_t(c)].Instrument : "";
        // An instrument the song does not have keeps its name in the list (marked), so it is not lost.
        std::vector<std::string> options = {"-"};
        options.insert(options.end(), instruments.begin(), instruments.end());
        int chosen = IndexOf(instruments, here) + 1;
        if (chosen == 0 && !here.empty()) {
            options.push_back("(?) " + here);
            chosen = int(options.size()) - 1;
        }
        const Rect combo{rack.X + 28, top, 86, h};
        // The note a click on a step writes, beside the instrument.
        const auto noteStyle = Mono(palette.TextColor.WithAlpha(0.85f), 1);
        const std::string base = StepNote(c);
        Text::Draw(base, {rack.X + 118, top + std::round((h - Text::Measure(base, noteStyle).Y) / 2)}, noteStyle);
        const bool lost = options.back().starts_with("(?) ");
        if (Fx::ComboBox(combo, chosen, options) && !(lost && chosen == int(options.size()) - 1)) {
            if (std::size_t(c) >= p.Tracks.size()) p.Tracks.resize(std::size_t(c) + 1);
            p.Tracks[std::size_t(c)].Instrument = chosen == 0 ? "" : options[std::size_t(chosen)];
            Touch();
        }
        // The track names an instrument the song does not have: a warning outline on its combo.
        if (lost && chosen == int(options.size()) - 1) Fx::RoundedOutline(combo, 3, Color::Hex(0xE0B040), 2);
        if (std::string payload; Fx::DropTarget(line, payload) && payload.starts_with("instrument:")) {
            if (std::size_t(c) >= p.Tracks.size()) p.Tracks.resize(std::size_t(c) + 1);
            p.Tracks[std::size_t(c)].Instrument = payload.substr(11);
            Touch();
        }
        bool m = muted[std::size_t(c)], s = solo[std::size_t(c)];
        if (Fx::ToggleButton({rack.X + 152, top, 38, h}, "M", m)) {
            muted[std::size_t(c)] = m;
            ApplyMutes();
        }
        UI::Tooltip(Tr("channel.mute.tip"));
        if (Fx::ToggleButton({rack.X + 194, top, 38, h}, "S", s)) {
            solo[std::size_t(c)] = s;
            ApplyMutes();
        }
        UI::Tooltip(Tr("channel.solo.tip"));
        if (UI::Clicked({rack.X, y, 28, cell.Y})) channel = c;
        DrawChannelMenu({rack.X, y, 28, cell.Y}, c);
    }
    Screen::PopClip();

    // The steps.
    int sounding = -1;
    const bool sounds = Sounding(sounding);
    const Fx::GridInput in = Fx::BeginGrid(gridArea, stepsGrid);
    const Color color = PatternColor(song, pattern);
    for (int c = 0; c < channels; ++c) {
        const std::string note = StepNote(c);
        for (int r = 0; r < p.Length; ++r) {
            const Rect box = Fx::GridCell(r, c).Expand(-3);
            const auto entry = NoteEntry::Parse(GetNote(p, c, r));
            if (!entry || entry->Type == NoteEntry::Kind::Keep) {
                Fx::RoundedRect(box, 3, Fx::Derive(palette.InnerBackground, 0.06f));
            } else if (entry->Type == NoteEntry::Kind::Off) {
                Fx::RoundedRect(box, 3, Fx::Derive(palette.InnerBackground, 0.06f));
                UI::Rect({box.X + 3, box.Center().Y - 1, box.Width - 6, 2}, palette.PromptText);
            } else {
                Fx::RoundedRect(box, 3, color.Lerp(palette.InnerBackground, 0.6f * (1 - entry->Volume)), color.Lerp(Colors::Black, 0.25f));
                // Steps with another note than the channel's show it, without the octave if it does not fit.
                std::string name = NoteName(entry->Note);
                if (name != note) {
                    if (Text::Measure(name, Mono(Colors::White, 1)).X > box.Width - 4 && name.size() > 1 &&
                        std::isdigit(static_cast<unsigned char>(name.back())))
                        name.pop_back();
                    if (Text::Measure(name, Mono(Colors::White, 1)).X <= box.Width - 4)
                        Text::Draw(name, {box.X + 2, box.Y + 2}, Mono(Colors::White, 1));
                }
            }
        }
    }
    // A clear line where each bar starts (every Rows * Beats steps: also in 3/4).
    for (int c = BarRows(song); c < p.Length; c += BarRows(song)) {
        const Rect first = Fx::GridCell(c, 0);
        UI::Rect({first.X - 1, first.Y, 2, float(channels) * cell.Y}, palette.Accent.WithAlpha(0.55f));
    }
    if (sounds)
        for (int c = 0; c < channels; ++c) Fx::RoundedOutline(Fx::GridCell(sounding, c).Expand(-1), 3, Color::Hex(0x7FD0FF), 2);
    Fx::EndGrid();

    // Click a step to set or clear it; drag along the channel to paint.
    if (in.Pressed.Valid()) {
        const int c = in.Pressed.Row, r = in.Pressed.Column;
        channel = c;
        row = r;
        const auto entry = NoteEntry::Parse(GetNote(p, c, r));
        paintOn = !entry || entry->Type != NoteEntry::Kind::On;
        SetNote(p, c, r, paintOn ? StepNote(c) : "");
        if (paintOn) {
            const int id = PreviewNote(*ParseNote(StepNote(c)));
            if (id >= 0) Music::ReleaseNote(id);
        }
        Touch();
    } else if (in.Dragging && in.Hovered.Valid() && in.Hovered.Row == in.DragFrom.Row) {
        const int c = in.Hovered.Row, r = in.Hovered.Column;
        const auto entry = NoteEntry::Parse(GetNote(p, c, r));
        const bool on = entry && entry->Type == NoteEntry::Kind::On;
        if (on != paintOn) {
            SetNote(p, c, r, paintOn ? StepNote(c) : "");
            Touch();
        }
    }
    if (in.RightPressed.Valid()) stepMenu = in.RightPressed;
    if (Fx::BeginContextMenu(gridArea)) {
        if (stepMenu.Valid()) {
            const int c = stepMenu.Row, r = stepMenu.Column;
            const auto entry = NoteEntry::Parse(GetNote(p, c, r));
            const bool on = entry && entry->Type == NoteEntry::Kind::On;
            for (int percent : {100, 75, 50, 25})
                if (Fx::MenuItem(Tr("step.volume", percent), {}, on)) {
                    NoteEntry changedEntry = *entry;
                    changedEntry.Volume = float(percent) / 100;
                    SetNote(p, c, r, changedEntry.ToString());
                    Touch();
                }
            if (Fx::MenuItem(Tr("step.default"), {}, on)) stepNotes[c] = NoteName(entry->Note);
            Fx::MenuSeparator();
            if (Fx::MenuItem(Tr("step.off"))) {
                SetNote(p, c, r, "off");
                Touch();
            }
            if (Fx::MenuItem(Tr("step.clear"))) {
                SetNote(p, c, r, "");
                Touch();
            }
        }
        Fx::EndMenu();
    }
}

// ---------------------------------------------------------------- piano roll

void Editor::DrawPianoRoll(Rect area) {
    Pattern& p = *Current();
    const Fx::Palette& palette = Fx::GetPalette();
    const int channels = std::max(1, ChannelCount());
    channel = std::clamp(channel, 0, channels - 1);

    // Which channel it edits.
    std::vector<std::string> labels;
    for (int c = 0; c < channels; ++c) {
        const std::string name = std::size_t(c) < p.Tracks.size() ? p.Tracks[std::size_t(c)].Instrument : "";
        labels.push_back(Tr("channel.name", c + 1, name.empty() ? "-" : name));
    }
    Fx::Label({area.X, area.Y, 60, 26}, Tr("piano.channel"));
    Fx::ComboBox({area.X + 62, area.Y, 180, 26}, channel, labels);
    Fx::Label({area.X + 252, area.Y, area.Width - 252, 26}, Tr("piano.hint"));

    const float keys = 52, lane = 56, top = area.Y + 32;
    const Rect gridArea{area.X + keys, top, area.Width - keys, area.Bottom() - top - lane - 6};
    const Rect laneArea{gridArea.X, gridArea.Bottom() + 6, gridArea.Width, lane};
    pianoGrid.Columns = p.Length;
    pianoGrid.Rows = 128;
    pianoGrid.MajorColumns = BarRows(song);
    pianoGrid.BandColumns = std::max(1, song.Rows);
    pianoGrid.MajorRows = 0;
    Zoom(gridArea, pianoGrid, true, true);
    const Vec2 cell = pianoGrid.Cell;

    // The keyboard on the left; a click plays the note.
    const Rect keyboard{area.X, top, keys - 4, gridArea.Height};
    Screen::PushClip(keyboard);
    for (int r = 0; r < 128; ++r) {
        const float y = top + 1 + float(r) * cell.Y - pianoGrid.Scroll.Y;
        if (y + cell.Y < keyboard.Y || y > keyboard.Bottom()) continue;
        const int note = 127 - r;
        const Rect key{keyboard.X, y, keyboard.Width, cell.Y - 1};
        UI::Rect(key, IsBlackKey(note) ? Color::Hex(0x202224) : Color::Hex(0xD8D8D8));
        if (note % 12 == 0) Text::Draw(NoteName(float(note)), {key.X + 3, key.Y + std::max(0.0f, (cell.Y - 8) / 2)}, Mono(Color::Hex(0x404040), 1));
        if (UI::Clicked(key)) {
            if (testNote >= 0) Music::ReleaseNote(testNote);
            testNote = PreviewNote(float(note));
        }
    }
    Screen::PopClip();

    int sounding = -1;
    const bool sounds = Sounding(sounding);
    const Fx::GridInput in = Fx::BeginGrid(gridArea, pianoGrid);
    // Black keys a little darker, and a line on every C.
    const Rect shown = Fx::ScrollView();
    for (int r = 0; r < 128; ++r) {
        const Rect first = Fx::GridCell(0, r);
        if (first.Bottom() < shown.Y || first.Y > shown.Bottom()) continue;
        if (IsBlackKey(127 - r)) UI::Rect({shown.X, first.Y, shown.Width, first.Height}, Colors::Black.WithAlpha(0.12f));
        if ((127 - r) % 12 == 0) UI::Rect({shown.X, first.Bottom() - 1, shown.Width, 1}, palette.BoxBorder.WithAlpha(0.6f));
    }
    // The other channels as grey ghosts, then this one's notes.
    for (int c = 0; c < channels; ++c) {
        if (c == channel) continue;
        for (const NoteBlock& b : Blocks(p, c)) {
            const Rect start = Fx::GridCell(b.Row, RollRow(b.Note));
            UI::Rect({start.X + 1, start.Y + 2, float(b.Length) * cell.X - 2, start.Height - 4}, palette.PromptText.WithAlpha(0.25f));
        }
    }
    const Color color = PatternColor(song, pattern);
    const std::vector<NoteBlock> blocks = Blocks(p, channel);
    auto blockAt = [&](int column, float note) -> const NoteBlock* {
        for (const NoteBlock& b : blocks)
            if (std::lround(b.Note) == std::lround(note) && column >= b.Row && column < b.Row + b.Length) return &b;
        return nullptr;
    };
    auto drawBlock = [&](const NoteBlock& b, Color fill, bool outline) {
        const Rect start = Fx::GridCell(b.Row, RollRow(b.Note));
        const Rect r{start.X + 1, start.Y + 1, float(b.Length) * cell.X - 2, start.Height - 2};
        Fx::RoundedRect(r, 2, fill);
        if (outline) Fx::RoundedOutline(r, 2, Colors::White, 1);
        UI::Rect({r.Right() - 3, r.Y + 2, 1, r.Height - 4}, Colors::Black.WithAlpha(0.35f)); // the handle to resize
        if (r.Width > 30 && cell.Y >= 12) Text::Draw(NoteName(b.Note), {r.X + 3, r.Y + (r.Height - 7) / 2}, Mono(Colors::Black.WithAlpha(0.7f), 1));
    };
    for (const NoteBlock& b : blocks) drawBlock(b, color.Lerp(palette.InnerBackground, 0.5f * (1 - b.Volume)), false);
    if (rollDrag != RollDrag::None && in.Dragging) drawBlock(rollBlock, color.WithAlpha(0.6f), true);
    for (int c = BarRows(song); c < p.Length; c += BarRows(song)) {
        const Rect first = Fx::GridCell(c, 0);
        UI::Rect({first.X - 1, shown.Y, 2, shown.Height}, palette.Accent.WithAlpha(0.45f));
    }
    if (sounds) {
        const Rect column = Fx::GridCell(sounding, 0);
        UI::Rect({column.X, shown.Y, 2, shown.Height}, Color::Hex(0x7FD0FF));
    }
    Fx::EndGrid();

    // Draw a note by dragging on empty cells; drag a note to move it, its right end to resize it; right-click
    // erases it.
    if (in.Pressed.Valid()) {
        const int column = in.Pressed.Column;
        const float note = RollNote(in.Pressed.Row);
        if (const NoteBlock* hit = blockAt(column, note)) {
            rollBlock = *hit;
            rollDrag = column == hit->Row + hit->Length - 1 && hit->Length > 1 ? RollDrag::Resize : RollDrag::Move;
            rollGrab = column - hit->Row;
        } else {
            rollBlock = {.Row = column, .Length = 1, .Note = note, .Volume = 1};
            rollDrag = RollDrag::Draw;
        }
        if (testNote >= 0) Music::ReleaseNote(testNote);
        testNote = PreviewNote(rollBlock.Note);
        dragOrigin = rollBlock;
    }
    if (in.Dragging && rollDrag != RollDrag::None) {
        if (rollDrag == RollDrag::Move) {
            const float note = RollNote(in.Hovered.Row);
            if (std::lround(note) != std::lround(rollBlock.Note)) {
                if (testNote >= 0) Music::ReleaseNote(testNote);
                testNote = PreviewNote(note);
            }
            rollBlock.Row = std::clamp(in.Hovered.Column - rollGrab, 0, p.Length - 1);
            rollBlock.Note = note;
        } else {
            rollBlock.Length = std::max(1, in.Hovered.Column - rollBlock.Row + 1);
        }
    }
    if (in.Released && rollDrag != RollDrag::None) {
        if (rollDrag == RollDrag::Move && !(rollBlock == dragOrigin)) EraseBlock(p, channel, dragOrigin.Row);
        if (!(rollBlock == dragOrigin) || rollDrag == RollDrag::Draw) {
            PlaceBlock(p, channel, rollBlock);
            stepNotes[channel] = NoteName(rollBlock.Note);
            Touch();
        }
        rollDrag = RollDrag::None;
    }
    if (in.RightPressed.Valid() && blockAt(in.RightPressed.Column, RollNote(in.RightPressed.Row)) &&
        EraseBlock(p, channel, in.RightPressed.Column))
        Touch();
    if (testNote >= 0 && !Input::MouseDown(MouseButton::Left)) {
        Music::ReleaseNote(testNote);
        testNote = -1;
    }

    // The velocity lane: the ":vol" of each note, drawn and dragged as bars.
    UI::Rect(laneArea, Fx::Derive(palette.InnerBackground, -0.05f));
    Screen::PushClip(laneArea);
    for (const NoteBlock& b : blocks) {
        const float x = laneArea.X + 1 + float(b.Row) * cell.X - pianoGrid.Scroll.X;
        const float h = (laneArea.Height - 6) * b.Volume;
        UI::Rect({x + 2, laneArea.Bottom() - 3 - h, std::max(3.0f, cell.X * 0.4f), h}, color);
    }
    Screen::PopClip();
    if (UI::Hovered(laneArea) && Input::MouseDown(MouseButton::Left)) {
        const Vec2 mouse = Input::Mouse();
        const int column = int(std::floor((mouse.X - laneArea.X - 1 + pianoGrid.Scroll.X) / cell.X));
        for (const NoteBlock& b : blocks)
            if (column >= b.Row && column < b.Row + b.Length) {
                NoteBlock changedBlock = b;
                changedBlock.Volume = std::clamp(std::round((1 - (mouse.Y - laneArea.Y - 3) / (laneArea.Height - 6)) * 20) / 20, 0.05f, 1.0f);
                if (changedBlock.Volume != b.Volume) {
                    PlaceBlock(p, channel, changedBlock);
                    Touch();
                }
                break;
            }
    }
    UI::Tooltip(Tr("piano.lane.tip"));
}

// ---------------------------------------------------------------- tracker

void Editor::DrawTracker(Rect area) {
    Pattern& p = *Current();
    const Fx::Palette& palette = Fx::GetPalette();
    const int channels = std::max(1, ChannelCount());
    if (int(muted.size()) < channels) ApplyMutes();
    channel = std::clamp(channel, 0, channels - 1);
    row = std::clamp(row, 0, p.Length - 1);
    const float numbers = 44, header = 30;
    const Rect gridArea{area.X + numbers, area.Y + header, area.Width - numbers, area.Height - header};
    trackerGrid.Columns = channels;
    trackerGrid.Rows = p.Length;
    trackerGrid.MajorRows = BarRows(song);
    trackerGrid.MajorColumns = 1;
    trackerGrid.BandColumns = 0;
    Zoom(gridArea, trackerGrid, false, true);
    const Vec2 cell = trackerGrid.Cell;

    int sounding = -1;
    const bool sounds = Sounding(sounding);
    const float viewHeight = gridArea.Height - 14;
    // Scrolls to the cursor when it moves (the wheel scrolls freely otherwise), or with the playback.
    if (sounds && Prefs().Follow) {
        KeepVisible(trackerGrid.Scroll.Y, std::min(p.Length - 1, sounding + 4), cell.Y, viewHeight);
    } else if (row != shownRow) {
        KeepVisible(trackerGrid.Scroll.Y, row, cell.Y, viewHeight);
        shownRow = row;
    }

    // The channels' headers: number, instrument, mute and solo; drop an instrument on one to use it.
    Screen::PushClip({gridArea.X, area.Y, gridArea.Width, header});
    for (int c = 0; c < channels; ++c) {
        const float x = gridArea.X + 1 + float(c) * cell.X - trackerGrid.Scroll.X;
        const Rect head{x, area.Y, cell.X - 2, header - 4};
        const std::string name = std::size_t(c) < p.Tracks.size() ? p.Tracks[std::size_t(c)].Instrument : "";
        Fx::RoundedRect(head, 3, c == channel ? palette.Accent.WithAlpha(0.35f) : Fx::Derive(palette.Background, -0.15f));
        Fx::Label({head.X + 6, head.Y, head.Width - 64, head.Height}, std::to_string(c + 1) + " " + (name.empty() ? "-" : name));
        bool m = muted[std::size_t(c)], s = solo[std::size_t(c)];
        if (Fx::ToggleButton({head.Right() - 56, head.Y + 2, 26, head.Height - 4}, "M", m)) {
            muted[std::size_t(c)] = m;
            ApplyMutes();
        }
        if (Fx::ToggleButton({head.Right() - 28, head.Y + 2, 26, head.Height - 4}, "S", s)) {
            solo[std::size_t(c)] = s;
            ApplyMutes();
        }
        if (std::string payload; Fx::DropTarget(head, payload) && payload.starts_with("instrument:")) {
            if (std::size_t(c) >= p.Tracks.size()) p.Tracks.resize(std::size_t(c) + 1);
            p.Tracks[std::size_t(c)].Instrument = payload.substr(11);
            Touch();
        }
        if (UI::Clicked(head)) channel = c;
        DrawChannelMenu(head, c);
    }
    Screen::PopClip();

    // Row numbers, brighter on every beat.
    const Rect rowNumbers{area.X, gridArea.Y, numbers - 4, gridArea.Height};
    Screen::PushClip(rowNumbers);
    for (int r = 0; r < p.Length; ++r) {
        const float y = gridArea.Y + 1 + float(r) * cell.Y - trackerGrid.Scroll.Y;
        if (y + cell.Y < rowNumbers.Y || y > rowNumbers.Bottom()) continue;
        const bool beat = r % std::max(1, song.Rows) == 0;
        Text::Draw(std::format("{:03}", r), {rowNumbers.X + 2, y + (cell.Y - 7) / 2},
                   Mono(beat ? palette.TextColor : palette.PromptText, 1));
    }
    Screen::PopClip();

    const Fx::GridInput in = Fx::BeginGrid(gridArea, trackerGrid);
    const Rect shown = Fx::ScrollView();
    for (int r = 0; r < p.Length; ++r) {
        const Rect first = Fx::GridCell(0, r);
        if (first.Bottom() < shown.Y || first.Y > shown.Bottom()) continue;
        if (r % std::max(1, song.Rows) == 0) UI::Rect({shown.X, first.Y, std::min(shown.Width, float(channels) * cell.X), first.Height}, Colors::White.WithAlpha(0.04f));
        if (sounds && r == sounding) UI::Rect({shown.X, first.Y, std::min(shown.Width, float(channels) * cell.X), first.Height}, Color::Hex(0x7FD0FF, 0.18f));
        for (int c = 0; c < channels; ++c) {
            const Rect box = Fx::GridCell(c, r);
            const std::string text = GetNote(p, c, r);
            const auto entry = NoteEntry::Parse(text);
            const float y = box.Y + std::max(1.0f, (box.Height - 14) / 2);
            const int size = cell.Y >= 16 ? 2 : 1;
            if (!entry) {
                Text::Draw(text.substr(0, 8), {box.X + 8, y}, Mono(Color::Hex(0xE06060), size));
                continue;
            }
            const Color noteColor = entry->Type == NoteEntry::Kind::On ? palette.TextColor
                                    : entry->Type == NoteEntry::Kind::Off ? Color::Hex(0xE0B040)
                                                                         : palette.PromptText.WithAlpha(0.5f);
            Text::Draw(TrackerNote(text), {box.X + 8, y}, Mono(noteColor, size));
            if (entry->Type == NoteEntry::Kind::On && entry->Volume != 1)
                Text::Draw(std::format("{:02}", std::min(99, int(std::lround(entry->Volume * 100)))), {box.X + 8 + 13.0f * float(size) * 3, y},
                           Mono(Color::Hex(0x7FD0FF), size));
        }
    }
    if (selectA.Valid()) Fx::GridSelection(selectA, selectB);
    Fx::RoundedOutline(Fx::GridCell(channel, row), 2, record ? Color::Hex(0xE05050) : palette.Focus, 2);
    Fx::EndGrid();

    // A click moves the cursor; a drag selects a block.
    if (in.Pressed.Valid()) {
        channel = in.Pressed.Column;
        row = in.Pressed.Row;
        selectA = selectB = {};
    } else if (in.Dragging && in.Hovered.Valid() && !(in.Hovered == in.DragFrom)) {
        selectA = in.DragFrom;
        selectB = in.Hovered;
        channel = in.Hovered.Column;
        row = in.Hovered.Row;
    }
    if (Fx::BeginContextMenu(gridArea)) {
        if (Fx::MenuItem(Tr("menu.cut"), "Ctrl+X")) Copy(true);
        if (Fx::MenuItem(Tr("menu.copy"), "Ctrl+C")) Copy(false);
        if (Fx::MenuItem(Tr("menu.paste"), "Ctrl+V", !clipboard.empty())) Paste();
        Fx::MenuSeparator();
        if (Fx::MenuItem(Tr("menu.transpose.up"), "Shift+Up")) TransposeSelection(1);
        if (Fx::MenuItem(Tr("menu.transpose.down"), "Shift+Down")) TransposeSelection(-1);
        if (Fx::MenuItem(Tr("menu.octave.up"), "Ctrl+Up")) TransposeSelection(12);
        if (Fx::MenuItem(Tr("menu.octave.down"), "Ctrl+Down")) TransposeSelection(-12);
        Fx::EndMenu();
    }
}

} // namespace BitMusic
