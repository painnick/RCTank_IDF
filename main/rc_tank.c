#include "rc_tank.h"
#include "dfplayer.h"
#include "driver/mcpwm_prelude.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>

static const char* TAG = "RC_TANK";

// 전역 변수
rc_tank_control_t rc_tank = {0};

// MCPWM 핸들 정의 (모터 제어용)
static mcpwm_timer_handle_t left_track_timer = NULL;
static mcpwm_timer_handle_t right_track_timer = NULL;
static mcpwm_timer_handle_t turret_timer = NULL;

static mcpwm_oper_handle_t left_track_oper = NULL;
static mcpwm_oper_handle_t right_track_oper = NULL;
static mcpwm_oper_handle_t turret_oper = NULL;

static mcpwm_cmpr_handle_t left_track_cmpr_a = NULL;
static mcpwm_cmpr_handle_t left_track_cmpr_b = NULL;
static mcpwm_cmpr_handle_t right_track_cmpr_a = NULL;
static mcpwm_cmpr_handle_t right_track_cmpr_b = NULL;
static mcpwm_cmpr_handle_t turret_cmpr_a = NULL;
static mcpwm_cmpr_handle_t turret_cmpr_b = NULL;

static mcpwm_gen_handle_t left_track_gen_a = NULL;
static mcpwm_gen_handle_t left_track_gen_b = NULL;
static mcpwm_gen_handle_t right_track_gen_a = NULL;
static mcpwm_gen_handle_t right_track_gen_b = NULL;
static mcpwm_gen_handle_t turret_gen_a = NULL;
static mcpwm_gen_handle_t turret_gen_b = NULL;

// GPIO 설정
static void setup_gpio(void) {
    // LED 핀 설정
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << CANNON_LED_PIN) | (1ULL << HEADLIGHT_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_config);
    
    // LED 초기 상태 설정
    gpio_set_level(CANNON_LED_PIN, 0);
    gpio_set_level(HEADLIGHT_LED_PIN, 0);
}

// LEDC 초기화 (서보 모터용)
static void setup_ledc(void) {
    // LEDC 타이머 설정
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);
    
    // 포 마운트 서보 모터 채널 설정
    ledc_channel_config_t mount_channel = {
        .channel = LEDC_CHANNEL_MOUNT,
        .duty = 0,
        .gpio_num = MOUNT_SERVO_PIN,
        .speed_mode = LEDC_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER
    };
    ledc_channel_config(&mount_channel);
    
    // 포신 서보 모터 채널 설정
    ledc_channel_config_t cannon_channel = {
        .channel = LEDC_CHANNEL_CANNON,
        .duty = 0,
        .gpio_num = CANNON_SERVO_PIN,
        .speed_mode = LEDC_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER
    };
    ledc_channel_config(&cannon_channel);
}

void rc_tank_init(void) {
    ESP_LOGI(TAG, "RC Tank initialization started");
    
    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // GPIO 설정
    setup_gpio();
    
    // LEDC 초기화 (서보 모터용)
    setup_ledc();
    
    // MCPWM 초기화 (모터 제어용)
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = MCPWM_TIMER_RESOLUTION,
        .period_ticks = MCPWM_TIMER_RESOLUTION / MCPWM_FREQ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    
    // 좌측 트랙 MCPWM 설정
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &left_track_timer));
    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){}, &left_track_oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(left_track_oper, left_track_timer));
    
    mcpwm_comparator_config_t cmpr_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(left_track_oper, &cmpr_config, &left_track_cmpr_a));
    ESP_ERROR_CHECK(mcpwm_new_comparator(left_track_oper, &cmpr_config, &left_track_cmpr_b));
    
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = LEFT_TRACK_IN1_PIN,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(left_track_oper, &gen_config, &left_track_gen_a));
    gen_config.gen_gpio_num = LEFT_TRACK_IN2_PIN;
    ESP_ERROR_CHECK(mcpwm_new_generator(left_track_oper, &gen_config, &left_track_gen_b));
    
    // 우측 트랙 MCPWM 설정
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &right_track_timer));
    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){}, &right_track_oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(right_track_oper, right_track_timer));
    
    ESP_ERROR_CHECK(mcpwm_new_comparator(right_track_oper, &cmpr_config, &right_track_cmpr_a));
    ESP_ERROR_CHECK(mcpwm_new_comparator(right_track_oper, &cmpr_config, &right_track_cmpr_b));
    
    gen_config.gen_gpio_num = RIGHT_TRACK_IN1_PIN;
    ESP_ERROR_CHECK(mcpwm_new_generator(right_track_oper, &gen_config, &right_track_gen_a));
    gen_config.gen_gpio_num = RIGHT_TRACK_IN2_PIN;
    ESP_ERROR_CHECK(mcpwm_new_generator(right_track_oper, &gen_config, &right_track_gen_b));
    
    // 터렛 MCPWM 설정
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &turret_timer));
    ESP_ERROR_CHECK(mcpwm_new_operator(&(mcpwm_operator_config_t){}, &turret_oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(turret_oper, turret_timer));
    
    ESP_ERROR_CHECK(mcpwm_new_comparator(turret_oper, &cmpr_config, &turret_cmpr_a));
    ESP_ERROR_CHECK(mcpwm_new_comparator(turret_oper, &cmpr_config, &turret_cmpr_b));
    
    gen_config.gen_gpio_num = TURRET_IN1_PIN;
    ESP_ERROR_CHECK(mcpwm_new_generator(turret_oper, &gen_config, &turret_gen_a));
    gen_config.gen_gpio_num = TURRET_IN2_PIN;
    ESP_ERROR_CHECK(mcpwm_new_generator(turret_oper, &gen_config, &turret_gen_b));
    
    // 타이머 시작
    ESP_ERROR_CHECK(mcpwm_timer_enable(left_track_timer));
    ESP_ERROR_CHECK(mcpwm_timer_enable(right_track_timer));
    ESP_ERROR_CHECK(mcpwm_timer_enable(turret_timer));
    
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(left_track_timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(right_track_timer, MCPWM_TIMER_START_NO_STOP));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(turret_timer, MCPWM_TIMER_START_NO_STOP));
    
    // 초기 상태 설정
    rc_tank.state = RC_TANK_STOP;
    rc_tank.left_track_speed = 0;
    rc_tank.right_track_speed = 0;
    rc_tank.turret_speed = 0;
    rc_tank.mount_angle = 90;  // 중앙 위치
    rc_tank.cannon_angle = 0;
    rc_tank.headlight_on = false;
    rc_tank.is_connected = false;
    
    // 속도 배율 로드
    rc_tank_load_speed_multipliers();
    
    // 모터 정지
    rc_tank_stop();
    
    ESP_LOGI(TAG, "RC Tank initialization completed");
}

void rc_tank_set_track_speed(int left_speed, int right_speed) {
    // 속도 범위 제한 (-255 ~ 255)
    left_speed = (left_speed > 255) ? 255 : (left_speed < -255) ? -255 : left_speed;
    right_speed = (right_speed > 255) ? 255 : (right_speed < -255) ? -255 : right_speed;
    
    // 속도 배율 적용
    left_speed = (int)(left_speed * rc_tank.left_speed_multiplier);
    right_speed = (int)(right_speed * rc_tank.right_speed_multiplier);
    
    rc_tank.left_track_speed = left_speed;
    rc_tank.right_track_speed = right_speed;
    
    // MCPWM period_ticks 계산 (10MHz / 5kHz = 2000)
    const uint32_t period_ticks = MCPWM_TIMER_RESOLUTION / MCPWM_FREQ;
    
    // 왼쪽 트랙 제어
    if (left_speed > 0) {
        // 전진 - duty cycle을 period_ticks 범위 내로 조정
        uint32_t compare_value = (uint32_t)((left_speed * period_ticks) / 255);
        mcpwm_comparator_set_compare_value(left_track_cmpr_a, compare_value);
        mcpwm_comparator_set_compare_value(left_track_cmpr_b, 0);
    } else if (left_speed < 0) {
        // 후진
        uint32_t compare_value = (uint32_t)((-left_speed * period_ticks) / 255);
        mcpwm_comparator_set_compare_value(left_track_cmpr_a, 0);
        mcpwm_comparator_set_compare_value(left_track_cmpr_b, compare_value);
    } else {
        // 정지
        mcpwm_comparator_set_compare_value(left_track_cmpr_a, 0);
        mcpwm_comparator_set_compare_value(left_track_cmpr_b, 0);
    }
    
    // 오른쪽 트랙 제어
    if (right_speed > 0) {
        // 전진
        uint32_t compare_value = (uint32_t)((right_speed * period_ticks) / 255);
        mcpwm_comparator_set_compare_value(right_track_cmpr_a, compare_value);
        mcpwm_comparator_set_compare_value(right_track_cmpr_b, 0);
    } else if (right_speed < 0) {
        // 후진
        uint32_t compare_value = (uint32_t)((-right_speed * period_ticks) / 255);
        mcpwm_comparator_set_compare_value(right_track_cmpr_a, 0);
        mcpwm_comparator_set_compare_value(right_track_cmpr_b, compare_value);
    } else {
        // 정지
        mcpwm_comparator_set_compare_value(right_track_cmpr_a, 0);
        mcpwm_comparator_set_compare_value(right_track_cmpr_b, 0);
    }
    
    ESP_LOGD(TAG, "Track speed set: left=%d, right=%d", left_speed, right_speed);
}

void rc_tank_set_turret_speed(int speed) {
    // 속도 범위 제한 (-255 ~ 255)
    speed = (speed > 255) ? 255 : (speed < -255) ? -255 : speed;
    
    rc_tank.turret_speed = speed;
    
    // MCPWM period_ticks 계산 (10MHz / 5kHz = 2000)
    const uint32_t period_ticks = MCPWM_TIMER_RESOLUTION / MCPWM_FREQ;
    
    // 터렛 제어
    if (speed > 0) {
        // 시계방향 회전
        uint32_t compare_value = (uint32_t)((speed * period_ticks) / 255);
        mcpwm_comparator_set_compare_value(turret_cmpr_a, compare_value);
        mcpwm_comparator_set_compare_value(turret_cmpr_b, 0);
    } else if (speed < 0) {
        // 반시계방향 회전
        uint32_t compare_value = (uint32_t)((-speed * period_ticks) / 255);
        mcpwm_comparator_set_compare_value(turret_cmpr_a, 0);
        mcpwm_comparator_set_compare_value(turret_cmpr_b, compare_value);
    } else {
        // 정지
        mcpwm_comparator_set_compare_value(turret_cmpr_a, 0);
        mcpwm_comparator_set_compare_value(turret_cmpr_b, 0);
    }
    
    ESP_LOGD(TAG, "Turret speed set: %d", speed);
}

void rc_tank_set_mount_angle(int angle) {
    // 각도 범위 제한
    if (angle < MOUNT_MIN_ANGLE) angle = MOUNT_MIN_ANGLE;
    if (angle > MOUNT_MAX_ANGLE) angle = MOUNT_MAX_ANGLE;
    
    rc_tank.mount_angle = angle;
    
    // 서보 모터 제어 (50Hz PWM, 0.5ms~2.5ms 펄스)
    // 0도 = 0.5ms = 2.5% duty cycle
    // 180도 = 2.5ms = 12.5% duty cycle
    float duty_percent = 2.5f + (angle * 10.0f) / 180.0f;
    uint32_t duty = (uint32_t)((duty_percent / 100.0f) * ((1 << LEDC_DUTY_RES) - 1));
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_MOUNT, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_MOUNT);
    
    ESP_LOGD(TAG, "Mount angle set: %d, duty: %.1f%%", angle, duty_percent);
}

void rc_tank_set_cannon_angle(int angle) {
    // 각도 범위 제한
    if (angle < CANNON_MIN_ANGLE) angle = CANNON_MIN_ANGLE;
    if (angle > CANNON_MAX_ANGLE) angle = CANNON_MAX_ANGLE;
    
    rc_tank.cannon_angle = angle;
    
    // 서보 모터 제어 (50Hz PWM, 0.5ms~2.5ms 펄스)
    // 0도 = 0.5ms = 2.5% duty cycle
    // 90도 = 2.5ms = 12.5% duty cycle
    float duty_percent = 2.5f + (angle * 10.0f) / 90.0f;
    uint32_t duty = (uint32_t)((duty_percent / 100.0f) * ((1 << LEDC_DUTY_RES) - 1));
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_CANNON, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_CANNON);
    
    ESP_LOGD(TAG, "Cannon angle set: %d, duty: %.1f%%", angle, duty_percent);
}

void rc_tank_toggle_headlight(void) {
    rc_tank.headlight_on = !rc_tank.headlight_on;
    gpio_set_level(HEADLIGHT_LED_PIN, rc_tank.headlight_on ? 1 : 0);
    ESP_LOGI(TAG, "Headlight %s", rc_tank.headlight_on ? "ON" : "OFF");
}

void rc_tank_stop(void) {
    rc_tank.state = RC_TANK_STOP;
    rc_tank_set_track_speed(0, 0);
    rc_tank_set_turret_speed(0);
    ESP_LOGI(TAG, "RC Tank stopped");
}

void rc_tank_control_from_gamepad(float left_y, float right_y, int dpad_x, int dpad_y) {
    // 데드존 설정 (조이스틱 노이즈 제거)
    const float DEADZONE = 0.1f;
    
    // 좌측 스틱으로 좌측 트랙 제어
    if (fabs(left_y) < DEADZONE) {
        left_y = 0;
    }
    
    // 우측 스틱으로 우측 트랙 제어
    if (fabs(right_y) < DEADZONE) {
        right_y = 0;
    }
    
    // 트랙 속도 계산 (최대 255)
    int left_speed = (int)(left_y * 255);
    int right_speed = (int)(right_y * 255);
    
    // 트랙 속도 설정
    rc_tank_set_track_speed(left_speed, right_speed);
    
    // D-PAD로 터렛 제어
    if (dpad_x != 0) {
        int turret_speed = (dpad_x > 0) ? 128 : -128;  // 중간 속도
        rc_tank_set_turret_speed(turret_speed);
    } else {
        rc_tank_set_turret_speed(0);
    }
    
    // D-PAD로 포 마운트 각도 제어
    if (dpad_y != 0) {
        int current_angle = rc_tank.mount_angle;
        int new_angle = current_angle + (dpad_y * 5);  // 5도씩 조절
        rc_tank_set_mount_angle(new_angle);
    }
    
        ESP_LOGD(TAG, "Gamepad control: LY=%.2f, RY=%.2f, DPAD_X=%d, DPAD_Y=%d",
             left_y, right_y, dpad_x, dpad_y);
}

void rc_tank_save_speed_multipliers(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("rc_tank", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }
    
    err = nvs_set_blob(nvs_handle, "speed_mult", &rc_tank.left_speed_multiplier, sizeof(float) * 2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Speed multiplier save failed: %s", esp_err_to_name(err));
    } else {
                ESP_LOGI(TAG, "Speed multiplier saved: left=%.2f, right=%.2f",
                 rc_tank.left_speed_multiplier, rc_tank.right_speed_multiplier);
    }
    
    nvs_close(nvs_handle);
}

void rc_tank_load_speed_multipliers(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("rc_tank", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed, using default values: %s", esp_err_to_name(err));
        rc_tank.left_speed_multiplier = 1.0f;
        rc_tank.right_speed_multiplier = 1.0f;
        return;
    }
    
    size_t required_size = sizeof(float) * 2;
    err = nvs_get_blob(nvs_handle, "speed_mult", &rc_tank.left_speed_multiplier, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Speed multiplier load failed, using default values: %s", esp_err_to_name(err));
        rc_tank.left_speed_multiplier = 1.0f;
        rc_tank.right_speed_multiplier = 1.0f;
    } else {
                ESP_LOGI(TAG, "Speed multiplier loaded: left=%.2f, right=%.2f",
                 rc_tank.left_speed_multiplier, rc_tank.right_speed_multiplier);
    }
    
    nvs_close(nvs_handle);
}

void rc_tank_update_state(void) {
    // 상태 업데이트 로직 (필요시 구현)
    // 예: 배터리 상태, 센서 읽기 등
}
