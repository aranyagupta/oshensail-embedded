# Implementation details

## Communication
Trac communication is carried out using prewritten LwIP and Ethernet modules found on PlatformIO. TracIO commands have been partially implemented to configure the Porpoise, and TracStream packet reception has been fully implemented to actually receive data. We initially wanted to use TracIO to configure it directly, but found that it wasn't necessary as the Trac software supplied by RS Aqua can be used to configure it once. The required and sufficient settings are:
- 96kHz sampling Rate
- PCM24 data format
- Data streaming turned on 

The Porpoise streams PCM 24 data, which is a signed 24 bit data format. TracStream sends each sample as char/uint8 data, with every three chars acting as one sample. Each packet sends either 255 or 333 samples, but since we are storing almost 48000 samples at once, we repeatedly read from ethernet until the longStore buffer is filled, at which point we preprocess and infer on that data before starting to read again. Eventually, these events will happen simultaneously with multithreading.  

## Preprocessing
Preprocessing is applied to raw audio data to convert it into a spectrogram format which is passed into the inference model. 

### Pre-emphasis
Pre-emphasis is applied by performing the operation $X(n) = S(n) - 0.95 * S(n-1)$ on the raw audio data, where S(n) is the raw audio data and X(n) is the emphasised audio. To preserve space, the original array of raw audio data (signed PCM24 stored as chars/uint8) is modified directly rather than using another array, which is achieved through software pipelining (also has a marginal speed benefit to moving to another array). 

### Hamming windowing
The audio data is logically split up into frames of 1024 samples each (no actual decomposing of the array occurs), and each window is multiplied by a set of hamming coefficients to make spikes in the spectrograms more visible. The hamming coefficients are pre-calculated to save calculation time, and a python file has been supplied to calculate the coefficients if the frame size changes. This operation is quite trivial, complicated only by the fact that each sample is stored as three consecutive chars/uint8s, so they need to be combined into an int32_t before being processed. There is a 512 sample gap (known as hop length) between frames, so frame 0 is samples 0 to 1023, frame 1 is samples 512 to 1535, etc.

### FFT
A Fast-Fourier Transform (FFT) is applied to each frame to produce a set of 129 frequency magnitudes. This is achieved by the ApproxFFT function, made by [this user](https://projecthub.arduino.cc/abhilashpatel121/approxfft-fastest-fft-function-for-arduino-f1b6ba) and modified slightly to account for the PCM24 format. After being applied to each frame, it is stored into a larger spectrogram (magnitudes) for inference. Only three frames are stored at once to optimise memory usage as max pooling pooling is performed. 

### Max Pooling
We apply 3x3 max pooling with stride 3 on the magnitudes spectrogram. This is stored in the `pooledMags` array to be sent for inference. This is the reason only three frames are stored at once for the FFT - we can perform 3x3 max pooling on these three frames, store them in `pooledMags`, then apply a FFT on the next set of three frames. This is done to optimise memory usage, as now we have enough memory to implement a double buffer technique while using multithreading.

## Inference
We have implemented both TFLite neural networks using EloquentTinyML library and decision trees using EML. The required files for both are already available. We are currently only *deploying* the decision tree as it has the best performace; however, if a superior neural network is found, it can be implemented using details below.  

### Neural Network
We use the TFLite Micro framework and the EloquentTinyML library to perform inference using a trained fully connected neural network. If you have trained your own network and want to deploy it, do the following:
- Download STM32CubeMX, a platform that allows you to convert TFLite files to a format recognised by microcontrollers. Set it up for the Nucleo-F767ZI board and download the X-CUBE-AI add-on. 
- Navigate to the X-CUBE-AI software package and select the .tflite file needed. Make sure you use the TFLite Micro runtime and not the STM32Cube.AI MCU runtime to generate the right files.
- Generate the project code, then go to the X-CUBE-AI/App directory in the generated project. Copy the `network.c`, `network_tflite_data.h` and `network_config.h` files from that directory to this src directory. 
- Adjust `NUM_INPUTS`, `NUM_OUTPUTS` and `TENSOR_ARENA_SIZE` in `config.h` to the values required by the model. `NUM_INPUTS` and `NUM_OUTPUTS` depend on the input and output size of the model you trained. `TENSOR_ARENA_SIZE` needs to be fiddled around with to find the correct size; if you make it really small and try to deploy it, an error will print to serial telling you the required size.
- Adjust the inference method in main to whatever is necessary. Currently it stores just a float telling us the probability that the current spectrogram has an event and thresholds it at 0.5 to give a result.

### Decision trees and Random Forests

We use the [emlearn](https://github.com/emlearn/emlearn) library to convert sklearn tree models into C code.

```[python]
import emlearn
import joblib

# Save the model using joblib (emlearn prefers joblib, can also load models later)
joblib.dump(tree_estimator, 'path/model.joblib')

# # Load the model using emlearn
model = joblib.load('path/model.joblib')

c_model = emlearn.convert(model, method='inline')

# Save the C code to a file
c_model.save(file='path/model.h', name='tree')
```

## Notes

Constants are defined in the [config.hpp](config.hpp) file. These constants and the rest of the codebase have been set up such that changing these constants (for example, to increase the size of spectrograms/amount of raw audio data stored) should be possible and easy, allowing models to become larger or smaller quickly and easily. Limited testing has been done and it does appear to work, but no promises!

Individual tests for ethernet communication, neural networks, decision trees and preprocessing can also be found in the config file, and there are preprocessed constants available to test each part individually before deploying. To test, comment out `#define DEPLOY`, `#define AI_DEPLOY` and `#TREE_DEPLOY`, and uncomment the required test defines to test individually.

Rate-monotonic scheduling analysis has been done using the individual tests, and the following performance is found. 

Neural network:

| Task                        | Initiation Interval | Execution time | $\lceil \frac{\tau_n}{\tau_i} \rceil T_i$ | $\frac{T_i}{\tau_i} $|
|-----------------------------|:-------------------:|:--------------:|:-----------------------------------------:|:----------:|
| Preprocessing and Inference |         750         | 83.245         | 83.245                                    | 0.1110     |
|      UDP Communication      |         750         | 627.673        | 627.673                                   | 0.8369     |
|             Sum             |                     |                |                  710.918                  |   0.9479   |

Decision Tree:
| Task                        | Initiation Interval | Execution time | $\lceil \frac{\tau_n}{\tau_i} \rceil T_i$ | $\frac{T_i}{\tau_i} $|
|-----------------------------|:-------------------:|:--------------:|:-----------------------------------------:|:----------:|
| Preprocessing and Inference |         750         | 83.245         | 83.245                                    | 0.1110     |
|      UDP Communication      |         750         | 627.673        | 627.673                                   | 0.8369     |
|             Sum             |                     |                |                  710.918                  |   0.9479   |

These results show that, as long as we have an initiation interval above 710.918ms, this model is deployable in a multithreaded fashion. However, due to the udp communication taking 627ms to process and store around 500ms of audio, we will consistently lose around 127 ms of audio in between samples. While unfortunate, this is unavoidable unless deployed on a microcontroller with a higher clock rate. 
