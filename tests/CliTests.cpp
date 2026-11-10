#include "TestMain.h"

#include <BitMusic/Cli.h>

#include <BitMotor/Core/Files.h>

#include <filesystem>
#include <sstream>
#include <vector>

using namespace BitMusic;

namespace {

struct Result {
    std::optional<int> Code;
    std::string Out, Error;
};

Result Run(std::vector<std::string> args) {
    std::ostringstream out, error;
    Result result;
    result.Code = RunCommand(args, out, error);
    result.Out = out.str();
    result.Error = error.str();
    return result;
}

const std::string Demo = std::string(BM_SOURCE_DIR) + "/assets/examples/demo.bitmusic";

} // namespace

TEST(NoCommandOpensTheEditor) {
    CHECK(!Run({}).Code);
    CHECK(!Run({"--debug"}).Code);
    CHECK(!Run({"song.bitmusic"}).Code);
}

TEST(HelpVersionAndMistakes) {
    const Result help = Run({"help"});
    CHECK(help.Code == 0);
    CHECK(help.Out.find("bitmusic export") != std::string::npos);
    CHECK(Run({"version"}).Out.starts_with("BitMusic "));
    CHECK(Run({"dance"}).Code == 2);
    CHECK(Run({"export", Demo}).Code == 2);
    CHECK(Run({"export", Demo, "--wav", "x.wav", "--passes", "0"}).Code == 2);
    CHECK(Run({"export", "missing.bitmusic", "--wav", "x.wav"}).Code == 1);
}

TEST(InfoDescribesTheSong) {
    const Result info = Run({"info", Demo});
    REQUIRE(info.Code == 0);
    CHECK(info.Out.find("Bpm") != std::string::npos);
    CHECK(info.Out.find("Order:") != std::string::npos);
}

TEST(ExportWritesTheSameWavTwice) {
    const auto folder = std::filesystem::temp_directory_path() / "BitMusicTests";
    const std::string a = (folder / "a.wav").string(), b = (folder / "b.wav").string();
    REQUIRE(Run({"export", Demo, "--wav", a}).Code == 0);
    REQUIRE(Run({"export", Demo, "--wav=" + b}).Code == 0);
    const auto first = BitMotor::Files::ReadBytes(a), second = BitMotor::Files::ReadBytes(b);
    REQUIRE(first && second);
    CHECK(first->size() > 44);
    CHECK(*first == *second);
    std::filesystem::remove_all(folder);
}
