// Following http://mirlab.org/jang/books/audiosignalprocessing/speechFeatureMfcc.asp?title=12-2%20MFCC
#include "preprocess.hpp"

// Hamming coefficients for multiplication with frame
// Stored as const and needs to be manually changed if frame size changes
// Need them const as otherwise they're stored in ram, which we need to optimize
const uint16_t hammingCoeffs[FRAME_SIZE] = {
  81, 81, 81, 81, 82, 82, 82, 82, 82, 82, 82, 82, 83, 83, 83, 83, 84, 84, 84, 85, 85, 85, 86, 86, 87, 87, 87, 88, 88, 89, 89, 90, 90, 
  91, 92, 92, 93, 94, 94, 95, 96, 96, 97, 98, 99, 99, 100, 101, 102, 103, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 
  115, 116, 117, 118, 120, 121, 122, 123, 124, 126, 127, 128, 129, 131, 132, 133, 134, 136, 137, 139, 140, 141, 143, 144, 146, 147, 
  149, 150, 152, 153, 155, 156, 158, 159, 161, 163, 164, 166, 168, 169, 171, 173, 174, 176, 178, 180, 181, 183, 185, 187, 189, 190, 
  192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 239, 241, 243, 
  245, 247, 249, 252, 254, 256, 258, 261, 263, 265, 268, 270, 272, 275, 277, 279, 282, 284, 286, 289, 291, 294, 296, 298, 301, 303, 
  306, 308, 311, 313, 316, 318, 321, 323, 326, 328, 331, 333, 336, 339, 341, 344, 346, 349, 352, 354, 357, 359, 362, 365, 367, 370, 
  373, 375, 378, 381, 383, 386, 389, 392, 394, 397, 400, 403, 405, 408, 411, 414, 416, 419, 422, 425, 427, 430, 433, 436, 439, 441, 
  444, 447, 450, 453, 456, 458, 461, 464, 467, 470, 473, 475, 478, 481, 484, 487, 490, 493, 495, 498, 501, 504, 507, 510, 513, 516, 
  518, 521, 524, 527, 530, 533, 536, 539, 542, 545, 547, 550, 553, 556, 559, 562, 565, 568, 571, 573, 576, 579, 582, 585, 588, 591, 
  594, 597, 599, 602, 605, 608, 611, 614, 617, 619, 622, 625, 628, 631, 634, 637, 639, 642, 645, 648, 651, 654, 656, 659, 662, 665, 
  668, 671, 673, 676, 679, 682, 684, 687, 690, 693, 696, 698, 701, 704, 707, 709, 712, 715, 717, 720, 723, 726, 728, 731, 734, 736, 
  739, 742, 744, 747, 749, 752, 755, 757, 760, 762, 765, 768, 770, 773, 775, 778, 780, 783, 785, 788, 790, 793, 795, 798, 800, 803, 
  805, 808, 810, 813, 815, 817, 820, 822, 825, 827, 829, 832, 834, 836, 838, 841, 843, 845, 848, 850, 852, 854, 857, 859, 861, 863, 
  865, 867, 870, 872, 874, 876, 878, 880, 882, 884, 886, 888, 890, 892, 894, 896, 898, 900, 902, 904, 906, 908, 910, 912, 914, 915, 
  917, 919, 921, 923, 925, 926, 928, 930, 931, 933, 935, 937, 938, 940, 942, 943, 945, 946, 948, 950, 951, 953, 954, 956, 957, 959, 
  960, 961, 963, 964, 966, 967, 968, 970, 971, 972, 974, 975, 976, 978, 979, 980, 981, 982, 984, 985, 986, 987, 988, 989, 990, 991, 
  992, 993, 994, 995, 996, 997, 998, 999, 1000, 1001, 1002, 1003, 1004, 1004, 1005, 1006, 1007, 1008, 1008, 1009, 1010, 1010, 1011, 
  1012, 1012, 1013, 1014, 1014, 1015, 1015, 1016, 1016, 1017, 1017, 1018, 1018, 1019, 1019, 1019, 1020, 1020, 1020, 1021, 1021, 1021, 
  1022, 1022, 1022, 1022, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 
  1023, 1023, 1023, 1023, 1022, 1022, 1022, 1022, 1021, 1021, 1021, 1020, 1020, 1020, 1019, 1019, 1019, 1018, 1018, 1017, 1017, 1016, 
  1016, 1015, 1015, 1014, 1014, 1013, 1012, 1012, 1011, 1010, 1010, 1009, 1008, 1008, 1007, 1006, 1005, 1004, 1004, 1003, 1002, 1001, 
  1000, 999, 998, 997, 996, 995, 994, 993, 992, 991, 990, 989, 988, 987, 986, 985, 984, 982, 981, 980, 979, 978, 976, 975, 974, 972, 
  971, 970, 968, 967, 966, 964, 963, 961, 960, 959, 957, 956, 954, 953, 951, 950, 948, 946, 945, 943, 942, 940, 938, 937, 935, 933, 
  931, 930, 928, 926, 925, 923, 921, 919, 917, 915, 914, 912, 910, 908, 906, 904, 902, 900, 898, 896, 894, 892, 890, 888, 886, 884, 
  882, 880, 878, 876, 874, 872, 870, 867, 865, 863, 861, 859, 857, 854, 852, 850, 848, 845, 843, 841, 838, 836, 834, 832, 829, 827, 
  825, 822, 820, 817, 815, 813, 810, 808, 805, 803, 800, 798, 795, 793, 790, 788, 785, 783, 780, 778, 775, 773, 770, 768, 765, 762, 
  760, 757, 755, 752, 749, 747, 744, 742, 739, 736, 734, 731, 728, 726, 723, 720, 717, 715, 712, 709, 707, 704, 701, 698, 696, 693, 
  690, 687, 684, 682, 679, 676, 673, 671, 668, 665, 662, 659, 656, 654, 651, 648, 645, 642, 639, 637, 634, 631, 628, 625, 622, 619, 
  617, 614, 611, 608, 605, 602, 599, 597, 594, 591, 588, 585, 582, 579, 576, 573, 571, 568, 565, 562, 559, 556, 553, 550, 547, 545, 
  542, 539, 536, 533, 530, 527, 524, 521, 518, 516, 513, 510, 507, 504, 501, 498, 495, 493, 490, 487, 484, 481, 478, 475, 473, 470, 
  467, 464, 461, 458, 456, 453, 450, 447, 444, 441, 439, 436, 433, 430, 427, 425, 422, 419, 416, 414, 411, 408, 405, 403, 400, 397, 
  394, 392, 389, 386, 383, 381, 378, 375, 373, 370, 367, 365, 362, 359, 357, 354, 352, 349, 346, 344, 341, 339, 336, 333, 331, 328, 
  326, 323, 321, 318, 316, 313, 311, 308, 306, 303, 301, 298, 296, 294, 291, 289, 286, 284, 282, 279, 277, 275, 272, 270, 268, 265, 
  263, 261, 258, 256, 254, 252, 249, 247, 245, 243, 241, 239, 236, 234, 232, 230, 228, 226, 224, 222, 220, 218, 216, 214, 212, 210, 
  208, 206, 204, 202, 200, 198, 196, 194, 192, 190, 189, 187, 185, 183, 181, 180, 178, 176, 174, 173, 171, 169, 168, 166, 164, 163, 
  161, 159, 158, 156, 155, 153, 152, 150, 149, 147, 146, 144, 143, 141, 140, 139, 137, 136, 134, 133, 132, 131, 129, 128, 127, 126, 
  124, 123, 122, 121, 120, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 103, 102, 101, 100, 99, 99, 
  98, 97, 96, 96, 95, 94, 94, 93, 92, 92, 91, 90, 90, 89, 89, 88, 88, 87, 87, 87, 86, 86, 85, 85, 85, 84, 84, 84, 83, 83, 83, 83, 82, 
  82, 82, 82, 82, 82, 82, 82, 81, 81, 81, 81
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
// Again, dataSize is number of SAMPLES, not number of bytes
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
}

// convert 2d coord (e.g. 0, 0) to 1d index e.g. 0
inline static uint16_t coord2dToIndex(uint16_t x, uint16_t y, uint16_t numColumns){
  return y*numColumns + x;
}

// Perform max pool on 1d array representing 2d spectrogram
// Prior size: 513x94
// New size: 171x31
void maxPool(int32_t* magnitudes, int32_t* pooledMags){
  for (uint16_t row=1; row<NUM_ROWS-1; row+=POOL_STRIDE){
    for (uint16_t col=1; col<NUM_COLUMNS-1; col+=POOL_STRIDE){
      // Row is y coordinate, column is x coordinate
      int32_t max = magnitudes[coord2dToIndex(col, row, NUM_COLUMNS)];
      
      for (uint16_t offsetX=-POOL_SIZE/2; offsetX<POOL_SIZE/2+1; offsetX++){
        for (uint16_t offsetY=-POOL_SIZE/2; offsetY<POOL_SIZE/2+1; offsetY++){
          int32_t val = magnitudes[coord2dToIndex(col+offsetY, row+offsetX, NUM_COLUMNS)];
          max = (val>max) ? val : max; 
        }
      }

      uint16_t newRow = row/POOL_SIZE;
      uint16_t newCol = col/POOL_SIZE;
      pooledMags[coord2dToIndex(newCol, newRow, POOLED_NUM_COLUMNS)] = max;
    }
  }
}

// Max pooling on memory optimized magnitudes
void maxPool(int32_t* magnitudes, int32_t* pooledMags, uint16_t iteration){
  for (uint16_t row=1; row<NUM_ROWS-1; row+=POOL_STRIDE){
    uint16_t col = POOL_SIZE/2;
    int32_t max = magnitudes[coord2dToIndex(col, row, POOL_SIZE)];

    for (uint16_t offsetX=-POOL_SIZE/2; offsetX<POOL_SIZE/2+1; offsetX++){
      for (uint16_t offsetY=-POOL_SIZE/2; offsetY<POOL_SIZE/2+1; offsetY++){
        int32_t val = magnitudes[coord2dToIndex(col+offsetY, row+offsetX, POOL_SIZE)];
        max = (val>max) ? val : max; 
      }
    }

    uint16_t newRow = row/POOL_SIZE;
    uint16_t newCol = iteration;
    pooledMags[coord2dToIndex(newCol, newRow, POOLED_NUM_COLUMNS)] = max;
  }

}



// Preprocesses data into magnitudes 
// longStore is the raw bytes data taken from hydrophone
// dataSize is the number of SAMPLES on the hydrophone, NOT the number of bytes
// cepstrumCoeffs is an (empty) buffer into which coeffs will be placed
void preprocess(char* longStore, uint32_t dataSize, int32_t* magnitudes, int32_t* pooledMags, const uint32_t iters){
  
  preEmphasis(longStore, dataSize);

  char temp_frame[FRAME_SIZE*3];
  int32_t temp_mag[FRAME_SIZE]; 
  int32_t temp_freq[FRAME_SIZE];

  // perform fft on each hamming windowed frame of length 1024
  #if !(defined(TREE_DEPLOY) || defined(TREE_TEST) || defined(PREPROCESS_TEST))
  for (uint16_t i=0; i<iters; i++){
    // Applies hamming windowing to one frame at a time, due to overlap between frames when hop length<frame size
    memcpy(temp_frame, longStore+i*HOP_LENGTH*3, FRAME_SIZE*3);
    hammingWindowing(temp_frame, FRAME_SIZE);
    
    // stores one frame's worth of frequencies and magnitudes at a time. each frame contains 1024 samples*3 bytes of data
    Approx_FFT(temp_frame, FRAME_SIZE, SAMPLE_RATE, temp_mag, temp_freq);
    
    // memcpy(magnitudes+i*(FRAME_SIZE/2+1), temp_mag, (FRAME_SIZE/2+1)*4); // 4 bytes per int

    /* memcpy stores as [FFT at t=0, FFT at t=1, ...] shape 94x513
      Code below stores as flattened spectrogram [48kHz magnitudes across all times, 46kHz magnitudes at all times, ...] shape 513x94      
    */
    for (uint16_t j=0; j<FRAME_SIZE/2+1; j++){
      magnitudes[NUM_COLUMNS*j+i] = temp_mag[FRAME_SIZE/2-j]; 
    }
  }

  // Perform max pooling 3x3, no padding, no dilation, stride 3
  #if defined(TREE_TEST) || defined(TREE_DEPLOY)
  maxPool(magnitudes, pooledMags);
  #endif // TREE_TEST || TREE_DEPLOY

  #endif // MULTITHREAD

  // Memory optimized preprocessing

  #if defined(TREE_DEPLOY) || defined(TREE_TEST) || defined(PREPROCESS_TEST)

  for (uint16_t i=0; i<iters/POOL_SIZE; i++){
    // Fully preprocesses three frames at a time and stores them into magnitudes
    for (uint16_t j=0; j<POOL_SIZE; j++){
      memcpy(temp_frame, longStore+(POOL_SIZE*i+j)*HOP_LENGTH*3, FRAME_SIZE*3);
      hammingWindowing(temp_frame, FRAME_SIZE);
    
      Approx_FFT(temp_frame, FRAME_SIZE, SAMPLE_RATE, temp_mag, temp_freq);
      
      // memcpy(magnitudes+i*(FRAME_SIZE/2+1), temp_mag, (FRAME_SIZE/2+1)*4); // 4 bytes per int

      /* memcpy stores as [FFT at t=0, FFT at t=1, ...] shape 94x513
        Code below stores as flattened spectrogram [48kHz magnitudes across all times, 46kHz magnitudes at all times, ...] shape 513x3      
      */
      for (uint16_t k=0; k<FRAME_SIZE/2+1; k++){
        magnitudes[POOL_SIZE*k+j] = temp_mag[FRAME_SIZE/2-k]; 
      }
    }
    // After preprocessing three frames, max pool into pooledMags
    maxPool(magnitudes, pooledMags, i);
    
  }

  #endif

}