#include "bsp/esp-bsp.h"
#include <atomic>
#include <opus.h>
#include <peer.h>
#include "esp_log.h"
#include "main.h"

// Exact settings from working code
#define GAIN 10.0f  // Same as working code

#define CHANNELS 1
#define SAMPLE_RATE (16000)
#define BITS_PER_SAMPLE 16

#define PCM_BUFFER_SIZE 640  // Same as working code

#define OPUS_BUFFER_SIZE 1276
#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

static const char *TAG = "pipecat_audio";

// Same codec configuration as working code
esp_codec_dev_sample_info_t fs = {
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel = CHANNELS,
    .channel_mask = 0,
    .sample_rate = SAMPLE_RATE,
    .mclk_multiple = 0,
};

esp_codec_dev_handle_t mic_codec_dev;
esp_codec_dev_handle_t spk_codec_dev;

OpusDecoder *opus_decoder = NULL;
opus_int16 *decoder_buffer = NULL;

OpusEncoder *opus_encoder = NULL;
uint8_t *encoder_output_buffer = NULL;
uint8_t *read_buffer = NULL;

std::atomic<bool> is_playing = false;

// Exact play state detection from working code
void set_is_playing(int16_t *in_buf) {
    bool any_set = false;
    for (size_t i = 0; i < (PCM_BUFFER_SIZE / 2); i++) {
        if (in_buf[i] != -1 && in_buf[i] != 0 && in_buf[i] != 1) {
            any_set = true;
        }
    }
    is_playing = any_set;
}

// Exact gain application from working code
void apply_gain(int16_t *samples) {
    for (size_t i = 0; i < (PCM_BUFFER_SIZE / 2); i++) {
        float scaled = (float)samples[i] * GAIN;

        // Clamp to 16-bit range
        if (scaled > 32767.0f)
            scaled = 32767.0f;
        if (scaled < -32768.0f)
            scaled = -32768.0f;

        samples[i] = (int16_t)scaled;
    }
}

// ---------------------- BSP Audio Initialization ----------------------
void pipecat_init_audio_capture() {
    ESP_LOGI(TAG, ">>> BSP AUDIO INIT <<<");
    
    // Initialize BSP board first
    esp_err_t ret = bsp_board_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP board init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // Speaker - exact same as working code
    spk_codec_dev = bsp_audio_codec_speaker_init();
    if (spk_codec_dev == NULL) {
        ESP_LOGE(TAG, "Failed to initialize speaker codec");
        return;
    }
    
    ret = esp_codec_dev_open(spk_codec_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open speaker codec: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = esp_codec_dev_set_out_vol(spk_codec_dev, 100);  // Same volume as working code
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set speaker volume: %s", esp_err_to_name(ret));
    }

    // Microphone - exact same as working code
    mic_codec_dev = bsp_audio_codec_microphone_init();
    if (mic_codec_dev == NULL) {
        ESP_LOGE(TAG, "Failed to initialize microphone codec");
        return;
    }
    
    ret = esp_codec_dev_set_in_gain(mic_codec_dev, 42.0);  // Same gain as working code
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set microphone gain: %s", esp_err_to_name(ret));
    }
    
    ret = esp_codec_dev_open(mic_codec_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open microphone codec: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, ">>> BSP AUDIO INIT COMPLETE <<<");
}

void pipecat_init_audio_decoder() {
    ESP_LOGI(TAG, ">>> BSP OPUS DECODER INIT <<<");
    
    // Decoder - exact same as working code
    int opus_error = 0;
    opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, &opus_error);
    if (opus_error != OPUS_OK) {
        ESP_LOGE(TAG, "Failed to create OPUS decoder: %d", opus_error);
        return;
    }
    
    decoder_buffer = (opus_int16 *)malloc(PCM_BUFFER_SIZE);  // Same allocation as working code
    if (!decoder_buffer) {
        ESP_LOGE(TAG, "Failed to allocate decoder buffer");
        return;
    }
    
    ESP_LOGI(TAG, ">>> BSP OPUS DECODER READY <<<");
}

void pipecat_init_audio_encoder() {
    ESP_LOGI(TAG, ">>> BSP OPUS ENCODER INIT <<<");
    
    // Encoder - exact same as working code
    int opus_error = 0;
    opus_encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &opus_error);
    if (opus_error != OPUS_OK) {
        ESP_LOGE(TAG, "Failed to create OPUS encoder: %d", opus_error);
        return;
    }
    
    // Same encoder configuration as working code
    opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
    opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
    opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    // Same buffer allocation as working code
    read_buffer = (uint8_t *)heap_caps_malloc(PCM_BUFFER_SIZE, MALLOC_CAP_DEFAULT);
    encoder_output_buffer = (uint8_t *)malloc(OPUS_BUFFER_SIZE);
    
    if (!read_buffer || !encoder_output_buffer) {
        ESP_LOGE(TAG, "Failed to allocate encoder buffers");
        return;
    }
    
    ESP_LOGI(TAG, ">>> BSP OPUS ENCODER READY <<<");
}

// ---------------------- BSP Audio Play (exact copy of working code) ----------------------
void pipecat_audio_decode(uint8_t *data, size_t size) {
    ESP_LOGI(TAG, ">>> BSP DECODE: %d bytes <<<", size);
    
    // Exact same decode logic as working code
    auto decoded_size = opus_decode(opus_decoder, data, size, decoder_buffer, PCM_BUFFER_SIZE, 0);

    if (decoded_size <= 0) {
        ESP_LOGW(TAG, ">>> BSP DECODE FAILED: %d <<<", decoded_size);
        return;
    }
    
    ESP_LOGI(TAG, ">>> BSP DECODED: %d samples <<<", decoded_size);

    // Exact same play state detection as working code
    set_is_playing(decoder_buffer);
    
    // Exact same gain application as working code
    apply_gain((int16_t *)decoder_buffer);
    
    // Use BSP codec instead of M5Stack
    esp_err_t ret = esp_codec_dev_write(spk_codec_dev, decoder_buffer, PCM_BUFFER_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, ">>> BSP SPEAKER WRITE FAILED: %s <<<", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, ">>> BSP PLAYING SUCCESS <<<");
    }
}

// ---------------------- BSP Audio Send (exact copy of working code) ----------------------
void pipecat_send_audio(PeerConnection *peer_connection) {
    if (is_playing) {
        // Send silence when playing - exact same as working code
        memset(read_buffer, 0, PCM_BUFFER_SIZE);
    } else {
        // Record from microphone using BSP codec
        esp_err_t ret = esp_codec_dev_read(mic_codec_dev, read_buffer, PCM_BUFFER_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Microphone read failed: %s", esp_err_to_name(ret));
            memset(read_buffer, 0, PCM_BUFFER_SIZE);  // Use silence on error
        }
    }

    // Exact same encoding as working code
    auto encoded_size = opus_encode(opus_encoder, 
                                  (const opus_int16 *)read_buffer,
                                  PCM_BUFFER_SIZE / sizeof(uint16_t),  // Same calculation
                                  encoder_output_buffer, 
                                  OPUS_BUFFER_SIZE);
    
    if (encoded_size > 0) {
        peer_connection_send_audio(peer_connection, encoder_output_buffer, encoded_size);
    } else {
        ESP_LOGW(TAG, "OPUS encode failed: %d", encoded_size);
    }
}