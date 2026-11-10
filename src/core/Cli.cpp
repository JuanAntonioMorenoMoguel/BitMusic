#include "BitMusic/Cli.h"

#include "AppInfo.h"
#include "BitMusic/Edit.h"
#include "BitMusic/Wav.h"

#include <BitMotor/Audio/Music.h>

#include <charconv>
#include <cmath>
#include <filesystem>
#include <format>
#include <string_view>

namespace BitMusic {

namespace {

constexpr std::string_view Usage = R"(BitMusic )" BM_APP_VERSION R"( - music for games, for BitMotor's synthesizer.

Usage:
  bitmusic                                 open the editor
  bitmusic <song.bitmusic>                 open the editor with a song
  bitmusic export <song> --wav <out.wav> [--passes N]
                                           render the song to a WAV file (48000 Hz, 16-bit, stereo);
                                           N passes: Order, then N-1 times more from Loop (default 1)
  bitmusic info <song>                     tempo, length, instruments, patterns and order
  bitmusic help                            this text
  bitmusic version                         the version
)";

// The value after `name` ("--wav out.wav" or "--wav=out.wav"), if it is there.
std::optional<std::string> Option(std::span<const std::string> args, std::string_view name) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == name && i + 1 < args.size()) return args[i + 1];
        if (args[i].starts_with(name) && args[i].size() > name.size() && args[i][name.size()] == '=')
            return args[i].substr(name.size() + 1);
    }
    return std::nullopt;
}

std::optional<BitMotor::Song> LoadSong(const std::string& path, std::ostream& error) {
    auto song = BitMotor::Song::Load(std::filesystem::absolute(path));
    if (!song) error << "bitmusic: cannot read the song " << path << '\n';
    return song;
}

int Export(std::span<const std::string> args, std::ostream& out, std::ostream& error) {
    if (args.size() < 2 || args[1].starts_with("--")) {
        error << "bitmusic export: which song? (bitmusic export song.bitmusic --wav out.wav)\n";
        return 2;
    }
    const auto wav = Option(args, "--wav");
    if (!wav || wav->empty()) {
        error << "bitmusic export: --wav <out.wav> is needed\n";
        return 2;
    }
    int passes = 1;
    if (const auto text = Option(args, "--passes")) {
        const auto [end, failed] = std::from_chars(text->data(), text->data() + text->size(), passes);
        if (failed != std::errc() || end != text->data() + text->size() || passes < 1) {
            error << "bitmusic export: --passes needs a whole number from 1\n";
            return 2;
        }
    }
    const auto song = LoadSong(args[1], error);
    if (!song) return 1;
    if (!ExportWav(*song, *wav, passes)) {
        error << "bitmusic export: cannot write " << *wav << '\n';
        return 1;
    }
    out << "bitmusic: " << *wav << " written\n";
    return 0;
}

int Info(std::span<const std::string> args, std::ostream& out, std::ostream& error) {
    if (args.size() < 2) {
        error << "bitmusic info: which song?\n";
        return 2;
    }
    const auto song = LoadSong(args[1], error);
    if (!song) return 1;
    const double seconds = song->Duration();
    out << std::format("{}\n  Bpm {}, Rows {} per beat, Beats {} per bar, Volume {}\n", args[1], song->Bpm, song->Rows,
                       song->Beats, song->Volume);
    out << std::format("  Length {}:{:02} ({:.2f} s), Loop {}\n", int(seconds) / 60, int(seconds) % 60, seconds,
                       song->Loop);
    out << std::format("  Channels {}\n  Instruments {}:", Channels(*song), song->Instruments.size());
    for (const auto& [name, instrument] : song->Instruments) out << ' ' << name << " (" << instrument.Wave << ')';
    out << std::format("\n  Patterns {}:", song->Patterns.size());
    for (const auto& [name, pattern] : song->Patterns) out << ' ' << name << " (" << pattern.Length << ')';
    out << "\n  Order:";
    for (std::size_t i = 0; i < song->Order.size(); ++i)
        out << (int(i) == song->Loop ? " |" : " ") << song->Order[i];
    out << '\n';
    return 0;
}

} // namespace

std::optional<int> RunCommand(std::span<const std::string> args, std::ostream& out, std::ostream& error) {
    if (args.empty() || args[0].starts_with("--")) return std::nullopt;
    const std::string& command = args[0];
    if (command == "help" || command == "-h") {
        out << Usage;
        return 0;
    }
    if (command == "version") {
        out << "BitMusic " << BM_APP_VERSION << '\n';
        return 0;
    }
    if (command == "export") return Export(args, out, error);
    if (command == "info") return Info(args, out, error);
    if (command.ends_with(".bitmusic")) return std::nullopt; // the editor opens it
    error << "bitmusic: unknown command '" << command << "'\n\n" << Usage;
    return 2;
}

} // namespace BitMusic
