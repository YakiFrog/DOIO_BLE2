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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BleKeyboard.h>

// HID関連の定義の競合を防ぐため、再度チェック
#ifdef HID_CLASS
#undef HID_CLASS
#endif
#ifdef HID_SUBCLASS_NONE
#undef HID_SUBCLASS_NONE
#endif

#include "EspUsbHost.h"

// DOIO KB16デバイス情報
#define DOIO_VID 0xD010
#define DOIO_PID 0x1601

// デバッグ設定
#define DEBUG_ENABLED 1
#define SERIAL_OUTPUT_ENABLED 1

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
    Adafruit_SSD1306* display;
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

public:
    PythonStyleAnalyzer(Adafruit_SSD1306* disp, BleKeyboard* bleKbd);
    
    // アイドル状態のディスプレイ更新（publicメソッド）
    void updateDisplayIdle();

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
    
    // EspUsbHostからの継承メソッド
    void onNewDevice(const usb_device_info_t &dev_info) override;
    void onGone(const usb_host_client_event_msg_t *eventMsg) override;
    void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override;
    void onReceive(const usb_transfer_t *transfer) override;
    void processRawReport16Bytes(const uint8_t* data) override;
};

// キーコードマッピングテーブルの宣言
extern const KeycodeMapping KEYCODE_MAP[];
extern const int KEYCODE_MAP_SIZE;

#endif // PYTHON_STYLE_ANALYZER_H
