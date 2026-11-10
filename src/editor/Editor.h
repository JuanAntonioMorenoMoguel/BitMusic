#pragma once
// The BitMusic editor: one Game that draws every view each frame with UI::JFxStyle (docs/Editor.md).
// Editor.cpp: start, keys, layout, menus, transport, dialogs, files and playback.
// Explorer.cpp: instruments and patterns on the left, the playlist (Order) on top.
// PatternViews.cpp: the Steps, Piano roll and Tracker tabs. InstrumentViews.cpp: the instrument and the mixer.
#include <BitMotor/BitMotor.h>
#include <BitMusic/Edit.h>
#include <BitMusic/History.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace BitMusic {

namespace Fx = BitMotor::UI::JFxStyle;
using BitMotor::Color;
using BitMotor::Rect;
using BitMotor::Vec2;

// The editor's own settings (settings.json, next to BitMotor's).
struct EditorSettings : BitMotor::Settings {
    std::vector<std::string> Recent;
    bool Follow = true;
    float ExplorerSplit = 0.17f, SideSplit = 0.74f, PlaylistSplit = 0.3f;
};
BITMOTOR_FIELDS_EXTENDS(EditorSettings, BitMotor::Settings, Recent, Follow, ExplorerSplit, SideSplit, PlaylistSplit)

enum class View { Steps, PianoRoll, Tracker };
enum class Dialog { None, Open, SaveAs, ExportWav, Rename, Properties, Shortcuts, About, Discard, Delete };
// What waits for the "unsaved changes" dialog.
enum class Pending { None, New, Open, Quit };

struct Editor : BitMotor::Game {
    // ---- the song
    Song song;
    std::filesystem::path path; // empty: not saved yet
    History history;
    Song committed;      // the song as the last undo step left it
    bool changed = false; // something changed since `committed`
    bool dirty = false;   // not saved

    // ---- selection
    std::string pattern;    // the pattern being edited
    std::string instrument; // the instrument of the right panel
    int orderCursor = 0;    // the selected entry of Order
    int channel = 0, row = 0;

    // ---- views
    BitMotor::Font inter;
    View view = View::Steps;
    int sideTab = 0; // 0 instrument, 1 mixer
    bool showExplorer = true, showPlaylist = true;
    Fx::Grid stepsGrid, pianoGrid, trackerGrid;
    Vec2 playlistScroll{0, 0}, instrumentScroll{0, 0}, mixerScroll{0, 0};
    float playlistZoom = 4; // pixels per row
    int instrumentListScroll = 0, patternListScroll = 0, fileListScroll = 0;
    // Tracker selection (channel, row) from a to b, and its clipboard: columns of entries.
    Fx::GridPoint selectA, selectB;
    std::vector<std::vector<std::string>> clipboard;
    int shownRow = -1; // the cursor row the tracker last scrolled to
    // Steps: painting with a drag; the right-clicked step.
    bool paintOn = true;
    Fx::GridPoint stepMenu;
    // Piano roll: drawing, moving or resizing a block.
    enum class RollDrag { None, Draw, Move, Resize } rollDrag = RollDrag::None;
    NoteBlock rollBlock, dragOrigin;
    int rollGrab = 0;
    // The note a step writes, per channel.
    std::map<int, std::string> stepNotes;

    // ---- playing
    bool playing = false, songMode = true, record = true;
    BitMotor::SongPosition position;
    std::vector<bool> muted, solo;
    int octave = 4, editStep = 1;
    std::map<int, int> heldKeys; // keyboard key -> PlayNote id
    int testNote = -1;           // the instrument panel's "Try" button

    // ---- dialogs
    Dialog dialog = Dialog::None;
    Pending pending = Pending::None;
    std::filesystem::path folder; // the open/save dialog's folder
    std::filesystem::path folderShown;
    std::string folderField; // the folder as it is being typed
    std::string field;            // its file name, or the rename field
    std::string renaming;         // "instrument:<name>" or "pattern:<name>"
    int fileSelected = -1;
    int exportPasses = 1;
    std::string message; // in the status bar
    double messageTime = -100;
    double autosaveTime = 0;

    // ---- Editor.cpp
    void Start() override;
    void Update() override;
    void Draw() override;
    void Stop() override;
    EditorSettings& Prefs();
    void UseTheme();
    void Say(std::string text);
    void Touch();  // the song changed (it becomes an undo step when the mouse is let go)
    void Commit(); // makes the pending change an undo step now
    void DoUndo();
    void DoRedo();
    void NewFile();
    bool OpenFile(const std::filesystem::path& file);
    bool SaveFile(const std::filesystem::path& file);
    void Ask(Pending what);
    void Finish(Pending what);
    void AddRecent(const std::filesystem::path& file);
    void Play(bool fromCursor = false);
    void StopPlaying();
    void ApplyMutes();
    int PreviewNote(float note); // a PlayNote id, -1 if there is no instrument
    void Keys();
    void TrackerKeys(bool ctrl, bool shift);
    void DrawMenus(Rect area);
    void DrawTransport(Rect area);
    void DrawStatus(Rect area);
    void DrawDialog();
    void DrawFileDialog(bool saving);

    // ---- Explorer.cpp
    void DrawExplorer(Rect area);
    void DrawPlaylist(Rect area);
    void StartRename(std::string what);
    void FinishRename();
    void DeleteInstrument(const std::string& name, bool ask);

    // ---- PatternViews.cpp
    void DrawPattern(Rect area);
    void DrawSteps(Rect area);
    void DrawPianoRoll(Rect area);
    void DrawTracker(Rect area);
    void DrawChannelMenu(Rect area, int track);
    Pattern* Current();
    int ChannelCount();
    std::string StepNote(int track);
    bool Sounding(int& sounding); // the row of the current pattern that plays (false: it is not playing)
    void Copy(bool cut);
    void Paste();
    void TransposeSelection(int semitones);

    // ---- InstrumentViews.cpp
    void DrawSide(Rect area);
    void DrawInstrument(Rect area);
    void DrawMixer(Rect area);
};

// A pattern's colour, from a fixed palette of 12 hues (by its place among the song's patterns).
Color PatternColor(const Song& song, const std::string& name);
// The pixel font (5x7 at 2x) in a colour, for notes and numbers.
BitMotor::TextStyle Mono(Color color, int size = 2);
// Text in the palette's font and colour.
void Write(std::string_view text, Vec2 at, Color color = {}, BitMotor::Align align = BitMotor::Align::Left);
// "C-4", "C#4", "===" (off), "---" (keep): three characters, as trackers show notes.
std::string TrackerNote(std::string_view entry);
// The instruments' names, and its index in them (-1 if missing).
std::vector<std::string> Names(const Song& song, bool patterns);
int IndexOf(const std::vector<std::string>& names, const std::string& name);
// The note of a computer key in the tracker's two piano rows (Z..M, Q..P), from C of `octave`; -1: not a note.
int KeyNote(BitMotor::Key key, int octave);
bool CtrlDown();
bool ShiftDown();

} // namespace BitMusic
