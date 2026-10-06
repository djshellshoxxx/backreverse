#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "backreverse/BackReverseEngine.h"

namespace br {
constexpr std::size_t RestChunk = static_cast<std::size_t>(-1);

std::vector<std::size_t> buildOrder(OrderMode mode, std::size_t count, std::uint64_t seed, const std::vector<int>& userPattern);

// Pattern grammar (comma/space separated):
// 3       absolute chunk 3
// +2/-1   relative to the last selected chunk
// REST/R  silent slot
// 2*3     repeat chunk 2 three times
// 4@50    50% deterministic probability, otherwise REST
// +1*4@75 modifiers may be combined.
std::vector<int> parseUserPattern(const std::string& text, std::size_t chunkCount, std::uint64_t seed);
}
