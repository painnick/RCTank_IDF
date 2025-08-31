// Example file - Public Domain
// Need help? https://tinyurl.com/bluepad32-help

#include <string.h>

#include <uni.h>
#include "rc_tank.h"
#include "dfplayer.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Custom "instance"
typedef struct my_platform_instance_s {
    uni_gamepad_seat_t gamepad_seat;  // which "seat" is being used
} my_platform_instance_t;

// Declarations
static void trigger_event_on_gamepad(uni_hid_device_t* d);
static my_platform_instance_t* get_my_platform_instance(uni_hid_device_t* d);

//
// Platform Overrides
//
static void my_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("custom: init()\n");
    
    // RC Tank 초기화
    rc_tank_init();
    
    // DFPlayer 초기화
    dfplayer_init();

#if 0
    uni_gamepad_mappings_t mappings = GAMEPAD_DEFAULT_MAPPINGS;

    // Inverted axis with inverted Y in RY.
    mappings.axis_x = UNI_GAMEPAD_MAPPINGS_AXIS_RX;
    mappings.axis_y = UNI_GAMEPAD_MAPPINGS_AXIS_RY;
    mappings.axis_ry_inverted = true;
    mappings.axis_rx = UNI_GAMEPAD_MAPPINGS_AXIS_X;
    mappings.axis_ry = UNI_GAMEPAD_MAPPINGS_AXIS_Y;

    // Invert A & B
    mappings.button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_B;
    mappings.button_b = UNI_GAMEPAD_MAPPINGS_BUTTON_A;

    uni_gamepad_set_mappings(&mappings);
#endif
    //    uni_bt_service_set_enabled(true);
}

static void my_platform_on_init_complete(void) {
    logi("custom: on_init_complete()\n");

    // Safe to call "unsafe" functions since they are called from BT thread

    // Start scanning
    uni_bt_start_scanning_and_autoconnect_unsafe();
    uni_bt_allow_incoming_connections(true);

    // Based on runtime condition, you can delete or list the stored BT keys.
    if (1)
        uni_bt_del_keys_unsafe();
    else
        uni_bt_list_keys_unsafe();
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    // You can filter discovered devices here.
    // Just return any value different from UNI_ERROR_SUCCESS;
    // @param addr: the Bluetooth address
    // @param name: could be NULL, could be zero-length, or might contain the name.
    // @param cod: Class of Device. See "uni_bt_defines.h" for possible values.
    // @param rssi: Received Signal Strength Indicator (RSSI) measured in dBms. The higher (255) the better.

    // As an example, if you want to filter out keyboards, do:
    if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
        logi("Ignoring keyboard\n");
        return UNI_ERROR_IGNORE_DEVICE;
    }

    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    logi("custom: device connected: %p\n", d);
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("custom: device disconnected: %p\n", d);
    
    // 게임패드 연결 해제 시 대기 효과음 재생
    rc_tank.is_connected = false;
    rc_tank_stop();  // 모터 정지
    dfplayer_play_file(SOUND_IDLE);
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    logi("custom: device ready: %p\n", d);
    my_platform_instance_t* ins = get_my_platform_instance(d);
    ins->gamepad_seat = GAMEPAD_SEAT_A;

    // 게임패드 연결 시 효과음 재생
    dfplayer_pause();  // 대기 효과음 중단
    dfplayer_play_file(SOUND_CONNECT);
    
    // RC Tank 연결 상태 설정
    rc_tank.is_connected = true;

    trigger_event_on_gamepad(d);
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    static uint8_t leds = 0;
    static uint8_t enabled = true;
    static uni_controller_t prev = {0};
    static bool cannon_firing = false;
    static bool machine_gun_firing = false;
    static uint32_t cannon_start_time = 0;
    static uint32_t machine_gun_start_time = 0;
    uni_gamepad_t* gp;

    // Optimization to avoid processing the previous data so that the console
    // does not get spammed with a lot of logs, but remove it from your project.
    if (memcmp(&prev, ctl, sizeof(*ctl)) == 0) {
        return;
    }
    prev = *ctl;

    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_GAMEPAD:
            gp = &ctl->gamepad;

            // RC Tank 제어
            if (rc_tank.is_connected) {
                // 트랙 제어 (좌측 스틱 Y축, 우측 스틱 Y축)
                float left_y = (float)gp->axis_y / 512.0f;   // -1.0 ~ 1.0
                float right_y = (float)gp->axis_ry / 512.0f; // -1.0 ~ 1.0
                
                // D-PAD 제어
                int dpad_x = 0, dpad_y = 0;
                if (gp->dpad & DPAD_LEFT) dpad_x = -1;
                if (gp->dpad & DPAD_RIGHT) dpad_x = 1;
                if (gp->dpad & DPAD_UP) dpad_y = -1;
                if (gp->dpad & DPAD_DOWN) dpad_y = 1;
                
                // RC Tank 제어
                rc_tank_control_from_gamepad(left_y, right_y, dpad_x, dpad_y);
                
                // 포신 발사 (B 버튼) - A/B 버튼이 뒤바뀜
                if ((gp->buttons & BUTTON_B) && !cannon_firing) {
                    cannon_firing = true;
                    cannon_start_time = esp_timer_get_time() / 1000; // ms
                    
                    // 포신 LED 깜빡임
                    for (int i = 0; i < 5; i++) {
                        gpio_set_level(CANNON_LED_PIN, 1);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        gpio_set_level(CANNON_LED_PIN, 0);
                        vTaskDelay(pdMS_TO_TICKS(50));
                    }
                    
                    // 포신 효과음 재생
                    dfplayer_play_file(SOUND_CANNON_FIRE);
                    
                    // 포신 서보 모터 제어 (당기기)
                    rc_tank_set_cannon_angle(45);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    rc_tank_set_cannon_angle(0);
                }
                
                // 기관총 발사 (A 버튼) - A/B 버튼이 뒤바뀜
                if ((gp->buttons & BUTTON_A) && !machine_gun_firing) {
                    machine_gun_firing = true;
                    machine_gun_start_time = esp_timer_get_time() / 1000; // ms
                    
                    // 기관총 효과음 재생
                    dfplayer_play_file(SOUND_MACHINE_GUN);
                }
                
                // 헤드라이트 토글 (R1 버튼)
                if (gp->buttons & BUTTON_SHOULDER_R) {
                    rc_tank_toggle_headlight();
                    vTaskDelay(pdMS_TO_TICKS(200)); // 디바운싱
                }
                
                // 속도 조절 (X/Y 버튼 + D-PAD)
                if (gp->buttons & BUTTON_X) {
                    if (dpad_y > 0) {
                        rc_tank.left_speed_multiplier += 0.02f;
                        if (rc_tank.left_speed_multiplier > 2.0f) rc_tank.left_speed_multiplier = 2.0f;
                        rc_tank_save_speed_multipliers();
                    } else if (dpad_y < 0) {
                        rc_tank.left_speed_multiplier -= 0.02f;
                        if (rc_tank.left_speed_multiplier < 0.1f) rc_tank.left_speed_multiplier = 0.1f;
                        rc_tank_save_speed_multipliers();
                    }
                }
                
                if (gp->buttons & BUTTON_Y) {
                    if (dpad_y > 0) {
                        rc_tank.right_speed_multiplier += 0.02f;
                        if (rc_tank.right_speed_multiplier > 2.0f) rc_tank.right_speed_multiplier = 2.0f;
                        rc_tank_save_speed_multipliers();
                    } else if (dpad_y < 0) {
                        rc_tank.right_speed_multiplier -= 0.02f;
                        if (rc_tank.right_speed_multiplier < 0.1f) rc_tank.right_speed_multiplier = 0.1f;
                        rc_tank_save_speed_multipliers();
                    }
                }
                
                // 기관총 발사 시간 체크 (3초)
                if (machine_gun_firing) {
                    uint32_t current_time = esp_timer_get_time() / 1000;
                    if (current_time - machine_gun_start_time >= 3000) {
                        machine_gun_firing = false;
                        // 포신 LED 점멸 중단
                        gpio_set_level(CANNON_LED_PIN, 0);
                    } else {
                        // 포신 LED 점멸 (500ms 간격)
                        static uint32_t last_blink = 0;
                        if (current_time - last_blink >= 500) {
                            static bool led_state = false;
                            led_state = !led_state;
                            gpio_set_level(CANNON_LED_PIN, led_state ? 1 : 0);
                            last_blink = current_time;
                        }
                    }
                }
                
                // 포신 발사 시간 체크 (500ms)
                if (cannon_firing) {
                    uint32_t current_time = esp_timer_get_time() / 1000;
                    if (current_time - cannon_start_time >= 500) {
                        cannon_firing = false;
                    }
                }
            }

            // Toggle Bluetooth connections
            if ((gp->buttons & BUTTON_SHOULDER_L) && enabled) {
                logi("*** Stop scanning\n");
                uni_bt_stop_scanning_safe();
                enabled = false;
            }
            if ((gp->buttons & BUTTON_SHOULDER_R) && !enabled) {
                logi("*** Start scanning\n");
                uni_bt_start_scanning_and_autoconnect_safe();
                enabled = true;
            }
            break;
        default:
            break;
    }
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    switch (event) {
        case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON: {
            uni_hid_device_t* d = data;

            if (d == NULL) {
                loge("ERROR: my_platform_on_oob_event: Invalid NULL device\n");
                return;
            }
            logi("custom: on_device_oob_event(): %d\n", event);

            my_platform_instance_t* ins = get_my_platform_instance(d);
            ins->gamepad_seat = ins->gamepad_seat == GAMEPAD_SEAT_A ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;

            trigger_event_on_gamepad(d);
            break;
        }

        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
            logi("custom: Bluetooth enabled: %d\n", (bool)(data));
            break;

        default:
            logi("my_platform_on_oob_event: unsupported event: 0x%04x\n", event);
            break;
    }
}

//
// Helpers
//
static my_platform_instance_t* get_my_platform_instance(uni_hid_device_t* d) {
    return (my_platform_instance_t*)&d->platform_data[0];
}

static void trigger_event_on_gamepad(uni_hid_device_t* d) {
    my_platform_instance_t* ins = get_my_platform_instance(d);

    if (d->report_parser.play_dual_rumble != NULL) {
        d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 150 /* duration ms */, 128 /* weak magnitude */,
                                          40 /* strong magnitude */);
    }

    if (d->report_parser.set_player_leds != NULL) {
        d->report_parser.set_player_leds(d, ins->gamepad_seat);
    }

    if (d->report_parser.set_lightbar_color != NULL) {
        uint8_t red = (ins->gamepad_seat & 0x01) ? 0xff : 0;
        uint8_t green = (ins->gamepad_seat & 0x02) ? 0xff : 0;
        uint8_t blue = (ins->gamepad_seat & 0x04) ? 0xff : 0;
        d->report_parser.set_lightbar_color(d, red, green, blue);
    }
}

//
// Entry Point
//
struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "custom",
        .init = my_platform_init,
        .on_init_complete = my_platform_on_init_complete,
        .on_device_discovered = my_platform_on_device_discovered,
        .on_device_connected = my_platform_on_device_connected,
        .on_device_disconnected = my_platform_on_device_disconnected,
        .on_device_ready = my_platform_on_device_ready,
        .on_oob_event = my_platform_on_oob_event,
        .on_controller_data = my_platform_on_controller_data,
        .get_property = my_platform_get_property,
    };

    return &plat;
}
