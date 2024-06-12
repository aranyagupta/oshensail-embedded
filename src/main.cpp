#include <Arduino.h>
#include "config.hpp"
#include "trac.hpp"
#include "preprocess.hpp"
#include <EloquentTinyML.h>
#include <STM32FreeRTOS.h>
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
#ifndef MULTITHREAD
char longStore[LONG_STORE_LENGTH];     // structure holding hydrophone data over several packets
#endif

uint16_t seqTracker(0);

#ifdef MULTITHREAD 
TaskHandle_t ethernetHandle = NULL;             //task and mutex handles for multithread
TaskHandle_t preprocessInferenceHandle = NULL;
SemaphoreHandle_t longStore0Mutex;
SemaphoreHandle_t longStore1Mutex;

char longStore0[LONG_STORE_LENGTH];     // structure holding hydrophone data over several packets - 1st half of double buffer
char longStore1[LONG_STORE_LENGTH];     // structure holding hydrophone data over several packets - 2nd half of double buffer
char (*lS0ptr)[LONG_STORE_LENGTH] = &longStore0; //pointers to each side of double buffer
char (*lS1ptr)[LONG_STORE_LENGTH] = &longStore1;
#endif

const uint32_t dataSize = LONG_STORE_LENGTH / 3; // Total number of samples in longStore
const uint32_t iters = dataSize / HOP_LENGTH; // Number of individual FFTs we need to store
int32_t magnitudes[MAG_LENGTH]; // spectrogram developed out of raw audio data
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
Eloquent::TinyML::TfLite<NUM_INPUTS, NUM_OUTPUTS, TENSOR_ARENA_SIZE> tf;


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
                #ifdef MULTITHREAD
                    longStore0[j + ioffset + koffset] = base[j];
                #endif
                #ifndef MULTITHREAD
                    longStore[j + ioffset + koffset] = base[j];
                #endif
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

#ifdef MULTITHREAD
    //ethernet comms task for multithreading
    void ethernetTask(void * pvParameters){
        #ifdef DEPLOY // Deploying full communication stream

            bool ethernetTaskBufSide = *(bool *)pvParameters;
            char (*longStorePtr)[LONG_STORE_LENGTH];

            while(1){
                if(ethernetTaskBufSide == 0){
                    xSemaphoreTake(longStore0Mutex, portMAX_DELAY);
                    longStorePtr = lS0ptr;
                }else{
                    xSemaphoreTake(longStore1Mutex, portMAX_DELAY);
                    longStorePtr = lS1ptr;
                }

                receiveDatagram(tracStreamServer, liveData);
                tracStatus status = parseDatagram(liveData, *longStorePtr);
                if (status == FINISH){
                    //Do  nothing, preprocessing task will take over
                } 
                else if (status==INVALID){
                    //set this side of double buffer to 0s
                    memset(*longStorePtr, 0, sizeof *longStorePtr);
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

                //at the end of ethernet read cycle, 
                //  -> switch the ptr for this task to process other side of double buffer
                //  -> give up the mutex to this side of double buffer
                if(ethernetTaskBufSide == 0){
                    ethernetTaskBufSide = 1;
                    longStorePtr = lS1ptr;
                    xSemaphoreGive(longStore0Mutex);
                }else{
                    ethernetTaskBufSide = 0;
                    longStorePtr = lS0ptr;
                    xSemaphoreGive(longStore1Mutex);
                }
            }
        #endif
    }

    //prepreprocessing and inference task for multithreading
    void preprocessInferenceTask(void * pvParameters){
        #ifdef DEPLOY // Deploying full communication stream

            bool preprocInferenceTaskBufSide = *(bool *)pvParameters;
            char (*longStorePtr)[LONG_STORE_LENGTH];

            while(1){
                if(preprocInferenceTaskBufSide == 0){
                    xSemaphoreTake(longStore0Mutex, portMAX_DELAY);
                    longStorePtr = lS0ptr;
                }else{
                    xSemaphoreTake(longStore1Mutex, portMAX_DELAY);
                    longStorePtr = lS1ptr;
                }

                preprocess(*longStorePtr, dataSize, magnitudes, iters);
                    #ifdef AI_DEPLOY
                        intToFloatMags(magnitudes, magFloats);
                        normaliseFloatMags(magFloats);
                        inferenceResult = tf.predict(magFloats);
                        if (inferenceResult>THRESHOLD) {
                            Serial.println("EVENT DETECTED");
                        }
                    #endif // AI_DEPLOY
                    #ifdef TREE_DEPLOY
                        intToFloatMags(magnitudes, magFloats, POOLED_MAG_LENGTH);
                        normaliseFloatMags(magFloats, POOLED_MAG_LENGTH);
                        int32_t predicted_class = dt_predict(magFloats, POOLED_MAG_LENGTH);
                        if (predicted_class){
                            Serial.println("EVENT DETECTED");
                        }
                    #endif // TREE_DEPLOY
                
                memset(*longStorePtr, 0, sizeof *longStorePtr);

                if(preprocInferenceTaskBufSide == 0){
                    preprocInferenceTaskBufSide = 1;
                    longStorePtr = lS1ptr;
                    xSemaphoreGive(longStore0Mutex);
                }else{
                    preprocInferenceTaskBufSide = 0;
                    longStorePtr = lS0ptr;
                    xSemaphoreGive(longStore1Mutex);
                }
            }
        #endif
    }
#endif


void setup()
{
    /* Begin Serial and NICLA interrupts */
    Serial.begin(BAUD_RATE);
    Serial.println("START SETUP");

#ifdef MULTITHREAD
    longStore0Mutex = xSemaphoreCreateMutex();
    longStore1Mutex = xSemaphoreCreateMutex();

    bool ethernetTaskBufSide = 0;
    bool preprocInferenceTaskBufSide = 1;

    xTaskCreate(
        ethernetTask,                       /* Function that implements the task */
        "ethernet",                         /* Text name for the task */
        64,                                 /* Stack size in words, not bytes */ //CHANGE STACK SIZE TO MAKE SURE NOT OVERFLOWING 
        &ethernetTaskBufSide,               /* Parameter passed into the task */
        1,	                                /* Task priority */
        &ethernetHandle                     /* Pointer to store the task handle */
    );

    xTaskCreate(
        preprocessInferenceTask,            /* Function that implements the task */
        "preprocess+inference",             /* Text name for the task */
        64,                                 /* Stack size in words, not bytes */ //CHANGE STACK SIZE TO MAKE SURE NOT OVERFLOWING 
        &preprocInferenceTaskBufSide,            /* Parameter passed into the task */
        1,	                                /* Task priority */
        &preprocessInferenceHandle          /* Pointer to store the task handle */
    );
#endif
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
    tf.begin(g_tflm_network_model_data);

#endif // AI_TEST

#endif // DEPLOY

    Serial.println("Finish setup");
    current = micros();

#ifdef MULTITHREAD
    vTaskStartScheduler();
#endif
}

// TODO: split into two threads
// Thread 1 - Handle UDP data communication (TracStream). Use longStore as double buffer (update values in one half)
// Thread 2 - Handle preprocessing and inference. Use longStore as double buffer (read values from other half). Once cepstralCoeffs is full, send for inference.

void loop()
{
#ifndef MULTITHREAD
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
            intToFloatMags(magnitudes, magFloats, POOLED_MAG_LENGTH);
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


            // preprocess(longStore, dataSize, magnitudes, iters);
            // for (int i=0; i<FRAME_SIZE*NUM_FRAMES; i++){
            //     int32_t val = (longStore[3*i]<<24)+(longStore[3*i+1]<<16)+(longStore[3*i+2]<<8);
            //     Serial.println(val);
            // }
            // int32_t max0 = 0;
            // int32_t max0mag = 0;
            // int32_t max1 = 0;
            // int32_t max1mag = 0;
            // int32_t max2 = 0;
            // int32_t max2mag = 0;
            // int32_t max3 = 0;
            // int32_t max3mag = 0;
            // int32_t max4 = 0;
            // int32_t max4mag = 0;

            // for (int i=0; i<FRAME_SIZE/2*NUM_FRAMES; i++){
            //     float frac = (float) i / (float) (FRAME_SIZE/2*NUM_FRAMES);
            //     float freq = frac * SAMPLE_RATE;
            //     int32_t mag = magnitudes[i];
            //     if (mag>max0mag) {
            //         max0 = freq;
            //         max0mag = mag;
            //     }
            //     else if (mag>max1mag) {
            //         max1 = freq;
            //         max1mag = mag;
            //     }
            //     else if (mag>max2mag) {
            //         max2 = freq;
            //         max2mag = mag;
            //     }
            //     else if (mag>max3mag){
            //          max3 = freq;
            //          max3mag = mag;
            //     }
            //     else if (mag>max4mag) {
            //         max4 = freq;
            //         max4mag = mag;
            //     }
            // }
            // Serial.print(max0);
            // Serial.print(" ");
            // Serial.print(max1);
            // Serial.print(" ");
            // Serial.print(max2);
            // Serial.print(" ");
            // Serial.print(max3);
            // Serial.print(" ");
            // Serial.println(max4);
            // memset(longStore, 0, sizeof longStore);
            // delay(1000);
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
#endif // MULTITHREAD
}
