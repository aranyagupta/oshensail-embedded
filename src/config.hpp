#ifndef CONFIG_H
#define CONFIG_H

/* Constant Definition */ 

// TCP/IP and UDP ports for TracIO/TracStream communication
#define TCP_PORT 5452
#define UDP_PORT 5453

// Max receive buffer length
#define TCP_BUFFER_LENGTH 32
#define LIVE_DATA_LENGTH 1024

#define BAUD_RATE 9600

// Live data header size, used to define starting point for deduction 
#define LIVE_DATA_HEADER_SIZE 18

// Sample rate and sample size required by hydrophone
#define SAMPLE_RATE 96000 
#define SAMPLE_SIZE 24

// Preprocessing constants
#define PREEMP_A 0.95f 
#define NUM_FRAMES 47 // Number of non-overlapping frames of raw audio stored in long store at once
#define FRAME_SIZE 1024 // 1024/96000 = 10.6ms frame size and power of 2 for FFT 
#define LONG_STORE_LENGTH FRAME_SIZE*NUM_FRAMES*3 // number of bytes in long store
#define HOP_LENGTH 512 // Number of samples between hops
#define NUM_ROWS (FRAME_SIZE/2 + 1) // number of rows (frequency bins) in magnitude spectrum
#define NUM_COLUMNS LONG_STORE_LENGTH / (3*HOP_LENGTH) // number of columns in magnitude spectrum
#define MAG_LENGTH NUM_ROWS*NUM_COLUMNS // length of magnitudes spectrogram
#define POOL_SIZE 3 // Max pooling kernel size
#define POOL_STRIDE 3 // Max pooling stride length
#define MAG_LENGTH_FOR_POOLING NUM_ROWS*POOL_SIZE // Mag length for memory-optimised pooling
// NB: No dilation/padding
#define POOLED_NUM_COLUMNS (NUM_COLUMNS-POOL_SIZE)/POOL_STRIDE + 1
#define POOLED_MAG_LENGTH (uint16_t) (NUM_ROWS/3) * (NUM_COLUMNS/3) // Pooled magnitudes length

// Neural net constants
#define NUM_INPUTS MAG_LENGTH // number of inputs into inference model
#define NUM_OUTPUTS 1 // number of outputs
#define TENSOR_ARENA_SIZE 1024*28 // tensor arena size - may need to be increased/decreased according to model reqs
#define THRESHOLD 0.5 // threshold for accepted guess

// Deployment options
// #define DEPLOY
// #define MULTITHREAD
// #define AI_DEPLOY
// #define TREE_DEPLOY

// #define PREPROCESS_TEST
#define ETHERNET_TEST
// #define AI_TEST
// #define TREE_TEST

const char base[192] = {
    0, 0, 14, 0, 0, 30, 0, 0, 35, 0, 0, 34, 0, 0, 34, 0, 0, 40, 0, 0, 46, 0, 0, 45, 0, 0, 30, 0, 0, 4, 255, 255, 230, 255, 255, 208, 255, 255, 201, 255, 255, 207, 255, 255, 219, 255, 255, 228, 255, 255, 232, 255, 255, 234, 255, 255, 243, 0, 0, 6, 0, 0, 32, 0, 0, 55, 0, 0, 65, 0, 0, 57, 0, 0, 38, 0, 0, 17, 0, 0, 1, 255, 255, 250, 255, 255, 245, 255, 255, 237, 255, 255, 222, 255, 255, 205, 255, 255, 195, 255, 255, 200, 255, 255, 221, 255, 255, 249, 0, 0, 18, 0, 0, 32, 0, 0, 35, 0, 0, 34, 0, 0, 35, 0, 0, 41, 0, 0, 46, 0, 0, 43, 0, 0, 26, 255, 255, 254, 255, 255, 225, 255, 255, 206, 255, 255, 201, 255, 255, 209, 255, 255, 221, 255, 255, 229, 255, 255, 232, 255, 255, 235, 255, 255, 246, 0, 0, 11, 0, 0, 37, 0, 0, 58, 0, 0, 64, 0, 0, 55, 0, 0, 34, 0, 0, 13, 255, 255, 255, 255, 255, 249
};

#endif