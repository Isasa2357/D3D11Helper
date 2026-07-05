#pragma once

#include "D3D11Core/D3D11Core.hpp"
#include "D3D11Framework/D3D11Framework.hpp"

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Test {

struct Failure : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Case {
    const char* suite;
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& Registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* suite, const char* name, void (*fn)()) {
        Registry().push_back({ suite, name, fn });
    }
};

inline void Fail(const char* file, int line, const std::string& message) {
    std::ostringstream oss;
    oss << file << ":" << line << ": " << message;
    throw Failure(oss.str());
}

inline int Run(const std::string& suiteFilter) {
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    for (const Case& c : Registry()) {
        if (!suiteFilter.empty() && suiteFilter != c.suite) continue;

        std::cout << "[ RUN      ] " << c.suite << "." << c.name << "\n";
        try {
            c.fn();
            ++passed;
            std::cout << "[       OK ] " << c.suite << "." << c.name << "\n";
        } catch (const Failure& e) {
            ++failed;
            std::cout << "[     FAIL ] " << c.suite << "." << c.name << "\n"
                      << "    " << e.what() << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "[  EXCEPT  ] " << c.suite << "." << c.name << "\n"
                      << "    " << e.what() << "\n";
        }
    }

    std::cout << "\n==== " << passed << " passed, " << failed
              << " failed, " << skipped << " skipped ====\n";
    return failed == 0 ? 0 : 1;
}

inline std::shared_ptr<D3D11CoreLib::D3D11Core> CreateCore() {
    D3D11CoreLib::D3D11CoreConfig config;
    config.enableDebugLayer = false;
    config.enableInfoQueue = false;
    config.allowWarpAdapter = true;
    return D3D11CoreLib::D3D11Core::CreateShared(config);
}

} // namespace Test

#define TEST(SUITE, NAME) \
    static void Test_##SUITE##_##NAME(); \
    static ::Test::Registrar Registrar_##SUITE##_##NAME(#SUITE, #NAME, &Test_##SUITE##_##NAME); \
    static void Test_##SUITE##_##NAME()

#define CHECK(EXPR) \
    do { if (!(EXPR)) ::Test::Fail(__FILE__, __LINE__, std::string("CHECK failed: ") + #EXPR); } while (0)

#define CHECK_EQ(A, B) \
    do { auto _a = (A); auto _b = (B); if (!(_a == _b)) { \
        std::ostringstream _oss; _oss << "CHECK_EQ failed: " << #A << " == " << #B \
                                      << " (" << _a << " vs " << _b << ")"; \
        ::Test::Fail(__FILE__, __LINE__, _oss.str()); } } while (0)

#define CHECK_NOTHROW(EXPR) \
    do { try { (void)(EXPR); } catch (const std::exception& _e) { \
        ::Test::Fail(__FILE__, __LINE__, std::string("unexpected exception: ") + _e.what()); } } while (0)

#define CHECK_THROWS(EXPR) \
    do { bool _thrown = false; try { (void)(EXPR); } catch (...) { _thrown = true; } \
        if (!_thrown) ::Test::Fail(__FILE__, __LINE__, std::string("expected exception: ") + #EXPR); } while (0)

#define REQUIRE_CORE(NAME) auto NAME = ::Test::CreateCore()
