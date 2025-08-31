#ifndef RC_TANK_H
#define RC_TANK_H

#include <stdint.h>
#include <stdbool.h>

// RC Tank 핀 정의
#define LEFT_TRACK_IN1_PIN    25  // 좌측 트랙 IN1
#define LEFT_TRACK_IN2_PIN    26  // 좌측 트랙 IN2
#define RIGHT_TRACK_IN1_PIN   27  // 우측 트랙 IN1
#define RIGHT_TRACK_IN2_PIN   13  // 우측 트랙 IN2
#define TURRET_IN1_PIN        22  // 터렛 IN1
#define TURRET_IN2_PIN        21  // 터렛 IN2

// LED 핀
#define CANNON_LED_PIN        4   // 포신 LED
#define HEADLIGHT_LED_PIN     16  // 헤드라이트 LED

// 서보 모터 핀
#define CANNON_SERVO_PIN      18  // 포신 서보 모터
#define MOUNT_SERVO_PIN       19  // 포 마운트 서보 모터

// DFPlayer 핀
#define DFPLAYER_RX_PIN       32  // DFPlayer RX
#define DFPLAYER_TX_PIN       33  // DFPlayer TX

// MCPWM 설정
#define MCPWM_FREQ           5000
#define MCPWM_TIMER_RESOLUTION 10000000  // 10MHz

// 서보 모터 각도 범위
#define MOUNT_MIN_ANGLE       0
#define MOUNT_MAX_ANGLE       180
#define CANNON_MIN_ANGLE      0
#define CANNON_MAX_ANGLE      90

// RC Tank 상태
typedef enum {
    RC_TANK_STOP = 0,
    RC_TANK_FORWARD,
    RC_TANK_BACKWARD,
    RC_TANK_LEFT,
    RC_TANK_RIGHT,
    RC_TANK_FORWARD_LEFT,
    RC_TANK_FORWARD_RIGHT,
    RC_TANK_BACKWARD_LEFT,
    RC_TANK_BACKWARD_RIGHT
} rc_tank_state_t;

// RC Tank 제어 구조체
typedef struct {
    rc_tank_state_t state;
    int left_track_speed;
    int right_track_speed;
    int turret_speed;
    int mount_angle;
    int cannon_angle;
    bool headlight_on;
    bool is_connected;
    float left_speed_multiplier;
    float right_speed_multiplier;
} rc_tank_control_t;

// 함수 선언
void rc_tank_init(void);
void rc_tank_set_track_speed(int left_speed, int right_speed);
void rc_tank_set_turret_speed(int speed);
void rc_tank_set_mount_angle(int angle);
void rc_tank_set_cannon_angle(int angle);
void rc_tank_toggle_headlight(void);
void rc_tank_stop(void);
void rc_tank_control_from_gamepad(float left_y, float right_y, int dpad_x, int dpad_y);
void rc_tank_update_state(void);
void rc_tank_save_speed_multipliers(void);
void rc_tank_load_speed_multipliers(void);

extern rc_tank_control_t rc_tank;

#endif // RC_TANK_H
