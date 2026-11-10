#include "Editor.h"

#include "AppInfo.h"

#include <BitMusic/Presets.h>
#include <BitMusic/Wav.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <system_error>

namespace BitMusic {

using namespace BitMotor;

namespace {

constexpr float MenuHeight = 26, TransportHeight = 42, StatusHeight = 24;
constexpr double AutosaveSeconds = 60;

std::filesystem::path WithExtension(std::filesystem::path file, std::string_view extension) {
    if (file.extension() != extension) file += extension;
    return file;
}

// The folders and .bitmusic files of a folder, folders first ("..": the parent).
struct Entry {
    std::string Label;
    std::filesystem::path Path;
    bool Folder = false;
};

std::vector<Entry> ListFolder(const std::filesystem::path& folder) {
    std::vector<Entry> folders, files;
    std::error_code error;
    for (const auto& item : std::filesystem::directory_iterator(folder, error)) {
        const std::string name = item.path().filename().string();
        if (name.starts_with(".")) continue;
        std::error_code kindError;
        if (item.is_directory(kindError)) folders.push_back({name + "/", item.path(), true});
        else if (item.path().extension() == ".bitmusic") files.push_back({name, item.path(), false});
    }
    auto byName = [](const Entry& a, const Entry& b) { return a.Label < b.Label; };
    std::sort(folders.begin(), folders.end(), byName);
    std::sort(files.begin(), files.end(), byName);
    std::vector<Entry> entries;
    if (folder.has_parent_path() && folder.parent_path() != folder) entries.push_back({"../", folder.parent_path(), true});
    entries.insert(entries.end(), folders.begin(), folders.end());
    entries.insert(entries.end(), files.begin(), files.end());
    return entries;
}

} // namespace

// ---------------------------------------------------------------- helpers shared by the views

Color PatternColor(const Song& song, const std::string& name) {
    static const Color hues[12] = {Color::Hex(0x4A88C7), Color::Hex(0x5FA55A), Color::Hex(0xD4A84B), Color::Hex(0xC75B5B),
                                   Color::Hex(0x9A6FC7), Color::Hex(0x4BB3B3), Color::Hex(0xC77A4A), Color::Hex(0x8FB34B),
                                   Color::Hex(0xC75B9A), Color::Hex(0x5B6FC7), Color::Hex(0xB3A04B), Color::Hex(0x6F9A8F)};
    int index = 0;
    for (const auto& [key, pattern] : song.Patterns) {
        if (key == name) return hues[index % 12];
        ++index;
    }
    return Color::Hex(0x777777);
}

TextStyle Mono(Color color, int size) { return {.Font = {}, .Size = size, .Color = color}; }

void Write(std::string_view text, Vec2 at, Color color, Align align) {
    TextStyle style = Fx::GetPalette().Text;
    style.Color = color.A > 0 ? color : Fx::GetPalette().TextColor;
    style.Align = align;
    Text::Draw(text, at, style);
}

std::string TrackerNote(std::string_view text) {
    const auto entry = NoteEntry::Parse(text);
    if (!entry) return "???";
    if (entry->Type == NoteEntry::Kind::Keep) return "---";
    if (entry->Type == NoteEntry::Kind::Off) return "===";
    std::string name = NoteName(entry->Note);
    if (name.size() == 2) name.insert(1, "-");
    return name;
}

std::vector<std::string> Names(const Song& song, bool patterns) {
    std::vector<std::string> names;
    if (patterns)
        for (const auto& [name, pattern] : song.Patterns) names.push_back(name);
    else
        for (const auto& [name, instrument] : song.Instruments) names.push_back(name);
    return names;
}

int IndexOf(const std::vector<std::string>& names, const std::string& name) {
    const auto found = std::find(names.begin(), names.end(), name);
    return found == names.end() ? -1 : int(found - names.begin());
}

int KeyNote(Key key, int octave) {
    static const std::pair<Key, int> keys[] = {
        {Key::Z, 0},     {Key::S, 1},     {Key::X, 2},     {Key::D, 3},      {Key::C, 4},     {Key::V, 5},
        {Key::G, 6},     {Key::B, 7},     {Key::H, 8},     {Key::N, 9},      {Key::J, 10},    {Key::M, 11},
        {Key::Comma, 12}, {Key::Period, 14}, {Key::Q, 12},  {Key::Num2, 13},  {Key::W, 14},    {Key::Num3, 15},
        {Key::E, 16},    {Key::R, 17},    {Key::Num5, 18}, {Key::T, 19},     {Key::Num6, 20}, {Key::Y, 21},
        {Key::Num7, 22}, {Key::U, 23},    {Key::I, 24},    {Key::Num9, 25},  {Key::O, 26},    {Key::Num0, 27},
        {Key::P, 28}};
    for (const auto& [k, offset] : keys)
        if (k == key) {
            const int note = 12 * (octave + 1) + offset;
            return note >= 0 && note <= 127 ? note : -1;
        }
    return -1;
}

bool CtrlDown() { return Input::KeyDown(Key::LeftCtrl) || Input::KeyDown(Key::RightCtrl); }
bool ShiftDown() { return Input::KeyDown(Key::LeftShift) || Input::KeyDown(Key::RightShift); }

// ---------------------------------------------------------------- start and stop

EditorSettings& Editor::Prefs() { return Settings::Get<EditorSettings>(); }

void Editor::Start() {
    Settings::Use<EditorSettings>();
    inter = Font::Load("assets/fonts/Inter-Regular.otf", {.Size = 14});
    UseTheme();
    // The keyboard is the tracker's: arrows move its cursor, not a focus through the widgets.
    UI::SetKeyboardFocus(false);
    folder = std::filesystem::current_path();
    stepsGrid = {.Cell = {26, 30}, .Columns = 16, .Rows = 4};
    pianoGrid = {.Cell = {22, 14}, .Columns = 16, .Rows = 128, .Scroll = {0, (127 - 84) * 14.0f}};
    trackerGrid = {.Cell = {124, 20}, .Columns = 4, .Rows = 16};

    std::filesystem::path file;
    for (const std::string& arg : Args::All())
        if (arg.ends_with(".bitmusic")) file = std::filesystem::absolute(arg);
    if (file.empty() || !OpenFile(file)) NewFile();
}

void Editor::Stop() {
    Music::ReleaseNotes();
    if (dirty) song.Save(Files::SavePath("autosave/last.bitmusic"));
    Settings::Save();
}

void Editor::UseTheme() {
    Fx::Palette palette = Fx::Modena(Color::Hex(0x3C3F41), Color::Hex(0x4A88C7));
    palette.Text = {.Font = inter};
    Fx::SetPalette(palette);
    UI::SetTheme(Fx::Theme());
}

void Editor::Say(std::string text) {
    message = std::move(text);
    messageTime = Time::RealTotal();
}

// ---------------------------------------------------------------- undo

void Editor::Touch() {
    changed = true;
    dirty = true;
}

void Editor::Commit() {
    if (!changed) return;
    Record(history, committed);
    committed = song;
    changed = false;
    if (playing) Play(); // what sounds follows the edit, from where it was
}

void Editor::DoUndo() {
    Commit();
    if (!Undo(history, song)) return;
    committed = song;
    dirty = true;
    if (!song.Patterns.contains(pattern)) pattern = song.Patterns.empty() ? "" : song.Patterns.begin()->first;
    Say(Tr("status.undone"));
    if (playing) Play();
}

void Editor::DoRedo() {
    Commit();
    if (!Redo(history, song)) return;
    committed = song;
    dirty = true;
    if (!song.Patterns.contains(pattern)) pattern = song.Patterns.empty() ? "" : song.Patterns.begin()->first;
    if (playing) Play();
}

// ---------------------------------------------------------------- files

void Editor::NewFile() {
    StopPlaying();
    song = NewSong();
    path.clear();
    committed = song;
    Clear(history);
    changed = dirty = false;
    pattern = song.Order.empty() ? "" : song.Order[0];
    instrument = song.Instruments.empty() ? "" : song.Instruments.begin()->first;
    orderCursor = channel = row = 0;
    stepNotes.clear();
}

bool Editor::OpenFile(const std::filesystem::path& file) {
    auto loaded = Song::Load(file);
    if (!loaded) {
        Say(Tr("status.cannot.open", file.filename().string()));
        return false;
    }
    StopPlaying();
    song = std::move(*loaded);
    path = file;
    committed = song;
    Clear(history);
    changed = dirty = false;
    pattern = !song.Order.empty() && song.Patterns.contains(song.Order[0]) ? song.Order[0]
              : song.Patterns.empty()                                     ? ""
                                                                          : song.Patterns.begin()->first;
    instrument = song.Instruments.empty() ? "" : song.Instruments.begin()->first;
    orderCursor = channel = row = 0;
    stepNotes.clear();
    folder = file.parent_path();
    AddRecent(file);
    Say(Tr("status.opened", file.filename().string()));
    return true;
}

bool Editor::SaveFile(const std::filesystem::path& file) {
    Commit();
    if (!song.Save(file)) {
        Say(Tr("status.cannot.save", file.filename().string()));
        return false;
    }
    path = file;
    dirty = false;
    folder = file.parent_path();
    AddRecent(file);
    Say(Tr("status.saved", file.filename().string()));
    return true;
}

void Editor::AddRecent(const std::filesystem::path& file) {
    auto& recent = Prefs().Recent;
    const std::string text = file.string();
    recent.erase(std::remove(recent.begin(), recent.end(), text), recent.end());
    recent.insert(recent.begin(), text);
    if (recent.size() > 10) recent.resize(10);
    Settings::Save();
}

// New, Open and Quit ask first when there are unsaved changes.
void Editor::Ask(Pending what) {
    Commit();
    if (dirty) {
        pending = what;
        dialog = Dialog::Discard;
        return;
    }
    Finish(what);
}

void Editor::Finish(Pending what) {
    pending = Pending::None;
    dialog = Dialog::None;
    switch (what) {
    case Pending::New: NewFile(); break;
    case Pending::Open:
        if (!field.empty() && std::filesystem::path(field).is_absolute() && Files::Exists(field)) {
            OpenFile(field); // a recent file
        } else {
            dialog = Dialog::Open;
            field.clear();
            fileSelected = -1;
        }
        break;
    case Pending::Quit: Quit(); break;
    case Pending::None: break;
    }
}

// ---------------------------------------------------------------- playing

void Editor::Play(bool fromCursor) {
    int order = 0, from = 0;
    if (playing) {
        position = Music::Position();
        order = std::max(0, position.Order);
        from = position.Row;
    } else if (fromCursor) {
        from = row;
        order = orderCursor;
    }
    if (songMode && !song.Order.empty()) {
        if (!playing && fromCursor && (orderCursor >= int(song.Order.size()) || song.Order[std::size_t(orderCursor)] != pattern)) {
            const auto found = std::find(song.Order.begin(), song.Order.end(), pattern);
            order = found == song.Order.end() ? 0 : int(found - song.Order.begin());
        }
        Music::Play(song, 0, true, order, from);
    } else {
        Song loop = song;
        loop.Order = {pattern};
        loop.Loop = 0;
        Music::Play(loop, 0, true, 0, from);
    }
    playing = true;
    ApplyMutes();
}

void Editor::StopPlaying() {
    if (playing) Music::Stop(0.05f);
    playing = false;
    position = {};
}

void Editor::ApplyMutes() {
    const int channels = std::max(ChannelCount(), int(muted.size()));
    muted.resize(std::size_t(channels), false);
    solo.resize(std::size_t(channels), false);
    const bool anySolo = std::find(solo.begin(), solo.end(), true) != solo.end();
    for (int c = 0; c < channels && c < 64; ++c) Music::Mute(c, muted[std::size_t(c)] || (anySolo && !solo[std::size_t(c)]));
}

int Editor::PreviewNote(float note) {
    std::string name;
    if (const Pattern* p = Current(); p && std::size_t(channel) < p->Tracks.size()) name = p->Tracks[std::size_t(channel)].Instrument;
    if (name.empty() || !song.Instruments.contains(name)) name = instrument;
    const auto found = song.Instruments.find(name);
    if (found == song.Instruments.end()) return -1;
    return Music::PlayNote(found->second, note);
}

// ---------------------------------------------------------------- keys

void Editor::Update() {
    if (playing) {
        position = Music::Position();
        if (!Music::IsPlaying()) StopPlaying();
        else if (songMode && Prefs().Follow && position.Order >= 0 && position.Order < int(song.Order.size()) &&
                 song.Patterns.contains(song.Order[std::size_t(position.Order)])) {
            pattern = song.Order[std::size_t(position.Order)];
            orderCursor = position.Order;
        }
    }
    Keys();
    if (dirty && Time::RealTotal() - autosaveTime > AutosaveSeconds) {
        autosaveTime = Time::RealTotal();
        const std::string name = path.empty() ? std::string("untitled.bitmusic") : path.filename().string();
        song.Save(Files::SavePath("autosave/" + name));
    }
}

void Editor::Keys() {
    // Notes held on the computer keyboard stop when their key goes up.
    for (auto it = heldKeys.begin(); it != heldKeys.end();) {
        if (!Input::KeyDown(Key(it->first))) {
            if (it->second >= 0) Music::ReleaseNote(it->second);
            it = heldKeys.erase(it);
        } else {
            ++it;
        }
    }
    if (dialog != Dialog::None || UI::Typing()) return;
    const bool ctrl = CtrlDown(), shift = ShiftDown();
    if (ctrl) {
        if (Input::KeyPressed(Key::N)) Ask(Pending::New);
        if (Input::KeyPressed(Key::O)) {
            field.clear();
            Ask(Pending::Open);
        }
        if (Input::KeyPressed(Key::S)) {
            if (shift || path.empty()) {
                dialog = Dialog::SaveAs;
                field = path.empty() ? "song.bitmusic" : path.filename().string();
            } else {
                SaveFile(path);
            }
        }
        if (Input::KeyPressed(Key::Z)) shift ? DoRedo() : DoUndo();
        if (Input::KeyPressed(Key::Y)) DoRedo();
        if (Input::KeyPressed(Key::Q)) Ask(Pending::Quit);
        if (Input::KeyPressed(Key::Space)) {
            StopPlaying();
            Play(true);
        }
        if (view == View::Tracker) TrackerKeys(ctrl, shift);
        return;
    }
    if (Input::KeyPressed(Key::Space)) playing ? StopPlaying() : Play();
    if (Input::KeyPressed(Key::F5)) showPlaylist = !showPlaylist;
    if (Input::KeyPressed(Key::F6)) view = View::Steps;
    if (Input::KeyPressed(Key::F7)) view = View::PianoRoll;
    if (Input::KeyPressed(Key::F8)) showExplorer = !showExplorer;
    if (Input::KeyPressed(Key::F9)) sideTab = sideTab == 1 ? 0 : 1;
    if (Input::KeyPressed(Key::F10)) view = View::Tracker;
    if (Input::KeyPressed(Key::F12)) Screen::Screenshot();
    if (Input::KeyPressed(Key::L)) {
        songMode = !songMode;
        if (playing) {
            StopPlaying();
            Play();
        }
    }
    if (Input::KeyPressed(Key::KeypadPlus)) octave = std::min(8, octave + 1);
    if (Input::KeyPressed(Key::KeypadMinus)) octave = std::max(0, octave - 1);
    if (view == View::Tracker) {
        TrackerKeys(ctrl, shift);
        return;
    }
    for (int k = 0; k < int(Key::Count); ++k) {
        const int note = KeyNote(Key(k), octave);
        if (note >= 0 && Input::KeyPressed(Key(k)) && !heldKeys.contains(k)) {
            heldKeys[k] = PreviewNote(float(note));
        }
    }
}

void Editor::TrackerKeys(bool ctrl, bool shift) {
    Pattern* p = Current();
    if (!p) return;
    const int channels = std::max(1, ChannelCount()), length = p->Length;
    auto moveTo = [&](int c, int r) {
        channel = std::clamp(c, 0, channels - 1);
        row = std::clamp(r, 0, length - 1);
        selectA = selectB = {};
    };
    if (ctrl) {
        if (Input::KeyPressed(Key::C)) Copy(false);
        if (Input::KeyPressed(Key::X)) Copy(true);
        if (Input::KeyPressed(Key::V)) Paste();
        if (Input::KeyPressed(Key::A)) {
            selectA = {0, 0};
            selectB = {channels - 1, length - 1};
        }
        if (Input::KeyPressed(Key::Up)) TransposeSelection(12);
        if (Input::KeyPressed(Key::Down)) TransposeSelection(-12);
        return;
    }
    if (shift) {
        if (Input::KeyPressed(Key::Up)) TransposeSelection(1);
        if (Input::KeyPressed(Key::Down)) TransposeSelection(-1);
        if (Input::KeyPressed(Key::Tab)) moveTo(channel - 1, row);
        return;
    }
    if (Input::KeyPressed(Key::Up)) moveTo(channel, row - 1);
    if (Input::KeyPressed(Key::Down)) moveTo(channel, row + 1);
    if (Input::KeyPressed(Key::Left)) moveTo(channel - 1, row);
    if (Input::KeyPressed(Key::Right) || Input::KeyPressed(Key::Tab)) moveTo(channel + 1, row);
    if (Input::KeyPressed(Key::PageUp)) moveTo(channel, row - BarRows(song));
    if (Input::KeyPressed(Key::PageDown)) moveTo(channel, row + BarRows(song));
    if (Input::KeyPressed(Key::Home)) moveTo(channel, 0);
    if (Input::KeyPressed(Key::End)) moveTo(channel, length - 1);
    if (Input::KeyPressed(Key::Delete)) {
        if (selectA.Valid()) {
            Copy(true);
            clipboard.clear();
        } else {
            SetNote(*p, channel, row, "");
            Touch();
            moveTo(channel, row + editStep);
        }
    }
    if (Input::KeyPressed(Key::Num1) && record) {
        SetNote(*p, channel, row, "off");
        Touch();
        moveTo(channel, row + editStep);
    }
    for (int k = 0; k < int(Key::Count); ++k) {
        const int note = KeyNote(Key(k), octave);
        if (note < 0 || !Input::KeyPressed(Key(k)) || heldKeys.contains(k)) continue;
        heldKeys[k] = PreviewNote(float(note));
        if (record) {
            std::string name = NoteName(float(note));
            if (const auto old = NoteEntry::Parse(GetNote(*p, channel, row)); old && old->Type == NoteEntry::Kind::On && old->Volume != 1)
                name = NoteEntry{.Type = NoteEntry::Kind::On, .Note = float(note), .Volume = old->Volume}.ToString();
            SetNote(*p, channel, row, name);
            stepNotes[channel] = NoteName(float(note));
            Touch();
            Commit();
            moveTo(channel, row + editStep);
        }
    }
}

// ---------------------------------------------------------------- the window

void Editor::Draw() {
    const float width = Screen::Width(), height = Screen::Height();
    Fx::Background();
    if (song.Patterns.contains(pattern) == false && !song.Patterns.empty()) pattern = song.Patterns.begin()->first;
    if (!song.Instruments.contains(instrument) && !song.Instruments.empty()) instrument = song.Instruments.begin()->first;

    DrawMenus({0, 0, width, MenuHeight});
    DrawTransport({0, MenuHeight, width, TransportHeight});
    const Rect body{0, MenuHeight + TransportHeight, width, height - MenuHeight - TransportHeight - StatusHeight};
    Rect left, rest = body;
    if (showExplorer) Fx::SplitPane(body, Prefs().ExplorerSplit, left, rest);
    Rect center, side;
    Fx::SplitPane(rest, Prefs().SideSplit, center, side);
    Rect top, bottom = center;
    if (showPlaylist) Fx::SplitPane(center, Prefs().PlaylistSplit, top, bottom, Fx::Orientation::Vertical);
    if (showExplorer) DrawExplorer(left);
    if (showPlaylist) DrawPlaylist(top);
    DrawPattern(bottom);
    DrawSide(side);
    DrawStatus({0, height - StatusHeight, width, StatusHeight});
    DrawDialog();

    // A drag or a slider becomes one undo step when the mouse is let go.
    if (changed && !Input::MouseDown(MouseButton::Left)) Commit();
}

void Editor::DrawMenus(Rect area) {
    Fx::BeginMenuBar(area);
    if (Fx::Menu(Tr("menu.file"))) {
        if (Fx::MenuItem(Tr("menu.new"), "Ctrl+N")) Ask(Pending::New);
        if (Fx::MenuItem(Tr("menu.open"), "Ctrl+O")) {
            field.clear();
            Ask(Pending::Open);
        }
        if (Fx::BeginSubMenu(Tr("menu.recent"), !Prefs().Recent.empty())) {
            const std::vector<std::string> recent = Prefs().Recent;
            for (const std::string& file : recent)
                if (Fx::MenuItem(std::filesystem::path(file).filename().string())) {
                    field = file;
                    Ask(Pending::Open);
                }
            Fx::MenuSeparator();
            if (Fx::MenuItem(Tr("menu.recent.clear"))) {
                Prefs().Recent.clear();
                Settings::Save();
            }
            Fx::EndMenu();
        }
        Fx::MenuSeparator();
        if (Fx::MenuItem(Tr("menu.save"), "Ctrl+S")) {
            if (path.empty()) {
                dialog = Dialog::SaveAs;
                field = "song.bitmusic";
            } else {
                SaveFile(path);
            }
        }
        if (Fx::MenuItem(Tr("menu.save.as"), "Ctrl+Shift+S")) {
            dialog = Dialog::SaveAs;
            field = path.empty() ? "song.bitmusic" : path.filename().string();
        }
        Fx::MenuSeparator();
        if (Fx::BeginSubMenu(Tr("menu.import"))) {
            for (const char* format : {"MIDI", "XM", "IT", "MOD"}) Fx::MenuItem(format, Tr("menu.soon"), false);
            Fx::EndMenu();
        }
        if (Fx::BeginSubMenu(Tr("menu.export"))) {
            if (Fx::MenuItem(Tr("menu.export.wav"))) {
                dialog = Dialog::ExportWav;
                std::filesystem::path out = path.empty() ? folder / "song.wav" : path;
                out.replace_extension(".wav");
                field = out.string();
            }
            for (const char* format : {"MIDI", "XM", "IT"}) Fx::MenuItem(format, Tr("menu.soon"), false);
            Fx::EndMenu();
        }
        Fx::MenuSeparator();
        if (Fx::MenuItem(Tr("menu.quit"), "Ctrl+Q")) Ask(Pending::Quit);
        Fx::EndMenu();
    }
    if (Fx::Menu(Tr("menu.edit"))) {
        if (Fx::MenuItem(Tr("menu.undo"), "Ctrl+Z", !history.Undo.empty() || changed)) DoUndo();
        if (Fx::MenuItem(Tr("menu.redo"), "Ctrl+Y", !history.Redo.empty())) DoRedo();
        Fx::MenuSeparator();
        const bool tracker = view == View::Tracker;
        if (Fx::MenuItem(Tr("menu.cut"), "Ctrl+X", tracker)) Copy(true);
        if (Fx::MenuItem(Tr("menu.copy"), "Ctrl+C", tracker)) Copy(false);
        if (Fx::MenuItem(Tr("menu.paste"), "Ctrl+V", tracker && !clipboard.empty())) Paste();
        if (Fx::MenuItem(Tr("menu.select.all"), "Ctrl+A", tracker && Current())) {
            selectA = {0, 0};
            selectB = {std::max(0, ChannelCount() - 1), Current()->Length - 1};
        }
        if (Fx::BeginSubMenu(Tr("menu.transpose"), tracker)) {
            if (Fx::MenuItem(Tr("menu.transpose.up"), "Shift+Up")) TransposeSelection(1);
            if (Fx::MenuItem(Tr("menu.transpose.down"), "Shift+Down")) TransposeSelection(-1);
            if (Fx::MenuItem(Tr("menu.octave.up"), "Ctrl+Up")) TransposeSelection(12);
            if (Fx::MenuItem(Tr("menu.octave.down"), "Ctrl+Down")) TransposeSelection(-12);
            Fx::EndMenu();
        }
        Fx::EndMenu();
    }
    if (Fx::Menu(Tr("menu.view"))) {
        Fx::CheckMenuItem(Tr("menu.view.playlist"), showPlaylist, "F5");
        if (Fx::MenuItem(Tr("tab.steps"), "F6")) view = View::Steps;
        if (Fx::MenuItem(Tr("tab.piano"), "F7")) view = View::PianoRoll;
        Fx::CheckMenuItem(Tr("menu.view.explorer"), showExplorer, "F8");
        if (Fx::MenuItem(Tr("tab.mixer"), "F9")) sideTab = 1;
        if (Fx::MenuItem(Tr("tab.tracker"), "F10")) view = View::Tracker;
        Fx::MenuSeparator();
        Fx::CheckMenuItem(Tr("menu.follow"), Prefs().Follow);
        if (Fx::BeginSubMenu(Tr("menu.language"))) {
            for (const std::string& code : Text::Languages()) {
                bool chosen = Text::Language() == code;
                if (Fx::CheckMenuItem(Text::LanguageName(code), chosen)) {
                    Prefs().Language = code;
                    Settings::Apply();
                    Settings::Save();
                }
            }
            Fx::EndMenu();
        }
        Fx::EndMenu();
    }
    if (Fx::Menu(Tr("menu.song"))) {
        if (Fx::MenuItem(Tr("menu.properties"))) dialog = Dialog::Properties;
        if (Fx::MenuItem(Tr("menu.unused"))) {
            int removed = 0;
            for (const std::string& name : Names(song, true))
                if (std::find(song.Order.begin(), song.Order.end(), name) == song.Order.end() && name != pattern) {
                    RemovePattern(song, name);
                    ++removed;
                }
            if (removed) Touch();
            Say(Tr("status.unused", removed));
        }
        Fx::EndMenu();
    }
    if (Fx::Menu(Tr("menu.instrument"))) {
        if (Fx::MenuItem(Tr("menu.instrument.new"))) {
            instrument = AddInstrument(song, Tr("instrument.default"));
            Touch();
        }
        if (Fx::BeginSubMenu(Tr("menu.instrument.preset"))) {
            for (const Preset& preset : Presets())
                if (Fx::MenuItem(Tr("preset." + std::string(preset.Name)))) {
                    instrument = AddInstrument(song, preset.Name, preset.Instrument);
                    Touch();
                }
            Fx::EndMenu();
        }
        const bool any = song.Instruments.contains(instrument);
        if (Fx::MenuItem(Tr("menu.instrument.duplicate"), {}, any)) {
            instrument = AddInstrument(song, instrument, song.Instruments[instrument]);
            Touch();
        }
        if (Fx::MenuItem(Tr("menu.rename"), {}, any)) StartRename("instrument:" + instrument);
        if (Fx::MenuItem(Tr("menu.delete"), {}, any)) DeleteInstrument(instrument, true);
        Fx::EndMenu();
    }
    if (Fx::Menu(Tr("menu.help"))) {
        if (Fx::MenuItem(Tr("menu.shortcuts"))) dialog = Dialog::Shortcuts;
        if (Fx::MenuItem(Tr("menu.about"))) dialog = Dialog::About;
        Fx::EndMenu();
    }
    Fx::EndMenuBar();
}

void Editor::DrawTransport(Rect area) {
    const Fx::Palette& palette = Fx::GetPalette();
    UI::Rect(area, Fx::Derive(palette.Background, -0.1f));
    Fx::Separator({area.X, area.Bottom() - 1, area.Width, 1});
    float x = area.X + 8;
    const float y = area.Y + 7, h = area.Height - 14;
    auto next = [&](float w) {
        const Rect r{x, y, w, h};
        x += w + 6;
        return r;
    };

    // Play / pause, stop, record: icons drawn over empty buttons.
    const Rect play = next(36);
    if (Fx::Button(play, "")) playing ? StopPlaying() : Play();
    UI::Tooltip(Tr("transport.play.tip"));
    const Vec2 c = play.Center();
    if (playing) {
        UI::Rect({c.X - 6, c.Y - 6, 4, 12}, palette.TextColor);
        UI::Rect({c.X + 2, c.Y - 6, 4, 12}, palette.TextColor);
    } else {
        UI::Triangle({c.X - 5, c.Y - 7}, {c.X - 5, c.Y + 7}, {c.X + 7, c.Y}, Color::Hex(0x6FCF6F));
    }
    const Rect stop = next(36);
    if (Fx::Button(stop, "")) StopPlaying();
    UI::Tooltip(Tr("transport.stop.tip"));
    UI::Rect(Rect::FromCenter(stop.Center(), {11, 11}), palette.TextColor);
    const Rect rec = next(36);
    Fx::ToggleButton(rec, "", record);
    UI::Tooltip(Tr("transport.record.tip"));
    UI::Circle(rec.Center(), 6, record ? Color::Hex(0xE05050) : palette.Mark);
    x += 8;

    bool patternMode = !songMode, songOn = songMode;
    if (Fx::ToggleButton(next(76), Tr("transport.pattern"), patternMode)) {
        songMode = false;
        if (playing) {
            StopPlaying();
            Play();
        }
    }
    UI::Tooltip(Tr("transport.mode.tip"));
    if (Fx::ToggleButton(next(76), Tr("transport.song"), songOn)) {
        songMode = true;
        if (playing) {
            StopPlaying();
            Play();
        }
    }
    UI::Tooltip(Tr("transport.mode.tip"));
    x += 8;

    auto label = [&](std::string_view text) {
        const float w = Text::Measure(text, palette.Text).X;
        Fx::Label(next(w), text);
    };
    label(Tr("transport.bpm"));
    if (Fx::Spinner(next(78), song.Bpm, 20, 400, 1, 0)) Touch();
    label(Tr("transport.rows"));
    if (Fx::Spinner(next(56), song.Rows, 1, 16)) Touch();
    UI::Tooltip(Tr("transport.rows.tip"));
    label(Tr("transport.beats"));
    if (Fx::Spinner(next(56), song.Beats, 1, 16)) Touch();
    UI::Tooltip(Tr("transport.beats.tip"));
    x += 8;

    // Where it plays: bar:beat:row of the pattern, and the Order entry.
    const Rect at = next(150);
    Fx::RoundedRect(at, 3, Color::Hex(0x1E2022));
    std::string where = "--  -:-:-";
    if (playing && position.Order >= 0) {
        const int r = position.Row, bar = BarRows(song), rows = std::max(1, song.Rows);
        where = std::format("{:02}  {}:{}:{}", position.Order + 1, r / bar + 1, (r % bar) / rows + 1, r % rows + 1);
    }
    Text::Draw(where, {at.X + 10, at.Center().Y - 7}, Mono(Color::Hex(0x7FD0FF)));
    UI::Tooltip(Tr("transport.position.tip"));
    x += 8;

    label(Tr("transport.pattern.current"));
    const std::vector<std::string> patterns = Names(song, true);
    int current = IndexOf(patterns, pattern);
    if (Fx::ComboBox(next(130), current, patterns, !patterns.empty()) && current >= 0) pattern = patterns[std::size_t(current)];
    label(Tr("transport.octave"));
    Fx::Spinner(next(56), octave, 0, 8);
    UI::Tooltip(Tr("transport.octave.tip"));
    label(Tr("transport.step"));
    Fx::Spinner(next(56), editStep, 0, 16);
    UI::Tooltip(Tr("transport.step.tip"));
}

void Editor::DrawStatus(Rect area) {
    const Fx::Palette& palette = Fx::GetPalette();
    UI::Rect(area, Fx::Derive(palette.Background, -0.15f));
    Fx::Separator({area.X, area.Y, area.Width, 1});
    std::string text;
    if (Time::RealTotal() - messageTime < 4) {
        text = message;
    } else if (const Pattern* p = Current()) {
        const std::string name = std::size_t(channel) < p->Tracks.size() ? p->Tracks[std::size_t(channel)].Instrument : "";
        text = Tr("status.cursor", channel + 1, name.empty() ? "-" : name, row, TrackerNote(GetNote(*p, channel, row))) +
               "   " + Tr("status.hint");
    }
    Fx::Label({area.X + 10, area.Y, area.Width * 0.7f, area.Height}, text);
    const std::string file = (path.empty() ? Tr("status.untitled") : path.filename().string()) + (dirty ? " *" : "");
    Fx::Label({area.X, area.Y, area.Width - 10, area.Height}, file, Align::Right);
}

// ---------------------------------------------------------------- dialogs

void Editor::DrawFileDialog(bool saving) {
    const Rect window = Rect::FromCenter(Screen::Center(), {560, 440});
    const Rect inside = Fx::BeginDialog(window, saving ? Tr("dialog.save.title") : Tr("dialog.open.title"));
    if (folder != folderShown) {
        folderShown = folder;
        folderField = folder.string();
    }
    std::error_code error;
    if (Fx::TextField({inside.X, inside.Y, inside.Width, 28}, {}, folderField) && std::filesystem::is_directory(folderField, error))
        folderShown = folder = folderField;
    const std::vector<Entry> entries = ListFolder(folder);
    std::vector<std::string> labels;
    for (const Entry& entry : entries) labels.push_back(entry.Label);
    const Rect list{inside.X, inside.Y + 36, inside.Width, inside.Height - 36 - 80};
    if (Fx::ListView(list, fileSelected, labels, fileListScroll) && fileSelected >= 0 && fileSelected < int(entries.size())) {
        const Entry& entry = entries[std::size_t(fileSelected)];
        if (entry.Folder) {
            folder = entry.Path;
            fileSelected = -1;
            fileListScroll = 0;
        } else {
            field = entry.Path.filename().string();
        }
    }
    Fx::TextField({inside.X, list.Bottom() + 8, inside.Width, 28}, Tr("dialog.file.name"), field,
                  {.Placeholder = "song.bitmusic"});
    const Rect buttons{inside.X, inside.Bottom() - 30, inside.Width, 30};
    const bool confirm = Fx::DefaultButton({buttons.Right() - 230, buttons.Y, 110, 30},
                                           saving ? Tr("dialog.save") : Tr("dialog.open"), !field.empty()) ||
                         (Input::KeyPressed(Key::Enter) && !field.empty() && !UI::Typing());
    const bool cancel = Fx::Button({buttons.Right() - 110, buttons.Y, 110, 30}, Tr("dialog.cancel")) ||
                        (UI::Pressed(Nav::Back) && !UI::Typing());
    Fx::EndDialog();
    if (cancel) {
        dialog = Dialog::None;
        pending = Pending::None;
    } else if (confirm) {
        const std::filesystem::path file = WithExtension(folder / field, ".bitmusic");
        dialog = Dialog::None;
        if (saving) {
            if (SaveFile(file) && pending != Pending::None) Finish(pending);
        } else {
            OpenFile(file);
        }
    }
}

void Editor::DrawDialog() {
    const Fx::Palette& palette = Fx::GetPalette();
    switch (dialog) {
    case Dialog::None: return;
    case Dialog::Open: DrawFileDialog(false); return;
    case Dialog::SaveAs: DrawFileDialog(true); return;
    default: break;
    }
    const bool back = UI::Pressed(Nav::Back) && !UI::Typing();
    const bool enter = Input::KeyPressed(Key::Enter) && !UI::Typing();

    if (dialog == Dialog::ExportWav) {
        const Rect inside = Fx::BeginDialog(Rect::FromCenter(Screen::Center(), {560, 200}), Tr("dialog.export.title"));
        Fx::TextField({inside.X, inside.Y, inside.Width, 28}, Tr("dialog.export.file"), field);
        Fx::Label({inside.X, inside.Y + 40, inside.Width * 0.5f, 28}, Tr("dialog.export.passes"));
        Fx::Spinner({inside.X + inside.Width * 0.5f, inside.Y + 40, 80, 28}, exportPasses, 1, 16);
        UI::Tooltip(Tr("dialog.export.passes.tip"));
        const bool go = Fx::DefaultButton({inside.Right() - 230, inside.Bottom() - 30, 110, 30}, Tr("dialog.export")) || enter;
        const bool cancel = Fx::Button({inside.Right() - 110, inside.Bottom() - 30, 110, 30}, Tr("dialog.cancel")) || back;
        Fx::EndDialog();
        if (go) {
            Commit();
            const std::filesystem::path out = WithExtension(std::filesystem::absolute(field), ".wav");
            Say(ExportWav(song, out, exportPasses) ? Tr("status.exported", out.filename().string())
                                                   : Tr("status.cannot.save", out.filename().string()));
            dialog = Dialog::None;
        } else if (cancel) {
            dialog = Dialog::None;
        }
        return;
    }
    if (dialog == Dialog::Rename) {
        const Rect inside = Fx::BeginDialog(Rect::FromCenter(Screen::Center(), {420, 150}), Tr("dialog.rename.title"));
        Fx::TextField({inside.X, inside.Y, inside.Width, 28}, {}, field);
        const bool ok = Fx::DefaultButton({inside.Right() - 230, inside.Bottom() - 30, 110, 30}, Tr("dialog.ok"), !field.empty()) ||
                        (enter && !field.empty());
        const bool cancel = Fx::Button({inside.Right() - 110, inside.Bottom() - 30, 110, 30}, Tr("dialog.cancel")) || back;
        Fx::EndDialog();
        if (ok) FinishRename();
        else if (cancel) dialog = Dialog::None;
        return;
    }
    if (dialog == Dialog::Properties) {
        const Rect inside = Fx::BeginDialog(Rect::FromCenter(Screen::Center(), {420, 300}), Tr("dialog.properties.title"));
        float y = inside.Y;
        auto line = [&](std::string_view text) {
            Fx::Label({inside.X, y, inside.Width * 0.5f, 28}, text);
            const Rect r{inside.X + inside.Width * 0.5f, y, inside.Width * 0.5f, 28};
            y += 36;
            return r;
        };
        if (Fx::Spinner(line(Tr("transport.bpm")), song.Bpm, 20, 400, 1, 0)) Touch();
        if (Fx::Spinner(line(Tr("dialog.properties.rows")), song.Rows, 1, 16)) Touch();
        if (Fx::Spinner(line(Tr("dialog.properties.beats")), song.Beats, 1, 16)) Touch();
        if (Fx::Slider(line(Tr("dialog.properties.volume")), song.Volume, 0, 1, 0.05f)) Touch();
        if (Fx::Spinner(line(Tr("dialog.properties.loop")), song.Loop, -1, std::max(0, int(song.Order.size()) - 1))) Touch();
        UI::Tooltip(Tr("dialog.properties.loop.tip"));
        const bool close = Fx::DefaultButton({inside.Right() - 110, inside.Bottom() - 30, 110, 30}, Tr("dialog.close")) ||
                           back || enter;
        Fx::EndDialog();
        if (close) dialog = Dialog::None;
        return;
    }
    if (dialog == Dialog::Discard) {
        const Rect inside = Fx::BeginDialog(Rect::FromCenter(Screen::Center(), {460, 160}), Tr("dialog.discard.title"));
        Text::Paragraph(Tr("dialog.discard.text"), inside.Position(), inside.Width,
                        {.Font = inter, .Color = palette.TextColor});
        const float b = inside.Bottom() - 30;
        const bool save = Fx::DefaultButton({inside.Right() - 350, b, 110, 30}, Tr("dialog.save")) || enter;
        const bool discard = Fx::Button({inside.Right() - 230, b, 110, 30}, Tr("dialog.discard"));
        const bool cancel = Fx::Button({inside.Right() - 110, b, 110, 30}, Tr("dialog.cancel")) || back;
        Fx::EndDialog();
        if (save) {
            if (path.empty()) {
                dialog = Dialog::SaveAs;
                field = "song.bitmusic";
            } else if (SaveFile(path)) {
                Finish(pending);
            }
        } else if (discard) {
            dirty = false;
            Finish(pending);
        } else if (cancel) {
            dialog = Dialog::None;
            pending = Pending::None;
        }
        return;
    }
    if (dialog == Dialog::Delete) {
        const Rect inside = Fx::BeginDialog(Rect::FromCenter(Screen::Center(), {460, 160}), Tr("dialog.delete.title"));
        Text::Paragraph(Tr("dialog.delete.text", renaming), inside.Position(), inside.Width,
                        {.Font = inter, .Color = palette.TextColor});
        const bool yes = Fx::DefaultButton({inside.Right() - 230, inside.Bottom() - 30, 110, 30}, Tr("menu.delete")) || enter;
        const bool cancel = Fx::Button({inside.Right() - 110, inside.Bottom() - 30, 110, 30}, Tr("dialog.cancel")) || back;
        Fx::EndDialog();
        if (yes) {
            dialog = Dialog::None;
            DeleteInstrument(renaming, false);
        } else if (cancel) {
            dialog = Dialog::None;
        }
        return;
    }
    // Shortcuts and About: a text and Close.
    const bool shortcuts = dialog == Dialog::Shortcuts;
    const Rect inside = Fx::BeginDialog(Rect::FromCenter(Screen::Center(), shortcuts ? Vec2{620, 470} : Vec2{460, 210}),
                                        shortcuts ? Tr("dialog.shortcuts.title") : Tr("dialog.about.title"));
    Text::Paragraph(shortcuts ? Tr("dialog.shortcuts.text") : Tr("dialog.about.text", BM_APP_VERSION), inside.Position(),
                    inside.Width, {.Font = inter, .Color = palette.TextColor});
    const bool close = Fx::DefaultButton({inside.Right() - 110, inside.Bottom() - 30, 110, 30}, Tr("dialog.close")) ||
                       back || enter;
    Fx::EndDialog();
    if (close) dialog = Dialog::None;
}

} // namespace BitMusic
