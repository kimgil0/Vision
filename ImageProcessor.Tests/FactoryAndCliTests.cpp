/**
 * @file FactoryAndCliTests.cpp
 * @brief 필터 스펙 파싱, 파이프라인 구성, CLI 인자 검증.
 */

#include "TestFramework.h"
#include "TestHelpers.h"

#include "Exceptions.h"
#include "FilterFactory.h"
#include "GaussianBlurFilter.h"
#include "GrayscaleFilter.h"
#include "SharpenFilter.h"

#include <string>

using namespace test;

namespace {

/// 정확히 FilterError 를 던지는지 확인한다 (std::invalid_argument 등 다른 타입이면 false).
bool throwsFilterError(const std::string& spec, bool asPipeline) {
    try {
        if (asPipeline) {
            (void)ip::FilterFactory::createPipeline(spec);
        }
        else {
            (void)ip::FilterFactory::create(spec);
        }
    }
    catch (const ip::FilterError&) {
        return true;
    }
    catch (...) {
        return false;
    }
    return false;
}

} // anonymous namespace

// ── FilterFactory ───────────────────────────────────────

TEST_CASE(Factory_ParsesSpecsCaseInsensitivelyAndTrimsSpaces) {
    CHECK_EQ(ip::FilterFactory::create("  BLUR : 2.5 ")->describe(), std::string("blur(sigma=2.5)"));
    CHECK_EQ(ip::FilterFactory::create("threshold")->describe(), std::string("threshold(otsu)"));
    CHECK_EQ(ip::FilterFactory::create("Threshold:OTSU")->describe(), std::string("threshold(otsu)"));
    CHECK_EQ(ip::FilterFactory::create("threshold:0")->describe(), std::string("threshold(level=0)"));
    CHECK_EQ(ip::FilterFactory::create("sharpen")->describe(), std::string("sharpen(amount=1)"));
}

TEST_CASE(Factory_RejectsMalformedSpecsWithFilterError) {
    // 모두 "Unexpected error"(종료 코드 1)가 아니라 FilterError(종료 코드 3)로 보고되어야 한다.
    for (const char* spec : { "", "   ", ":5", "unknown", "threshold:", "threshold:abc",
                              "threshold:128abc", "threshold:1e2", "threshold:256",
                              "threshold:99999999999", "grayscale:1", "sobel:3",
                              "blur:0", "blur:-1", "blur:abc", "blur:nan", "sharpen:0" }) {
        CHECK_MSG(throwsFilterError(spec, false), std::string("spec=\"") + spec + "\"");
    }
}

TEST_CASE(Factory_SingleFilterWithCommaSuggestsPipeline) {
    CHECK_THROWS_CONTAINS(ip::FilterFactory::create("grayscale,blur"), ip::FilterError, "--pipeline");
}

// ── FilterPipeline ──────────────────────────────────────

TEST_CASE(Pipeline_BuildsStagesInOrder) {
    const auto pipeline = ip::FilterFactory::createPipeline("grayscale, blur:1.5 ,threshold:otsu");
    CHECK_EQ(pipeline->size(), std::size_t{ 3 });
    CHECK_EQ(pipeline->describe(), std::string("grayscale -> blur(sigma=1.5) -> threshold(otsu)"));
}

TEST_CASE(Pipeline_RejectsEmptyStages) {
    for (const char* spec : { "", ",", "grayscale,", ",grayscale", "grayscale,,blur", "grayscale, ,blur" }) {
        CHECK_MSG(throwsFilterError(spec, true), std::string("spec=\"") + spec + "\"");
    }
}

TEST_CASE(Pipeline_EqualsSequentialApplication) {
    const ip::ImageBuffer source = makeRandom(64, 48, 99);

    ip::ImageBuffer viaPipeline = source;
    ip::FilterFactory::createPipeline("grayscale, blur:1.2, sharpen:0.8")
        ->apply(viaPipeline, ip::FilterContext{ 4, nullptr });

    ip::ImageBuffer manual = source;
    ip::GrayscaleFilter().apply(manual);
    ip::GaussianBlurFilter(1.2).apply(manual);
    ip::SharpenFilter(0.8).apply(manual);

    CHECK(sameImage(viaPipeline, manual));
}

TEST_CASE(Pipeline_RejectsNullStageAndEmptyApply) {
    ip::FilterPipeline pipeline;
    CHECK_THROWS_AS(pipeline.add(nullptr), ip::FilterError);

    ip::ImageBuffer image = makeGray(2, 2, 0);
    CHECK_THROWS_AS(pipeline.apply(image), ip::FilterError);
}

// ── CommandLineParser ───────────────────────────────────

TEST_CASE(Cli_ParsesAllOptions) {
    const ip::ProgramOptions options = parseArgs(
        { "-i", "in.bmp", "-o", "out.bmp", "-p", "grayscale, blur", "-t", "4", "-l", "run.log" });
    CHECK_EQ(options.inputPath, std::string("in.bmp"));
    CHECK_EQ(options.outputPath, std::string("out.bmp"));
    CHECK_EQ(options.pipelineSpec, std::string("grayscale, blur"));
    CHECK_EQ(options.logPath, std::string("run.log"));
    CHECK_EQ(options.threadCount, 4u);
    CHECK(options.filterName.empty());
}

TEST_CASE(Cli_HelpAndListFiltersSkipRequiredArguments) {
    CHECK(parseArgs({ "--help" }).showHelp);
    CHECK(parseArgs({ "--list-filters" }).listFilters);
}

TEST_CASE(Cli_RequiresExactlyOneOfFilterOrPipeline) {
    CHECK_THROWS_AS(parseArgs({ "-i", "a", "-o", "b" }), ip::ArgumentError);
    CHECK_THROWS_AS(parseArgs({ "-i", "a", "-o", "b", "-f", "grayscale", "-p", "invert" }), ip::ArgumentError);
}

TEST_CASE(Cli_RejectsMissingValuesDuplicatesAndUnknownOptions) {
    CHECK_THROWS_AS(parseArgs({ "-i" }), ip::ArgumentError);
    CHECK_THROWS_AS(parseArgs({ "-i", "a", "-i", "b", "-o", "c", "-f", "invert" }), ip::ArgumentError);
    CHECK_THROWS_AS(parseArgs({ "-i", "a", "-o", "b", "-f", "invert", "--bogus" }), ip::ArgumentError);
    CHECK_THROWS_AS(parseArgs({ "-i", "", "-o", "b", "-f", "invert" }), ip::ArgumentError);
}

TEST_CASE(Cli_ParsesThresholdOption) {
    // 과제 README 예시: --filter blur --threshold 128
    const ip::ProgramOptions options = parseArgs({ "-i", "a", "-o", "b", "-f", "blur", "--threshold", "128" });
    CHECK_EQ(options.filterName, std::string("blur"));
    CHECK_EQ(options.thresholdSpec, std::string("128"));

    CHECK_THROWS_AS(parseArgs({ "-i", "a", "-o", "b", "-f", "blur", "--threshold" }), ip::ArgumentError);
    CHECK_THROWS_AS(parseArgs({ "-i", "a", "-o", "b", "-f", "blur", "--threshold", "1", "--threshold", "2" }),
                    ip::ArgumentError);
    CHECK_THROWS_AS(parseArgs({ "-i", "a", "-o", "b", "--threshold", "128" }), ip::ArgumentError);  // 필터 없음
}

TEST_CASE(Cli_ParsesThreadCountStrictly) {
    for (const char* bad : { "-1", "abc", "4x", "", "257", "99999999999" }) {
        CHECK_THROWS_AS(parseArgs({ "-i", "a", "-o", "b", "-f", "invert", "-t", bad }), ip::ArgumentError);
    }
    CHECK_EQ(parseArgs({ "-i", "a", "-o", "b", "-f", "invert", "-t", "0" }).threadCount, 0u);
    CHECK_EQ(parseArgs({ "-i", "a", "-o", "b", "-f", "invert", "-t", "256" }).threadCount, 256u);
}
