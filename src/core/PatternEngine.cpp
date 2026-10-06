#include "backreverse/PatternEngine.h"
#include <algorithm>
#include <numeric>
#include <random>

namespace br {
std::vector<std::size_t> buildOrder(OrderMode mode, std::size_t count, std::uint64_t seed, const std::vector<int>& userPattern) {
    std::vector<std::size_t> out(count);
    std::iota(out.begin(), out.end(), 0);
    if (count == 0) return out;
    switch (mode) {
        case OrderMode::Sequential: break;
        case OrderMode::ReverseOrder: std::reverse(out.begin(), out.end()); break;
        case OrderMode::Random: {
            std::mt19937_64 rng(seed);
            std::shuffle(out.begin(), out.end(), rng);
            break;
        }
        case OrderMode::ShuffleNoRepeat: {
            std::mt19937_64 rng(seed);
            std::shuffle(out.begin(), out.end(), rng);
            break;
        }
        case OrderMode::PingPong: {
            std::vector<std::size_t> p;
            p.reserve(count == 1 ? 1 : count * 2 - 2);
            for (std::size_t i=0;i<count;++i) p.push_back(i);
            if (count > 1) for (std::size_t i=count-2;i>0;--i) p.push_back(i);
            return p;
        }
        case OrderMode::OddsThenEvens: {
            out.clear();
            for (std::size_t i=1;i<count;i+=2) out.push_back(i);
            for (std::size_t i=0;i<count;i+=2) out.push_back(i);
            break;
        }
        case OrderMode::EvensThenOdds: {
            out.clear();
            for (std::size_t i=0;i<count;i+=2) out.push_back(i);
            for (std::size_t i=1;i<count;i+=2) out.push_back(i);
            break;
        }
        case OrderMode::RotateLeft: std::rotate(out.begin(), out.begin()+1, out.end()); break;
        case OrderMode::RotateRight: std::rotate(out.begin(), out.end()-1, out.end()); break;
        case OrderMode::UserPattern: {
            out.clear();
            for (int v : userPattern) {
                if (v >= 0 && static_cast<std::size_t>(v) < count) out.push_back(static_cast<std::size_t>(v));
            }
            if (out.empty()) { out.resize(count); std::iota(out.begin(), out.end(), 0); }
            break;
        }
    }
    return out;
}
}
