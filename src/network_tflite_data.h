/* Generated file - v1.0
 *
 * Original file         : nn_model.tflite
 */

#ifndef __NETWORK_TFLITE_DATA_H__
#define __NETWORK_TFLITE_DATA_H__

#include <stdint.h>
#include "config.hpp"

#if  defined(AI_DEPLOY) || defined(AI_TEST) 
#ifdef __cplusplus
extern "C"
{
#endif

extern const uint8_t g_tflm_network_model_data[];

extern const int g_tflm_network_model_data_len;

#undef TFLM_NETWORK_TENSOR_AREA_SIZE
#define TFLM_NETWORK_TENSOR_AREA_SIZE 24284

#undef TFLM_NETWORK_NAME
#define TFLM_NETWORK_NAME "network"

#undef TFLM_NETWORK_FILE_NAME
#define TFLM_NETWORK_FILE_NAME "nn_model"

#ifdef __cplusplus
}
#endif
#endif /* __NETWORK_TFLITE_DATA_H__ */
#endif // AI_DEPLOY || AI_TEST
