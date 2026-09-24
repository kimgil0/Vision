#pragma once

/**
 * @file CommandLineParser.h
 * @brief 커맨드라인 인자 파싱.
 *
 * 기본 옵션(--input, --output, --filter)에 더해
 * 고급 옵션(--pipeline, --threshold, --threads, --log, --list-filters)을 파싱한다.
 */

#include <string>

namespace ip {

/// 파싱 결과를 담는 단순 구조체.
struct ProgramOptions {
    std::string inputPath;           ///< --input    / -i
    std::string outputPath;          ///< --output   / -o
    std::string filterName;          ///< --filter   / -f   단일 필터 스펙 (예: "threshold:128")
    std::string pipelineSpec;        ///< --pipeline / -p   쉼표로 구분된 필터 체인
    std::string thresholdSpec;       ///< --threshold       필터 적용 후 이진화 단계 추가 ("128" 또는 "otsu")
    std::string logPath;             ///< --log      / -l   로그 파일 경로 (선택)
    unsigned    threadCount = 0;     ///< --threads  / -t   0 = 자동 (하드웨어 스레드 수)
    bool        showHelp    = false; ///< --help     / -h
    bool        listFilters = false; ///< --list-filters
};

class CommandLineParser {
public:
    CommandLineParser() = delete;  // 인스턴스화 금지 (정적 메서드만 제공)

    /**
     * @brief argv 를 파싱하여 ProgramOptions 를 반환한다.
     *
     * --help / --list-filters 는 정보 출력 모드이므로 필수 인자 검사를 생략한다.
     * (프로세스 종료 여부는 호출자가 결정한다 — 파서는 std::exit 를 호출하지 않는다.)
     *
     * @throws ArgumentError 필수 인자 누락, 알 수 없는 옵션, 중복 옵션, 형식 오류 등.
     */
    static ProgramOptions parse(int argc, char* argv[]);

    /// 사용법을 표준 출력에 출력한다.
    static void printUsage(const std::string& exeName);
};

} // namespace ip
