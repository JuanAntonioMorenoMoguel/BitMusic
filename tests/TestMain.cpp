#include "TestMain.h"

#include <exception>

int main() {
    int failedCases = 0;
    for (const test::Case& c : test::Registry()) {
        const int before = test::Failures();
        try {
            c.body();
        } catch (const test::RequireFailed&) {
        } catch (const std::exception& e) {
            test::Fail(c.name, 0, std::string("exception: ") + e.what());
        }
        const bool passed = test::Failures() == before;
        if (!passed) ++failedCases;
        std::cout << (passed ? "[ OK ] " : "[FAIL] ") << c.name << '\n';
    }
    std::cout << '\n' << test::Registry().size() - static_cast<size_t>(failedCases) << '/' << test::Registry().size() << " tests passed\n";
    return failedCases == 0 ? 0 : 1;
}
