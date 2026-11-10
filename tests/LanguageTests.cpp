#include "TestMain.h"

#include <fstream>
#include <set>
#include <string>

namespace {

// The keys of a language file ("menu.file"...), read line by line: the files keep one key per line, sections
// one level deep.
std::set<std::string> Keys(const std::string& code) {
    std::ifstream lines(std::string(BM_SOURCE_DIR) + "/lang/" + code + ".json");
    std::set<std::string> keys;
    std::string line, section;
    while (std::getline(lines, line)) {
        const std::size_t open = line.find('"');
        if (open == std::string::npos) continue;
        const std::size_t close = line.find('"', open + 1);
        const std::string key = line.substr(open + 1, close - open - 1);
        if (line.find('{', close) != std::string::npos) section = key;
        else keys.insert(open <= 2 ? key : section + "." + key);
    }
    return keys;
}

} // namespace

TEST(SpanishAndEnglishHaveTheSameTexts) {
    const std::set<std::string> es = Keys("es"), en = Keys("en");
    REQUIRE(es.size() > 100);
    CHECK(es == en);
    for (const std::string& key : en)
        if (!es.contains(key)) test::Fail(__FILE__, __LINE__, "missing in es: " + key);
    for (const std::string& key : es)
        if (!en.contains(key)) test::Fail(__FILE__, __LINE__, "missing in en: " + key);
}
