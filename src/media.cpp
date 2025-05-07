#include <driver/i2s.h>
#include <opus.h>
#include "esp_log.h"

#include "main.h"

#define SAMPLE_RATE     (16000)
#define I2S_BCK_PIN     (8)
#define I2S_WS_PIN      (7)
#define I2S_DO_PIN      (43)
#define I2S_DI_PIN      (44)

#define OPUS_SAMPLE_RATE 8000

#define OPUS_OUT_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode

#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

#define READ_BUFFER_SIZE          (2560)
#define RESAMPLE_BUFFER_SIZE ((READ_BUFFER_SIZE / 4) / 2)

static uint8_t read_buffer[READ_BUFFER_SIZE];
static uint8_t encode_resample_buffer[RESAMPLE_BUFFER_SIZE];
static uint8_t decode_resample_buffer[READ_BUFFER_SIZE];

static const char *TAG = "oai_media";

void convert_int32_to_int16_and_downsample(int32_t *in, int16_t *out, size_t count) {
    for (size_t j = 0, i = 0; i < count; i += 4, j++) {
        int32_t s = in[i] >> 16;
        if (s > INT16_MAX) s = INT16_MAX;
        if (s < INT16_MIN) s = INT16_MIN;
        out[j] = (int16_t)s;
    }
}

void convert_16k8_to_32k32(int16_t *in_buf, size_t in_samples, int32_t *out_buf) {
    size_t out_index = 0;
    for (size_t i = 0; i < in_samples; i++) {
        int32_t s = ((int32_t)in_buf[i]) << 16;
        out_buf[out_index++] = s;
        out_buf[out_index++] = s;
        out_buf[out_index++] = s;
        out_buf[out_index++] = s;
    }
}

static void i2s_driver_setup(i2s_mode_t mode, i2s_port_t i2s_num)
{
    esp_err_t err;
    i2s_config_t cfg = {
        .mode = (i2s_mode_t) (mode | I2S_MODE_SLAVE),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    if ((err = i2s_driver_install(i2s_num, &cfg, 0, NULL)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_driver_install failed: %s", esp_err_to_name(err));
        return;
    }

    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_DO_PIN,
        .data_in_num  = I2S_DI_PIN
    };

    if ((err = i2s_set_pin(i2s_num, &pins)) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_set_pin failed: %s", esp_err_to_name(err));
    }
}

void oai_init_audio_capture() {
  i2s_driver_setup(I2S_MODE_RX, I2S_NUM_0);
  i2s_driver_setup(I2S_MODE_TX, I2S_NUM_1);
}

opus_int16 *output_buffer = NULL;
OpusDecoder *opus_decoder = NULL;

void oai_init_audio_decoder() {
  int decoder_error = 0;
  opus_decoder = opus_decoder_create(OPUS_SAMPLE_RATE, 1, &decoder_error);
  if (decoder_error != OPUS_OK) {
    printf("Failed to create OPUS decoder");
    return;
  }

  output_buffer = (opus_int16 *)malloc(RESAMPLE_BUFFER_SIZE);
}

void oai_audio_decode(uint8_t *data, size_t size) {
  esp_err_t ret;
  int decoded_size = opus_decode(opus_decoder, data, size, output_buffer, RESAMPLE_BUFFER_SIZE, 0);

  if (decoded_size > 0) {
    convert_16k8_to_32k32((int16_t *)output_buffer,  RESAMPLE_BUFFER_SIZE / sizeof(uint16_t), (int32_t *) decode_resample_buffer);
    size_t bytes_written = 0;
    if ((ret = i2s_write(I2S_NUM_1, decode_resample_buffer, READ_BUFFER_SIZE, &bytes_written, portMAX_DELAY)) != ESP_OK) {
      ESP_LOGE(TAG, "i2s_write failed: %s", esp_err_to_name(ret));
    }
  }
}

OpusEncoder *opus_encoder = NULL;
uint8_t *encoder_output_buffer = NULL;

void oai_init_audio_encoder() {
  int encoder_error;
  opus_encoder = opus_encoder_create(OPUS_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &encoder_error);
  if (encoder_error != OPUS_OK) {
    printf("Failed to create OPUS encoder");
    return;
  }

  if (opus_encoder_init(opus_encoder, OPUS_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP) != OPUS_OK) {
    printf("Failed to initialize OPUS encoder");
    return;
  }

  opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
  opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
  opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  encoder_output_buffer = (uint8_t *)malloc(OPUS_OUT_BUFFER_SIZE);
}

void oai_send_audio(PeerConnection *peer_connection) {
  size_t bytes_read = 0;
  esp_err_t ret = i2s_read(I2S_NUM_0, read_buffer, READ_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "i2s_read failed: %s", esp_err_to_name(ret));
  }

  convert_int32_to_int16_and_downsample((int32_t *) &read_buffer, (int16_t*) &encode_resample_buffer, READ_BUFFER_SIZE / sizeof(uint32_t));
  auto encoded_size = opus_encode(opus_encoder, (const opus_int16*) encode_resample_buffer, RESAMPLE_BUFFER_SIZE / sizeof(uint16_t), encoder_output_buffer, OPUS_OUT_BUFFER_SIZE);
  peer_connection_send_audio(peer_connection, encoder_output_buffer, encoded_size);
}
