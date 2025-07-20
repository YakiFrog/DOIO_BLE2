#ifndef PYTHON_STYLE_ANALYZER_H
#define PYTHON_STYLE_ANALYZER_H

// HID_CLASS定義の競合を回避
#ifdef HID_CLASS
#undef HID_CLASS
#endif
#ifdef HID_SUBCLASS_NONE
#undef HID_SUBCLASS_NONE
#endif

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <BleKeyboard.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// HID関連の定義の競合を防ぐため、再度チェック
#ifdef HID_CLASS
#undef HID_CLASS
#endif
#ifdef HID_SUBCLASS_NONE
#undef HID_SUBCLASS_NONE
#endif

#include "EspUsbHost.h"
#include "Peripherals.h"

// DOIO KB16デバイス情報
#define DOIO_VID 0xD010
#define DOIO_PID 0x1601

// デバッグ設定
#define DEBUG_ENABLED 1
#define SERIAL_OUTPUT_ENABLED 1

// ディスプレイ設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// パフォーマンス設定
#define USB_TASK_INTERVAL 0
#define DISPLAY_UPDATE_INTERVAL 50
#define KEY_REPEAT_DELAY 200
#define KEY_REPEAT_RATE 30

// キーコードマッピング構造体
struct KeycodeMapping {
    uint8_t keycode;
    const char* normal;
    const char* shifted;
};

// レポート形式構造体
struct ReportFormat {
    int size;
    int modifier_index;
    int reserved_index;
    int key_indices[6];
    String format;
};

// PythonアナライザーのUSBホストクラス（KB16認識対応修正版）
class PythonStyleAnalyzer : public EspUsbHost {
private:
    U8G2* display;
    BleKeyboard* bleKeyboard;
    
    // Pythonアナライザーの状態変数（完全一致）
    uint8_t last_report[32] = {0};
    bool has_last_report = false;
    int report_size = 16;  // DOIO KB16は16バイト
    ReportFormat report_format;
    bool report_format_initialized = false;
    
    // デバイス情報
    bool is_doio_kb16 = false;
    bool isConnected = false;
    
    // OLED表示用データ
    String lastHexData = "";
    String lastKeyPresses = "";
    String currentPressedKeys = "";
    String lastCharacters = "";
    bool displayNeedsUpdate = false;
    unsigned long lastDisplayUpdate = 0;
    unsigned long lastKeyEventTime = 0;

    // BLE送信用（重複防止）
    String lastSentChars = "";
    
    // BLE送信タイミング計測用
    unsigned long lastBleTransmissionTime = 0;
    unsigned long bleTransmissionCount = 0;
    
    // パフォーマンス統計用
    unsigned long minTransmissionInterval = 999999;
    unsigned long maxTransmissionInterval = 0;
    unsigned long totalTransmissionInterval = 0;
    unsigned long intervalCount = 0;
    unsigned long lastStatsReport = 0;
    static const unsigned long STATS_REPORT_INTERVAL = 10000;  // 10秒間隔で統計レポート
    
    // 長押し検出用
    String currentPressedChars = "";
    unsigned long keyPressStartTime = 0;
    unsigned long lastRepeatTime = 0;
    bool isRepeating = false;
    static const unsigned long REPEAT_DELAY = 250;   // 長押し開始までの遅延（ms）- 極限高速化
    static const unsigned long REPEAT_RATE = 50;     // リピート間隔（ms）- 極限高速化


public:
    PythonStyleAnalyzer(U8G2* disp, BleKeyboard* bleKbd);
    
    // アイドル状態のディスプレイ更新（publicメソッド）
    void updateDisplayIdle();
    
    // パフォーマンス統計レポート
    void reportPerformanceStats();
    
    // 長押しリピート処理（publicメソッド）
    void handleKeyRepeat();
    
    // 複数文字を効率的に送信
    void sendString(const String& chars);  // 複数文字を効率的に送信

private:
    
    // ディスプレイ更新用のヘルパー関数
    void updateDisplayForDevice(const String& deviceType);
    
    // キー押下時のディスプレイ更新
    void updateDisplayWithKeys(const String& hexData, const String& keyNames, const String& characters, bool shiftPressed = false);
    
    // Pythonのkeycode_to_string関数を完全移植
    String keycodeToString(uint8_t keycode, bool shift = false);
    
    // Pythonの_analyze_report_format関数を完全移植
    ReportFormat analyzeReportFormat(const uint8_t* report_data, int data_size);
    
    // Pythonのpretty_print_report関数を完全移植
    void prettyPrintReport(const uint8_t* report_data, int data_size);
    
    // BLE送信用のヘルパー関数
    void sendSingleCharacter(const String& character);
    void sendSingleCharacterFast(const String& character);  // 高速化版単一文字送信
    void sendSpecialKey(uint8_t keycode, const String& keyName);  // 特殊キー送信用（press+release方式）
    
    // 長押し処理用
    void processKeyPress(const String& pressed_chars);  // キー押下処理（長押し対応）
    
    // EspUsbHostからの継承メソッド
    void onNewDevice(const usb_device_info_t &dev_info) override;
    void onGone(const usb_host_client_event_msg_t *eventMsg) override;
    void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override;
    void onReceive(const usb_transfer_t *transfer) override;
    void processRawReport16Bytes(const uint8_t* data) override;
};

// BLE送信キュー（他ファイルから参照可能に）
extern QueueHandle_t bleSendQueue;

// キーコードマッピングテーブルの宣言
extern const KeycodeMapping KEYCODE_MAP[];
extern const int KEYCODE_MAP_SIZE;

#endif // PYTHON_STYLE_ANALYZER_H
