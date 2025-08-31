#ifndef DFPLAYER_H
#define DFPLAYER_H

#include <stdint.h>
#include <stdbool.h>

// DFPlayer 명령어
#define DFPLAYER_CMD_PLAY_NEXT      0x01
#define DFPLAYER_CMD_PLAY_PREV      0x02
#define DFPLAYER_CMD_PLAY_FILE      0x03
#define DFPLAYER_CMD_VOLUME_UP      0x04
#define DFPLAYER_CMD_VOLUME_DOWN    0x05
#define DFPLAYER_CMD_SET_VOLUME     0x06
#define DFPLAYER_CMD_SET_EQ         0x07
#define DFPLAYER_CMD_PLAY_MODE      0x08
#define DFPLAYER_CMD_PLAY_SOURCE    0x09
#define DFPLAYER_CMD_STANDBY        0x0A
#define DFPLAYER_CMD_NORMAL         0x0B
#define DFPLAYER_CMD_RESET          0x0C
#define DFPLAYER_CMD_PLAY           0x0D
#define DFPLAYER_CMD_PAUSE          0x0E
#define DFPLAYER_CMD_PLAY_FOLDER    0x0F
#define DFPLAYER_CMD_VOLUME_ADJUST  0x10
#define DFPLAYER_CMD_REPEAT_PLAY    0x11

// 효과음 파일 번호
#define SOUND_IDLE          1   // 대기 시 반복 재생
#define SOUND_CANNON_FIRE   2   // 포신 발사 시 재생
#define SOUND_MACHINE_GUN   3   // 기관총 발사 시 재생
#define SOUND_CONNECT       4   // 게임 패드 연결 시 재생

// 함수 선언
void dfplayer_init(void);
void dfplayer_play_file(uint8_t file_number);
void dfplayer_set_volume(uint8_t volume);
void dfplayer_pause(void);
void dfplayer_resume(void);
void dfplayer_stop(void);
bool dfplayer_is_playing(void);

#endif // DFPLAYER_H
