#include "main.h"

#include <esp_event.h>
#include <esp_log.h>
#include <peer.h>

#ifndef LINUX_BUILD
#include "nvs_flash.h"

extern "C" void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Configure M5Stack BEFORE calling M5.begin()
  auto cfg = M5.config();
  
  // M5Stack Core S3 audio configuration
  cfg.speaker_config.pin_bck = GPIO_NUM_7;
  cfg.speaker_config.pin_ws = GPIO_NUM_5;
  cfg.speaker_config.pin_data_out = GPIO_NUM_6;
  cfg.speaker_config.sample_rate = 16000;
  cfg.speaker_config.stereo = false;
  cfg.speaker_config.buzzer = false;
  cfg.speaker_config.use_dac = false;
  
  cfg.microphone_config.pin_bck = GPIO_NUM_4;
  cfg.microphone_config.pin_ws = GPIO_NUM_5;
  cfg.microphone_config.pin_data_in = GPIO_NUM_6;
  cfg.microphone_config.sample_rate = 16000;
  cfg.microphone_config.stereo = false;
  
  M5.begin(cfg);

  M5.Display.setBrightness(70);
  M5.Display.setTextSize(1.5);
  M5.Display.fillScreen(BLACK);
  M5.Display.println("Pipecat ESP32 client initialized\n");

  ESP_LOGI("MAIN", "Starting initialization sequence...");

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  
  // Initialize audio components in correct order
  ESP_LOGI("MAIN", "Initializing audio capture...");
  pipecat_init_audio_capture();
  
  ESP_LOGI("MAIN", "Initializing audio decoder...");
  pipecat_init_audio_decoder();
  
  ESP_LOGI("MAIN", "Initializing audio encoder...");
  pipecat_init_audio_encoder();
  
  ESP_LOGI("MAIN", "Initializing WiFi...");
  pipecat_init_wifi();
  
  ESP_LOGI("MAIN", "Initializing WebRTC...");
  pipecat_init_webrtc();
  
  ESP_LOGI("MAIN", "Initialization complete, starting main loop...");

  while (1) {
    pipecat_webrtc_loop();
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
  }
}
#else
int main(void) {
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  peer_init();
  pipecat_init_audio_encoder();
  pipecat_webrtc();

  while (1) {
    pipecat_webrtc_loop();
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
  }
}
#endif