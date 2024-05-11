 /* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "main_functions.h"

#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "person_detect_model_data.h"
#include "arrow_classification.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "esp_main.h"
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_camera.h"
#include <lwip/ip4_addr.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define TAG "VISIO"
#define SSID "VISIO_V1.1"
#define PASSWORD "VISIO_V1.1"

// Globals, used for compatibility with Arduino-style sketches.
namespace {
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;

const tflite::Model* model_classification = nullptr;
tflite::MicroInterpreter* interpreter_classification = nullptr;
TfLiteTensor* input_classification = nullptr;

int64_t fr_start;

int FomoDetConfidence =-1, FomoPosX =-1, FomoPosY =-1;
int classRight = -1; int classLeft = -1;

size_t prev_jpg_buf_len;
int temp = 128*1024;
uint8_t * prev_jpg_buf = (uint8_t *)heap_caps_malloc(temp,MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);


static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}


// In order to use optimized tensorflow lite kernels, a signed int8_t quantized
// model is preferred over the legacy unsigned model format. This means that
// throughout this project, input images must be converted from unisgned to
// signed format. The easiest and quickest way to convert from unsigned to
// signed 8-bit integers is to subtract 128 from the unsigned value to get a
// signed value.

#ifdef CONFIG_IDF_TARGET_ESP32S3
constexpr int scratchBufSize = 39 * 1024;
#else
constexpr int scratchBufSize = 0;
#endif
// An area of memory to use for input, output, and intermediate arrays.
constexpr int kTensorArenaSize = 250 * 1024 + scratchBufSize;
constexpr int kTensorArenaSize_classification = 400 * 1024 + scratchBufSize;
static uint8_t *tensor_arena;//[kTensorArenaSize]; // Maybe we should move this to external
static uint8_t *tensor_arena_classification;
}  // namespace


float A[288];
bool tostop= false;

//turn decisions
int right_left_class_threshold = 85;
int left_right_detections = 0;
int num_arrow_threshold = 2; // value to be reached to decide if turn to be taken
int turn_decision = 0 ; // 0 if no turn , 1 is left , 2 is right
int send_turn_decision = 0; // holds prev turn decision till it sent to the server
int turn_decisions_sent = 0; // number of turn decisions sent to the server
long long turn_decision_time = esp_timer_get_time()+10000000; // will be implemented later



esp_err_t camera_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;
    int64_t fr_str = esp_timer_get_time();

    //fb = esp_camera_fb_get();
    /*if (!fb) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }*/

    res = httpd_resp_set_type(req, "image/jpeg");
    if (res == ESP_OK) {
        res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    }
     printf("here1");
    if (res == ESP_OK) {
        /*if (fb-> format == PIXFORMAT_JPEG) {
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *) fb->buf, fb->len);
             printf("here2");
        } else {*/
            jpg_chunking_t jchunk = {req, 0};
            //size_t _jpg_buf_len;
            //uint8_t * _jpg_buf;
            //char * part_buf[129*1024];
            //static int64_t last_frame = 0;
            
             //printf("here3");
            //res = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len) ? ESP_OK : ESP_FAIL;
            //printf("\n\n&u\n\n",_jpg_buf_len);
            //fb_len = jchunk.len;
            //httpd_resp_send_chunk(req, NULL, 0);
            //httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            MicroPrintf("sedning image with buf len %d",prev_jpg_buf_len);
            httpd_resp_send(req, (const char *)prev_jpg_buf, prev_jpg_buf_len);
        //}
    }

    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    //ESP_LOGI(TAG, "JPEG Captured: %ukB %ums", (uint32_t) (fb_len/1024), (uint32_t) ((fr_end - fr_start)/1000));
    return res;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        //ESP_LOGI(TAG, "station join, AID=%d",event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        //ESP_LOGI(TAG, "station leave, AID=%d",event->aid);
    }
}

/*
int FomoDetConfidence =-1, FomoPosX =-1, FomoPosY =-1;
int classRight = -1; int classLeft = -1;
turn =1 left, turn =2 right;
*/ 


esp_err_t mpu_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/csv");
    printf("mpu found ______________\n");
    if(send_turn_decision!=0) turn_decisions_sent++;
    if(turn_decisions_sent>5){
      send_turn_decision = 0;
      turn_decisions_sent = 0;
    }
    char resp[512];
    sprintf(resp, "%d,%d,%d,%d,%d,%d \n",FomoDetConfidence,FomoPosX,FomoPosY,classRight,classLeft,send_turn_decision);
    //sprintf(resp, "hellp \n");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_uri_t mpu_uri = {
    .uri      = "/mpu",
    .method   = HTTP_GET,
    .handler  = mpu_handler,
    .user_ctx = NULL
};
httpd_uri_t image_uri = {
    .uri      = "/image",
    .method   = HTTP_GET,
    .handler  = camera_handler,
    .user_ctx = NULL
};

/*
int FomoDetConfidence =-1, FomoPosX =-1, FomoPosY =-1;
int classRight = -1; int classLeft = -1;
*/

void storeDetection(int Fomocon, int Fomox, int Fomoy, int classR, int classL){
  prev_jpg_buf_len = copy_jpg_buf_len;
  MicroPrintf("storing image with buf len %d",prev_jpg_buf_len);
  for(int i =0 ; i < copy_jpg_buf_len; i++)
  {
    prev_jpg_buf[i] = copy_jpg_buf[i]; 
  }
  FomoDetConfidence = Fomocon;
  FomoPosX = Fomox;
  FomoPosY = Fomoy;
  classRight = classR;
  classLeft = classL;

}

int max(int a, int b){
  if(a>b) return a;
  return b;
}
int min(int a, int b){
  if(a<b) return a;
  return b;
}

// The name of this function is important for Arduino compatibility.
void setup() {
  //httpd_register_uri_handler("/image", camera_handler);
  //write a function to initialize the camera uri
  //httpd_register_uri_handler("/mpu", mpu_handler);
  wifi_config_t wifi_config = {
        .ap = {
            .ssid = SSID,
            .password = PASSWORD,
            .ssid_len = strlen(SSID),
            .channel = 1,
            .authmode = WIFI_AUTH_WPA2_WPA3_PSK,
            .max_connection = 4,
            
        },
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t * p_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
  
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

  

    esp_netif_ip_info_t if_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(p_netif, &if_info));
    ESP_LOGI(TAG, "ESP32 IP:" IPSTR, IP2STR(&if_info.ip));

    esp_netif_ip_info_t ipInfo;
    IP4_ADDR(&ipInfo.ip, 192,168,1,1);
    IP4_ADDR(&ipInfo.gw, 192,168,1,1);
    IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
    esp_netif_dhcps_stop(p_netif);
    esp_netif_set_ip_info(p_netif, &ipInfo);
    esp_netif_dhcps_start(p_netif);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    config.core_id =1;
    config.send_wait_timeout=1;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &mpu_uri);
        httpd_register_uri_handler(server, &image_uri);
    }


  size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_8BIT);
  printf("PSRAM size: %d bytes\n", psram_size);
  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_person_detect_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    MicroPrintf("Model provided is schema version %d not equal to supported "
                "version %d.", model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  model_classification = tflite::GetModel(g_arrow_classify_model_data);
  if (model_classification->version() != TFLITE_SCHEMA_VERSION) {
    MicroPrintf("Model provided is schema version %d not equal to supported "
                "version %d.", model_classification->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  if (tensor_arena == NULL) {
    psram_size = heap_caps_get_largest_free_block( MALLOC_CAP_8BIT);
    printf("PSRAM size: %d bytes\n", psram_size);
    tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize,  MALLOC_CAP_8BIT);
  }
  if (tensor_arena == NULL) {
    printf("Couldn't allocate memory of %d bytes\n", kTensorArenaSize);
    return;
  }

  if (tensor_arena_classification == NULL) {
    psram_size = heap_caps_get_largest_free_block( MALLOC_CAP_8BIT);
    printf("PSRAM size: %d bytes\n", psram_size);
    tensor_arena_classification = (uint8_t *) heap_caps_malloc(kTensorArenaSize_classification,  MALLOC_CAP_8BIT);
  }
  if (tensor_arena_classification == NULL) {
    printf("Couldn't allocate memory of %d bytes\n", kTensorArenaSize_classification);
    return;
  }
  

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  //
  // tflite::AllOpsResolver resolver;
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroMutableOpResolver<8> micro_op_resolver;
  micro_op_resolver.AddAveragePool2D();
  micro_op_resolver.AddConv2D();
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddReshape();
  micro_op_resolver.AddSoftmax();
  micro_op_resolver.AddPad();
  micro_op_resolver.AddAdd();
  micro_op_resolver.AddFullyConnected();

   static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  printf("here\n");



  // Build an interpreter to run the model with.
  // NOLINTNEXTLINE(runtime-global-variables)
 

  static tflite::MicroInterpreter static_interpreter_classification(
      model_classification, micro_op_resolver, tensor_arena_classification, kTensorArenaSize_classification);
  interpreter_classification = &static_interpreter_classification;

  printf("here2\n");

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    MicroPrintf("AllocateTensors() failed");
    return;
  }

  TfLiteStatus allocate_status2 = interpreter_classification->AllocateTensors();
  if (allocate_status2 != kTfLiteOk) {
    MicroPrintf("AllocateTensors() for classification mdoel failed");
    return;
  }

  // Get information about the memory area to use for the model's input.
  input = interpreter->input(0);
  input_classification = interpreter_classification->input(0);

#ifndef CLI_ONLY_INFERENCE
  // Initialize Camera
  TfLiteStatus init_status = InitCamera();
  if (init_status != kTfLiteOk) {
    MicroPrintf("InitCamera failed\n");
    return;
  }
#endif
 fr_start = esp_timer_get_time();
}

#ifndef CLI_ONLY_INFERENCE
// The name of this function is important for Arduino compatibility.

float model_class_output(TfLiteTensor* output_classification , int pos){
    //arrow_score_right -> arrow_score
    int8_t arrow_score = output_classification->data.uint8[pos];
    float arrow_score_f =
      (arrow_score - output_classification->params.zero_point) * output_classification->params.scale;
    float arrow_score_int = (arrow_score_f)*100;
    return arrow_score_int;
    
}


//int left_right =0;
int loopno = 0;
bool fomo_detect = false;
bool fomo_last_detect_time = 0;
void loop() {
  int tempFomoDetConfidence =-1, tempFomoPosX =-1, tempFomoPosY =-1;
  int tempclassRight = -1; int tempclassLeft = -1;
 // MicroPrintf("size of int %d",sizeof(int));
  //MicroPrintf("size of float %d",sizeof(float));

  // Get image from provider.
  if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, input->data.int8)) {
    MicroPrintf("Image capture failed.");
  }
  loopno++;
  if(loopno%3!=0 && left_right_detections==0){
    return;
  }
  loopno = 0;
  // Run the model on this input and make sure it succeeds.
  if(left_right_detections==0){

  
    if (kTfLiteOk != interpreter->Invoke()) {
      MicroPrintf("Invoke failed.");
    }
    TfLiteTensor* output = interpreter->output(0);
    //MicroPrintf("output type %d", (output->bytes));
    int x_cordi = 48;
    int y_cordi = 48;
    
    bool possible_arrow = false;
    
    for(int i = 1; i <288  ;i=i+2){
      int8_t person_score = output->data.uint8[i];
      float person_score_f =
        (person_score - output->params.zero_point) * output->params.scale;
      float person_score_int = (person_score_f)*100;

      if(i%2==1  &&person_score_int>90){
        tostop = true;
        MicroPrintf("arrow detected at %d :%f%%",i,
                person_score_int);
        tempFomoDetConfidence = person_score_int;
        int pos = (i-1)/2;
        int y = pos/12;
        int x = pos%12;
        tempFomoPosX = x;
        tempFomoPosY = y;
        Get_cordi(x*8,y*8);

      }
    }
  }
  
  float arrow_score_right_int = 0;
  float arrow_score_left_int = 0;
  float arrow_score_far_int = 0;
  if(tostop||true ){
    MicroPrintf("arrow detected ");
    //trying
    if (kTfLiteOk != GetImage(kNumCols, kNumRows, kNumChannels, input_classification->data.int8)) {
      MicroPrintf("Image capture failed.");
    }

    // Run the model on this input and make sure it succeeds.
    if (kTfLiteOk != interpreter_classification->Invoke()) {
      MicroPrintf("Invoke failed.");
    }

    TfLiteTensor* output_classification = interpreter_classification->output(0);
    //MicroPrintf("output classification type %d", (output_classification->bytes));

    //r = 2nd pos , l = 0th pos, f = 1/3st pos
    arrow_score_right_int = model_class_output(output_classification ,2);
    arrow_score_left_int = model_class_output(output_classification ,0);
    MicroPrintf("r:%f , l : %f",arrow_score_right_int,arrow_score_left_int);

    //only check arrow values if time since last decision is greater than 3 seconds 
    long long time_since_last_decision = esp_timer_get_time()-turn_decision_time;
    MicroPrintf("time: %lld", time_since_last_decision);
    if(time_since_last_decision>3000000){
      if(arrow_score_right_int>=right_left_class_threshold||arrow_score_left_int>=right_left_class_threshold){
        
        if(arrow_score_right_int>arrow_score_left_int) left_right_detections++;
        else left_right_detections--;
        left_right_detections = min(num_arrow_threshold,left_right_detections);
        left_right_detections = max(-num_arrow_threshold,left_right_detections);
        MicroPrintf("left_right_detections : %d",left_right_detections);
        
        //vTaskDelay(500);
        
        
        if(left_right_detections>= num_arrow_threshold){
          turn_decision = 1;
          turn_decision_time = esp_timer_get_time();
          send_turn_decision = turn_decision;
          MicroPrintf("Current decision : %d", send_turn_decision);
          vTaskDelay(200);
          
        }
        else if(left_right_detections<= -num_arrow_threshold){
          turn_decision = 2;
          turn_decision_time = esp_timer_get_time();
          send_turn_decision = turn_decision;
          MicroPrintf("Current decision : %d", send_turn_decision);
          vTaskDelay(200);
        }


      }
    }
    else{
        turn_decision = 0;
        left_right_detections = 0;
        MicroPrintf("turn decision time not reached, Current decision : %d", send_turn_decision);
      }
    
    tempclassRight = arrow_score_right_int;
    tempclassLeft = arrow_score_left_int;

    
    //trying ends
    tostop = false;
    //sleep(5);
    //RespondToDetection(arrow_score_left_int, arrow_score_right_int);
  }
   MicroPrintf(" ");
  vTaskDelay(5);
  storeDetection(tempFomoDetConfidence,tempFomoPosX,tempFomoPosY,tempclassRight,tempclassLeft);
  // Respond to detection
  RespondToDetection(arrow_score_left_int, arrow_score_right_int);
  vTaskDelay(5); // to avoid watchdog trigger
}
#endif

#if defined(COLLECT_CPU_STATS)
  long long total_time = 0;
  long long start_time = 0;
  extern long long softmax_total_time;
  extern long long dc_total_time;
  extern long long conv_total_time;
  extern long long fc_total_time;
  extern long long pooling_total_time;
  extern long long add_total_time;
  extern long long mul_total_time;
#endif

void run_inference(void *ptr) {
  /* Convert from uint8 picture data to int8 */
  for (int i = 0; i < kNumCols * kNumRows; i++) {
    input->data.int8[i] = ((uint8_t *) ptr)[i] ^ 0x80;
  }

#if defined(COLLECT_CPU_STATS)
  long long start_time = esp_timer_get_time();
#endif
  // Run the model on this input and make sure it succeeds.
  if (kTfLiteOk != interpreter->Invoke()) {
    MicroPrintf("Invoke failed.");
  }

#if defined(COLLECT_CPU_STATS)
  long long total_time = (esp_timer_get_time() - start_time);
  printf("Total time = %lld\n", total_time / 1000);
  //printf("Softmax time = %lld\n", softmax_total_time / 1000);
  printf("FC time = %lld\n", fc_total_time / 1000);
  printf("DC time = %lld\n", dc_total_time / 1000);
  printf("conv time = %lld\n", conv_total_time / 1000);
  printf("Pooling time = %lld\n", pooling_total_time / 1000);
  printf("add time = %lld\n", add_total_time / 1000);
  printf("mul time = %lld\n", mul_total_time / 1000);

  /* Reset times */
  total_time = 0;
  //softmax_total_time = 0;
  dc_total_time = 0;
  conv_total_time = 0;
  fc_total_time = 0;
  pooling_total_time = 0;
  add_total_time = 0;
  mul_total_time = 0;
#endif

  TfLiteTensor* output = interpreter->output(0);

  // Process the inference results.
  int8_t person_score = output->data.uint8[kPersonIndex];
  int8_t no_person_score = output->data.uint8[kNotAPersonIndex];

  float person_score_f =
      (person_score - output->params.zero_point) * output->params.scale;
  float no_person_score_f =
      (no_person_score - output->params.zero_point) * output->params.scale;
  RespondToDetection(person_score_f, no_person_score_f);




}
