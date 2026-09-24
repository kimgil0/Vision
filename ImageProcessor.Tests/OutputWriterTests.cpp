/**
 * @file OutputWriterTests.cpp
 * @brief 원자적 저장: 폴더 자동 생성, 덮어쓰기, 실패 시 기존 파일 보존·임시 파일 미잔존.
 */

#include "TestFramework.h"
#include "TestHelpers.h"

#include "BmpParser.h"
#include "Exceptions.h"
#include "OutputWriter.h"

#include <filesystem>
#include <system_error>

using namespace test;

namespace {

namespace fs = std::filesystem;

/// 테스트 전용 임시 폴더. 생성·소멸 시 통째로 삭제한다 (RAII).
class TempDirectory {
public:
    explicit TempDirectory(const std::string& name)
        : m_path(fs::temp_directory_path() / ("ip_test_" + name)) {
        std::error_code ignored;
        fs::remove_all(m_path, ignored);
    }

    ~TempDirectory() {
        std::error_code ignored;
        fs::remove_all(m_path, ignored);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    const fs::path& path() const noexcept { return m_path; }

private:
    fs::path m_path;
};

bool tempFileExists(const std::string& target) {
    return fs::exists(fs::path(target + ".tmp"));
}

} // anonymous namespace

TEST_CASE(OutputWriter_CreatesMissingDirectories) {
    const TempDirectory directory("writer_nested");
    const std::string target = (directory.path() / "a" / "b" / "out.bmp").string();
    const ip::ImageBuffer image = makeRandom(7, 5, 1);

    ip::saveBmpAtomically(target, image);

    CHECK(sameImage(ip::BmpParser::loadFromFile(target), image));
    CHECK(!tempFileExists(target));
}

TEST_CASE(OutputWriter_ReplacesExistingFile) {
    const TempDirectory directory("writer_replace");
    const std::string target = (directory.path() / "out.bmp").string();
    const ip::ImageBuffer second = makeGray(6, 3, 200);

    ip::saveBmpAtomically(target, makeGray(4, 4, 10));
    ip::saveBmpAtomically(target, second);

    CHECK(sameImage(ip::BmpParser::loadFromFile(target), second));
    CHECK(!tempFileExists(target));
}

TEST_CASE(OutputWriter_FailedWriteKeepsPreviousOutput) {
    // 쓰기 단계에서 실패해도(빈 이미지 → BmpParseError) 이전 결과 파일은 그대로 남아야 한다.
    const TempDirectory directory("writer_keep");
    const std::string target = (directory.path() / "out.bmp").string();
    const ip::ImageBuffer original = makeRandom(5, 5, 3);
    ip::saveBmpAtomically(target, original);

    CHECK_THROWS_AS(ip::saveBmpAtomically(target, ip::ImageBuffer{}), ip::BmpParseError);

    CHECK(sameImage(ip::BmpParser::loadFromFile(target), original));
    CHECK(!tempFileExists(target));
}

TEST_CASE(OutputWriter_RejectsDirectoryAsTarget) {
    const TempDirectory directory("writer_dir_target");
    const fs::path target = directory.path() / "occupied";
    fs::create_directories(target);

    CHECK_THROWS_AS(ip::saveBmpAtomically(target.string(), makeGray(2, 2, 0)), ip::BmpParseError);

    CHECK(fs::is_directory(target));
    CHECK(!tempFileExists(target.string()));
}

TEST_CASE(OutputWriter_RejectsEmptyPath) {
    CHECK_THROWS_AS(ip::saveBmpAtomically("", makeGray(2, 2, 0)), ip::BmpParseError);
}
