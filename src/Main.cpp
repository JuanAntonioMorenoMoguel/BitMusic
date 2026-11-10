// BitMusic: the editor, or a command for scripts and builds (bitmusic help).
#include "AppInfo.h"
#include "editor/Editor.h"

#include <BitMusic/Cli.h>

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (const auto code = BitMusic::RunCommand(args, std::cout, std::cerr)) return *code;
    return BitMotor::Run<BitMusic::Editor>({.Title = BM_APP_NAME,
                                            .Organization = "Nyiengi",
                                            .Version = BM_APP_VERSION,
                                            .WindowWidth = 1600,
                                            .WindowHeight = 900,
                                            .TickRate = 0});
}
