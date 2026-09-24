#pragma once

/**
 * @file Logger.h
 * @brief 콘솔 + (선택) 파일 로거. 여러 스레드에서 동시에 호출해도 안전하다.
 */

#include <chrono>
#include <fstream>
#include <mutex>
#include <string>

namespace ip {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

class Logger {
public:
    Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 로그 파일을 append 모드로 연다 (폴더가 없으면 생성). 이후 모든 로그가 파일에도 기록된다.
     * @return 파일을 열지 못하면 false.
     */
    [[nodiscard]] bool openFile(const std::string& path);

    /**
     * @brief 메시지를 기록한다.
     *  - 콘솔: 메시지만 출력 (Info → stdout, Warning/Error → stderr).
     *  - 파일: "타임스탬프 [레벨] 메시지" 형식, 매 줄 flush (비정상 종료 시에도 보존).
     */
    void write(LogLevel level, const std::string& message);

    void info(const std::string& message)    { write(LogLevel::Info, message); }
    void warning(const std::string& message) { write(LogLevel::Warning, message); }
    void error(const std::string& message)   { write(LogLevel::Error, message); }

private:
    std::mutex    m_mutex;
    std::ofstream m_file;
};

/// 로그 출력용 경과 시간 포맷 (예: "12.34 ms").
std::string formatDuration(std::chrono::steady_clock::duration elapsed);

} // namespace ip
