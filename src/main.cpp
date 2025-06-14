// PythonアナライザーのUSB HID読み取りを完全移植したバージョン（KB16認識対応修正版）

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "EspUsbHost.h"

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

// デバッグ設定
#define DEBUG_ENABLED 1
#define SERIAL_OUTPUT_ENABLED 1

// パフォーマンス設定（最高応答性）
#define USB_TASK_INTERVAL 0
#define DISPLAY_UPDATE_INTERVAL 50  // 50msに短縮
#define KEY_REPEAT_DELAY 200
#define KEY_REPEAT_RATE 30

// Pythonアナライザーと同じキーコードマッピング
struct KeycodeMapping {
    uint8_t keycode;
    const char* normal;
    const char* shifted;
};

const KeycodeMapping KEYCODE_MAP[] = {
    // アルファベット (0x04-0x1D: a-z) - 標準キーボード
    {0x04, "a", "A"}, {0x05, "b", "B"}, {0x06, "c", "C"}, {0x07, "d", "D"},
    
    // DOIO KB16カスタムアルファベット (0x08-0x21)
    {0x08, "a", "A"}, {0x09, "b", "B"}, {0x0A, "c", "C"}, {0x0B, "d", "D"},
    {0x0C, "e", "E"}, {0x0D, "f", "F"}, {0x0E, "g", "G"}, {0x0F, "h", "H"},
    {0x10, "i", "I"}, {0x11, "j", "J"}, {0x12, "k", "K"}, {0x13, "l", "L"},
    {0x14, "m", "M"}, {0x15, "n", "N"}, {0x16, "o", "O"}, {0x17, "p", "P"},
    {0x18, "q", "Q"}, {0x19, "r", "R"}, {0x1A, "s", "S"}, {0x1B, "t", "T"},
    {0x1C, "u", "U"}, {0x1D, "v", "V"}, {0x1E, "w", "W"}, {0x1F, "x", "X"},
    {0x20, "y", "Y"}, {0x21, "z", "Z"},
    
    // 標準数字 (0x1E-0x27) - 通常のキーボード用
    {0x1E, "1", "!"}, {0x1F, "2", "@"}, {0x20, "3", "#"}, {0x21, "4", "$"},
    {0x22, "5", "%"}, {0x23, "6", "^"}, {0x24, "7", "&"}, {0x25, "8", "*"},
    {0x26, "9", "("}, {0x27, "0", ")"},
    
    // DOIO KB16カスタム数字 (0x22-0x2B)
    {0x22, "1", "!"}, {0x23, "2", "@"}, {0x24, "3", "#"}, {0x25, "4", "$"},
    {0x26, "5", "%"}, {0x27, "6", "^"}, {0x28, "7", "&"}, {0x29, "8", "*"},
    {0x2A, "9", "("}, {0x2B, "0", ")"},
    
    // 一般的なキー
    {0x28, "Enter", "Enter"}, {0x29, "Esc", "Esc"}, {0x2A, "Backspace", "Backspace"},
    {0x2B, "Tab", "Tab"}, {0x2C, "Space", "Space"}, {0x2D, "-", "_"}, {0x2E, "=", "+"},
    {0x2F, "[", "{"}, {0x30, "]", "}"}, {0x31, "\\", "|"}, {0x33, ";", ":"},
    {0x34, "'", "\""}, {0x35, "`", "~"}, {0x36, ",", "<"}, {0x37, ".", ">"},
    {0x38, "/", "?"},
    
    // 制御キー
    {0xE0, "Ctrl", "Ctrl"}, {0xE1, "Shift", "Shift"}, {0xE2, "Alt", "Alt"},
    {0xE3, "GUI", "GUI"}, {0xE4, "右Ctrl", "右Ctrl"}, {0xE5, "右Shift", "右Shift"},
    {0xE6, "右Alt", "右Alt"}, {0xE7, "右GUI", "右GUI"}
};

// レポート形式構造体（Pythonと同じ）
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
    bool displayNeedsUpdate = false;
    unsigned long lastDisplayUpdate = 0;

public:
    PythonStyleAnalyzer(Adafruit_SSD1306* disp) : display(disp) {
        memset(&report_format, 0, sizeof(report_format));
    }
    
    // ディスプレイ更新用のヘルパー関数
    void updateDisplayForDevice(const String& deviceType) {
        if (!display) return;
        
        display->clearDisplay();
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);
        display->setCursor(0, 0);
        display->println("Python Style");
        display->println("KB16 Analyzer");
        display->println("");
        display->println(deviceType);
        display->println("Connected!");
        display->println("");
        display->println("Reading USB data...");
        display->println("Check Serial Monitor");
        display->display();
    }
    
    // Pythonのkeycode_to_string関数を完全移植
    String keycodeToString(uint8_t keycode, bool shift = false) {
        for (int i = 0; i < sizeof(KEYCODE_MAP) / sizeof(KeycodeMapping); i++) {
            if (KEYCODE_MAP[i].keycode == keycode) {
                return shift ? KEYCODE_MAP[i].shifted : KEYCODE_MAP[i].normal;
            }
        }
        return "不明(0x" + String(keycode, HEX) + ")";
    }
    
    // Pythonの_analyze_report_format関数を完全移植
    ReportFormat analyzeReportFormat(const uint8_t* report_data, int data_size) {
        if (!report_format_initialized) {
            // 最初のレポートから形式を推測（Pythonと同じロジック）
            report_format.size = data_size;
            
            if (is_doio_kb16 && data_size == 16) {
                // DOIO KB16: 16バイトレポート（元のプログラムと同じ）
                report_format.modifier_index = 1;  // バイト1が修飾キー（通常は0x00）
                report_format.reserved_index = 0;  // バイト0はDOIO固有フィールド（0x06など）
                report_format.key_indices[0] = 2;  // バイト2からキーコード開始
                report_format.key_indices[1] = 3;
                report_format.key_indices[2] = 4;
                report_format.key_indices[3] = 5;
                report_format.key_indices[4] = 6;
                report_format.key_indices[5] = 7;
                // 16バイトレポートでは8-15バイトも使用可能
                report_format.format = "DOIO_KB16_16BYTE";
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
            
            // NKROの検出（Pythonと同じ）
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
            
            #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
            Serial.printf("レポート形式解析完了: サイズ=%d, 形式=%s\n", 
                         report_format.size, report_format.format.c_str());
            #endif
        }
        
        return report_format;
    }
    
    // Pythonのpretty_print_report関数を完全移植
    void prettyPrintReport(const uint8_t* report_data, int data_size) {
        ReportFormat format = analyzeReportFormat(report_data, data_size);
        
        #if SERIAL_OUTPUT_ENABLED
        // バイト列を16進数で表示（Pythonと同一フォーマット）
        String hex_data = "";
        for (int i = 0; i < data_size; i++) {
            if (report_data[i] < 16) hex_data += "0";
            hex_data += String(report_data[i], HEX) + " ";
        }
        hex_data.trim();
        hex_data.toUpperCase();
        Serial.printf("\nHIDレポート [%dバイト]: %s\n", data_size, hex_data.c_str());
        #endif
        
        // 修飾キー（Pythonと同じロジック）
        bool shift_pressed = false;
        if (format.format == "Standard" || format.format == "NKRO") {
            uint8_t modifier = report_data[format.modifier_index];
            String mod_str = "";
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
            
            #if SERIAL_OUTPUT_ENABLED
            Serial.printf("修飾キー: %s\n", mod_str.length() > 0 ? mod_str.c_str() : "なし");
            #endif
        } else if (format.format == "DOIO_KB16_16BYTE") {
            // DOIO KB16では修飾キーフィールドは通常0x00
            uint8_t modifier = report_data[format.modifier_index];
            if (modifier != 0x00) {
                #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
                Serial.printf("DOIO KB16: 予期しない修飾キー値=0x%02X\n", modifier);
                #endif
            } else {
                #if SERIAL_OUTPUT_ENABLED
                Serial.println("修飾キー: なし (DOIO KB16)");
                #endif
            }
        }
        
        // 押されているキー（Pythonと同じロジック）
        String pressed_keys = "";
        String pressed_chars = "";
        
        if (format.format == "DOIO_KB16_16BYTE") {
            // DOIO KB16: 16バイトレポート - 2-15バイトすべてをチェック
            for (int i = 2; i < data_size; i++) {
                if (report_data[i] != 0) {
                    uint8_t keycode = report_data[i];
                    if (pressed_keys.length() > 0) pressed_keys += ", ";
                    pressed_keys += "0x" + String(keycode, HEX);
                    
                    // キーコードを文字に変換
                    String key_str = keycodeToString(keycode, shift_pressed);
                    if (pressed_chars.length() > 0) pressed_chars += ", ";
                    pressed_chars += key_str;
                }
            }
        } else if (format.format == "Standard") {
            // 標準的な6KROレポート
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
                }
            }
        } else {
            // NKRO形式
            for (int i = 2; i < data_size; i++) {
                uint8_t b = report_data[i];
                for (int bit = 0; bit < 8; bit++) {
                    if (b & (1 << bit)) {
                        uint8_t keycode = (i - 2) * 8 + bit + 4;
                        if (pressed_keys.length() > 0) pressed_keys += ", ";
                        pressed_keys += "0x" + String(keycode, HEX);
                        
                        // キーコードを文字に変換
                        String key_str = keycodeToString(keycode, shift_pressed);
                        if (pressed_chars.length() > 0) pressed_chars += ", ";
                        pressed_chars += key_str;
                    }
                }
            }
        }
        
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("押されているキー: %s\n", pressed_keys.length() > 0 ? pressed_keys.c_str() : "なし");
        if (pressed_chars.length() > 0) {
            Serial.printf("文字表現: %s\n", pressed_chars.c_str());
        }
        #endif
        
        // 変更の検出（Pythonと同じロジック）
        if (has_last_report) {
            bool has_changes = false;
            #if SERIAL_OUTPUT_ENABLED
            Serial.println("変更点:");
            #endif
            for (int i = 0; i < data_size; i++) {
                if (last_report[i] != report_data[i]) {
                    has_changes = true;
                    #if SERIAL_OUTPUT_ENABLED
                    Serial.printf("  バイト%d: 0x%02X -> 0x%02X\n", i, last_report[i], report_data[i]);
                    #endif
                }
            }
            
            if (!has_changes) {
                #if SERIAL_OUTPUT_ENABLED
                Serial.println("  変更なし");
                #endif
            }
        }
        
        // 現在のレポートを保存（Pythonと同じ）
        memcpy(last_report, report_data, data_size);
        has_last_report = true;
    }
    
    // デバイス接続時の処理（元のプログラムと同じ処理を追加）
    void onNewDevice(const usb_device_info_t &dev_info) override {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("\n=== USB デバイス接続 ===");
        
        // デバイス情報の詳細表示
        String manufacturer = getUsbDescString(dev_info.str_desc_manufacturer);
        String product = getUsbDescString(dev_info.str_desc_product);
        String serialNum = getUsbDescString(dev_info.str_desc_serial_num);
        
        Serial.printf("製造元: %s\n", manufacturer.length() > 0 ? manufacturer.c_str() : "不明");
        Serial.printf("製品名: %s\n", product.length() > 0 ? product.c_str() : "不明");
        Serial.printf("シリアル: %s\n", serialNum.length() > 0 ? serialNum.c_str() : "不明");
        Serial.printf("VID: 0x%04X, PID: 0x%04X\n", device_vendor_id, device_product_id);
        Serial.printf("速度: %s\n", dev_info.speed == USB_SPEED_LOW ? "Low" : 
                                  dev_info.speed == USB_SPEED_FULL ? "Full" : "High");
        #endif
        
        // DOIO KB16の検出（より詳細なチェック）
        if (device_vendor_id == DOIO_VID && device_product_id == DOIO_PID) {
            is_doio_kb16 = true;
            isConnected = true;
            report_size = 16;  // DOIO KB16は16バイト固定
            #if SERIAL_OUTPUT_ENABLED
            Serial.println("✓ DOIO KB16を検出: 16バイト固定レポートサイズ");
            Serial.println("✓ Pythonアナライザーと同じ処理を開始");
            #endif
            
            // DOIO KB16専用の初期化処理
            updateDisplayForDevice("DOIO KB16");
        } else {
            is_doio_kb16 = false;
            isConnected = true;
            report_size = 8;   // 標準キーボードは8バイト
            #if SERIAL_OUTPUT_ENABLED
            Serial.println("標準キーボードとして処理 (8バイトレポート)");
            #endif
            
            // 標準キーボード用の初期化処理
            updateDisplayForDevice("Standard Keyboard");
        }
        
        // レポート形式を初期化
        report_format_initialized = false;
        has_last_report = false;
        
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("========================\n");
        #endif
    }
    
    // デバイス切断時の処理
    void onGone(const usb_host_client_event_msg_t *eventMsg) override {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("USBデバイスが切断されました");
        #endif
        is_doio_kb16 = false;
        isConnected = false;
        has_last_report = false;
        report_format_initialized = false;
        
        // ディスプレイを元の状態に戻す
        if (display) {
            display->clearDisplay();
            display->setTextSize(1);
            display->setTextColor(SSD1306_WHITE);
            display->setCursor(0, 0);
            display->println("Python Style");
            display->println("KB16 Analyzer");
            display->println("");
            display->println("Waiting for USB");
            display->println("device...");
            display->println("");
            display->println("Check Serial Monitor");
            display->println("for detailed output");
            display->display();
        }
    }
    
    // 標準キーボード処理（8バイトレポート）
    void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
        if (is_doio_kb16) {
            // DOIO KB16の場合は16バイトレポート処理を優先
            return;
        }
        
        // 標準8バイトレポートをPythonスタイルで処理
        uint8_t current_report[8] = {0};
        current_report[0] = report.modifier;
        current_report[1] = report.reserved;
        for (int i = 0; i < 6; i++) {
            current_report[i + 2] = report.keycode[i];
        }
        
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("\n╔══════════════════════════════════════╗");
        Serial.println("║        標準キーボード データ受信      ║");
        Serial.println("╚══════════════════════════════════════╝");
        Serial.printf("受信時刻: %lu ms\n", millis());
        Serial.printf("受信バイト数: 8\n");
        Serial.printf("デバイス: 標準キーボード\n");
        
        // 生データを16進数で表示
        Serial.print("生データ: ");
        for (int i = 0; i < 8; i++) {
            if (current_report[i] < 16) Serial.print("0");
            Serial.print(current_report[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.println("──────────────────────────────────────");
        #endif
        
        // Pythonのpretty_print_reportを呼び出し
        prettyPrintReport(current_report, 8);
        
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("════════════════════════════════════════");
        #endif
    }
    
    // USBデータ受信時の処理（Pythonのread処理と同等）
    void onReceive(const usb_transfer_t *transfer) override {
        if (transfer->actual_num_bytes == 0) return;
        
        // Pythonアナライザーのメイン処理と同じフロー
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("\n╔══════════════════════════════════════╗");
        Serial.println("║           USB データ受信             ║");
        Serial.println("╚══════════════════════════════════════╝");
        Serial.printf("受信時刻: %lu ms\n", millis());
        Serial.printf("受信バイト数: %d\n", transfer->actual_num_bytes);
        Serial.printf("デバイス: %s\n", is_doio_kb16 ? "DOIO KB16" : "標準キーボード");
        Serial.printf("期待サイズ: %d バイト\n", report_size);
        
        // 生データを16進数で表示
        Serial.print("生データ: ");
        for (int i = 0; i < transfer->actual_num_bytes; i++) {
            if (transfer->data_buffer[i] < 16) Serial.print("0");
            Serial.print(transfer->data_buffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        Serial.println("──────────────────────────────────────");
        #endif
        
        // Pythonのpretty_print_reportを呼び出し
        prettyPrintReport(transfer->data_buffer, transfer->actual_num_bytes);
        
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("════════════════════════════════════════");
        #endif
    }
    
    // 16バイトレポート処理（DOIO KB16専用）
    void processRawReport16Bytes(const uint8_t* data) override {
        #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
        Serial.println("16バイトDOIO KB16レポート処理");
        #endif
        
        // Pythonと同じ処理フローを呼び出し
        prettyPrintReport(data, 16);
    }
};

// グローバル変数
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
PythonStyleAnalyzer* analyzer;

void setup() {
    // CPU最高速度で動作
    setCpuFrequencyMhz(240);
    
    #if SERIAL_OUTPUT_ENABLED
    Serial.begin(115200);
    while (!Serial && millis() < 5000) {
        delay(100);
    }
    
    Serial.println("=======================================");
    Serial.println("DOIO KB16 Python Algorithm Port");
    Serial.println("100% Python Compatible USB HID Reader");
    Serial.printf("CPU周波数: %d MHz\n", getCpuFrequencyMhz());
    Serial.println("=======================================");
    #endif
    
    // I2C初期化
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);  // 高速I2C
    delay(100);
    
    // ディスプレイ初期化
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("エラー: SSD1306初期化失敗");
        #endif
        while (true) delay(1000);
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Python Style");
    display.println("KB16 Analyzer");
    display.println("");
    display.println("Waiting for USB");
    display.println("device...");
    display.println("");
    display.println("Check Serial Monitor");
    display.println("for detailed output");
    display.display();
    
    // Pythonスタイルアナライザー初期化
    analyzer = new PythonStyleAnalyzer(&display);
    analyzer->begin();
    
    #if SERIAL_OUTPUT_ENABLED
    Serial.println("システム初期化完了");
    Serial.println("USBキーボードを接続してください...");
    Serial.println("すべてのUSBデータがPythonと同じ形式で表示されます");
    #endif
}

void loop() {
    // USBタスクの実行（Pythonのread相当の処理が内部で行われる）
    analyzer->task();
    
    // 最小限の遅延（応答性最優先）
    delayMicroseconds(100);
}
