/**
 * @file FilterBase.cpp
 */

#include "FilterBase.h"
#include "Exceptions.h"

#include <sstream>

namespace ip {

void FilterBase::apply(ImageBuffer& image, const FilterContext& context) const {
    if (image.empty()) {
        throw FilterError(describe() + ": cannot apply to an empty image");
    }
    if (context.threadCount == 0) {
        throw FilterError(describe() + ": threadCount must be at least 1");
    }
    process(image, context);
}

std::string FilterBase::formatNumber(double value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

} // namespace ip
