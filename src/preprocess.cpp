// Following http://mirlab.org/jang/books/audiosignalprocessing/speechFeatureMfcc.asp?title=12-2%20MFCC
#include "preprocess.hpp"
#include <Arduino.h>
const uint16_t hammingCoeffs[FRAME_SIZE] = {
  20, 20, 20, 20, 21, 21, 21, 22, 22, 23, 24, 24, 25, 26, 27, 28, 29, 30, 31, 33, 34, 35, 37, 38, 40, 42, 43, 45, 47, 49, 51, 53,
   55, 57, 59, 61, 63, 66, 68, 70, 73, 75, 78, 80, 83, 85, 88, 91, 93, 96, 99, 101, 104, 107, 110, 113, 115, 118, 121, 124, 127, 
   130, 133, 136, 138, 141, 144, 147, 150, 153, 156, 159, 162, 164, 167, 170, 173, 176, 178, 181, 184, 186, 189, 192, 194, 197, 
   199, 202, 204, 206, 209, 211, 213, 215, 218, 220, 222, 224, 226, 228, 229, 231, 233, 235, 236, 238, 239, 241, 242, 243, 245,
    246, 247, 248, 249, 250, 251, 252, 252, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 253, 
    253, 252, 252, 251, 250, 249, 248, 247, 246, 245, 243, 242, 241, 239, 238, 236, 235, 233, 231, 229, 228, 226, 224, 222, 220, 
    218, 215, 213, 211, 209, 206, 204, 202, 199, 197, 194, 192, 189, 186, 184, 181, 178, 176, 173, 170, 167, 164, 162, 159, 156,
     153, 150, 147, 144, 141, 138, 136, 133, 130, 127, 124, 121, 118, 115, 113, 110, 107, 104, 101, 99, 96, 93, 91, 88, 85, 83, 
     80, 78, 75, 73, 70, 68, 66, 63, 61, 59, 57, 55, 53, 51, 49, 47, 45, 43, 42, 40, 38, 37, 35, 34, 33, 31, 30, 29, 28, 27, 26, 
     25, 24, 24, 23, 22, 22, 21, 21, 21, 20, 20, 20, 20
};

// longStore is the most raw char PCM24 data we can store and at once
// dataSize is the number of 24 bit integers we have, not the number of bytes (so always should be multiplied by 3 when necessary)
inline void preEmphasis(char* longStore, uint32_t dataSize){
  // Using software pipelining to ensure that original data is overwritten to save space
  // X(n) = S(n) - 0.95 * S(n-1)

  // Prologue: Precompute X(0) = S(0), X(1) = S(1)-0.95*S(0)
  int32_t twoAgoResult = (longStore[0]<<24) + (longStore[1]<<16) + (longStore[2]<<8);
  int32_t oneAgoResult = (longStore[3]<<24) + (longStore[4]<<16) + (longStore[5]<<8) - PREEMP_A * twoAgoResult;

  for (int i=2; i<dataSize; i++){
    /* Kernel - computation for most of the array */

    // store X(i-2) into array 
    longStore[3*i-6] = (twoAgoResult&0xFF000000) >> 24;
    longStore[3*i-5] = (twoAgoResult&0x00FF0000) >> 16;
    longStore[3*i-4] = (twoAgoResult&0x0000FF00) >> 8;

    // Hold, but do not store, X(i-1) as X(i-2) as X(i-2) has been stored
    twoAgoResult = oneAgoResult;

    // Compute X(i) and hold it as X(i-1), as X(i-1) has been held as X(i-2)
    int32_t currentValue = (longStore[3*i]<<24) + (longStore[3*i+1]<<16) + (longStore[3*i+2]<<8);
    int32_t priorValue = (longStore[3*i-3]<<24) + (longStore[3*i-2]<<16) + (longStore[3*i-1]<<8);
    oneAgoResult = currentValue - PREEMP_A * priorValue;
  }
  /* Epilogue: store remaining values*/
  // Store X(n-2) and X(n-1)
  longStore[3*dataSize-6] = (twoAgoResult&0xFF000000) >> 24;
  longStore[3*dataSize-5] = (twoAgoResult&0x00FF0000) >> 16;
  longStore[3*dataSize-4] = (twoAgoResult&0x0000FF00) >> 8;

  longStore[3*dataSize-3] = (oneAgoResult&0xFF000000) >> 24;
  longStore[3*dataSize-2] = (oneAgoResult&0x00FF0000) >> 16;
  longStore[3*dataSize-1] = (oneAgoResult&0x0000FF00) >> 8;
}

// Apply a hamming window to each frame 
inline void hammingWindowing(char* longStore, uint32_t dataSize){
  // Multiply signal frames by hamming window w(n, a) = (1-a) - acos(2*pi*n/(N-1)) for n = 0...N-1 
  // a = 0.46
  uint32_t iters = dataSize/FRAME_SIZE;
  for (uint32_t j=0; j<iters; j++){
    uint32_t frameOffset = 3*j*FRAME_SIZE;
    for (uint32_t i=0; i<FRAME_SIZE; i++){
      int32_t bytesToInt = (longStore[frameOffset+3*i]<<24) + (longStore[frameOffset+3*i+1]<<16) + (longStore[frameOffset+3*i+2]<<8);
      bytesToInt /= FRAME_SIZE;
      bytesToInt *= (int32_t) hammingCoeffs[i];
      longStore[3*i+frameOffset] = (bytesToInt&0xFF000000) >> 24;
      longStore[3*i+1+frameOffset] = (bytesToInt&0x00FF0000) >> 16;
      longStore[3*i+2+frameOffset] = (bytesToInt&0x0000FF00) >> 8;
    } 
  }

  // set end bytes to 0 as not all samples will be in frames
  // uint16_t numBytes = dataSize*3 - FRAME_SIZE*iters*3;
  // memset(longStore+FRAME_SIZE*iters*3, 0, numBytes);
}


// Preprocesses data into cepstrum coefficients 
// longStore is the raw bytes data taken from hydrophone
// dataSize is the number of SAMPLES on the hydrophone, NOT the number of bytes
// cepstrumCoeffs is an (empty) buffer into which coeffs will be placed
void preprocess(char* longStore, uint32_t dataSize, int32_t* magnitudes, const uint32_t iters){
  
  preEmphasis(longStore, dataSize);
  // Serial.println("Finish Pre-emphasis");
  hammingWindowing(longStore, dataSize);
  // Serial.println("Finish Hamming Windowing");
  
  int32_t temp_mag[FRAME_SIZE]; 
  int32_t temp_freq[FRAME_SIZE];

  // perform fft on each hamming windowed frame of length 1024
  for (uint16_t i=0; i<iters; i++){
    // stores one frame's worth of frequencies and magnitudes at a time. each frame contains 1024 samples*3 bytes of data
    // store into temporary area to avoid memory overwriting errors
    Approx_FFT(longStore+i*HOP_LENGTH*3, FRAME_SIZE, SAMPLE_RATE, temp_mag, temp_freq);
    memcpy(magnitudes+i*FRAME_SIZE/2, temp_mag, FRAME_SIZE/2*4); // 4 bytes per int
  }

  // TODO: normalisation of magnitudes
  
}