#pragma once

#include <stdint.h>

#define N (624) // length of state vector

class CPseudoRandGen final
{
private:
	uint32_t state[N + 1]; // state vector + 1 extra to not violate ANSI C
	uint32_t* next;        // next random value is computed from here
	int left;              // can *next++ this many times before reloading
private:
	uint32_t Reload();

public:
	CPseudoRandGen();
	~CPseudoRandGen();
	void Seed(uint32_t seed);
	uint32_t Rand();
	float Rand(float fMin, float fMax);
};
