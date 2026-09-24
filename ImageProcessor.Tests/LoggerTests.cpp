/**
 * @file LoggerTests.cpp
 * @brief 로그 파일: 폴더 자동 생성, 레벨·타임스탬프 형식, 열기 실패 보고.
 */

#include "TestFramework.h"

#include "Logger.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

namespace fs = std::filesystem;

fs::path freshTestDirectory(const std::string& name) {
    const fs::path directory = fs::temp_directory_path() / ("ip_test_" + name);
    std::error_code ignored;
    fs::remove_all(directory, ignored);
    return directory;
}

} // anonymous namespace

TEST_CASE(Logger_CreatesMissingDirectoryAndWritesLevels) {
    const fs::path directory = freshTestDirectory("logger_nested");
    const fs::path logPath = directory / "a" / "run.log";
    {
        ip::Logger logger;
        CHECK(logger.openFile(logPath.string()));
        logger.info("hello");
        logger.error("(expected) logger test error line");
    }

    std::ifstream file(logPath);
    std::string first;
    std::string second;
    std::getline(file, first);
    std::getline(file, second);
    CHECK(first.find("[INFO ] hello") != std::string::npos);
    CHECK(second.find("[ERROR] (expected) logger test error line") != std::string::npos);
    CHECK(first.size() > 23 && first[4] == '-' && first[13] == ':');  // "YYYY-MM-DD HH:MM:SS.mmm"

    file.close();
    std::error_code ignored;
    fs::remove_all(directory, ignored);
}

TEST_CASE(Logger_ReportsFailureWhenPathIsDirectory) {
    const fs::path directory = freshTestDirectory("logger_dir");
    fs::create_directories(directory);

    ip::Logger logger;
    CHECK(!logger.openFile(directory.string()));

    std::error_code ignored;
    fs::remove_all(directory, ignored);
}
