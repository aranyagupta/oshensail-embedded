#pragma once
#include "config.hpp"
#include <cstdint>
#include <cstring>
#include <math.h>
#include "approxFFT.hpp"

inline void preEmphasis(char* longStore, uint32_t dataSize);

inline void hammingWindowing(char* longStore, uint32_t dataSize);

inline void triangularBandpass(int32_t* magnitudes, int32_t* frequencies, uint16_t dataSize, int32_t* bandPassedSpectrum);

inline void logAndNormalise(int32_t* bandPassedSpectrum, float* newBandPassedSpectrum);

inline void DCT(float* normalisedSpectrum, int32_t* cepstrumCoeffs);

// Preprocess data - Pre-emphasis, frame blocking, hammming windowing, ApproxFFT, Bandpass filtering, DCT, Log Energy, normalise
void preprocess(char* longStore, uint32_t dataSize, int32_t* magnitudes, const uint32_t iters);