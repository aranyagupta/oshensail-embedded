#include <Arduino.h>
#include "config.hpp"
#include "trac.hpp"
#include "preprocess.hpp"
#include <EloquentTinyML.h>
#include "network_tflite_data.h"
#include "network_config.h"


#include "random_forest_85_float32.h"

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


const uint32_t dataSize = LONG_STORE_LENGTH / 3; // Total number of samples in longStore
const uint32_t iters = dataSize / HOP_LENGTH; // Number of individual FFTs we need to store
#if ! (defined(TREE_DEPLOY) || defined(TREE_TEST) || defined(PREPROCESS_TEST))
int32_t magnitudes[MAG_LENGTH]; // spectrogram developed out of raw audio data
#endif // ! (defined(TREE_DEPLOY) || defined(TREE_TEST) || defined(PREPROCESS_TEST))

#if  defined(TREE_DEPLOY) || defined(TREE_TEST) || defined(PREPROCESS_TEST)
int32_t magnitudes[MAG_LENGTH_FOR_POOLING];
#endif

int32_t pooledMags[POOLED_MAG_LENGTH]; // pooled magnitudes length
#if defined(TREE_TEST) || defined(TREE_DEPLOY) || defined(PREPROCESS_TEST)
float magFloats[POOLED_MAG_LENGTH]; // cast to float array
#endif
#if defined(AI_TEST) || defined(AI_DEPLOY) 
float magFloats[MAG_LENGTH];
#endif
/* 
    ENSURE magFloats LENGTH IS THE CORRECT LENGTH FOR THE CORRECT TASK 
    FOR DECISION TREE - POOLED_MAG_LENGTH
    FOR NN - MAG_LENGTH
*/ 


uint32_t current; // timing

float inferenceResult;
#if defined(AI_DEPLOY) || defined(AI_TEST)
Eloquent::TinyML::TfLite<NUM_INPUTS, NUM_OUTPUTS, TENSOR_ARENA_SIZE> tf;
#endif

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

// cast integer to float
// helper because spectrogram from fft is in ints but we need floats for NN/DT
void intToFloatMags(int32_t* intMags, float* floatMags, uint16_t length){
    for (int i=0; i<length; i++){
        floatMags[i]= (float) intMags[i];
        if (floatMags[i]<0) floatMags[i] = 0;
    }
}

// normalise magnitudes to between 0 and 1
void normaliseFloatMags(float* floatMags, uint16_t length){
    float max = floatMags[0];
    for (uint16_t i=1; i<length; i++){
        if (floatMags[i]>max) {
            max = floatMags[i];
        }
    }

    for (uint16_t i=0; i<length; i++){
        floatMags[i] = floatMags[i]/max;
    }
}

// scale integers between 0 and 0x7fffffff
// not currently used as DT takes in normalised values, not scaled values
void scaleIntMags(int32_t* magnitudes){
    float max = (float) magnitudes[0];
    float min = (float) magnitudes[0];
    for (uint16_t i=1; i<MAG_LENGTH; i++){
        if ((float) magnitudes[i]>max){
            max = (float) magnitudes[i];
        }
        if ((float) magnitudes[i]<min){
            min = (float) magnitudes[i];
        }
    }

    for (uint16_t i=0; i<MAG_LENGTH; i++){
        float val = ((float) magnitudes[i] - min)/(max-min);
        magnitudes[i] = (int32_t) (val*0x7fffffff);
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
    Serial.println("HERE 1");

    tracIOServer.begin();
    Serial.println("HERE 2");

    tracStreamServer.begin(UDP_PORT);
    Serial.println("HERE 3");

    
    client.setConnectionTimeout(0xFFFF);
    Serial.println("HERE 4");


    // porpoiseSetup(tracIOServer, client, tracIOBuffer);
    Serial.println("HERE 5");


    // Begin tf instance with model data
    #if defined(AI_DEPLOY) || defined(AI_TEST)
    tf.begin(g_tflm_network_model_data);
    #endif // AI_DEPLOY || AI_TEST

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

    while (Ethernet.linkStatus() == LinkOFF)
    {
        Serial.println("Ethernet cable is not connected.");
        delay(1000);
    }
    Serial.println("LINK OK");

    // tracIOServer.begin();
    // Serial.println("BEGAN tracIO");
    tracStreamServer.begin(UDP_PORT);
    Serial.println("BEGAN tracStream");
    // do
    // {
    //     delay(500);
    //     client = tracIOServer.available();
    // } while (!client.connected());
    // Serial.println("GOT CLIENT");
    // client.setConnectionTimeout(0xFFFF);
    // porpoiseSetup(tracIOServer, client, tracIOBuffer);
#endif // ETHERNET_TEST

#ifdef AI_TEST
    // Handle ai_input/output
    tf.begin(g_tflm_network_model_data);

#endif // AI_TEST

#endif // DEPLOY

    Serial.println("Finish setup");
    current = micros();

}

// TODO: split into two threads
// Thread 1 - Handle UDP data communication (TracStream). Use longStore as double buffer (update values in one half)
// Thread 2 - Handle preprocessing and inference. Use longStore as double buffer (read values from other half). Once cepstralCoeffs is full, send for inference.

void loop()
{
#ifdef DEPLOY // Deploying full communication stream
    receiveDatagram(tracStreamServer, liveData);
    tracStatus status = parseDatagram(liveData, longStore);
    if (status == FINISH)
    {
        preprocess(longStore, dataSize, magnitudes, pooledMags, iters);
    #ifdef AI_DEPLOY
        intToFloatMags(magnitudes, magFloats, MAG_LENGTH);
        normaliseFloatMags(magFloats, MAG_LENGTH);
        inferenceResult = tf.predict(magFloats);
        if (inferenceResult>THRESHOLD) {
            Serial.println("EVENT DETECTED");
        }
    #endif // AI_DEPLOY
    #ifdef TREE_DEPLOY
        intToFloatMags(pooledMags, magFloats, POOLED_MAG_LENGTH);
        normaliseFloatMags(magFloats, POOLED_MAG_LENGTH);
        int32_t predicted_class = dt_predict(magFloats, POOLED_MAG_LENGTH);
        if (predicted_class){
            Serial.println("EVENT DETECTED");
        }
    #endif // TREE_DEPLOY
        memset(longStore, 0, sizeof longStore);
    }
    else if (status==INVALID){
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
    preprocess(longStore, dataSize, magnitudes, pooledMags, iters);
    intToFloatMags(pooledMags, magFloats, POOLED_MAG_LENGTH);
    normaliseFloatMags(magFloats, POOLED_MAG_LENGTH);
    
    Serial.print(millis() - current);
    Serial.println(" ms to End");

    test_setup();

#endif // PREPROCESS_TEST

#ifdef ETHERNET_TEST

    receiveDatagram(tracStreamServer, liveData);
    tracStatus status = parseDatagram(liveData, longStore);
    if (status==FINISH){
        Serial.print(micros()-current);
        Serial.println(" us to End");
        current = micros();
    }
    memset(liveData, 0, sizeof liveData);

#endif // ETHERNET_TEST

#ifdef AI_TEST
    test_setup();
    preprocess(longStore, dataSize, magnitudes, pooledMags, iters);
    intToFloatMags(magnitudes, magFloats, MAG_LENGTH);
    normaliseFloatMags(magFloats, MAG_LENGTH);

    uint32_t current = micros();
    inferenceResult = tf.predict(magFloats);
    uint32_t now = micros();
    Serial.print("RESULT: ");
    Serial.println(inferenceResult);
    Serial.print("TIME (us): ");
    Serial.println(now-current);

#endif // AI_TEST

#ifdef TREE_TEST
    test_setup();
    preprocess(longStore, dataSize, magnitudes, pooledMags, iters);
    intToFloatMags(pooledMags, magFloats, POOLED_MAG_LENGTH);
    normaliseFloatMags(magFloats, POOLED_MAG_LENGTH);

    uint32_t current = micros();
    const int32_t predicted_class = dt_predict(magFloats, POOLED_MAG_LENGTH);
    uint32_t now = micros();
    Serial.print("TIME (us): ");
    Serial.println(now-current);
    if (predicted_class){
        Serial.println("EVENT DETECTED");
    }

#endif //TREE_TEST

#endif // DEPLOY
}
