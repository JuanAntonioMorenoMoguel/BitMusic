#pragma once
#include <optional>
#include <ostream>
#include <span>
#include <string>

// The command line: the same program is the editor (no command) and a tool for builds and scripts.
//     bitmusic                                     the editor
//     bitmusic song.bitmusic                       the editor, with that song open
//     bitmusic export song.bitmusic --wav out.wav [--passes 2]
//     bitmusic info song.bitmusic
//     bitmusic help | version
// Options of BitMotor's for the editor (--debug, --frames N, --screenshot f.png...) go to the editor.
namespace BitMusic {

// Runs the command in `args` (without the executable) and returns the exit code: 0 done, 1 it failed, 2 the
// command line is wrong. nullopt: there is no command, so the editor opens.
std::optional<int> RunCommand(std::span<const std::string> args, std::ostream& out, std::ostream& error);

} // namespace BitMusic
