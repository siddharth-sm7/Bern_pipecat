#include <peer.h>

#define LOG_TAG "pipecat"
#define MAX_HTTP_OUTPUT_BUFFER 4096
#define HTTP_TIMEOUT_MS 10000

void pipecat_wifi(void);
void pipecat_init_audio_capture(void);
void pipecat_init_audio_decoder(void);
void pipecat_init_audio_encoder();
void pipecat_send_audio(PeerConnection *peer_connection);
void pipecat_audio_decode(uint8_t *data, size_t size);
void pipecat_webrtc();
void pipecat_http_request(char *offer, char *answer);
