#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "backreverse/BackReverseEngine.h"

namespace br {
std::vector<std::size_t> buildOrder(OrderMode mode, std::size_t count, std::uint64_t seed, const std::vector<int>& userPattern);
}
