/**
 * @file TestMain.cpp
 * @brief 등록된 모든 테스트를 실행하고, 하나라도 실패하면 종료 코드 1 을 반환한다.
 */

#include "TestFramework.h"

#include <exception>
#include <iostream>

int main() {
    const std::vector<test::TestCase>& tests = test::registry();
    std::size_t failed = 0;

    for (const test::TestCase& testCase : tests) {
        try {
            testCase.body();
            std::cout << "[PASS] " << testCase.name << '\n';
        }
        catch (const std::exception& e) {
            ++failed;
            std::cout << "[FAIL] " << testCase.name << "\n       " << e.what() << '\n';
        }
        catch (...) {
            ++failed;
            std::cout << "[FAIL] " << testCase.name << "\n       unknown exception\n";
        }
    }

    std::cout << '\n' << (tests.size() - failed) << " / " << tests.size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
