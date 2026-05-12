//
// Created by awalol on 2026/3/5.
// Modified for DS5 Bridge companion firmware and app integration.
//

#ifndef DS5_BRIDGE_AUDIO_H
#define DS5_BRIDGE_AUDIO_H

#include <cstdint>

void audio_init();
void audio_loop();
void audio_test_haptics_loop();
bool audio_schedule_test_haptics();
bool audio_test_haptics_busy();
bool audio_test_haptics_cooldown();
bool audio_recent();
bool audio_haptics_ready();
void audio_set_quiet_mode(bool enabled);
bool audio_quiet_mode_enabled();
void audio_debug_copy_report_payload(uint8_t *buffer, uint8_t max_len);
struct audio_debug_stats {
    uint32_t usb_audio_gap_max_us;
    uint32_t usb_audio_gap_over_1500_count;
    uint32_t opus_encode_max_us;
    uint32_t opus_encode_over_budget_count;
    uint32_t audio_generation_drop_count;
};
void audio_debug_get_stats(audio_debug_stats *stats);
void audio_set_haptics_buffer_length(uint8_t length);
uint8_t audio_haptics_buffer_length();
void audio_set_state_data(uint8_t const *data, uint8_t len);
void audio_set_lightbar_state(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness_percent);
void audio_handle_controller_disconnect();
void set_headset(bool state);

#endif //DS5_BRIDGE_AUDIO_H
