#include <Arduino.h>
#include "config.hpp"
#include "trac.hpp"
#include "preprocess.hpp"

#include "ai_datatypes_defines.h"
#include "ai_platform.h"
#include "network.h"
#include "network_data.h"

// #define DEPLOY
// #define PREPROCESS_TEST
// #define ETHERNET_TEST
// #define AI_TEST

// AI Test
// #include "ai_datatypes_defines.h"
// #include "ai_platform.h"
// #include "network.h"
// #include "network_data.h"


#define TREE_TEST
#include "decision_tree_68_int32.h"

// Ethernet information/setup variables
uint8_t mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x02};
IPAddress ip(169, 254, 34, 1);         // the IP address of the ethernet shield (array of 4 bytes)
EthernetServer tracIOServer(TCP_PORT); // TracIO Server Port for TCP/IP Communication
EthernetClient client;                 // TCP/IP Client for reading TracIO Data
char tracIOBuffer[TCP_BUFFER_LENGTH];  // Buffer to read client device TracIO data
EthernetUDP tracStreamServer;          // TracStream client for UDP communication
char liveData[LIVE_DATA_LENGTH];       // structure holding raw hydrophone data for one packet
char longStore[LONG_STORE_LENGTH];     // structure holding hydrophone data over several packets
uint16_t seqTracker(0);

int32_t magnitudes[FRAME_SIZE/2*NUM_FRAMES]; // spectrogram developed out of raw audio data
const uint32_t dataSize = LONG_STORE_LENGTH / 3; // Total number of samples in longStore
const uint32_t iters = dataSize / HOP_LENGTH; // Number of individual FFTs we need to store
float magFloats[FRAME_SIZE/2*NUM_FRAMES];

#ifdef AI_TEST
float inferenceResult;
ai_error ai_err;
ai_i32 nbatch;

AI_ALIGNED(4) ai_u8 activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
AI_ALIGNED(4) ai_i8 in_data[AI_NETWORK_IN_1_SIZE_BYTES];
AI_ALIGNED(4) ai_i8 out_data[AI_NETWORK_OUT_1_SIZE_BYTES];

ai_handle model = AI_HANDLE_NULL;
ai_buffer* ai_input = AI_NETWORK_IN; // Probably optional
ai_buffer* ai_output = AI_NETWORK_OUT; // Probably optional

ai_network_params ai_params = {
    AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
    AI_NETWORK_DATA_ACTIVATIONS(activations)
};
#endif //AI_TEST

void test_setup()
{
    int16_t kmax = LONG_STORE_LENGTH / (FRAME_SIZE * 3);
    for (int k = 0; k < kmax; k++)
    {
        int koffset = k * FRAME_SIZE/64 * 192;
        for (int i = 0; i < FRAME_SIZE/64; i++)
        {
            int ioffset = i * 192;
            for (int j = 0; j < 192; j++)
            {
                longStore[j + ioffset + koffset] = base[j];
            }
        }
    }
}

void intToFloatMags(int32_t* intMags, float* floatMags){
    for (int i=0; i<FRAME_SIZE/2*NUM_FRAMES; i++){
        floatMags[i]= (float) intMags[i];
        if (floatMags[i]<0) floatMags[i] = 0;
    }
}

void setup()
{
    /* Begin Serial and NICLA interrupts */
    Serial.begin(BAUD_RATE);
    Serial.println("START SETUP");

    /* Register event handlers for error, match,j and event */

#ifdef DEPLOY
    /* Setup Ethernet communication and ensure client is available */
    Ethernet.begin(mac, ip);
    while (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println("Ethernet cable is not connected.");
        delay(100);
    }

    tracIOServer.begin();
    tracStreamServer.begin(UDP_PORT);
    
    client.setConnectionTimeout(0xFFFF);

    /* Configure device using TracIO */
    // porpoiseSetup(tracIOServer, client, tracIOBuffer);
#endif

#ifndef DEPLOY

#ifdef PREPROCESS_TEST
    Serial.println("Starting to setup test");
    test_setup();
    // for (int i=0; i<LONG_STORE_LENGTH; i++){
    //     Serial.println((uint8_t)longStore[i]);
    // }
    Serial.println("Finished setting up test");
#endif // PREPROCESS_TEST

#ifdef ETHERNET_TEST

    Ethernet.begin(mac, ip);
    Serial.println("BEGAN ETHERNET");

    if (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println("Ethernet cable is not connected.");
        delay(10000);
    }
    Serial.println("LINK OK");

    tracIOServer.begin();
    Serial.println("BEGAN tracIO");
    tracStreamServer.begin(UDP_PORT);
    Serial.println("BEGAN tracStream");
    // do
    // {
    //     delay(500);
    //     client = tracIOServer.available();
    // } while (!client.connected());
    Serial.println("GOT CLIENT");
    client.setConnectionTimeout(0xFFFF);
#endif // ETHERNET_TEST

#ifdef AI_TEST
    // Handle ai_input/output
    ai_input->data = AI_HANDLE_PTR(in_data);
    ai_output->data = AI_HANDLE_PTR(out_data);

    // Create network instance
    ai_err = ai_network_create(&model, AI_NETWORK_DATA_CONFIG);
    if (ai_err.type != AI_ERROR_NONE){
        Serial.println("ERROR: Could not create NN instance");
        while (true);
    }

    // Initialise network
    if (!ai_network_init(model, &ai_params)){
        Serial.println("ERROR: Could not create NN instance");
        while (true);
    }

#endif

#endif // DEPLOY

    Serial.println("Finish setup");
}

// TODO: split into two threads
// Thread 1 - Handle UDP data communication (TracStream). Use longStore as double buffer (update values in one half)
// Thread 2 - Handle preprocessing and inference. Use longStore as double buffer (read values from other half). Once cepstralCoeffs is full, send for inference.
// Thread 3 - Handle TCP Communication (TracIO)
// Maybe find a way to split thread 1 into comms thread and inference thread? Maybe even into three (comms, preprocessing, inference)?
// Seems difficult as we'd need three copies of data for each thread, and we're already running low on space
// Perhaps not worth it energy wise either - would constantly be running something

void loop()
{
    // Serial.println("Start loop");

#ifdef DEPLOY // Deploying full communication stream
    /* Handle UDP data communication (TracStream) and send to inference model */
    receiveDatagram(tracStreamServer, liveData);
    uint16_t dataSize = 0;
    tracStatus status = parseDatagram(liveData, longStore);
    if (status == FINISH)
    {
        preprocess(longStore, dataSize, magnitudes, iters);
        // Run inference model
        memset(longStore, 0, sizeof longStore);
    }
    memset(liveData, 0, sizeof liveData);

    /* Handle TCP Communication (TracIO) */

    // Read TracIO data from client into buffer if available
    uint16_t size = client.available();
    client.readBytes(tracIOBuffer, size);

    // Decode tracIO buffer - if okay, respond with same thing
    if (decodeTracIO(tracIOBuffer) == OK)
    {
        tracIOServer.write(tracIOBuffer, size);
    }
#endif

#ifndef DEPLOY // Timing/testing system
#ifdef PREPROCESS_TEST

    uint32_t current = millis();

    preprocess(longStore, dataSize, magnitudes, iters);
    
    Serial.print(millis() - current);
    Serial.println(" ms to End");

    test_setup();

#endif // PREPROCESS_TEST

#ifdef ETHERNET_TEST

    // uint32_t current = millis();
    receiveDatagram(tracStreamServer, liveData);
    tracStatus status = parseDatagram(liveData, longStore);
    // Serial.print(millis()-current);
    // Serial.println(" ms to End");

    if (status==FINISH){
        preprocess(longStore, dataSize, magnitudes, iters);
        // for (int i=0; i<FRAME_SIZE*NUM_FRAMES; i++){
        //     int32_t val = (longStore[3*i]<<24)+(longStore[3*i+1]<<16)+(longStore[3*i+2]<<8);
        //     Serial.println(val);
        // }
        int32_t max0 = 0;
        int32_t max0mag = 0;
        int32_t max1 = 0;
        int32_t max1mag = 0;
        int32_t max2 = 0;
        int32_t max2mag = 0;
        int32_t max3 = 0;
        int32_t max3mag = 0;
        int32_t max4 = 0;
        int32_t max4mag = 0;

        for (int i=0; i<FRAME_SIZE/2*NUM_FRAMES; i++){
            float frac = (float) i / (float) (FRAME_SIZE/2*NUM_FRAMES);
            float freq = frac * SAMPLE_RATE;
            int32_t mag = magnitudes[i];
            if (mag>max0mag) {
                max0 = freq;
                max0mag = mag;
            }
            else if (mag>max1mag) {
                max1 = freq;
                max1mag = mag;
            }
            else if (mag>max2mag) {
                max2 = freq;
                max2mag = mag;
            }
            else if (mag>max3mag){
                 max3 = freq;
                 max3mag = mag;
            }
            else if (mag>max4mag) {
                max4 = freq;
                max4mag = mag;
            }
        }
        Serial.print(max0);
        Serial.print(" ");
        Serial.print(max1);
        Serial.print(" ");
        Serial.print(max2);
        Serial.print(" ");
        Serial.print(max3);
        Serial.print(" ");
        Serial.println(max4);
        memset(longStore, 0, sizeof longStore);
        // delay(1000);
    }
    else if (status!=OK){
        // Serial.print("STATUS: ");
        // Serial.println(status);
    }
    memset(liveData, 0, sizeof liveData);

#endif // ETHERNET_TEST

#ifdef AI_TEST
    for (uint32_t i=0; i< AI_NETWORK_IN_1_SIZE; i++){
        ((ai_float*) in_data)[i] = (ai_float)2.0f; // TODO: normalise magnitudes, then send
    }

    uint32_t current = micros();

    nbatch = ai_network_run(model, &ai_input[0], &ai_output[0]);
    if (nbatch != 1){
        Serial.println("Error: could not run inference");
    }

    inferenceResult = ((float*) out_data)[0];
    Serial.print("RESULT: ");
    Serial.println(inferenceResult);
    Serial.print("TIME (us): ");
    Serial.println(micros()-current);
    delay(500);

#endif // AI_TEST

#ifdef TREE_TEST
    test_setup();
    preprocess(longStore, dataSize, magnitudes, iters);
    intToFloatMags(magnitudes, magFloats);
    const int32_t predicted_class = dt_predict(magFloats, FRAME_SIZE/2*NUM_FRAMES);
    if (predicted_class){
        Serial.println("EVENT DETECTED LETS FUCKING GOOOOOOO");
    }

#endif //TREE_TEST

#endif // DEPLOY
}
