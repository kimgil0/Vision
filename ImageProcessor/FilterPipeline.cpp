/**
 * @file FilterPipeline.cpp
 */

#include "FilterPipeline.h"
#include "Exceptions.h"
#include "Logger.h"

#include <chrono>
#include <exception>

namespace ip {

void FilterPipeline::add(std::unique_ptr<FilterBase> filter) {
    if (!filter) {
        throw FilterError("pipeline: cannot add a null filter");
    }
    m_stages.push_back(std::move(filter));
}

std::string FilterPipeline::describe() const {
    if (m_stages.empty()) {
        return "(empty pipeline)";
    }
    std::string result;
    for (std::size_t i = 0; i < m_stages.size(); ++i) {
        if (i > 0) {
            result += " -> ";
        }
        result += m_stages[i]->describe();
    }
    return result;
}

void FilterPipeline::process(ImageBuffer& image, const FilterContext& context) const {
    if (m_stages.empty()) {
        throw FilterError("pipeline: no filters to apply");
    }

    const std::string total = std::to_string(m_stages.size());
    for (std::size_t i = 0; i < m_stages.size(); ++i) {
        const FilterBase& stage = *m_stages[i];
        const std::string label = "[" + std::to_string(i + 1) + "/" + total + "] " + stage.describe();

        const auto start = std::chrono::steady_clock::now();
        try {
            stage.apply(image, context);
        }
        catch (const std::exception&) {
            // 어느 단계에서 실패했는지 남긴 뒤, 원래 예외 타입 그대로 재전파한다.
            if (context.logger != nullptr) {
                context.logger->error("  " + label + " failed");
            }
            throw;
        }

        if (context.logger != nullptr) {
            context.logger->info("  " + label + "  " +
                                 formatDuration(std::chrono::steady_clock::now() - start));
        }
    }
}

} // namespace ip
