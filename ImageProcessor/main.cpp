/**
 * @file main.cpp
 * @brief ImageProcessor 진입점.
 *
 * 처리 순서와 실패 시 종료 코드:
 *   1. CLI 파싱                (ArgumentError → 4)
 *   2. 필터 구성 (fail-fast)   (FilterError   → 3)  잘못된 스펙이면 이미지 I/O 전에 실패
 *   3. BMP 로드                (BmpParseError → 2)
 *   4. 필터 적용               (FilterError   → 3)
 *   5. BMP 저장                (BmpParseError → 2)
 * 저장은 모든 처리가 성공한 뒤 한 번만, 임시 파일을 거쳐 원자적으로 수행한다 (OutputWriter.h).
 * 따라서 어느 단계에서 실패해도 불완전한 출력 파일이 생기거나 기존 파일이 손상되지 않는다.
 *
 * 탐색기에서 더블클릭하면(인자 없음) 샘플 이미지 데모를 실행한다 — DemoMode.h 참고.
 */

#include "BmpParser.h"
#include "CommandLineParser.h"
#include "DemoMode.h"
#include "Exceptions.h"
#include "FilterFactory.h"
#include "FilterPipeline.h"
#include "ImageBuffer.h"
#include "Logger.h"
#include "OutputWriter.h"
#include "Parallel.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <new>
#include <string>

namespace {

/// 프로세스 종료 코드. 스크립트/CI 가 실패 원인을 구분할 수 있도록 오류 도메인별로 분리한다.
enum class ExitCode : int {
    Success         = 0,
    Unexpected      = 1,
    BmpFailure      = 2,
    FilterFailure   = 3,
    ArgumentFailure = 4,
};

constexpr int toInt(ExitCode code) noexcept {
    return static_cast<int>(code);
}

/**
 * @brief 옵션으로부터 실행할 필터 체인을 만든다. 단일 필터도 1단계 파이프라인으로 통일한다.
 *
 *   --filter blur                    → [blur]
 *   --pipeline "grayscale, blur"     → [grayscale, blur]
 *   --filter blur --threshold 128    → [blur, threshold(128)]   (과제 README 예시 형식)
 */
std::unique_ptr<ip::FilterPipeline> buildFilter(const ip::ProgramOptions& options) {
    std::unique_ptr<ip::FilterPipeline> pipeline;
    if (!options.pipelineSpec.empty()) {
        pipeline = ip::FilterFactory::createPipeline(options.pipelineSpec);
    }
    else {
        pipeline = std::make_unique<ip::FilterPipeline>();
        pipeline->add(ip::FilterFactory::create(options.filterName));
    }

    if (!options.thresholdSpec.empty()) {
        pipeline->add(ip::FilterFactory::create("threshold:" + options.thresholdSpec));
    }
    return pipeline;
}

void run(const ip::ProgramOptions& options, ip::Logger& logger) {
    // ── 1. 필터 구성: 스펙 오류는 이미지 로딩(I/O) 전에 즉시 보고한다 ──
    const std::unique_ptr<ip::FilterPipeline> filter = buildFilter(options);

    ip::FilterContext context;
    context.threadCount = ip::resolveThreadCount(options.threadCount);
    context.logger      = &logger;

    // ── 2. BMP 로드 ─────────────────────────────────────
    ip::ImageBuffer image = ip::BmpParser::loadFromFile(options.inputPath);
    logger.info("Loaded:  " + options.inputPath + " (" + std::to_string(image.width()) +
                " x " + std::to_string(image.height()) + ")");
    logger.info("Filter:  " + filter->describe());
    logger.info("Threads: " + std::to_string(context.threadCount));

    // ── 3. 필터 적용 ────────────────────────────────────
    const auto start = std::chrono::steady_clock::now();
    filter->apply(image, context);
    logger.info("Applied: " + ip::formatDuration(std::chrono::steady_clock::now() - start));

    // ── 4. BMP 저장 (출력 폴더 자동 생성, 임시 파일 → 교체) ──
    ip::saveBmpAtomically(options.outputPath, image);
    logger.info("Saved:   " + options.outputPath);
}

/// 명령줄 한 번 실행. 예외를 종료 코드로 변환한다. (데모 모드도 각 시나리오마다 이 함수를 호출)
int runCli(int argc, char* argv[]) {
    const std::string exeName = (argc > 0 && argv[0] != nullptr) ? argv[0] : "ImageProcessor";
    ip::Logger logger;  // --log 이 지정되기 전까지는 콘솔에만 출력한다.

    try {
        const ip::ProgramOptions options = ip::CommandLineParser::parse(argc, argv);

        if (options.showHelp) {
            ip::CommandLineParser::printUsage(exeName);
            return toInt(ExitCode::Success);
        }
        if (options.listFilters) {
            ip::FilterFactory::printCatalog(std::cout);
            return toInt(ExitCode::Success);
        }
        if (!options.logPath.empty() && !logger.openFile(options.logPath)) {
            throw ip::ArgumentError("--log: cannot open file for writing: " + options.logPath);
        }

        run(options, logger);
        return toInt(ExitCode::Success);
    }
    catch (const ip::ArgumentError& e) {
        logger.error(e.what());
        std::cerr << '\n';
        ip::CommandLineParser::printUsage(exeName);
        return toInt(ExitCode::ArgumentFailure);
    }
    catch (const ip::BmpParseError& e) {
        logger.error(e.what());
        return toInt(ExitCode::BmpFailure);
    }
    catch (const ip::FilterError& e) {
        logger.error(e.what());
        return toInt(ExitCode::FilterFailure);
    }
    catch (const std::bad_alloc&) {
        logger.error("Out of memory");
        return toInt(ExitCode::Unexpected);
    }
    catch (const std::exception& e) {
        logger.error(std::string("Unexpected error: ") + e.what());
        return toInt(ExitCode::Unexpected);
    }
    catch (...) {
        logger.error("Unexpected non-standard exception");
        return toInt(ExitCode::Unexpected);
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // 더블클릭(인자 없음 + 전용 콘솔 창)만 데모로 처리한다.
    // 명령 프롬프트에서 인자 없이 실행하면 기존대로 사용법 안내(종료 코드 4) — 스크립트 동작 불변.
    if (argc == 1 && ip::demo::isLaunchedByDoubleClick()) {
        return ip::demo::run(&runCli, true);
    }
    if (argc == 2 && std::string(argv[1]) == "--demo") {
        return ip::demo::run(&runCli, false);
    }
    return runCli(argc, argv);
}
