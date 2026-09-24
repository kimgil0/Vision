/**
 * @file CommandLineParser.cpp
 */

#include "CommandLineParser.h"
#include "Exceptions.h"
#include "Parallel.h"

#include <charconv>
#include <iostream>
#include <string>

namespace ip {

namespace {
    /// argv 의 다음 인자를 안전하게 가져온다.
    std::string nextArg(int argc, char* argv[], int& i, const std::string& flag) {
        if (i + 1 >= argc) {
            throw ArgumentError(flag + ": missing value");
        }
        return argv[++i];
    }

    /// 같은 옵션을 두 번 지정하면 뒤의 값이 앞의 값을 조용히 덮어쓰지 않도록 거부한다.
    void assignOnce(std::string& field, const std::string& value, const std::string& flag) {
        if (!field.empty()) {
            throw ArgumentError(flag + ": specified more than once");
        }
        if (value.empty()) {
            throw ArgumentError(flag + ": value must not be empty");
        }
        field = value;
    }

    unsigned parseThreadCount(const std::string& text, const std::string& flag) {
        unsigned value = 0;
        const char* last = text.data() + text.size();
        const auto [ptr, ec] = std::from_chars(text.data(), last, value);
        if (text.empty() || ec != std::errc() || ptr != last || value > MAX_THREAD_COUNT) {
            throw ArgumentError(flag + ": expected an integer in [0, " +
                                std::to_string(MAX_THREAD_COUNT) + "] (0 = auto), got \"" +
                                text + "\"");
        }
        return value;
    }
} // anonymous namespace

ProgramOptions CommandLineParser::parse(int argc, char* argv[]) {
    ProgramOptions options;
    bool threadsSpecified = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--input" || arg == "-i") {
            assignOnce(options.inputPath, nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--output" || arg == "-o") {
            assignOnce(options.outputPath, nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--filter" || arg == "-f") {
            assignOnce(options.filterName, nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--pipeline" || arg == "-p") {
            assignOnce(options.pipelineSpec, nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--threshold") {
            // 값의 유효성(0~255, otsu)은 필터 생성 시 FilterFactory 가 검증한다.
            assignOnce(options.thresholdSpec, nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--log" || arg == "-l") {
            assignOnce(options.logPath, nextArg(argc, argv, i, arg), arg);
        }
        else if (arg == "--threads" || arg == "-t") {
            if (threadsSpecified) {
                throw ArgumentError(arg + ": specified more than once");
            }
            options.threadCount = parseThreadCount(nextArg(argc, argv, i, arg), arg);
            threadsSpecified = true;
        }
        else if (arg == "--list-filters") {
            options.listFilters = true;
            return options;
        }
        else if (arg == "--help" || arg == "-h") {
            // [수정] 기존 코드는 여기서 std::exit(0) 을 호출했다.
            //        std::exit 는 스택 객체의 소멸자를 실행하지 않고, 파서가 프로세스 수명을
            //        결정하게 되어 테스트도 불가능하다. 플래그만 세우고 호출자가 판단한다.
            options.showHelp = true;
            return options;
        }
        else {
            throw ArgumentError("Unknown option: " + arg);
        }
    }

    // 필수 인자 검증
    if (options.inputPath.empty()) {
        throw ArgumentError("--input is required");
    }
    if (options.outputPath.empty()) {
        throw ArgumentError("--output is required");
    }
    const bool hasFilter   = !options.filterName.empty();
    const bool hasPipeline = !options.pipelineSpec.empty();
    if (hasFilter && hasPipeline) {
        throw ArgumentError("--filter and --pipeline cannot be used together");
    }
    if (!hasFilter && !hasPipeline) {
        throw ArgumentError("either --filter or --pipeline is required");
    }

    return options;
}

void CommandLineParser::printUsage(const std::string& exeName) {
    std::cout
        << "Usage:\n"
        << "  " << exeName << " --input <path> --output <path>\n"
        << "      (--filter <spec> | --pipeline \"<spec>, <spec>, ...\") [--threshold <n|otsu>]\n"
        << "      [--threads <n>] [--log <path>]\n\n"
        << "Options:\n"
        << "  -i, --input    <path>   Input BMP file (24-bit, uncompressed)\n"
        << "  -o, --output   <path>   Output BMP file (missing folders are created)\n"
        << "  -f, --filter   <spec>   Single filter (e.g. grayscale, threshold:128, blur:2)\n"
        << "  -p, --pipeline <list>   Comma-separated chain (e.g. \"grayscale, blur:1.5, threshold:otsu\")\n"
        << "      --threshold <n>     Binarize after the filter(s): 0-255 or otsu\n"
        << "  -t, --threads  <n>      Worker threads, 0 = auto (default 0, max "
        << MAX_THREAD_COUNT << ")\n"
        << "  -l, --log      <path>   Append a timestamped log to <path>\n"
        << "      --list-filters      Show available filters and parameters\n"
        << "      --demo              Run the built-in demo on .\\Resource samples (same as double-click)\n"
        << "  -h, --help              Show this message\n\n"
        << "Exit codes:\n"
        << "  0 success, 1 unexpected, 2 BMP I/O error, 3 filter error, 4 invalid arguments\n\n"
        << "Examples:\n"
        << "  " << exeName << " -i input.bmp -o result.bmp -f grayscale\n"
        << "  " << exeName << " -i input.bmp -o result.bmp -f threshold:128\n"
        << "  " << exeName << " -i input.bmp -o result.bmp -f blur --threshold 128\n"
        << "  " << exeName << " -i input.bmp -o result.bmp -p \"grayscale, blur:1.5, threshold:otsu\" -t 8 -l run.log\n";
}

} // namespace ip
