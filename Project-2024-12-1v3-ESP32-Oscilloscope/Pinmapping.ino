/*
ESP32 Connections

DISPLAY_______________________________________
ESP32               Display
                    Vin
3V3                 3V3
GND                 GND
18                  SCK
                    MISO
19                  MOSI
5                   TFTCS  (CS)
23                  RST
16                  DC
                    SDCS (SD card)
4                   LITE (Backlight) 
(GPIO 04 findes ikke nogen steder i coden)

PUSHBUTTON - CONTROL
ESP32
22                   MENU
21                   mV/div   /   +
17                   uSec/div    /   -
26                   Back

ADC-CHANNEL
ESP32
33                   ADC1_CHANNEL

DISPLAY MUTE (START)
ESP32
34                   MUTE

ESP32  PSU
VCC                  5V
GND                  GND

Updated: 2023-11

 */
