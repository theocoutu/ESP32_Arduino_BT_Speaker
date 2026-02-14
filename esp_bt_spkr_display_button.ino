/**
 * This presents a Bluetooth (Classic) Audio Sink via A2DP -- Acting as a  
 * Bluetooth Speaker/Headphones with the below defined name.
 *
 * The audio is routed via I2S (at 44.1kHz for A2DP) to the below defined 
 * pins (no MCLK / Master Clock).
 *
 * The 'play/pause', 'next' and 'previous' music control buttons are
 * connected to the below defined pins and are pulled {UP/DOWN}.
 *
 * For volume control, the centre pin of a logarithmic potentiometer is 
 * attached to the below defined pin, and the number of readings to average
 * over is defined below as well.
 *
 * The 'now playing' metadata is displayed on an I2C display attached to the below pins.
 * The sketch is designed for a resolution of {128x64}.
 * 
 * The 'now playing' metadata has end-of-text strings, for adding space when scrolling on the
 *  display, and functions to dynamically allocate memory for the text and to safely clean up
 *  that memory by deallocating.
 *
 * 
 */



/* Definitions CAN BE CHANGED */
#define BT_NAME  "ESP32-BT-Spkr"  /* Friendly Bluetooth name */

#define I2S_SCK       14    /* Audio data bit clock */
#define I2S_WS        25    /* Audio data left and right clock */
#define I2S_SDOUT     26    /* ESP32 audio data output (to speakers) */

#define PAUSE_BTN     0   

#define VOL_POT_PIN   36    
#define NUM_READINGS  50    

#define DISP_I2C_SDA   21 //default
#define DISP_I2C_SCL   22 //default
#define DISP_I2C_ADDR  0x3C

/* The end-of-text string, when scrolling on the display */
//#define TERM_TEXT " | "
//#define INIT_TEXT "Scrolling Text Example!"  /* for testing only */
//#define U8G2_USE_DYNAMIC_ALLOC


/* Libraries */
#include "ESP_I2S.h" // Use the ESP-IDF builtin I2S driver
//#include <freertos/FreeRTOS.h>

//https://github.com/pschatzmann/ESP32-A2DP/
#include "BluetoothA2DPSink.h"

//Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>



/* Objects */
I2SClass i2s;                         /* Our I2S object from the ESP-IDF driver */
BluetoothA2DPSink a2dp_sink(i2s);     /* Our A2DP Sink object, for receiving BT audio data */
        //BluetoothA2DPSinkQueued a2dp_sink(i2s);  /* Can help with popping when changing volume */

Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);


  /* Derive a custom class to access a protected method */
class VolCon : public A2DPLinearVolumeControl {  /* A2DPLinearVolumeControl as we have a log pot */
public:                                        /* A2DPSimpleExponentialVolumeControl if we didn't */
    void adjustVolume(uint8_t volume) {  /* Create our own function */
        set_volume(volume); /* Accessing protected method in derived class */
    }
};  /* end class */
VolCon vc;  /* Our new volume control object */



/* Globals */
char song_text[127] = " - ";
char artist_text[127] = " - ";
//char *album_text = nullptr;
//char *time_text = nullptr;
char play_state[32] = " - ";
bool is_playing = false;



/* Function declarations */
void init_i2s(void);
void init_oled(void);
void init_a2dp(void);
uint8_t analog_to_volume(void);

// FreeRTOS tasks
void display_task(void *pvParameters);
void button_task(void *pvParameters);

// Callback function declarations
void avrc_metadata_cb(uint8_t id, const uint8_t *text);
void avrc_playstat_cb(esp_avrc_playback_stat_t state);



/* Code */
void setup()
{
  Serial.begin(115200);

  pinMode(PAUSE_BTN, INPUT);  // external pulldown

  init_i2s();
  init_a2dp();

  xTaskCreatePinnedToCore(
    display_task,
    "Display",
    8192,
    NULL,
    2,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    button_task,
    "Button",
    4096,
    NULL,
    2,
    NULL,
    0
  );

}



void loop()
{
  uint8_t vol = analog_to_volume();

  // Serial.printf("getVol: %d, setVol: %d\n", vol, vc.get_volume_factor());
  vc.adjustVolume( vol );
  
  vTaskDelay( pdMS_TO_TICKS(2) );
}



/* Get metadata from AVRC (song, artist, etc) */
void avrc_metadata_cb(uint8_t id, const uint8_t *text)
{
  if (id == ESP_AVRC_MD_ATTR_TITLE)
  {
    snprintf(song_text, sizeof(song_text), "%s", (const char*)text);
  }
  
  if (id == ESP_AVRC_MD_ATTR_ARTIST)
  {
    snprintf(artist_text, sizeof(artist_text), "%s", (const char*)text);
  }
}



// Get playing state from AVRC (playing, paused, etc)
void avrc_playstat_cb(esp_avrc_playback_stat_t state)
{
  //char playing[32] = a2dp_sink.to_str(state)
  
  is_playing = (state == 1);
  snprintf(
    play_state,
    sizeof(play_state),
    "%s", 
    is_playing?"Playing":"Not playing"
  );
}



/* Set up I2S (using internal ESP-IDF implementation) */
void init_i2s(void)
{
  i2s.setPins(I2S_SCK, I2S_WS, I2S_SDOUT);
  
  /* Try starting I2S with our set pins and 44.1kHz,
   16bit width, stereo out, both i2s0 and i2s1 slots */
  bool i2s_success = i2s.begin(
    I2S_MODE_STD,
    44100,
    I2S_DATA_BIT_WIDTH_16BIT,
    I2S_SLOT_MODE_STEREO,
    I2S_STD_SLOT_BOTH
  );

  if (!i2s_success)
  {
    Serial.println("Failed to initialize I2S!");  /* If I2S start fails */
    while (1);  /* do nothing */
  }
}



void init_a2dp(void)
{
  // Initialize A2DP
  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.set_volume_control(&vc);
  
  a2dp_sink.set_avrc_metadata_attribute_mask(
    ESP_AVRC_MD_ATTR_TITLE | 
    ESP_AVRC_MD_ATTR_ARTIST // | 
    //ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_PLAYING_TIME
  );
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_cb);

  //a2dp_sink.set_avrc_rn_events(
  //  ESP_AVRC_RN_PLAY_STATUS_CHANGE |
  //  ESP_AVRC_RN_TRACK_CHANGE |
  //  ESP_AVRC_RN_VOLUME_CHANGE
  //);
  a2dp_sink.set_avrc_rn_playstatus_callback(avrc_playstat_cb);

  a2dp_sink.set_task_core(1);

  a2dp_sink.start(BT_NAME);
}



uint8_t analog_to_volume(void)  /* returns 0 to 127 */
{
  uint32_t sum = 0; // To store our sum of readings to average later
  
  for (int i=0; i < NUM_READINGS; i++)
  { // read analog value and cast to uint16, then add to sum. 
    sum += (uint16_t) analogRead(VOL_POT_PIN);
    vTaskDelay( pdMS_TO_TICKS(2) ); // short 2ms delay for stability
  }

  uint16_t result = (sum / NUM_READINGS); // divide the sum by the number of readings
  
  //Serial.printf("result: %d, ", result);
  if (result < 1111)
  {
    return 0;
  } else {
    return (uint8_t) map(result, 1110, 4095, 1, 127);
  }
}



/* FreeRTOS task function definitions */
void display_task(void *pvParameters)
{
  //
  display.begin(DISP_I2C_ADDR, true);
  display.display();

  vTaskDelay( pdMS_TO_TICKS(1500) );
  

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);

  display.clearDisplay();
  display.display();

  // Scroll state variables
  int song_scroll_offset = 0;
  int artist_scroll_offset = 0;
  unsigned long last_scroll = 0;
  const int scroll_ms = 100;  // Adjust speed (smaller = faster)
  const int scroll_step = 2;        // Pixels per step

  while (true)
  {
    display.clearDisplay();

    // Line 1 (top): Song title with conditional scrolling
    display.setCursor(0, 1);
    display.setTextWrap(false);
    
    // Calculate song text width (chars * 6 pixels at size 1)
    int song_width = strlen(song_text) * 6;
    if (song_width > 128)
    {
      // TODO implement end of scroll character " | "
      //char new_song_text[36];
      //snprintf(new_song_text, 36, "", song_text)
      
      // Scroll: draw text multiple times to fill screen + wrap around
      int x = song_scroll_offset;
      while (x < 128)
      {
        display.setCursor(x, 1);
        display.print(song_text);
        x += song_width + 10;  // +10 for spacing between repeats
      }
      // Update offset periodically
      if (xTaskGetTickCount() - last_scroll > pdMS_TO_TICKS(scroll_ms))
      {
        song_scroll_offset -= scroll_step;
        if (song_scroll_offset <= -song_width)
        {
          song_scroll_offset = 0;
        }
        last_scroll = xTaskGetTickCount();
      }
    } 
    else
    {
      display.print(song_text);  // Static if short
    }

    // Line 2: Artist with conditional scrolling
    display.setCursor(0, 22);
    int artist_width = strlen(artist_text) * 6;
    if (artist_width > 128)
    {
      int x = artist_scroll_offset;
      while (x < 128)
      {
        display.setCursor(x, 22);
        display.print(artist_text);
        x += artist_width + 10;
      }

      if (xTaskGetTickCount() - last_scroll > pdMS_TO_TICKS(scroll_ms))
      {
        artist_scroll_offset -= scroll_step;
        if (artist_scroll_offset <= -artist_width)
        {
          artist_scroll_offset = 0;
        }
        last_scroll = xTaskGetTickCount();
      }
    }
    else
    {
      display.print(artist_text);  // Static if short
    }

    // Line 3 (bottom): play state (no scroll)
    display.setCursor(0, 43);
    display.print(play_state);

    display.display();
    vTaskDelay( pdMS_TO_TICKS(50) );  // Refresh rate
  }//while(true)
}



void button_task(void *pvParameters)
{
  const uint16_t debounce_delay = 60;
  bool current_button_state = false;
  bool previous_button_state = false;
  uint32_t last_debounce_time = 0;
  bool new_reading = false;

  while(true)
  {
    new_reading = digitalRead(PAUSE_BTN);

    if (new_reading != previous_button_state)
    {
      last_debounce_time = millis();
    }

    if ( (millis() - last_debounce_time) > debounce_delay )
    {
      if (new_reading != current_button_state)
      current_button_state = new_reading;

      if (current_button_state == HIGH)
      {
        if (is_playing) a2dp_sink.pause();
        else            a2dp_sink.play();
      }
    }

    previous_button_state = new_reading;
  } //while(true)
}