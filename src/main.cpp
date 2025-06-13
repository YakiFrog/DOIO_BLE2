#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "EspUsbHost.h"
#include <map>
#include <string>

// SSD1306ディスプレイ設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Seeed XIAO ESP32S3のI2Cピン設定
#define SDA_PIN 5
#define SCL_PIN 6

// DOIO KB16デバイス情報
#define DOIO_VID 0xD010
#define DOIO_PID 0x1601

// 設定（Pythonアナライザーに対応）
#define MAX_TEXT_LENGTH 50
#define DEBUG_ENABLED 1
#define VERBOSE_DEBUG 1  // Pythonのように詳細なバイト情報を表示

// パフォーマンス設定（最高応答性）
#define USB_TASK_INTERVAL 0
#define DISPLAY_UPDATE_INTERVAL 100
#define KEY_REPEAT_DELAY 200
#define KEY_REPEAT_RATE 30

// HIDキーコードマッピング（Pythonアナライザーと完全一致）
struct KeycodeMapping {
    uint8_t keycode;
    const char* normal;
    const char* shifted;
};

// DOIO KB16専用キーコードマッピング（DOIO_Bluetoothプロジェクトからの正確な移植）
const KeycodeMapping KEYCODE_MAP[] = {
    // 標準的なアルファベット (0x04-0x1D)
    {0x04, "a", "A"}, {0x05, "b", "B"}, {0x06, "c", "C"}, {0x07, "d", "D"},
    
    // DOIO KB16カスタムアルファベット (0x08-0x21)
    {0x08, "a", "A"}, {0x09, "b", "B"}, {0x0A, "c", "C"}, {0x0B, "d", "D"},
    {0x0C, "e", "E"}, {0x0D, "f", "F"}, {0x0E, "g", "G"}, {0x0F, "h", "H"},
    {0x10, "i", "I"}, {0x11, "j", "J"}, {0x12, "k", "K"}, {0x13, "l", "L"},
    {0x14, "m", "M"}, {0x15, "n", "N"}, {0x16, "o", "O"}, {0x17, "p", "P"},
    {0x18, "q", "Q"}, {0x19, "r", "R"}, {0x1A, "s", "S"}, {0x1B, "t", "T"},
    {0x1C, "u", "U"}, {0x1D, "v", "V"}, {0x1E, "w", "W"}, {0x1F, "x", "X"},
    {0x20, "y", "Y"}, {0x21, "z", "Z"},
    
    // DOIO KB16カスタム数字 (0x22-0x2B)
    {0x22, "1", "!"}, {0x23, "2", "@"}, {0x24, "3", "#"}, {0x25, "4", "$"},
    {0x26, "5", "%"}, {0x27, "6", "^"}, {0x28, "7", "&"}, {0x29, "8", "*"},
    {0x2A, "9", "("}, {0x2B, "0", ")"},
    
    // 標準的な数字 (0x1E-0x27) - 通常のキーボード用
    {0x1E, "1", "!"}, {0x1F, "2", "@"}, {0x20, "3", "#"}, {0x21, "4", "$"},
    // 標準的な数字 (0x1E-0x27) - 通常のキーボード用
    {0x1E, "1", "!"}, {0x1F, "2", "@"}, {0x20, "3", "#"}, {0x21, "4", "$"},
    
    // 特殊キー（DOIO KB16対応）
    {0x28, "Enter", "Enter"}, {0x29, "Esc", "Esc"}, {0x2A, "Backspace", "Backspace"},
    {0x2B, "Tab", "Tab"}, {0x2C, "Space", "Space"}, {0x2D, "-", "_"}, {0x2E, "=", "+"},
    {0x2F, "[", "{"}, {0x30, "]", "}"}, {0x31, "\\", "|"}, {0x32, "9", "("},
    {0x33, "0", ")"}, {0x34, ";", ":"}, {0x35, "'", "\""}, {0x36, ",", "<"}, 
    {0x37, ".", ">"}, {0x38, "/", "?"},
    
    // 制御キー
    {0xE0, "Ctrl", "Ctrl"}, {0xE1, "Shift", "Shift"}, {0xE2, "Alt", "Alt"},
    {0xE3, "GUI", "GUI"}, {0xE4, "RCtrl", "RCtrl"}, {0xE5, "RShift", "RShift"},
    {0xE6, "RAlt", "RAlt"}, {0xE7, "RGUI", "RGUI"}
};

// レポート形式構造体（Pythonアナライザーと同一）
struct ReportFormat {
    int size;
    int modifier_index;
    int reserved_index;
    int key_indices[6];
    String format;
};

// グローバル変数の前方宣言
extern bool displayInitialized;

// Pythonアナライザーの完全移植版USBホストクラス
class PythonStyleUsbHost : public EspUsbHost {
private:
    Adafruit_SSD1306* display;
    String textBuffer;
    bool isDOIOKeyboard = false;
    bool isConnected = false;
    
    // Pythonアナライザーの状態管理
    uint8_t last_report[32] = {0};
    bool has_last_report = false;
    int report_size = 16;  // DOIO KB16のデフォルト
    ReportFormat report_format;
    bool report_format_initialized = false;
    
    // 表示用状態
    int lastPressedKeyRow = -1;
    int lastPressedKeyCol = -1;
    bool displayNeedsUpdate = false;
    
    // パフォーマンス最適化
    unsigned long lastDisplayUpdate = 0;
    
    // OLED表示用の受信データ
    String lastHexData = "";
    String lastKeyPresses = "";
    unsigned long lastDataTime = 0;
    uint8_t lastRawData[16] = {0};  // 生データ保存
    int lastDataSize = 0;
    bool detailMode = false;  // 詳細表示モード

public:
    PythonStyleUsbHost(Adafruit_SSD1306* disp) : display(disp) {
        textBuffer = "";
        memset(&report_format, 0, sizeof(report_format));
        lastHexData = "";
        lastKeyPresses = "";
        lastDataTime = 0;
        memset(lastRawData, 0, sizeof(lastRawData));
        lastDataSize = 0;
        detailMode = false;
        displayNeedsUpdate = true;  // 初期表示のため
    }
    
    // 接続状態確認
    bool isDeviceConnected() const {
        return isConnected;
    }
    
    // デバイス接続時の処理
    void onNewDevice(const usb_device_info_t &dev_info) override {
        #if DEBUG_ENABLED
        Serial.println("\n=== USB キーボード接続 ===");
        
        String manufacturer = getUsbDescString(dev_info.str_desc_manufacturer);
        String product = getUsbDescString(dev_info.str_desc_product);
        String serialNum = getUsbDescString(dev_info.str_desc_serial_num);
        
        Serial.printf("製造元: %s\n", manufacturer.length() > 0 ? manufacturer.c_str() : "不明");
        Serial.printf("製品名: %s\n", product.length() > 0 ? product.c_str() : "不明");
        Serial.printf("シリアル: %s\n", serialNum.length() > 0 ? serialNum.c_str() : "不明");
        Serial.printf("VID: 0x%04X, PID: 0x%04X\n", device_vendor_id, device_product_id);
        #endif
        
        // DOIO KB16の検出（Pythonアナライザーと同一ロジック）
        if (device_vendor_id == DOIO_VID && device_product_id == DOIO_PID) {
            isDOIOKeyboard = true;
            isConnected = true;
            report_size = 16;  // DOIO KB16は16バイト固定
            Serial.println("DOIO KB16を検出: 16バイト固定レポートサイズを使用します");
        } else {
            // その他のキーボードも処理対象とする
            isDOIOKeyboard = true;
            isConnected = true;
            report_size = 8;   // 標準キーボードは8バイト
            Serial.println("標準キーボードとして処理します");
        }
        
        displayNeedsUpdate = true;
        Serial.println("=========================\n");
        
        // 初期表示の更新をトリガー
        updateDisplay();
    }
    
    // デバイス切断時の処理
    void onGone(const usb_host_client_event_msg_t *eventMsg) override {
        Serial.println("キーボードが切断されました");
        isDOIOKeyboard = false;
        isConnected = false;
        has_last_report = false;
        report_format_initialized = false;
        lastPressedKeyRow = -1;
        lastPressedKeyCol = -1;
        displayNeedsUpdate = true;
    }

    // キーボード入力処理（Pythonアナライザーアルゴリズムを使用）
    void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
        if (isDOIOKeyboard) {
            processReportPythonStyle(report);
            return;
        }
        
        // 標準キーボード処理
        EspUsbHost::onKeyboard(report, last_report);
    }
    
    // Pythonアナライザーのキーコード→文字変換（完全移植）
    String keycodeToString(uint8_t keycode, bool shift_pressed = false) {
        for (int i = 0; i < sizeof(KEYCODE_MAP) / sizeof(KeycodeMapping); i++) {
            if (KEYCODE_MAP[i].keycode == keycode) {
                return shift_pressed ? KEYCODE_MAP[i].shifted : KEYCODE_MAP[i].normal;
            }
        }
        return "Unknown";
    }
    
    // Pythonアナライザーのレポート形式解析（DOIO KB16対応修正版）
    ReportFormat analyzeReportFormat(const uint8_t* report_data, int data_size) {
        if (!report_format_initialized) {
            // DOIO KB16の実際のレポート形式に対応
            report_format.size = data_size;
            
            if (isDOIOKeyboard && data_size == 16) {
                // DOIO KB16: 16バイトレポート
                report_format.modifier_index = 1;  // バイト1が修飾キー（通常は0x00）
                report_format.reserved_index = 0;  // バイト0はDOIO固有フィールド（0x06など）
                report_format.key_indices[0] = 2;  // バイト2からキーコード開始
                report_format.key_indices[1] = 3;
                report_format.key_indices[2] = 4;
                report_format.key_indices[3] = 5;
                report_format.key_indices[4] = 6;
                report_format.key_indices[5] = 7;
                report_format.format = "DOIO_KB16";
            } else {
                // 標準キーボード: 8バイトレポート
                report_format.modifier_index = 0;  // バイト0が修飾キー
                report_format.reserved_index = 1;  // バイト1は予約
                report_format.key_indices[0] = 2;  // バイト2からキーコード開始
                report_format.key_indices[1] = 3;
                report_format.key_indices[2] = 4;
                report_format.key_indices[3] = 5;
                report_format.key_indices[4] = 6;
                report_format.key_indices[5] = 7;
                report_format.format = "Standard";
            }
            
            // NKROの検出（多くのバイトにキーコードが分散している場合）
            int non_zero_count = 0;
            for (int i = 2; i < data_size; i++) {
                if (report_data[i] != 0) non_zero_count++;
            }
            
            if (non_zero_count > 6 || data_size > 8) {
                report_format.format = "NKRO";
                // NKROでは通常、各ビットが1つのキーに対応
                for (int i = 0; i < 6; i++) {
                    report_format.key_indices[i] = i + 2;
                }
            }
            
            report_format_initialized = true;
            
            #if DEBUG_ENABLED
            Serial.printf("レポート形式を解析: サイズ=%d, 形式=%s\n", 
                         report_format.size, report_format.format.c_str());
            #endif
        }
        
        return report_format;
    }
    
    // Pythonアナライザーのメインレポート処理（完全移植）
    void processReportPythonStyle(const hid_keyboard_report_t &report) {
        // レポートデータを配列に変換
        uint8_t current_report[32] = {0};
        current_report[0] = report.modifier;
        current_report[1] = report.reserved;
        for (int i = 0; i < 6; i++) {
            current_report[2 + i] = report.keycode[i];
        }
        
        processRawReportData(current_report, min(report_size, 8));
    }
    
    // 生のレポートデータ処理（Pythonアナライザーのpretty_print_reportを移植）
    void processRawReportData(const uint8_t* report_data, int data_size) {
        // レポート形式を解析
        ReportFormat format = analyzeReportFormat(report_data, data_size);
        
        // Pythonアナライザーと同じ詳細なレポート表示
        Serial.println("\n--- HID REPORT ANALYSIS ---");
        
        // バイト列を16進数で表示（Pythonと同一）
        String hex_data = "";
        for (int i = 0; i < data_size; i++) {
            if (report_data[i] < 16) hex_data += "0";
            hex_data += String(report_data[i], HEX) + " ";
        }
        hex_data.trim();
        hex_data.toUpperCase();
        Serial.printf("HIDレポート [%dバイト]: %s\n", data_size, hex_data.c_str());
        
        // Pythonアナライザーのpretty_print_reportと同じフォーマット
        Serial.println("バイト構造:");
        for (int i = 0; i < data_size; i++) {
            String description = "";
            if (isDOIOKeyboard) {
                if (i == 0) description = " (DOIO特殊フィールド)";
                else if (i == 1) description = " (修飾キー)";
                else if (i >= 2 && i <= 7) description = " (キーコード" + String(i-1) + ")";
                else description = " (拡張データ)";
            } else {
                if (i == 0) description = " (修飾キー)";
                else if (i == 1) description = " (予約)";
                else if (i >= 2 && i <= 7) description = " (キーコード" + String(i-1) + ")";
                else description = " (拡張データ)";
            }
            Serial.printf("  [%2d] 0x%02X = %3d%s\n", i, report_data[i], report_data[i], description.c_str());
        }
        
        // DOIO KB16の特殊フィールド解析
        if (isDOIOKeyboard && data_size >= 3) {
            Serial.println("DOIO KB16固有解析:");
            Serial.printf("  特殊フィールド: 0x%02X\n", report_data[0]);
            Serial.printf("  修飾キーフィールド: 0x%02X\n", report_data[1]);
            Serial.printf("  最初のキーコード: 0x%02X\n", report_data[2]);
            
            // バイトパターンの解析
            Serial.println("  パターン解析:");
            if (report_data[0] == 0x06) Serial.println("    - 標準DOIOパターン検出");
            if (report_data[1] == 0x00) Serial.println("    - 修飾キーなし");
            
            // キーコード範囲の確認
            bool has_doio_keys = false;
            for (int i = 2; i < data_size; i++) {
                if (report_data[i] >= 0x08 && report_data[i] <= 0x17) {
                    has_doio_keys = true;
                    break;
                }
            }
            if (has_doio_keys) Serial.println("    - DOIOカスタムキーコード検出");
        }
        
        Serial.println("-----------------------------");
        
        // 修飾キー解析（DOIO KB16対応）
        bool shift_pressed = false;
        uint8_t modifier = report_data[format.modifier_index];
        String mod_str = "";
        
        if (isDOIOKeyboard) {
            // DOIO KB16では修飾キーフィールドは通常使用されない（0x00）
            if (modifier != 0x00) {
                #if DEBUG_ENABLED
                Serial.printf("DOIO KB16: 予期しない修飾キー値=0x%02X\n", modifier);
                #endif
            }
            // DOIO KB16では物理的なShiftキーの検出は別途行う
        } else {
            // 標準キーボードの修飾キー処理
            if (modifier & 0x01) mod_str += "L-Ctrl ";
            if (modifier & 0x02) { 
                mod_str += "L-Shift ";
                shift_pressed = true;
            }
            if (modifier & 0x04) mod_str += "L-Alt ";
            if (modifier & 0x08) mod_str += "L-GUI ";
            if (modifier & 0x10) mod_str += "R-Ctrl ";
            if (modifier & 0x20) { 
                mod_str += "R-Shift ";
                shift_pressed = true;
            }
            if (modifier & 0x40) mod_str += "R-Alt ";
            if (modifier & 0x80) mod_str += "R-GUI ";
        }
        
        #if DEBUG_ENABLED
        if (mod_str.length() > 0) {
            Serial.printf("修飾キー: %s\n", mod_str.c_str());
        }
        #endif
        
        // 押されているキーの検出（Pythonアナライザーと同一）
        String pressed_keys = "";
        String pressed_chars = "";
        
        Serial.println("キー検出:");
        
        if (format.format == "Standard") {
            // 標準的な6KROレポート
            Serial.println("  標準6KROフォーマット");
            for (int i = 0; i < 6; i++) {
                int idx = format.key_indices[i];
                if (idx < data_size && report_data[idx] != 0) {
                    uint8_t keycode = report_data[idx];
                    
                    if (pressed_keys.length() > 0) pressed_keys += ", ";
                    pressed_keys += "0x" + String(keycode, HEX);
                    
                    // キーコードを文字に変換
                    String key_str = keycodeToString(keycode, shift_pressed);
                    if (pressed_chars.length() > 0) pressed_chars += ", ";
                    pressed_chars += key_str;
                    
                    Serial.printf("    位置[%d]: キーコード=0x%02X, 文字='%s'\n", 
                                 idx, keycode, key_str.c_str());
                    
                    // 仮想マトリックス位置の計算（a-oマッピング用）
                    processDetectedKey(keycode, key_str, shift_pressed);
                }
            }
        } else {
            // NKRO形式
            Serial.println("  NKRO形式");
            for (int i = 2; i < data_size; i++) {
                for (int bit = 0; bit < 8; bit++) {
                    if (report_data[i] & (1 << bit)) {
                        uint8_t keycode = (i - 2) * 8 + bit + 4;
                        
                        if (pressed_keys.length() > 0) pressed_keys += ", ";
                        pressed_keys += "0x" + String(keycode, HEX);
                        
                        String key_str = keycodeToString(keycode, shift_pressed);
                        if (pressed_chars.length() > 0) pressed_chars += ", ";
                        pressed_chars += key_str;
                        
                        Serial.printf("    ビット[%d:%d]: キーコード=0x%02X, 文字='%s'\n", 
                                     i, bit, keycode, key_str.c_str());
                        
                        processDetectedKey(keycode, key_str, shift_pressed);
                    }
                }
            }
        }
        
        if (pressed_keys.length() > 0) {
            Serial.printf("押されているキー: %s\n", pressed_keys.c_str());
            Serial.printf("文字表現: %s\n", pressed_chars.c_str());
        } else {
            Serial.println("押されているキー: なし");
        }
        
        // 変更の検出（Pythonアナライザーと同一）
        if (has_last_report) {
            bool has_changes = false;
            Serial.println("変更検出:");
            for (int i = 0; i < data_size; i++) {
                if (last_report[i] != report_data[i]) {
                    has_changes = true;
                    Serial.printf("  バイト%d: 0x%02X (%3d) -> 0x%02X (%3d)\n", 
                                 i, last_report[i], last_report[i], 
                                 report_data[i], report_data[i]);
                }
            }
            
            if (!has_changes) {
                Serial.println("  変更なし（前回と同一）");
            } else {
                displayNeedsUpdate = true;
                Serial.println("  ディスプレイ更新をスケジュール");
            }
        } else {
            Serial.println("初回レポート - 前回データなし");
        }
        
        // 現在のレポートを保存
        memcpy(last_report, report_data, data_size);
        has_last_report = true;
    }
    
    // 検出されたキーの処理（DOIO KB16のa-oマッピング）
    void processDetectedKey(uint8_t keycode, const String& key_str, bool shift_pressed) {
        // DOIO KB16カスタムマッピング（0x08-0x17がa-o）
        if (keycode >= 0x08 && keycode <= 0x17) {
            int keyIndex = keycode - 0x08;  // 0x08='a', 0x09='b', ... 0x17='p'
            int virtualRow = keyIndex / 4;
            int virtualCol = keyIndex % 4;
            
            lastPressedKeyRow = virtualRow;
            lastPressedKeyCol = virtualCol;
            
            // テキストバッファに追加
            char key_char = 'a' + keyIndex;
            if (shift_pressed) key_char = 'A' + keyIndex;
            
            textBuffer += key_char;
            if (textBuffer.length() >= MAX_TEXT_LENGTH) {
                textBuffer = textBuffer.substring(1);
            }
            
            #if DEBUG_ENABLED
            Serial.printf("DOIO KB16検出: キーコード=0x%02X, 仮想位置=(%d,%d), 文字=%c\n", 
                         keycode, virtualRow, virtualCol, key_char);
            #endif
            
            displayNeedsUpdate = true;
        } 
        // DOIO KB16数字マッピング（0x22-0x2B）
        else if (keycode >= 0x22 && keycode <= 0x2B) {
            char digit_char;
            if (keycode == 0x2B) {
                digit_char = '0';  // 0x2Bは'0'
            } else {
                digit_char = '1' + (keycode - 0x22);  // 0x22='1', 0x23='2', ...
            }
            
            textBuffer += digit_char;
            if (textBuffer.length() >= MAX_TEXT_LENGTH) {
                textBuffer = textBuffer.substring(1);
            }
            
            #if DEBUG_ENABLED
            Serial.printf("DOIO KB16数字: キーコード=0x%02X, 文字=%c\n", keycode, digit_char);
            #endif
            
            displayNeedsUpdate = true;
        }
        // 標準キーボードマッピング（0x04-0x1D）
        else if (keycode >= 0x04 && keycode <= 0x1D) {
            int keyIndex = keycode - 0x04;
            int virtualRow = keyIndex / 4;
            int virtualCol = keyIndex % 4;
            
            lastPressedKeyRow = virtualRow;
            lastPressedKeyCol = virtualCol;
            
            char key_char = 'a' + keyIndex;
            if (shift_pressed) key_char = 'A' + keyIndex;
            
            textBuffer += key_char;
            if (textBuffer.length() >= MAX_TEXT_LENGTH) {
                textBuffer = textBuffer.substring(1);
            }
            
            #if DEBUG_ENABLED
            Serial.printf("標準キーボード検出: キーコード=0x%02X, 仮想位置=(%d,%d), 文字=%c\n", 
                         keycode, virtualRow, virtualCol, key_char);
            #endif
            
            displayNeedsUpdate = true;
        } 
        else if (keycode == 0xE1 || keycode == 0xE5) {
            // Shiftキー
            lastPressedKeyRow = 3;
            lastPressedKeyCol = 3;
            textBuffer += "[SHIFT]";
            displayNeedsUpdate = true;
        }
        else if (keycode == 0x2C) {
            // スペースキー
            textBuffer += " ";
            displayNeedsUpdate = true;
        }
        else if (keycode == 0x29) {
            // ESCキー - 表示モード切替
            detailMode = !detailMode;
            displayNeedsUpdate = true;
            Serial.printf("表示モード切替: %s\n", detailMode ? "詳細モード" : "通常モード");
        }
        else {
            // その他のキー
            textBuffer += "[" + key_str + "]";
            if (textBuffer.length() >= MAX_TEXT_LENGTH) {
                textBuffer = textBuffer.substring(1);
            }
            displayNeedsUpdate = true;
        }
    }
    
    // 拡張されたonReceiveメソッド（実際の生データを処理）
    void onReceive(const usb_transfer_t *transfer) override {
        EspUsbHost::onReceive(transfer);
        
        if (transfer->actual_num_bytes == 0) return;
        
        // Pythonアナライザーと同じように全ての受信データを詳細表示
        Serial.println("\n╔═══════════════════════════════╗");
        Serial.println("║     USB RAW DATA RECEIVED     ║");
        Serial.println("╚═══════════════════════════════╝");
        Serial.printf("受信時刻: %lu ms\n", millis());
        Serial.printf("データサイズ: %d bytes\n", transfer->actual_num_bytes);
        Serial.println("─────────────────────────────────");
        
        // 16進数表示（Pythonと同じフォーマット）
        String hex_data = "";
        for (int i = 0; i < transfer->actual_num_bytes; i++) {
            if (transfer->data_buffer[i] < 16) hex_data += "0";
            hex_data += String(transfer->data_buffer[i], HEX) + " ";
        }
        hex_data.trim();
        hex_data.toUpperCase();
        Serial.printf("RAW HEX: %s\n", hex_data.c_str());
        Serial.println("─────────────────────────────────");
        
        // バイト毎の詳細表示（Pythonと同じ）
        Serial.println("バイト詳細:");
        for (int i = 0; i < transfer->actual_num_bytes; i++) {
            Serial.printf("  [%2d] = 0x%02X (%3d) = %c\n", 
                         i, 
                         transfer->data_buffer[i], 
                         transfer->data_buffer[i],
                         (transfer->data_buffer[i] >= 32 && transfer->data_buffer[i] <= 126) ? 
                         transfer->data_buffer[i] : '.');
        }
        
        // バイナリ表示（8ビット）
        Serial.println("バイナリ表示:");
        for (int i = 0; i < transfer->actual_num_bytes; i++) {
            Serial.printf("  [%2d] = ", i);
            for (int bit = 7; bit >= 0; bit--) {
                Serial.print((transfer->data_buffer[i] & (1 << bit)) ? '1' : '0');
            }
            Serial.printf(" (0x%02X)\n", transfer->data_buffer[i]);
        }
        
        // デバイス固有の解析
        Serial.println("デバイス解析:");
        if (isDOIOKeyboard) {
            Serial.println("  DOIO KB16 モード");
            if (transfer->actual_num_bytes >= 3) {
                Serial.printf("  特殊フィールド[0]: 0x%02X\n", transfer->data_buffer[0]);
                Serial.printf("  修飾キー[1]: 0x%02X\n", transfer->data_buffer[1]);
                Serial.printf("  キーコード開始[2]: 0x%02X\n", transfer->data_buffer[2]);
                
                // 全てのキーコードバイトを表示
                Serial.print("  全キーコード: ");
                for (int i = 2; i < transfer->actual_num_bytes; i++) {
                    if (transfer->data_buffer[i] != 0) {
                        Serial.printf("0x%02X ", transfer->data_buffer[i]);
                    }
                }
                Serial.println();
            }
        } else {
            Serial.println("  標準キーボード モード");
            if (transfer->actual_num_bytes >= 3) {
                Serial.printf("  修飾キー[0]: 0x%02X\n", transfer->data_buffer[0]);
                Serial.printf("  予約[1]: 0x%02X\n", transfer->data_buffer[1]);
                Serial.printf("  キーコード開始[2]: 0x%02X\n", transfer->data_buffer[2]);
            }
        }
        
        Serial.println("─────────────────────────────────");
        Serial.println("╚═══════════════════════════════╝\n");
        
        // OLED表示用にデータを保存
        lastHexData = "";
        lastDataSize = min(transfer->actual_num_bytes, 16);
        memcpy(lastRawData, transfer->data_buffer, lastDataSize);
        
        for (int i = 0; i < min(transfer->actual_num_bytes, 8); i++) {
            if (transfer->data_buffer[i] < 16) lastHexData += "0";
            lastHexData += String(transfer->data_buffer[i], HEX);
            if (i < min(transfer->actual_num_bytes, 8) - 1) lastHexData += " ";
        }
        lastHexData.toUpperCase();
        lastDataTime = millis();
        
        // 押されたキーを保存
        lastKeyPresses = "";
        for (int i = 2; i < min(transfer->actual_num_bytes, 8); i++) {
            if (transfer->data_buffer[i] != 0) {
                String key_str = keycodeToString(transfer->data_buffer[i], false);
                if (lastKeyPresses.length() > 0) lastKeyPresses += ",";
                lastKeyPresses += key_str;
            }
        }
        
        displayNeedsUpdate = true;
        
        // 実際のサイズでレポートを処理
        processRawReportData(transfer->data_buffer, transfer->actual_num_bytes);
    }
    
    // ディスプレイ更新処理（受信データのバイト表示）
    void updateDisplay() {
        if (!display || !displayInitialized) return;
        
        if (!displayNeedsUpdate) return;
        
        unsigned long now = millis();
        if (now - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
        
        display->clearDisplay();
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);
        
        if (!detailMode) {
            // 通常モード: 概要表示
            display->setCursor(0, 0);
            display->println("DOIO Byte Analyzer");
            
            // 接続状態
            display->setCursor(0, 10);
            if (isConnected) {
                display->print("Connected");
                // データ受信からの経過時間
                if (lastDataTime > 0) {
                    display->setCursor(70, 10);
                    display->print((now - lastDataTime) / 1000);
                    display->print("s ago");
                }
            } else {
                display->print("Waiting for device...");
            }
            
            // 最後に受信したHEXデータ
            display->setCursor(0, 22);
            display->print("HEX: ");
            if (lastHexData.length() > 0) {
                display->print(lastHexData);
            } else {
                display->print("No data");
            }
            
            // 押されたキーの表示
            display->setCursor(0, 34);
            display->print("Key: ");
            if (lastKeyPresses.length() > 0) {
                String keyDisplay = lastKeyPresses;
                if (keyDisplay.length() > 18) {
                    keyDisplay = keyDisplay.substring(0, 15) + "...";
                }
                display->print(keyDisplay);
            } else {
                display->print("None");
            }
            
            // バイト詳細（最初の3バイト）
            if (lastDataSize > 0) {
                display->setCursor(0, 46);
                display->print("Bytes: ");
                for (int i = 0; i < min(lastDataSize, 3); i++) {
                    display->printf("%02X ", lastRawData[i]);
                }
            }
            
            // モード切替ヒント
            display->setCursor(0, 56);
            display->print("Press ESC for detail");
            
        } else {
            // 詳細モード: バイト詳細表示
            display->setCursor(0, 0);
            display->println("Byte Detail Mode");
            
            if (lastDataSize > 0) {
                // 8バイトまで表示（4バイト x 2行）
                for (int row = 0; row < 2; row++) {
                    display->setCursor(0, 12 + row * 12);
                    for (int col = 0; col < 4; col++) {
                        int idx = row * 4 + col;
                        if (idx < lastDataSize) {
                            display->printf("%02X ", lastRawData[idx]);
                        } else {
                            display->print("-- ");
                        }
                    }
                }
                
                // 解釈表示
                display->setCursor(0, 38);
                display->print("Mod:");
                if (lastDataSize > 1) {
                    display->printf("%02X", lastRawData[isDOIOKeyboard ? 1 : 0]);
                }
                
                display->setCursor(40, 38);
                display->print("Key1:");
                if (lastDataSize > 2) {
                    display->printf("%02X", lastRawData[2]);
                }
                
                display->setCursor(0, 50);
                display->printf("Size:%d Type:%s", lastDataSize, 
                               isDOIOKeyboard ? "DOIO" : "STD");
            } else {
                display->setCursor(0, 20);
                display->print("No data received");
            }
            
            // モード切替ヒント
            display->setCursor(0, 56);
            display->print("Press ESC for normal");
        }
        
        display->display();
        lastDisplayUpdate = now;
        displayNeedsUpdate = false;
    }
    
    // 定期更新処理
    void periodicUpdate() {
        updateDisplay();
    }
};

// グローバル変数
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
PythonStyleUsbHost* usbHost;
bool displayInitialized = false;

// 関数宣言
void initDisplay();
void showStartupMessage();

void setup() {
    setCpuFrequencyMhz(240);
    
    Serial.begin(115200);
    for(int i = 0; i < 50 && !Serial; i++) {
        delay(100);
    }
    
    Serial.println("=================================");
    Serial.println("Python Style DOIO KB16 Analyzer");
    Serial.println("Complete Python Algorithm Port");
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    Serial.println("=================================");
    
    // I2C初期化
    Serial.printf("Initializing I2C: SDA=%d, SCL=%d\n", SDA_PIN, SCL_PIN);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    delay(500);
    
    // ディスプレイ初期化
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306初期化に失敗しました");
        while (true) delay(1000);
    }
    
    Serial.println("SSD1306初期化完了");
    displayInitialized = true;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Byte Analyzer");
    display.println("Ready");
    display.println("");
    display.println("Check Serial Monitor");
    display.println("for detailed output");
    display.display();
    
    // USBホスト初期化
    usbHost = new PythonStyleUsbHost(&display);
    usbHost->begin();
    
    Serial.println("System initialized. Waiting for keyboard...");
}

void loop() {
    static unsigned long lastCheck = 0;
    unsigned long now = millis();
    
    // USB処理（最高頻度）
    usbHost->task();
    
    // 定期更新
    if (now - lastCheck >= DISPLAY_UPDATE_INTERVAL) {
        usbHost->periodicUpdate();
        lastCheck = now;
    }
    
    // ウォッチドッグリセット（ESP32では通常自動）
    yield();
}
