/**
 * @file DemoMode.cpp
 */

#include "DemoMode.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <conio.h>
#endif

namespace ip {
namespace demo {

namespace {

namespace fs = std::filesystem;

constexpr const char* DEMO_TITLE = "ImageProcessor";

/// 출력 파일명 앞에 붙이면 Output\demo 폴더 경로로 치환된다 (예: 로그 파일 경로).
constexpr const char* OUT_TOKEN = "{OUT}\\";

struct Scenario {
    const char*              title;
    const char*              input;   ///< Resource 폴더 기준 파일명
    const char*              output;  ///< Output\demo 폴더 기준 파일명
    std::vector<std::string> args;    ///< 필터 관련 인자
    const char*              note;    ///< 실행 후 덧붙일 설명 (없으면 nullptr)
};

// 지원하는 필터와 옵션을 모두 한 번씩 보여 주는 시나리오. 마지막은 오류 처리 시연이다.
const std::vector<Scenario>& scenarios() {
    static const std::vector<Scenario> list = {
        { "흑백",             "1_astronaut.bmp",    "1_grayscale.bmp",       { "-f", "grayscale" }, nullptr },
        { "색 반전",          "2_coffee.bmp",       "2_invert.bmp",          { "-f", "invert" }, nullptr },
        { "이진화",           "4_text_page.bmp",    "3_threshold.bmp",       { "-f", "threshold:128" }, nullptr },
        { "Otsu 자동 이진화", "4_text_page.bmp",    "4_otsu.bmp",            { "-f", "threshold:otsu" }, nullptr },
        { "블러",             "2_coffee.bmp",       "5_blur.bmp",            { "-f", "blur:3" }, nullptr },
        { "블러 + 임계값",    "2_coffee.bmp",       "6_blur_threshold.bmp",  { "-f", "blur", "--threshold", "128" }, nullptr },
        { "파이프라인",       "3_chelsea_cat.bmp",  "7_pipeline.bmp",        { "--pipeline", "grayscale, blur, threshold:128" }, nullptr },
        { "샤프닝",           "1_astronaut.bmp",    "8_sharpen.bmp",         { "-f", "sharpen:2" }, nullptr },
        { "엣지 검출",        "3_chelsea_cat.bmp",  "9_sobel_cat.bmp",       { "-f", "sobel" }, nullptr },
        { "엣지 검출",        "5_checkerboard.bmp", "10_sobel_checker.bmp",  { "-f", "sobel" }, nullptr },
        { "강한 블러 + 로그", "2_coffee.bmp",       "11_blur_strong.bmp",
          { "-f", "blur:6", "--threads", "8", "--log", "{OUT}\\demo.log" }, nullptr },
        { "오류 처리 시연",   "1_astronaut.bmp",    "12_unknown_filter.bmp", { "-f", "emboss" },
          "의도된 실패: 없는 필터는 오류로 중단하고 파일을 만들지 않음 (종료 코드 3)" },
    };
    return list;
}

fs::path executableDirectory() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        buffer.resize(length);
        return fs::path(buffer).parent_path();
    }
#endif
    std::error_code ignored;
    return fs::current_path(ignored);
}

/// exe 위치에서 위로 올라가며 샘플 이미지가 든 Resource 폴더를 찾는다.
///   <프로젝트>\x64\Release\ImageProcessor.exe → <프로젝트>\Resource
std::optional<fs::path> findResourceDirectory(fs::path directory) {
    std::error_code ignored;
    for (int level = 0; level < 4 && !directory.empty(); ++level) {
        const fs::path candidate = directory / "Resource";
        if (fs::exists(candidate / "1_astronaut.bmp", ignored)) {
            return candidate;
        }
        if (directory == directory.parent_path()) {
            break;
        }
        directory = directory.parent_path();
    }
    return std::nullopt;
}

std::string quoteIfNeeded(const std::string& arg) {
    return arg.find(' ') == std::string::npos ? arg : "\"" + arg + "\"";
}

void openFolder(const fs::path& folder) {
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", folder.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)folder;
#endif
}

void waitForKey() {
#ifdef _WIN32
    std::cout << "\n아무 키나 누르면 창이 닫힙니다..." << std::flush;
    (void)_getch();
#endif
}

} // anonymous namespace

bool isLaunchedByDoubleClick() {
#ifdef _WIN32
    // 콘솔에 붙은 프로세스가 자기 자신뿐이면 탐색기가 이 exe 를 위해 새 콘솔 창을 만든 것이다.
    // 명령 프롬프트/PowerShell 에서 실행하면 셸도 같은 콘솔에 붙어 있으므로 2 이상이 된다.
    DWORD processIds[2] = {};
    return GetConsoleProcessList(processIds, 2) == 1;
#else
    return false;
#endif
}

int run(CliEntry runCli, bool interactive) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // 한글 안내문(UTF-8)이 깨지지 않도록
#endif

    const fs::path exeDirectory = executableDirectory();
    const std::optional<fs::path> resourceDirectory = findResourceDirectory(exeDirectory);
    if (!resourceDirectory) {
        std::cerr << "[Demo] 샘플 이미지 폴더(Resource)를 찾지 못했습니다.\n"
                  << "       exe 위치: " << exeDirectory.u8string() << "\n"
                  << "       프로젝트 폴더 안의 x64\\Release 에 있는 exe 를 실행해 주세요.\n";
        if (interactive) {
            waitForKey();
        }
        return 2;
    }

    const fs::path outputDirectory = resourceDirectory->parent_path() / "Output" / "demo";
    std::error_code error;
    fs::create_directories(outputDirectory, error);
    if (error) {
        std::cerr << "[Demo] 출력 폴더를 만들 수 없습니다: " << outputDirectory.u8string()
                  << " (" << error.message() << ")\n";
        if (interactive) {
            waitForKey();
        }
        return 2;
    }

    std::cout << "================================================================\n"
              << "  " << DEMO_TITLE << " - 필터 적용 데모\n"
              << "  입력: " << resourceDirectory->u8string() << "\n"
              << "  출력: " << outputDirectory.u8string() << "\n"
              << "================================================================\n";

    const std::string exePath = (exeDirectory / "ImageProcessor.exe").string();
    int index = 0;
    for (const Scenario& scenario : scenarios()) {
        std::vector<std::string> args = {
            exePath,
            "-i", (*resourceDirectory / scenario.input).string(),
            "-o", (outputDirectory / scenario.output).string(),
        };
        std::string shown;
        for (const std::string& arg : scenario.args) {
            if (arg.rfind(OUT_TOKEN, 0) == 0) {
                const std::string fileName = arg.substr(std::char_traits<char>::length(OUT_TOKEN));
                args.push_back((outputDirectory / fileName).string());
                shown += " Output\\demo\\" + fileName;
            }
            else {
                args.push_back(arg);
                shown += " " + quoteIfNeeded(arg);
            }
        }

        std::cout << "\n[" << ++index << "] " << scenario.title << "   " << scenario.input << shown << '\n'
                  << std::flush;

        // 이전 실행의 결과 파일이 남아 있으면 실패한 시나리오를 성공으로 착각할 수 있다.
        fs::remove(outputDirectory / scenario.output, error);

        std::vector<char*> argv;
        for (std::string& arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);  // 표준 main 과 같이 argv[argc] == nullptr

        const int exitCode = runCli(static_cast<int>(args.size()), argv.data());
        std::cout.flush();
        std::cerr.flush();

        if (exitCode == 0) {
            std::cout << "    => 성공: " << scenario.output << " 생성\n";
        }
        else {
            std::cout << "    => 실패: 종료 코드 " << exitCode << "\n";
        }
        if (scenario.note != nullptr) {
            std::cout << "    " << scenario.note << '\n';
        }
    }

    std::cout << "\n================================================================\n"
              << "  완료. 결과 이미지 폴더: " << outputDirectory.u8string() << "\n"
              << "================================================================\n";

    if (interactive) {
        std::cout << "  결과 폴더를 탐색기로 엽니다. BMP 파일을 더블클릭하면 이미지를 볼 수 있습니다.\n";
        openFolder(outputDirectory);
        waitForKey();
    }
    return 0;
}

} // namespace demo
} // namespace ip
