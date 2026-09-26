#include "test_harness.h"

int main() {
    int passed_cases = 0;
    for (auto& c : ::test::Registry()) {
        int before = ::test::g_failures;
        try {
            c.fn();
        } catch (...) {
            ::test::g_failures++;
        }
        if (::test::g_failures == before) {
            passed_cases++;
            std::printf("PASS %s\n", c.name);
        } else {
            std::printf("FAIL %s\n", c.name);
        }
    }
    std::printf("%d test(s), %d assertion failure(s)\n",
                static_cast<int>(::test::Registry().size()), ::test::g_failures);
    return ::test::g_failures == 0 ? 0 : 1;
}
