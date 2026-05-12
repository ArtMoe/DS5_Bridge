//
// Created by awalol on 2026/3/4.
//

#ifndef DS5_BRIDGE_BT_H
#define DS5_BRIDGE_BT_H

#include <cstdint>
#include <vector>

enum CHANNEL_TYPE {
    INTERRUPT,
    CONTROL
};

typedef void (*bt_data_callback_t)(CHANNEL_TYPE channel, uint8_t *data, uint16_t len);

int bt_init();
void bt_register_data_callback(bt_data_callback_t callback);
bool bt_is_controller_connected();
uint8_t bt_controller_type();
bool bt_disconnect();
void bt_write(uint8_t* data,uint16_t len);
bool bt_write_classified_output(uint8_t* data,uint16_t len);
bool bt_sanitize_host_speaker_amp_ownership(uint8_t* data,uint16_t len);
bool bt_write_audio_stream(uint8_t* data,uint16_t len);
void bt_drain_audio_stream();
struct bt_output_debug_stats {
    uint32_t audio_0x36_enqueue_to_send_max_us;
    uint32_t audio_0x36_send_gap_max_us;
    uint32_t audio_0x36_late_count_over_12000_us;
    uint32_t audio_0x36_drop_oldest_count;
    uint32_t non_audio_reports_between_audio_max;
    uint32_t bt_audio_queue_depth_max;
    uint32_t audio_0x36_enqueued_count;
    uint32_t audio_0x36_sent_count;
    uint32_t critical_starving_audio_count;
};
void bt_get_output_debug_stats(bt_output_debug_stats *stats);
void bt_set_lightbar_color(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness_percent);
void bt_set_mute_led(bool enabled);
void bt_set_speaker_output_enabled(bool enabled);
void bt_refresh_speaker_output();
void bt_set_adaptive_trigger_effect(uint8_t mode, uint8_t intensity_percent, uint8_t target = 0);
void bt_reset_adaptive_triggers();
void bt_schedule_lightbar_restore(uint32_t delay_ms);
void bt_lightbar_loop();
std::vector<uint8_t> get_feature_data(uint8_t reportId,uint16_t len);
void init_feature();
void set_feature_data(uint8_t reportId, uint8_t* data,uint16_t len);

#endif //DS5_BRIDGE_BT_H
