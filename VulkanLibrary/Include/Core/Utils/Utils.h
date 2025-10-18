#pragma once
#include "../Config.h"

VK_BEGIN
VK_UTILS_BEGIN

static size_t CombineHash(size_t h1, size_t h2)
{
	// A common constant used in hash combining algorithms
	// It's a large prime number, often derived from the golden ratio
	const std::size_t prime_constant = 0x9e3779b9;

	// Combine h1 with h2 using XOR, addition, and bit shifts
	// The bit shifts help to mix the bits and avoid patterns
	// The prime constant adds randomness and helps distribute values
	return h1 ^ (h2 + prime_constant + (h1 << 6) + (h1 >> 2));
}

VK_UTILS_END
VK_END
