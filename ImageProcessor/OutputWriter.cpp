/**
 * @file OutputWriter.cpp
 */

#include "OutputWriter.h"
#include "BmpParser.h"
#include "Exceptions.h"

#include <filesystem>
#include <system_error>
#include <utility>

namespace ip {

namespace {

namespace fs = std::filesystem;

/// 소멸될 때 임시 파일을 지운다. 교체에 성공하면 release() 로 해제한다 (RAII).
class TempFileGuard {
public:
    explicit TempFileGuard(fs::path path) : m_path(std::move(path)) {}

    ~TempFileGuard() {
        if (!m_path.empty()) {
            std::error_code ignored;
            fs::remove(m_path, ignored);
        }
    }

    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;

    void release() noexcept { m_path.clear(); }

private:
    fs::path m_path;
};

} // anonymous namespace

void saveBmpAtomically(const std::string& path, const ImageBuffer& image) {
    if (path.empty()) {
        throw BmpParseError("Output path is empty");
    }

    const fs::path target(path);
    std::error_code error;

    // ── 1. 출력 폴더 준비 ───────────────────────────────
    const fs::path directory = target.parent_path();
    if (!directory.empty()) {
        fs::create_directories(directory, error);
        if (error) {
            throw BmpParseError("Cannot create output directory: " + directory.string() +
                                " (" + error.message() + ")");
        }
    }
    if (fs::is_directory(target, error)) {
        throw BmpParseError("Output path is a directory: " + path);
    }

    // ── 2. 임시 파일에 쓰기 ─────────────────────────────
    // 같은 폴더에 두어야 rename 이 같은 볼륨 안의 교체가 된다.
    fs::path temporary = target;
    temporary += ".tmp";
    TempFileGuard guard(temporary);
    BmpParser::saveToFile(temporary.string(), image);  // 실패 시 BmpParseError, guard 가 정리

    // ── 3. 대상 파일 교체 ───────────────────────────────
    fs::rename(temporary, target, error);
    if (error) {
        throw BmpParseError("Cannot replace output file: " + path + " (" + error.message() + ")");
    }
    guard.release();
}

} // namespace ip
