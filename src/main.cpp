#include <Arduino.h>
#include "config.hpp"
#include "trac.hpp"
#include "preprocess.hpp"
#include <EloquentTinyML.h>
#include <STM32FreeRTOS.h>
#include "network_tflite_data.h"
#include "network_config.h"
#include <FreeRTOS_IP.h>

#include "random_forest_85_float32.h"

// Ethernet information/setup variables
uint8_t mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x02};
uint8_t ip[] = {169, 254, 34, 1};
uint8_t gateway[] = {169, 254, 34, 1};
uint8_t dns[] = {169, 254, 34, 1};
uint8_t subnet[] = {255, 255, 255, 0};
// IPAddress ip(169, 254, 34, 1);         // the IP address of the ethernet shield (array of 4 bytes)
// IPAddress subnet(255, 255, 255, 0);
// EthernetServer tracIOServer(TCP_PORT); // TracIO Server Port for TCP/IP Communication
// EthernetClient client;                 // TCP/IP Client for reading TracIO Data
char tracIOBuffer[TCP_BUFFER_LENGTH];  // Buffer to read client device TracIO data
//EthernetUDP tracStreamServer;          // TracStream client for UDP communication
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
#if ! (defined(TREE_DEPLOY) || defined(TREE_TEST) || defined(PREPROCESS_TEST))
int32_t magnitudes[MAG_LENGTH]; // spectrogram developed out of raw audio data
#endif // MULTITHREAD

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
#if defined(AI_TEST) || defined(AI_DEPLOY)
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

//User-definition of this function is required buy TCP/IP stack
//This would be better if hardware-generated, but not done for now
BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber ){
    *pulNumber = rand();
    return pdTRUE;
};

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
                                             uint16_t usSourcePort,
                                             uint32_t ulDestinationAddress,
                                             uint16_t usDestinationPort )
{
    return rand();
}

void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
    if( eStatus == eSuccess )
    {
        // Handle successful ping reply
    }
    else
    {
        // Handle failed ping reply
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
    // void ethernetTask(void * pvParameters){
    //     #ifdef DEPLOY // Deploying full communication stream

    //         bool ethernetTaskBufSide = *(bool *)pvParameters;
    //         char (*longStorePtr)[LONG_STORE_LENGTH];

    //         while(1){
    //             if(ethernetTaskBufSide == 0){
    //                 xSemaphoreTake(longStore0Mutex, portMAX_DELAY);
    //                 longStorePtr = lS0ptr;
    //             }else{
    //                 xSemaphoreTake(longStore1Mutex, portMAX_DELAY);
    //                 longStorePtr = lS1ptr;
    //             }

    //             receiveDatagram(tracStreamServer, liveData);
    //             tracStatus status = parseDatagram(liveData, *longStorePtr);
    //             if (status == FINISH){
    //                 //Do  nothing, preprocessing task will take over
    //             } 
    //             else if (status==INVALID){
    //                 //set this side of double buffer to 0s
    //                 memset(*longStorePtr, 0, sizeof *longStorePtr);
    //             }
    //             memset(liveData, 0, sizeof liveData);

    //             /* Handle TCP Communication (TracIO) */

    //             // Read TracIO data from client into buffer if available
    //             uint16_t size = client.available();
    //             client.readBytes(tracIOBuffer, size);

    //             // Decode tracIO buffer - if okay, respond with same thing
    //             if (decodeTracIO(tracIOBuffer) == OK)
    //             {
    //                 tracIOServer.write(tracIOBuffer, size);
    //             }

    //             //at the end of ethernet read cycle, 
    //             //  -> switch the ptr for this task to process other side of double buffer
    //             //  -> give up the mutex to this side of double buffer
    //             if(ethernetTaskBufSide == 0){
    //                 ethernetTaskBufSide = 1;
    //                 longStorePtr = lS1ptr;
    //                 xSemaphoreGive(longStore0Mutex);
    //             }else{
    //                 ethernetTaskBufSide = 0;
    //                 longStorePtr = lS0ptr;
    //                 xSemaphoreGive(longStore1Mutex);
    //             }
    //         }
    //     #endif
    // }

    static void ethernetTask( void *pvParameters ){
        Serial.println("ethernet task");
        #ifdef DEPLOY // Deploying full communication stream
            long lBytes;
            struct freertos_sockaddr xClient, xBindAddress;
            uint32_t xClientLength = sizeof( xClient );
            Socket_t xListeningSocket;
            const TickType_t xSendTimeOut = 200 / portTICK_PERIOD_MS;

            bool ethernetTaskBufSide = *(bool *)pvParameters;
            char (*longStorePtr)[LONG_STORE_LENGTH];

            /* Attempt to open the UDP socket. */
            xListeningSocket = FreeRTOS_socket( FREERTOS_AF_INET, 
                                                    /* Use FREERTOS_AF_INET6 for IPv6 UDP socket */
                                                FREERTOS_SOCK_DGRAM,
                                                    /*FREERTOS_SOCK_DGRAM for UDP.*/
                                                FREERTOS_IPPROTO_UDP );

            /* Check for errors. */
            if(xListeningSocket == FREERTOS_INVALID_SOCKET){
                Serial.println("UDP Socket invalid, couldn't be created");
                delay(100);
            };

            /* Ensure calls to FreeRTOS_sendto() timeout if a network buffer cannot be
            obtained within 200ms. */
            FreeRTOS_setsockopt( xListeningSocket,
                                    0,
                                    FREERTOS_SO_SNDTIMEO,
                                    &xSendTimeOut,
                                    sizeof( xSendTimeOut ) );

            /* Bind the socket to UDP_PORT - defined in config.hpp. */
                memset( &xBindAddress, 0, sizeof(xBindAddress) );
                xBindAddress.sin_port = FreeRTOS_htons( UDP_PORT );
                xBindAddress.sin_family = FREERTOS_AF_INET; /* FREERTOS_AF_INET6 to be used for IPv6 */
                FreeRTOS_bind( xListeningSocket, &xBindAddress, sizeof( xBindAddress ) );

            while(1){
                if(ethernetTaskBufSide == 0){
                    xSemaphoreTake(longStore0Mutex, portMAX_DELAY);
                    longStorePtr = lS0ptr;
                }else{
                    xSemaphoreTake(longStore1Mutex, portMAX_DELAY);
                    longStorePtr = lS1ptr;
                }

                /* Receive data from the socket.  ulFlags is zero, so the standard
                    interface is used.  By default the block time is portMAX_DELAY, but it
                    can be changed using FreeRTOS_setsockopt(). */
                lBytes = FreeRTOS_recvfrom( xListeningSocket,
                                            liveData,
                                            LIVE_DATA_LENGTH,
                                            0,
                                            &xClient,
                                            &xClientLength );

                if( lBytes > 0 )
                {
                    /* Data was received and can be process here. */
                    //receiveDatagram(tracStreamServer, liveData);
                    tracStatus status = parseDatagram(liveData, *longStorePtr);
                    if (status == FINISH){
                        //Do  nothing, preprocessing task will take over
                    } 
                    else if (status==INVALID){
                        //set this side of double buffer to 0s
                        memset(*longStorePtr, 0, sizeof *longStorePtr);
                    }
                    memset(liveData, 0, sizeof liveData);
                }

                //ALL TCP/TRACIO STUFF MORE OR LESS USELESS
                /* Handle TCP Communication (TracIO) */

                // Read TracIO data from client into buffer if available
                // uint16_t size = client.available();
                // client.readBytes(tracIOBuffer, size);

                // // Decode tracIO buffer - if okay, respond with same thing
                // if (decodeTracIO(tracIOBuffer) == OK)
                // {
                //     tracIOServer.write(tracIOBuffer, size);
                // }

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
        #endif //DEPLOY
    }

    //prepreprocessing and inference task for multithreading
    void preprocessInferenceTask(void * pvParameters){
        Serial.println("preproc task");
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

                preprocess(*longStorePtr, dataSize, magnitudes, pooledMags, iters);
                    #ifdef AI_DEPLOY
                        intToFloatMags(magnitudes, magFloats);
                        normaliseFloatMags(magFloats);
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

#ifdef MULTITHREAD

//vApplicationIPNetworkEventHook called when network ready for use
void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
    Serial.println("network event hook");
    static BaseType_t xTasksAlreadyCreated = pdFALSE;

    /* Both eNetworkUp and eNetworkDown events can be processed here. */
    if( eNetworkEvent == eNetworkUp )
    {
        Serial.println("Network UP.");
        /* Create the tasks that use the TCP/IP stack if they have not already
        been created. */
        if( xTasksAlreadyCreated == pdFALSE )
        {
            //ethernetTask initiliasies this to 0, preprocTask initialises this to 1
            bool ethernetTaskBufSide = 0;
            /*
             * For convenience, tasks that use FreeRTOS-Plus-TCP can be created here
             * to ensure they are not created before the network is usable.
             */
                xTaskCreate(
                ethernetTask,                       /* Function that implements the task */
                "ethernet",                         /* Text name for the task */
                64,                                 /* Stack size in words, not bytes */ //CHANGE STACK SIZE TO MAKE SURE NOT OVERFLOWING 
                &ethernetTaskBufSide,               /* Parameter passed into the task */
                1,	                                /* Task priority */
                &ethernetHandle                     /* Pointer to store the task handle */
            );

            xTasksAlreadyCreated = pdTRUE;
        }
    }
}
#endif

void setup()
{
    /* Begin Serial and NICLA interrupts */
    Serial.begin(BAUD_RATE);
    Serial.println("START SETUP");

#ifdef MULTITHREAD
    //setup ethernet comms
    FreeRTOS_IPInit( ip,
                    subnet,
                    gateway,
                    dns,
                    mac );
    Serial.println("UDP Socket initialised");

    longStore0Mutex = xSemaphoreCreateMutex();
    longStore1Mutex = xSemaphoreCreateMutex();

    //ethernetTask initiliasies this to 0, preprocTask initialises this to 1
    bool preprocInferenceTaskBufSide = 1;

    //ethernetTask created in vApplicationIPNetworkEventHook

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
    //Ethernet.begin(mac, ip);
                     
    // while (Ethernet.linkStatus() == LinkOFF)
    // {
    //     Serial.println("Ethernet cable is not connected.");
    //     delay(100);
    // }

    if (FreeRTOS_IsNetworkUp() == pdFALSE) {
    // Network is down
        Serial.println("Network is down.");
        delay(100);
    }
    // Serial.println("HERE 1");

    //     tracIOServer.begin();
    // Serial.println("HERE 2");

    //     tracStreamServer.begin(UDP_PORT);
    // Serial.println("HERE 3");

        
    //     client.setConnectionTimeout(0xFFFF);
    // Serial.println("HERE 4");


    // porpoiseSetup(tracIOServer, client, tracIOBuffer);
    // Serial.println("HERE 5");


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

    //REMOVE
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
            Serial.println("preprocessed");
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
            Serial.println(predicted_class);
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
#endif // MULTITHREAD
}
