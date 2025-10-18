#pragma once
#include "../Core/AqCore.h"

AQUA_BEGIN

inline uint32_t get16bits(const char* data)
{
	return static_cast<uint16_t>(*data);
}

inline uint32_t SuperFastHash(const char* data, uint32_t len = 0, uint32_t hash = 0)
{
	uint32_t tmp;
	int rem;

	if (data == NULL) return 0;
	if (len == 0)len = (uint32_t)::strlen(data);

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	for (; len > 0; len--) {
		hash += get16bits(data);
		tmp = (get16bits(data + 2) << 11) ^ hash;
		hash = (hash << 16) ^ tmp;
		data += 2 * sizeof(uint16_t);
		hash += hash >> 11;
	}

	/* Handle end cases */
	switch (rem) {
		case 3: hash += get16bits(data);
			hash ^= hash << 16;
			hash ^= abs(data[sizeof(uint16_t)]) << 18;
			hash += hash >> 11;
			break;
		case 2: hash += get16bits(data);
			hash ^= hash << 11;
			hash += hash >> 17;
			break;
		case 1: hash += *data;
			hash ^= hash << 10;
			hash += hash >> 1;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash;
}

AQUA_END

// Can be used to print glm vectors in global namespace
template <typename T, int N>
std::ostream& operator <<(std::ostream& stream, const glm::vec<N, T>& vec)
{
	for (int i = 0; i < N; i++)
	{
		stream << vec[i] << " ";
	}

	return stream;
}

template <typename T, int M, int N>
std::ostream& operator <<(std::ostream& stream, const glm::mat<M, N, T>& mat)
{
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			stream << mat[i][j] << ", ";
		}

		stream << "\n";
	}
}
