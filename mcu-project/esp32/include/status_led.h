#pragma once

bool status_led_init();
void status_led_audio_received();
void status_led_set_upload_active(bool active);
void status_led_set_download_active(bool active);
void status_led_set_bootloader_active(bool active);
