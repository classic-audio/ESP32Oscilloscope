void setup_screen() {
  // Initialise the TFT registers
  tft.init();
  tft.setRotation(1);
  spr.setColorDepth(8);

  // --- Create a sprite of defined size ---
  // --- Width and height of sprite --------
  #define WIDTH  240
  #define HEIGHT 280
  spr.createSprite(HEIGHT,WIDTH);
  
  // --- Clear the TFT screen to black -----
  tft.fillScreen(TFT_BLACK);

  // --- set text size and color -----------
  spr.setTextFont(2);
  spr.setTextColor(TFT_YELLOW,TFT_BLACK);
}

int data[280] = {0};    

float to_scale(float reading) {
  float temp = HEIGHT - (((reading / 4095.0) + (offset / 3.3)) * 3300 / (v_div * 6)) * (HEIGHT - 1) - 1; return temp;
}
float to_voltage(float reading) { return reading / 4095.0 * 3.3; }
uint32_t from_voltage(float voltage) { return ((uint32_t)(voltage / 3.3 * 4095)) ; }

void update_screen(uint16_t *i2s_buff, float sample_rate){

  float mean = 0;
  float max_v, min_v;

  peak_mean(i2s_buff, BUFF_SIZE, &max_v, &min_v, &mean);

  float freq = 0;
  float period = 0;
  uint32_t trigger0 = 0;
  uint32_t trigger1 = 0;

  //if analog mode OR auto mode and wave recognized as analog
  bool digital_data = false;
  if (digital_wave_option == 1) {
    trigger_freq_analog(i2s_buff, sample_rate, mean, max_v, min_v, &freq, &period, &trigger0, &trigger1);
  }
  else if (digital_wave_option == 0) {
    digital_data = digital_analog(i2s_buff, max_v, min_v);
    if (!digital_data) {
      trigger_freq_analog(i2s_buff, sample_rate, mean, max_v, min_v, &freq, &period, &trigger0, &trigger1);
    }
    else {
      trigger_freq_digital(i2s_buff, sample_rate, mean, max_v, min_v, &freq, &period, &trigger0);
    }
  }
  else {
    trigger_freq_digital(i2s_buff, sample_rate, mean, max_v, min_v, &freq, &period, &trigger0);
  }

  draw_sprite(freq, period, mean, max_v, min_v, trigger0, sample_rate, digital_data, true);
}

void draw_sprite(float freq,
                 float period,
                 float mean,
                 float max_v,
                 float min_v,
                 uint32_t trigger,
                 float sample_rate,
                 bool digital_data,
                 bool new_data
                ) {

  max_v = to_voltage(max_v);
  min_v = to_voltage(min_v);
  Vpp = max_v - min_v;

  String frequency = "";
  // --- frequency ----------------------------------------
  float freq_c = freq * 1.0;                      
  if (freq_c < 1000)
    frequency = String(freq_c,0) + " Hz";
  else if (freq_c < 100000)
    frequency = String(freq_c / 1000.0,1) + " kHz";
  else
    frequency = "----";
  // ------------------------------------------------------

  String s_mean = "";
  if (mean > 1.0)
    s_mean = "Avg: " + String(mean) + "V";
  else
    s_mean = "Avg: " + String(mean * 1000.0) + "mV";

  // -------------------------------------------------------
  String str_filter = "";
  if (current_filter == 0)
    str_filter = "None";
  else if (current_filter == 1)
    str_filter = "Pixel";
  else if (current_filter == 2)
    str_filter = "Mean-5";
  else if (current_filter == 3)
    str_filter = "Lpass9";

  String str_stop = "";
  if (!stop)
    str_stop = "RUNNING";
  else
    str_stop = "STOPPED";

  String wave_option = "";
  if (digital_wave_option == 0)
    if (digital_data )
      wave_option = "AUTO:Dig./data";
    else
      wave_option = "AUTO:Analog";
  else if (digital_wave_option == 1)
    wave_option = "MODE:Analog";
  else
    wave_option = "MODE:Dig./data";


  if (new_data){
    // Fill the whole sprite with black (Sprite is in memory so not visible yet)
    spr.fillSprite(TFT_BLACK);

    draw_grid();

    // === Frame =====================================================================================
    spr.drawRoundRect(0, 0, 279, 239, 40, White);
    spr.drawRoundRect(1, 1, 277, 237, 40, LGrey);
    // ===  Autoscale ================================================================================
    if (auto_scale){
      auto_scale = false;
      // --- Calibrating mV/div -> 3.0 -> 3.0 div of 487 mV @ 1470 mVpp ---
      v_div = 1000.0 * Vpp / 3.0;
      // --- Calibrating uSec/div -> 39.0 -> 3.0 div of 333 usec @ 1kHz --- 
      s_div = period / 3.0;
      // ------------------------------------------------------------------------
      if (s_div > 7000 || s_div <= 0){s_div = 7000;}
      if (v_div <= 0){v_div = 550;}

      // === offset calculation - by Classic Audio Design ===========================================     
      offset = 1.162 * Vpp - 1.462;  // BreadBoard version
      //offset = 1.135 * Vpp - 1.482;  // Assambled version
    }
    // ===============================================================================================
    //only draw digital data if a trigger was in the data
    if (!(digital_wave_option == 2 && trigger == 0))
      draw_channel1(trigger, 0, i2s_buff, sample_rate);
  }

  int shift = 160;                                                                      

  // --- M E N U ------------------------------------------------------------------------------------
  if (menu){  
    // --- Center Line ------------------------------------------                          
    drawDashedLine(0, 120, 280, 120, 5, 5);
    // ----------------------------------------------------------
    spr.fillRect(shift - 5, 0, 105, 190, Black);                          
    spr.drawRect(shift - 5, 0, 105, 190, White);                         
    // --- Menu Pointer ------------------------------------------
    spr.fillRect(shift - 2, 4 + 15 * (opt - 1), 4, 15, TFT_RED); 
   
    spr.drawString("AUTOSCALE",  shift + 5, 5);                                         
    spr.drawString(String(int(v_div)) + "mV/div",  shift + 5, 20);                      
    spr.drawString(String(int(s_div)) + "uS/div",  shift + 5, 35);                 
    spr.drawString("Offset: " + String(offset) + "V",  shift + 5, 50);                  
    spr.drawString("T-Off: " + String((uint32_t)toffset) + "uS",  shift + 5, 65);      
    spr.drawString("Filter: " + str_filter, shift + 5, 80);                             
    spr.drawString(str_stop, shift + 5, 95);                                            
    spr.drawString(wave_option, shift + 5, 110);                                       
    spr.drawString("Single " + String(single_trigger ? "ON" : "OFF"), shift + 5, 125);  

    spr.drawLine(shift, 140, shift + 100, 140, TFT_WHITE);                             
 
    spr.drawString("Vmax: " + String(max_v) + "V",  shift + 5, 140);                   
    spr.drawString("Vmin: " + String(min_v) + "V",  shift + 5, 155);                    
    spr.drawString(s_mean,  shift + 5, 170);                                            

    shift -= 85;                                                                        

    // --- Frequency - p-p - menu ON -------------------------------------------------------------  
    spr.fillRect(shift, 0, 80, 40, TFT_BLACK);                                          
    spr.drawRect(shift, 0, 80, 40, TFT_WHITE);
    spr.drawString(String(max_v - min_v) + " Vpp",  shift + 5, 5);                      
    spr.drawString(frequency,  shift + 5, 20);                                          
    // --- Offset voltage - center line - in the menu --------------------------------------------
    String offset_line = String((3.0 * v_div) / 1000.0 - offset) + "V";                 
    spr.drawString(offset_line,  shift + 15, 112);
    // -------------------------------------------------------------------------------------------
 
    if (set_value) {
      spr.fillRect(239, 0, 11, 11, Blue);                                           
      spr.drawRect(239, 0, 11, 11, LGrey);                                         
      spr.drawLine(241, 5, 248 , 5, LGrey);                                         
      spr.drawLine(244, 2, 244, 8, LGrey);                                          

      spr.fillRect(239, 129, 11, 11, Blue);                                         
      spr.drawRect(239, 129, 11, 11, LGrey);                                        
      spr.drawLine(241, 134, 248, 134, LGrey);                                      
    }
  }
  // --- Measured data displayed at the bottum of the screen -------------------------------------
  else if (info){
    float Upp = (max_v - min_v);
    float Urms = (Upp / 2 / sqrt(2));
    // --- Center line -------------------------------------------------------   
    drawDashedLine(0, 120, 280, 120, 5, 5); 
    // --- Data - bottom of the screen ---------------------------------------   
    spr.drawString(String((Upp * 1000.0),0) + " mVpp",  shift - 140, 205); 
    spr.drawString(String((Urms * 1000.0),0) + " mVrms",  shift - 50, 205);              
    spr.drawString(frequency,  shift + 45, 205);                                      
    // === Bottom line ======================================================
    spr.drawString(String(int(v_div)) + " mV/div",  shift + 0, 220);
    // --- Display time / div -------------------------------------------
    float s_div_m = 0.0;
    int s_div_u = 0;
    
    if(s_div > 500){
        s_div_m = (s_div) / 1000.0;
        spr.drawString(String(s_div_m,1) + " mS/div",  shift - 115, 220);                   
    }
    else{
        s_div_u = int(s_div);
        spr.drawString(String(s_div_u) + " uS/div",  shift - 115, 220);
    }
  // --- Voltage level - center line --------------------------------------  
    String offset_line = String((3.0 * v_div) / 1000.0 - offset) + " V";             
    spr.drawString(offset_line,  shift + 70, 112);
  }


  //push the drawed sprite to the screen
  spr.pushSprite(0, 0);

  yield(); // Stop watchdog reset
}

void draw_grid(){
  for (int i = 0; i < 28; i++) {
    spr.drawPixel(i * 10, 40, TFT_WHITE);
    spr.drawPixel(i * 10, 80, TFT_WHITE);
    spr.drawPixel(i * 10, 120, TFT_WHITE);
    spr.drawPixel(i * 10, 160, TFT_WHITE);
    spr.drawPixel(i * 10, 200, TFT_WHITE);
  }
  for (int i = 0; i < 240; i += 10){
    for (int j = 0; j < 280; j += 40) {
      spr.drawPixel(j, i, TFT_WHITE);
    }
  }
}

void draw_channel1(uint32_t trigger0, uint32_t trigger1, uint16_t *i2s_buff, float sample_rate) {
  //screen wave drawing
  data[0] = to_scale(i2s_buff[trigger0]);
  low_pass filter(0.99);
  mean_filter mfilter(5);
  mfilter.init(i2s_buff[trigger0]);
  filter._value = i2s_buff[trigger0];
  
  // --- Calibrating display uSec/div -> 39.0 -> 3 div of 333 usec @ 1kHz ---------
  float data_per_pixel = (s_div / 39.0) / (sample_rate / 1000);   

  uint32_t index_offset = (uint32_t)(toffset / data_per_pixel);
  trigger0 += index_offset;  
  uint32_t old_index = trigger0;
  float n_data = 0, o_data = to_scale(i2s_buff[trigger0]);
  for (uint32_t i = 1; i < 280; i++) {
    uint32_t index = trigger0 + (uint32_t)((i + 1) * data_per_pixel);
    if (index < BUFF_SIZE) {
      if (full_pix && s_div > 40 && current_filter == 0) {
        uint32_t max_val = i2s_buff[old_index];
        uint32_t min_val = i2s_buff[old_index];
        for (int j = old_index; j < index; j++) {
          
          //draw lines for all this data points on pixel i
          if (i2s_buff[j] > max_val)
            max_val = i2s_buff[j];
          else if (i2s_buff[j] < min_val)
            min_val = i2s_buff[j];

        }
        spr.drawLine(i, to_scale(min_val), i, to_scale(max_val), Orange);
      }
      else {
        if (current_filter == 2)
          n_data = to_scale(mfilter.filter((float)i2s_buff[index]));
        else if (current_filter == 3)
          n_data = to_scale(filter.filter((float)i2s_buff[index]));
        else
          n_data = to_scale(i2s_buff[index]);

        spr.drawLine(i - 1, o_data, i, n_data, Orange);
        o_data = n_data;
      }

    }
    else {
      break;
    }
    old_index = index;
  }
}
// --- Dashed Center Linee ------------------------------------------------------------------------------
void drawDashedLine(int x0, int y0, int x1, int y1, int dashLength, int spaceLength) {
  int totalLength = abs(x1 - x0);
  int numDashes = totalLength / (dashLength + spaceLength);
  int currentX = x0;

  for (int i = 0; i < numDashes; i++) {
    spr.drawLine(currentX, y0, currentX + dashLength, y0, White);
    currentX += dashLength + spaceLength;
  }
}
