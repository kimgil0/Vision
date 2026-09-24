/**
 * @file Logger.cpp
 */

#include "Logger.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>

namespace ip {

namespace {

const char* levelTag(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Info:    return "INFO ";
    case LogLevel::Warning: return "WARN ";
    case LogLevel::Error:   return "ERROR";
    }
    return "?????";
}

/// "2026-09-23 14:05:31.042" 형식의 로컬 시각.
std::string currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const long long millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now.time_since_epoch()).count() % 1000;

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &seconds);  // std::localtime 은 정적 버퍼를 공유하므로 스레드 안전하지 않다.
#else
    localtime_r(&seconds, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << millis;
    return oss.str();
}

} // anonymous namespace

bool Logger::openFile(const std::string& path) {
    // 출력 이미지와 같은 규칙: 로그 파일의 폴더가 없으면 만든다. 실패하면 아래 open 이 실패한다.
    const std::filesystem::path directory = std::filesystem::path(path).parent_path();
    if (!directory.empty()) {
        std::error_code ignored;
        std::filesystem::create_directories(directory, ignored);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_file.open(path, std::ios::out | std::ios::app);
    return m_file.is_open();
}

void Logger::write(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostream& console = (level == LogLevel::Info) ? std::cout : std::cerr;
    console << message << '\n';

    if (m_file.is_open()) {
        m_file << currentTimestamp() << " [" << levelTag(level) << "] " << message << std::endl;
    }
}

std::string formatDuration(std::chrono::steady_clock::duration elapsed) {
    const double millis = std::chrono::duration<double, std::milli>(elapsed).count();
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << millis << " ms";
    return oss.str();
}

} // namespace ip
