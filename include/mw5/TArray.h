#pragma once
#include <cstdint>

template<typename T>
struct TArray {
	T*      Data;
	int32_t Count;
	int32_t Capacity;
};
