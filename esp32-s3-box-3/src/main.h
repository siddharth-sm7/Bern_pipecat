#include <lvgl.h>
#include <peer.h>

#define LOG_TAG "pipecat"
#define MAX_HTTP_OUTPUT_BUFFER 4096
#define HTTP_TIMEOUT_MS 10000
#define TICK_INTERVAL 15

// Wifi
extern void pipecat_init_wifi();

// WebRTC / Media
extern void pipecat_init_audio_capture();
extern void pipecat_init_audio_decoder();
extern void pipecat_init_audio_encoder();
extern void pipecat_send_audio(PeerConnection *peer_connection);
extern void pipecat_audio_decode(uint8_t *data, size_t size);

// WebRTC / Signalling
extern void pipecat_init_webrtc();
extern void pipecat_webrtc_loop();
extern void pipecat_http_request(char *offer, char *answer);

// Screen
extern lv_style_t STYLE_BLUE;
extern lv_style_t STYLE_GREEN;
extern lv_style_t STYLE_DEFAULT;

extern void pipecat_init_screen();
extern void pipecat_screen_loop();
extern void pipecat_screen_system_log(const char *text);
extern void pipecat_screen_add_log(const char *text, const lv_style_t *style);
