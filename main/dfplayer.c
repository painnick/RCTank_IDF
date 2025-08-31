#include "dfplayer.h"
#include "rc_tank.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "DFPLAYER";

// DFPlayer 통신 설정
#define UART_NUM UART_NUM_1
#define BUF_SIZE 1024
#define DFPLAYER_TIMEOUT 1000

// DFPlayer 패킷 구조
typedef struct {
    uint8_t start_byte;    // 0x7E
    uint8_t version;       // 0xFF
    uint8_t length;        // 데이터 길이
    uint8_t command;       // 명령어
    uint8_t feedback;      // 피드백
    uint8_t param_msb;     // 파라미터 상위 바이트
    uint8_t param_lsb;     // 파라미터 하위 바이트
    uint8_t checksum_msb;  // 체크섬 상위 바이트
    uint8_t checksum_lsb;  // 체크섬 하위 바이트
    uint8_t end_byte;      // 0xEF
} dfplayer_packet_t;

// 체크섬 계산
static uint16_t calculate_checksum(uint8_t* data, uint8_t length) {
    uint16_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += data[i];
    }
    return -sum;
}

// DFPlayer에 명령 전송
static esp_err_t dfplayer_send_command(uint8_t command, uint16_t parameter) {
    dfplayer_packet_t packet;
    packet.start_byte = 0x7E;
    packet.version = 0xFF;
    packet.length = 0x06;
    packet.command = command;
    packet.feedback = 0x00;
    packet.param_msb = (parameter >> 8) & 0xFF;
    packet.param_lsb = parameter & 0xFF;
    
    uint16_t checksum = calculate_checksum((uint8_t*)&packet.version, 6);
    packet.checksum_msb = (checksum >> 8) & 0xFF;
    packet.checksum_lsb = checksum & 0xFF;
    packet.end_byte = 0xEF;
    
    int written = uart_write_bytes(UART_NUM, &packet, sizeof(packet));
    if (written != sizeof(packet)) {
        ESP_LOGE(TAG, "DFPlayer command transmission failed");
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "DFPlayer command sent: CMD=0x%02X, PARAM=0x%04X", command, parameter);
    return ESP_OK;
}

void dfplayer_init(void) {
    ESP_LOGI(TAG, "DFPlayer initialization started");
    
    // UART 설정
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    esp_err_t ret = uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver installation failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = uart_param_config(UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART parameter configuration failed: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = uart_set_pin(UART_NUM, DFPLAYER_TX_PIN, DFPLAYER_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART pin configuration failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // DFPlayer 초기화 명령
    vTaskDelay(pdMS_TO_TICKS(1000));  // DFPlayer 부팅 대기
    
    // 볼륨 설정 (20/30)
    dfplayer_set_volume(20);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // EQ 설정 (Normal)
    dfplayer_send_command(DFPLAYER_CMD_SET_EQ, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 재생 모드 설정 (Repeat all)
    dfplayer_send_command(DFPLAYER_CMD_PLAY_MODE, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 대기 효과음 재생 시작
    dfplayer_play_file(SOUND_IDLE);
    
    ESP_LOGI(TAG, "DFPlayer initialization completed");
}

void dfplayer_play_file(uint8_t file_number) {
    ESP_LOGI(TAG, "DFPlayer playing file: %d", file_number);
    dfplayer_send_command(DFPLAYER_CMD_PLAY_FILE, file_number);
}

void dfplayer_set_volume(uint8_t volume) {
    if (volume > 30) volume = 30;
    ESP_LOGI(TAG, "DFPlayer volume set: %d", volume);
    dfplayer_send_command(DFPLAYER_CMD_SET_VOLUME, volume);
}

void dfplayer_pause(void) {
    ESP_LOGI(TAG, "DFPlayer paused");
    dfplayer_send_command(DFPLAYER_CMD_PAUSE, 0);
}

void dfplayer_resume(void) {
    ESP_LOGI(TAG, "DFPlayer resumed");
    dfplayer_send_command(DFPLAYER_CMD_PLAY, 0);
}

void dfplayer_stop(void) {
    ESP_LOGI(TAG, "DFPlayer stopped");
    dfplayer_send_command(DFPLAYER_CMD_PAUSE, 0);
}

bool dfplayer_is_playing(void) {
    // 간단한 구현 - 실제로는 DFPlayer로부터 상태를 읽어와야 함
    return true;
}
