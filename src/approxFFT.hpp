#pragma once
#include "config.hpp"
#include <cstdint>

void Approx_FFT(char in[], uint16_t N, float Frequency, int32_t out_r[], int32_t out_im[]);

int fast_cosine(int Amp, int th);