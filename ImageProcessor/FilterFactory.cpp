/**
 * @file FilterFactory.cpp
 */

#include "FilterFactory.h"
#include "Exceptions.h"
#include "GaussianBlurFilter.h"
#include "GrayscaleFilter.h"
#include "InvertFilter.h"
#include "SharpenFilter.h"
#include "SobelFilter.h"
#include "ThresholdFilter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <iomanip>
#include <optional>
#include <ostream>
#include <string_view>

namespace ip {

namespace {

using Param = std::optional<std::string_view>;

std::string_view trim(std::string_view text) noexcept {
    const auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (!text.empty() && isSpace(text.front())) { text.remove_prefix(1); }
    while (!text.empty() && isSpace(text.back()))  { text.remove_suffix(1); }
    return text;
}

std::string toLower(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

/// 문자열 전체가 정수여야 한다. std::stoi 와 달리 "128abc" 를 거부하고,
/// 실패 시 std::invalid_argument 가 아닌 FilterError 를 던져 종료 코드(3)가 일관된다.
int parseInt(std::string_view text, const char* filterName) {
    int value = 0;
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), last, value);
    if (text.empty() || ec != std::errc() || ptr != last) {
        throw FilterError(std::string(filterName) + ": expected an integer parameter, got \"" +
                          std::string(text) + "\"");
    }
    return value;
}

double parseReal(std::string_view text, const char* filterName) {
    double value = 0.0;
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), last, value);
    if (text.empty() || ec != std::errc() || ptr != last) {
        throw FilterError(std::string(filterName) + ": expected a numeric parameter, got \"" +
                          std::string(text) + "\"");
    }
    return value;
}

void rejectParam(const Param& param, const char* filterName) {
    if (param) {
        throw FilterError(std::string(filterName) + ": this filter takes no parameter (got \"" +
                          std::string(*param) + "\")");
    }
}

struct CatalogEntry {
    std::string_view name;
    std::string_view syntax;
    std::string_view summary;
    std::unique_ptr<FilterBase> (*create)(const Param& param);
};

// 새 필터는 여기에 한 줄 추가하면 --filter / --pipeline / --list-filters 에 모두 반영된다.
const std::array<CatalogEntry, 6> CATALOG = { {
    { "grayscale", "grayscale", "Grayscale using BT.601 luma",
      [](const Param& p) -> std::unique_ptr<FilterBase> {
          rejectParam(p, "grayscale");
          return std::make_unique<GrayscaleFilter>();
      } },
    { "invert", "invert", "Invert every channel (255 - v)",
      [](const Param& p) -> std::unique_ptr<FilterBase> {
          rejectParam(p, "invert");
          return std::make_unique<InvertFilter>();
      } },
    { "threshold", "threshold[:<0-255>|:otsu]", "Binarize by luma (default: Otsu auto level)",
      [](const Param& p) -> std::unique_ptr<FilterBase> {
          if (!p || toLower(*p) == "otsu") {
              return std::make_unique<ThresholdFilter>(std::nullopt);
          }
          return std::make_unique<ThresholdFilter>(parseInt(*p, "threshold"));
      } },
    { "blur", "blur[:<sigma>]", "Separable Gaussian blur (default sigma=1, max 50)",
      [](const Param& p) -> std::unique_ptr<FilterBase> {
          return std::make_unique<GaussianBlurFilter>(
              p ? parseReal(*p, "blur") : GaussianBlurFilter::DEFAULT_SIGMA);
      } },
    { "sharpen", "sharpen[:<amount>]", "Unsharp mask (default amount=1, max 10)",
      [](const Param& p) -> std::unique_ptr<FilterBase> {
          return std::make_unique<SharpenFilter>(
              p ? parseReal(*p, "sharpen") : SharpenFilter::DEFAULT_AMOUNT);
      } },
    { "sobel", "sobel", "Sobel edge magnitude",
      [](const Param& p) -> std::unique_ptr<FilterBase> {
          rejectParam(p, "sobel");
          return std::make_unique<SobelFilter>();
      } },
} };

std::string availableNames() {
    std::string names;
    for (const CatalogEntry& entry : CATALOG) {
        if (!names.empty()) {
            names += ", ";
        }
        names += entry.name;
    }
    return names;
}

/// 쉼표가 없는 단일 토큰 "name[:param]" 을 필터로 변환한다.
std::unique_ptr<FilterBase> createFromToken(std::string_view token) {
    token = trim(token);

    std::string_view namePart = token;
    Param param;
    const std::size_t colon = token.find(':');
    if (colon != std::string_view::npos) {
        namePart = trim(token.substr(0, colon));
        param = trim(token.substr(colon + 1));
        if (param->empty()) {
            throw FilterError("\"" + std::string(token) + "\": missing parameter after ':'");
        }
    }
    if (namePart.empty()) {
        throw FilterError("\"" + std::string(token) + "\": missing filter name");
    }

    const std::string name = toLower(namePart);
    for (const CatalogEntry& entry : CATALOG) {
        if (entry.name == name) {
            return entry.create(param);
        }
    }
    throw FilterError("Unknown filter: \"" + std::string(namePart) +
                      "\" (available: " + availableNames() + ")");
}

} // anonymous namespace

std::unique_ptr<FilterBase> FilterFactory::create(const std::string& spec) {
    if (spec.find(',') != std::string::npos) {
        throw FilterError("\"" + spec + "\": --filter accepts a single filter; "
                          "use --pipeline to chain multiple filters");
    }
    return createFromToken(spec);
}

std::unique_ptr<FilterPipeline> FilterFactory::createPipeline(const std::string& specList) {
    auto pipeline = std::make_unique<FilterPipeline>();

    std::string_view remaining = specList;
    std::size_t position = 1;
    while (true) {
        const std::size_t comma = remaining.find(',');
        const std::string_view token = trim(remaining.substr(0, comma));
        if (token.empty()) {
            throw FilterError("pipeline: filter #" + std::to_string(position) +
                              " is empty in \"" + specList + "\"");
        }
        pipeline->add(createFromToken(token));

        if (comma == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(comma + 1);
        ++position;
    }
    return pipeline;
}

void FilterFactory::printCatalog(std::ostream& os) {
    os << "Available filters:\n";
    for (const CatalogEntry& entry : CATALOG) {
        os << "  " << std::left << std::setw(28) << entry.syntax << entry.summary << '\n';
    }
    os << "\nPipeline example:\n"
       << "  --pipeline \"grayscale, blur:1.5, threshold:otsu\"\n";
}

} // namespace ip
