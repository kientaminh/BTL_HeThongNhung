#ifndef ESP_LINK_H
#define ESP_LINK_H

#include <stdint.h>

// So mau trong 1 khung gui ESP: phai khop AudioFrame::sample_count trong esp32/include/audio_frame.h
#define ESP_LINK_FRAME_SAMPLES 1024
// So khung toi da nguoi goi duoc giu o esp_link cung luc (da submit, chua take_sent)
#define ESP_LINK_MAX_IN_FLIGHT 3

void esp_link_init(void);
void esp_link_submit(int16_t *p_pcm);
int16_t *esp_link_take_sent(void);

#endif /* ESP_LINK_H */
