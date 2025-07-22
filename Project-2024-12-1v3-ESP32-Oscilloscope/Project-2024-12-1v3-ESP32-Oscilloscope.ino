/*  Project:   Monitor oscilloscope
 *             Classic Audio Design
 *                   2025-01
 *  ******************************************************************************************                  
 *  Reference:  https://github.com/gustavollps/esp32TTGO_i2s_scope/tree/master  2020
 *  Reference:  https://github.com/Circuit-Digest/ESP32-Oscilloscope            2022
 *  ******************************************************************************************
 *  Main Parts:
 *  ESP32
 *  TFT 1.69" 240x280 ST7789 Round corner display
 *  ******************************************************************************************
 *  Arduino Ide compiling
 *  Board: ESP32 Wrover Module
 *  Core: ESP32 core 1.0.6 (I2S - not working on later versions)
 *  Display: Setup74_ESP32_ST7789_240x280.h
 ********************************************************************************************/
#include <Arduino.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include <soc/syscon_reg.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "esp_adc_cal.h"
#include "filters.h"

#define DELAY 1000

#define Black   0x0000
#define Blue    0x001F
#define Lblue   0x0C3F
#define Green   0x07E0
#define DGreen  0x0300
#define Yellow  0xFFE0
#define White   0xFFFF
#define LGrey   0x652F
#define DGrey   0x4228
#define Red     0xF800
#define DRed    0x8000 
#define Olive   0x7BE0
#define Orange  0xFC00
#define Pink    0xFC18
#define DBlue   0x2A35
#define Gold    0x6320

#define ADC_CHANNEL   ADC1_CHANNEL_5    // GPIO33
#define NUM_SAMPLES   1000              // number of samples
#define I2S_NUM         (0)
#define BUFF_SIZE 50000
#define B_MULT BUFF_SIZE/NUM_SAMPLES
#define BUTTON_Ok        22             // menu     
#define BUTTON_Plus      21             // mV/div      
#define BUTTON_Minus     17             // usec/div     
#define BUTTON_Back      26             // back     

TFT_eSPI    tft = TFT_eSPI();         // Declare object "tft"
TFT_eSprite spr = TFT_eSprite(&tft);  // Declare Sprite object "spr" with pointer to "tft" object

esp_adc_cal_characteristics_t adc_chars;

TaskHandle_t task_menu;
TaskHandle_t task_adc;

float v_div = 825;
float s_div = 10;
float Vpp = 0;
float toffset = 0;

//options handler
enum Option {
  None,
  Autoscale,
  Vdiv,
  Sdiv,
  Offset,
  TOffset,
  Filter,
  Stop,
  Mode,
  Single,
  Clear,
  Reset,
  Probe,
  UpdateF,
  Cursor1,
  Cursor2
};

int8_t volts_index = 0;
int8_t tscale_index = 0;
uint8_t opt = None;

bool menu = false;
bool info = true;
bool set_value  = false;
float RATE = 1000;                           //in ksps --> 1000 = 1Msps

// === Setting ============================================================================
bool auto_scale = true;                            // Autoscale v/start
float offset = 0.36;                              // offset
uint8_t current_filter = 2;           // 0:none 1:pixel 2:mean5 3:Lpass
uint8_t digital_wave_option = 0;  //0-auto | 1-analog | 2-digital data (SERIAL/SPI/I2C/etc)
// -=======================================================================================

bool full_pix = true;
bool stop = false;
bool stop_change = false;
uint16_t i2s_buff[BUFF_SIZE];
bool single_trigger = false;
bool data_trigger = false;
bool updating_screen = false;
bool new_data = false;
bool menu_action = false;

int btnok,btnpl,btnmn,btnbk;
void IRAM_ATTR btok(){btnok = 1;}
void IRAM_ATTR btplus(){btnpl = 1;}
void IRAM_ATTR btminus(){btnmn = 1;}
void IRAM_ATTR btback(){btnbk = 1;}


void setup(){
  Serial.begin(115200);

  configure_i2s(1000000);
  // --- setup display ----------------------------
  setup_screen();
  // --- Setup control buttons --------------------
  pinMode(BUTTON_Ok , INPUT);          // Menu
  pinMode(BUTTON_Plus , INPUT);        // mV/div
  pinMode(BUTTON_Minus , INPUT);       // usec/div
  pinMode(BUTTON_Back , INPUT);        // back
  attachInterrupt(BUTTON_Ok, btok, RISING);
  attachInterrupt(BUTTON_Plus, btplus, RISING);
  attachInterrupt(BUTTON_Minus, btminus, RISING);
  attachInterrupt(BUTTON_Back, btback, RISING);

  characterize_adc();

  xTaskCreatePinnedToCore(
    core0_task,
    "menu_handle",
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &task_menu,  /* Task handle. */
    0); /* Core where the task should run */

  xTaskCreatePinnedToCore(
    core1_task,
    "adc_handle",
    10000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    3,  /* Priority of the task */
    &task_adc,  /* Task handle. */
    1); /* Core where the task should run */
}

void core0_task( void * pvParameters ){
  (void) pvParameters;

  for (;;){
    menu_handler();

    if (new_data || menu_action){
      new_data = false;
      menu_action = false;

      updating_screen = true;
      update_screen(i2s_buff, RATE);
      updating_screen = false;
      vTaskDelay(pdMS_TO_TICKS(10));
      Serial.println("CORE0");
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

}

void core1_task( void * pvParameters ){
  (void) pvParameters;
  for (;;){
    if (!single_trigger){
      while (updating_screen){
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      if (!stop){
        if (stop_change){
          i2s_adc_enable(I2S_NUM_0);
          stop_change = false;
        }
        ADC_Sampling(i2s_buff);
        new_data = true;
      }
      else{
        if (!stop_change){
          i2s_adc_disable(I2S_NUM_0);
          i2s_zero_dma_buffer(I2S_NUM_0);
          stop_change = true;
        }
      }
      Serial.println("CORE1");
      vTaskDelay(pdMS_TO_TICKS(300));
    }
    else{
      float old_mean = 0;
      while (single_trigger) {
        stop = true;
        ADC_Sampling(i2s_buff);
        float mean = 0;
        float max_v, min_v;
        peak_mean(i2s_buff, BUFF_SIZE, &max_v, &min_v, &mean);

        //signal captured (pp > 0.4V || changing mean > 0.2V) -> DATA ANALYSIS
        if ((old_mean != 0 && fabs(mean - old_mean) > 0.2) || to_voltage(max_v) - to_voltage(min_v) > 0.05) {
          float freq = 0;
          float period = 0;
          uint32_t trigger0 = 0;
          uint32_t trigger1 = 0;

          //if analog mode OR auto mode and wave recognized as analog
          bool digital_data = !false;
          if (digital_wave_option == 1) {
            trigger_freq_analog(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0, &trigger1);
          }
          else if (digital_wave_option == 0) {
            digital_data = digital_analog(i2s_buff, max_v, min_v);
            if (!digital_data) {
              trigger_freq_analog(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0, &trigger1);
            }
            else {
              trigger_freq_digital(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0);
            }
          }
          else {
            trigger_freq_digital(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0);
          }

          single_trigger = false;
          new_data = true;
          Serial.println("Single GOT");
          //return to normal execution in stop mode
        }

        vTaskDelay(pdMS_TO_TICKS(1));   //time for the other task to start (low priorit)

      }
      vTaskDelay(pdMS_TO_TICKS(300));
    }
  }
}

void loop(){}
