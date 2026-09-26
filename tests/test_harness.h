#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& Registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        Registry().push_back({name, std::move(fn)});
    }
};

inline int g_failures = 0;

}  // namespace test

#define TEST(name)                                                             \
    static void test_fn_##name();                                              \
    static ::test::Registrar test_reg_##name(#name, test_fn_##name);           \
    static void test_fn_##name()

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (!(expr)) {                                                         \
            ::test::g_failures++;                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);        \
        }                                                                      \
    } while (0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ(actual, expected)                                            \
    do {                                                                       \
        auto a_value = (actual);                                               \
        auto e_value = (expected);                                             \
        if (!(a_value == e_value)) {                                           \
            ::test::g_failures++;                                              \
            std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #actual, \
                        #expected);                                            \
        }                                                                      \
    } while (0)
