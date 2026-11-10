// The right panel: the instrument being edited (every field of BitMotor's Instrument) and the mixer (a strip per
// instrument, with the song's master volume).
#include "Editor.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace BitMusic {

using namespace BitMotor;

namespace {

const std::vector<std::string> Waves = {"sine", "square", "saw", "triangle", "noise"};

// The envelope as a line: attack, decay, a held sustain, release.
void DrawEnvelope(Rect area, const Instrument& instrument, Color color) {
    const Fx::Palette& palette = Fx::GetPalette();
    Fx::RoundedRect(area, 3, Fx::Derive(palette.InnerBackground, -0.05f));
    const float hold = 0.3f;
    const float total = std::max(0.05f, instrument.Attack + instrument.Decay + hold + instrument.Release);
    const Rect inside = area.Expand(-6);
    auto at = [&](float seconds, float level) {
        return Vec2{inside.X + inside.Width * seconds / total, inside.Bottom() - inside.Height * level};
    };
    const float sustain = std::clamp(instrument.Sustain, 0.0f, 1.0f);
    const Vec2 points[] = {at(0, 0), at(instrument.Attack, 1), at(instrument.Attack + instrument.Decay, sustain),
                           at(instrument.Attack + instrument.Decay + hold, sustain), at(total, 0)};
    for (int i = 0; i < 4; ++i) UI::Line(points[i], points[i + 1], color, 2);
    for (const Vec2& p : points) UI::Circle(p, 3, Colors::White);
}

// One cycle of the wave (noise: a fixed scribble).
void DrawWave(Rect area, const Instrument& instrument, Color color) {
    const Fx::Palette& palette = Fx::GetPalette();
    Fx::RoundedRect(area, 3, Fx::Derive(palette.InnerBackground, -0.05f));
    const Rect inside = area.Expand(-6);
    Vec2 last{};
    std::uint32_t noise = 12345;
    for (int i = 0; i <= 64; ++i) {
        const float t = float(i) / 64;
        float v = 0;
        if (instrument.Wave == "sine") v = std::sin(t * 6.2831853f);
        else if (instrument.Wave == "square") v = t < instrument.Width ? 1.0f : -1.0f;
        else if (instrument.Wave == "saw") v = 1 - 2 * t;
        else if (instrument.Wave == "triangle") v = t < 0.5f ? 4 * t - 1 : 3 - 4 * t;
        else {
            noise = noise * 1664525u + 1013904223u;
            v = float(noise >> 8) / float(1 << 24) * 2 - 1;
        }
        const Vec2 p{inside.X + inside.Width * t, inside.Center().Y - v * inside.Height * 0.45f};
        if (i > 0) UI::Line(last, p, color, 2);
        last = p;
    }
}

// A level meter, from the bottom.
void Meter(Rect area, float level) {
    UI::Rect(area, Color::Hex(0x1E2022));
    const float h = area.Height * std::clamp(level, 0.0f, 1.0f);
    const Color color = level > 0.9f ? Color::Hex(0xE05050) : level > 0.6f ? Color::Hex(0xE0B040) : Color::Hex(0x5FC55A);
    UI::Rect({area.X + 1, area.Bottom() - h, area.Width - 2, h}, color);
}

} // namespace

void Editor::DrawSide(Rect area) {
    const Rect page = Fx::TabPane(area.Expand(-6), sideTab, {Tr("tab.instrument"), Tr("tab.mixer")});
    if (sideTab == 0) DrawInstrument(page.Expand(-8));
    else DrawMixer(page.Expand(-8));
    // The "Try" button and the piano roll's keys play while the mouse is held.
    if (testNote >= 0 && !Input::MouseDown(MouseButton::Left)) {
        Music::ReleaseNote(testNote);
        testNote = -1;
    }
}

void Editor::DrawInstrument(Rect area) {
    const std::vector<std::string> instruments = Names(song, false);
    int selected = IndexOf(instruments, instrument);
    if (instruments.empty()) {
        Fx::Label({area.X, area.Y, area.Width, 28}, Tr("instrument.none"));
        if (Fx::Button({area.X, area.Y + 34, 160, 28}, Tr("menu.instrument.new"))) {
            instrument = AddInstrument(song, Tr("instrument.default"));
            Touch();
        }
        return;
    }
    if (Fx::ComboBox({area.X, area.Y, area.Width - 96, 28}, selected, instruments) && selected >= 0)
        instrument = instruments[std::size_t(selected)];
    const Rect test{area.Right() - 90, area.Y, 90, 28};
    Fx::Button(test, Tr("instrument.try"));
    if (UI::Hovered(test) && Input::MousePressed(MouseButton::Left)) {
        if (testNote >= 0) Music::ReleaseNote(testNote);
        testNote = Music::PlayNote(song.Instruments[instrument], float(12 * (octave + 1)));
    }
    UI::Tooltip(Tr("instrument.try.tip"));

    Instrument& in = song.Instruments[instrument];
    const float rowHeight = 30, content = 15 * (rowHeight + 6) + 2 * (86 + 8);
    const Rect scrollArea{area.X, area.Y + 36, area.Width, area.Height - 36};
    const Vec2 at = Fx::BeginScrollArea(scrollArea, {0, content}, instrumentScroll);
    const float width = Fx::ScrollView().Width - 4, labelWidth = std::min(110.0f, width * 0.4f);
    float y = at.Y;
    const Color accent = Fx::GetPalette().Accent;
    auto row = [&](std::string_view label, std::string value = {}) {
        Fx::Label({at.X, y, labelWidth, rowHeight}, label);
        const float valueWidth = value.empty() ? 0.0f : 52.0f;
        if (!value.empty()) Fx::Label({at.X + width - valueWidth, y, valueWidth, rowHeight}, value, Align::Right);
        const Rect r{at.X + labelWidth, y, width - labelWidth - valueWidth - (value.empty() ? 0 : 6), rowHeight};
        y += rowHeight + 6;
        return r;
    };
    auto seconds = [](float s) { return s < 1 ? std::format("{:.0f} ms", s * 1000) : std::format("{:.2f} s", s); };

    int wave = std::max(0, IndexOf(Waves, in.Wave));
    std::vector<std::string> waveNames;
    for (const std::string& w : Waves) waveNames.push_back(Tr("wave." + w));
    if (Fx::ComboBox(row(Tr("instrument.wave")), wave, waveNames)) {
        in.Wave = Waves[std::size_t(wave)];
        Touch();
    }
    DrawWave({at.X, y, width, 86}, in, accent);
    y += 94;
    if (Fx::Slider(row(Tr("instrument.volume"), std::format("{:.2f}", in.Volume)), in.Volume, 0, 1, 0.01f)) Touch();
    if (Fx::Slider(row(Tr("instrument.width"), std::format("{:.3f}", in.Width)), in.Width, 0.05f, 0.95f, 0.005f, in.Wave == "square"))
        Touch();
    DrawEnvelope({at.X, y, width, 86}, in, accent);
    y += 94;
    if (Fx::Slider(row(Tr("instrument.attack"), seconds(in.Attack)), in.Attack, 0, 2, 0.005f)) Touch();
    if (Fx::Slider(row(Tr("instrument.decay"), seconds(in.Decay)), in.Decay, 0, 2, 0.005f)) Touch();
    if (Fx::Slider(row(Tr("instrument.sustain"), std::format("{:.2f}", in.Sustain)), in.Sustain, 0, 1, 0.01f)) Touch();
    if (Fx::Slider(row(Tr("instrument.release"), seconds(in.Release)), in.Release, 0, 3, 0.005f)) Touch();
    if (Fx::Slider(row(Tr("instrument.lowpass"), in.LowPass >= 1 ? Tr("instrument.off") : std::format("{:.2f}", in.LowPass)),
                   in.LowPass, 0.01f, 1, 0.01f))
        Touch();
    UI::Tooltip(Tr("instrument.lowpass.tip"));
    if (Fx::Spinner(row(Tr("instrument.crush")), in.Crush, 0, 64)) Touch();
    UI::Tooltip(Tr("instrument.crush.tip"));
    if (Fx::Spinner(row(Tr("instrument.downsample")), in.Downsample, 1, 64)) Touch();
    UI::Tooltip(Tr("instrument.downsample.tip"));
    const Rect panRow = row(Tr("instrument.pan"), std::format("{:+.2f}", in.Pan));
    if (Fx::Knob({panRow.X, panRow.Y, panRow.Height, panRow.Height}, in.Pan, -1.0f, 1.0f, 0.02f, 0.0f)) Touch();
    if (Fx::Spinner(row(Tr("instrument.sweep")), in.Sweep, -48, 48, 1, 0)) Touch();
    UI::Tooltip(Tr("instrument.sweep.tip"));
    if (Fx::Slider(row(Tr("instrument.sweep.time"), seconds(in.SweepTime)), in.SweepTime, 0.005f, 1, 0.005f)) Touch();
    if (Fx::Spinner(row(Tr("instrument.transpose")), in.Transpose, -48, 48, 1, 0)) Touch();
    Fx::EndScrollArea();
}

void Editor::DrawMixer(Rect area) {
    const Fx::Palette& palette = Fx::GetPalette();
    const std::vector<std::string> instruments = Names(song, false);
    const float strip = 96, gap = 8;
    const Vec2 at = Fx::BeginScrollArea(area, {float(instruments.size() + 1) * (strip + gap), 380}, mixerScroll);

    // Which channels play which instrument now (the pattern that sounds, or the one being edited).
    std::string sounding = pattern;
    if (playing && songMode && position.Order >= 0 && position.Order < int(song.Order.size())) sounding = song.Order[std::size_t(position.Order)];
    const auto found = song.Patterns.find(sounding);
    auto level = [&](const std::string& name) {
        float most = 0;
        if (!playing || found == song.Patterns.end()) return most;
        for (std::size_t c = 0; c < found->second.Tracks.size() && c < 64; ++c)
            if (name.empty() || found->second.Tracks[c].Instrument == name) most = std::max(most, Music::Level(int(c)));
        return most;
    };

    auto drawStrip = [&](float x, const std::string& title, bool master) {
        const Rect box{x, at.Y, strip, 370};
        Fx::RoundedRect(box, 4, Fx::Derive(palette.Background, master ? -0.2f : -0.1f));
        Fx::Label({box.X + 4, box.Y + 4, box.Width - 8, 22}, title, Align::Center);
        Meter({box.X + 8, box.Y + 32, 10, 120}, level(master ? "" : title));
        const float kx = box.X + 30, k = 52;
        if (master) {
            Fx::Label({kx, box.Y + 30, k + 10, 18}, Tr("mixer.volume"), Align::Center);
            if (Fx::Knob({kx + 4, box.Y + 50, k, k}, song.Volume, 0, 1, 0.01f)) Touch();
            Fx::Label({kx, box.Y + 104, k + 10, 18}, std::format("{:.2f}", song.Volume), Align::Center);
            return;
        }
        Instrument& in = song.Instruments[title];
        if (UI::Clicked({box.X, box.Y, box.Width, 26})) {
            instrument = title;
            sideTab = 0;
        }
        Fx::Label({kx, box.Y + 30, k + 10, 18}, Tr("mixer.volume"), Align::Center);
        if (Fx::Knob({kx + 4, box.Y + 50, k, k}, in.Volume, 0, 1, 0.01f)) Touch();
        Fx::Label({kx, box.Y + 104, k + 10, 18}, std::format("{:.2f}", in.Volume), Align::Center);
        Fx::Label({box.X + 4, box.Y + 132, box.Width - 8, 18}, Tr("instrument.pan"), Align::Center);
        if (Fx::Knob({box.Center().X - 20, box.Y + 152, 40, 40}, in.Pan, -1.0f, 1.0f, 0.02f, 0.0f)) Touch();
        Fx::Label({box.X + 4, box.Y + 198, box.Width - 8, 18}, Tr("mixer.lowpass"), Align::Center);
        if (Fx::Knob({box.Center().X - 20, box.Y + 218, 40, 40}, in.LowPass, 0.01f, 1, 0.01f)) Touch();
        Fx::Label({box.X + 4, box.Y + 264, box.Width - 8, 18}, Tr("mixer.crush"), Align::Center);
        if (Fx::Spinner({box.X + 8, box.Y + 284, box.Width - 16, 26}, in.Crush, 0, 64)) Touch();
        Fx::Label({box.X + 4, box.Y + 314, box.Width - 8, 18}, Tr("mixer.downsample"), Align::Center);
        if (Fx::Spinner({box.X + 8, box.Y + 334, box.Width - 16, 26}, in.Downsample, 1, 64)) Touch();
        if (title == instrument) Fx::RoundedOutline(box, 4, palette.Accent, 1);
    };
    drawStrip(at.X, Tr("mixer.master"), true);
    for (std::size_t i = 0; i < instruments.size(); ++i) drawStrip(at.X + float(i + 1) * (strip + gap), instruments[i], false);
    Fx::EndScrollArea();
}

} // namespace BitMusic
