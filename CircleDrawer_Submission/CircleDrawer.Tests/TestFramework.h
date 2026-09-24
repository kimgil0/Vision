#pragma once

/**
 * @file TestFramework.h
 * @brief 외부 의존성 없는 최소 단위 테스트 프레임워크.
 *
 * 과제 조건(외부 라이브러리 없이 STL 만 사용)을 지키기 위해 GoogleTest 대신 사용한다.
 * TEST_CASE 로 선언한 함수는 정적 초기화 시점에 레지스트리에 등록되고 TestMain 이 순서대로 실행한다.
 */

#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace test {

struct TestCase {
    const char* name;
    void (*body)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;  // 함수 내 정적 변수: 번역 단위 간 초기화 순서 문제 회피
    return tests;
}

struct Registrar {
    Registrar(const char* name, void (*body)()) {
        registry().push_back({ name, body });
    }
};

class AssertionFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

template <typename T>
std::string toPrintable(const T& value) {
    std::ostringstream oss;
    if constexpr (std::is_arithmetic_v<T>) {
        oss << +value;  // uint8_t 가 문자로 출력되지 않도록 정수 승격
    }
    else {
        oss << value;
    }
    return oss.str();
}

inline std::string location(const char* file, int line) {
    return std::string(file) + ":" + std::to_string(line) + ": ";
}

template <typename A, typename B>
void checkEqual(const A& actual, const B& expected,
                const char* actualExpr, const char* expectedExpr, const char* file, int line) {
    if (!(actual == expected)) {
        throw AssertionFailure(location(file, line) + "CHECK_EQ(" + actualExpr + ", " + expectedExpr +
                               ") failed: " + toPrintable(actual) + " != " + toPrintable(expected));
    }
}

} // namespace test

#define TEST_CASE(name)                                                   \
    static void name();                                                   \
    static const test::Registrar name##_registrar(#name, &name);          \
    static void name()

#define CHECK(condition)                                                  \
    do {                                                                  \
        if (!(condition)) {                                               \
            throw test::AssertionFailure(test::location(__FILE__, __LINE__) + \
                                         "CHECK(" #condition ") failed"); \
        }                                                                 \
    } while (false)

#define CHECK_MSG(condition, message)                                     \
    do {                                                                  \
        if (!(condition)) {                                               \
            throw test::AssertionFailure(test::location(__FILE__, __LINE__) + \
                                         "CHECK(" #condition ") failed: " + (message)); \
        }                                                                 \
    } while (false)

#define CHECK_EQ(actual, expected) \
    test::checkEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)

/// expression 이 정확히 ExceptionType 을 던져야 통과. 다른 타입의 예외는 그대로 전파되어 실패로 기록된다.
#define CHECK_THROWS_AS(expression, ExceptionType)                        \
    do {                                                                  \
        bool caught_ = false;                                             \
        try { (void)(expression); }                                       \
        catch (const ExceptionType&) { caught_ = true; }                  \
        if (!caught_) {                                                   \
            throw test::AssertionFailure(test::location(__FILE__, __LINE__) + \
                "CHECK_THROWS_AS(" #expression ", " #ExceptionType ") failed: nothing thrown"); \
        }                                                                 \
    } while (false)

/// CHECK_THROWS_AS + 예외 메시지에 text 가 포함되어야 통과.
#define CHECK_THROWS_CONTAINS(expression, ExceptionType, text)            \
    do {                                                                  \
        std::string message_;                                             \
        bool caught_ = false;                                             \
        try { (void)(expression); }                                       \
        catch (const ExceptionType& e_) { caught_ = true; message_ = e_.what(); } \
        if (!caught_ || message_.find(text) == std::string::npos) {       \
            throw test::AssertionFailure(test::location(__FILE__, __LINE__) + \
                "CHECK_THROWS_CONTAINS(" #expression ", " #ExceptionType ", " #text \
                ") failed: got \"" + message_ + "\"");                    \
        }                                                                 \
    } while (false)
