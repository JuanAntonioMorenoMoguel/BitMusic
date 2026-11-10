#pragma once

// A deliberately tiny test harness: TEST(name) registers a function, CHECK
// records a failure (with file/line) without stopping the test, REQUIRE stops
// it. main() lives in TestMain.cpp and returns non-zero if anything failed,
// which is all CTest needs.

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace test {

struct Case {
    const char* name;
    std::function<void()> body;
};

inline std::vector<Case>& Registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& Failures() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { Registry().push_back({name, std::move(body)}); }
};

struct RequireFailed {};

inline void Fail(const char* file, int line, const std::string& expression) {
    ++Failures();
    std::cerr << "  FAILED " << file << ':' << line << ": " << expression << '\n';
}

} // namespace test

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)
#define TEST(name)                                                                              \
    static void name();                                                                         \
    static test::Registrar TEST_CONCAT(registrar_, name)(#name, name);                          \
    static void name()

#define CHECK(expr)                                           \
    do {                                                      \
        if (!(expr)) test::Fail(__FILE__, __LINE__, #expr);   \
    } while (0)

#define REQUIRE(expr)                                         \
    do {                                                      \
        if (!(expr)) {                                        \
            test::Fail(__FILE__, __LINE__, #expr);            \
            throw test::RequireFailed{};                      \
        }                                                     \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::abs((a) - (b)) <= (eps))
