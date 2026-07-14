#pragma once

#include <random>

namespace broom {

// Uniform random integer in [lo, hi], thread-safe.
inline int rng_int(int lo, int hi) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    return std::uniform_int_distribution<int>(lo, hi)(rng);
}

} // namespace broom
