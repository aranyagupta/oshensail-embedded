#pragma once
#include "config.hpp"
#include <cstdint>
#include <cstring>
#include <math.h>
#include "approxFFT.hpp"

inline void preEmphasis(char* longStore, uint32_t dataSize);

// Preprocess data - Pre-emphasis, frame blocking, hammming windowing, ApproxFFT, Bandpass filtering, DCT, Log Energy, normalise
void preprocess(char* longStore, uint32_t dataSize, int32_t* magnitudes, int32_t* pooledMags, const uint32_t iters);