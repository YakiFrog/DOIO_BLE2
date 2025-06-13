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

// DOIO KB16デバイス情報
#define DOIO_VID 0xD010
#define DOIO_PID 0x1601

// 設定
#define WRITE_MODE_DURATION 5000  // 5秒間書き込みモード
#define MAX_TEXT_LENGTH 50
#define DEBUG_ENABLED 0  // デバッグ出力を無効化（パフォーマンス向上）
#define VERBOSE_DEBUG 0  // 詳細デバッグ（必要時のみ有効化）

// パフォーマンス設定
#define USB_TASK_INTERVAL 5      // USB処理間隔（ms）- さらに短縮
#define DISPLAY_UPDATE_INTERVAL 500  // ディスプレイ更新間隔（ms）
#define KEY_REPEAT_DELAY 500     // 長押し検出遅延（ms）
#define KEY_REPEAT_RATE 100      // 長押し時のリピート間隔（ms）

// DOIO KB16 キーマッピング構造体
struct KeyMapping {
    uint8_t byte_idx;  // レポート内のバイトインデックス
    uint8_t bit_mask;  // ビットマスク
    uint8_t row;       // キーボードマトリックス行
    uint8_t col;       // キーボードマトリックス列
};

// DOIO KB16キーマッピング（KEYBOARD_BLEプロジェクトから移植）
const KeyMapping kb16_key_map[] = {
    { 5, 0x20, 0, 0 },  // byte5_bit5
    { 1, 0x01, 0, 1 },  // byte1_bit0  
    { 1, 0x02, 0, 2 },  // byte1_bit1
    { 5, 0x01, 0, 3 },  // byte5_bit0
    { 4, 0x01, 1, 0 },  // byte4_bit0
    { 5, 0x02, 1, 1 },  // byte5_bit1
    { 4, 0x08, 1, 2 },  // byte4_bit3
    { 4, 0x80, 1, 3 },  // byte4_bit7
    { 4, 0x02, 2, 0 },  // byte4_bit1
    { 4, 0x20, 2, 1 },  // byte4_bit5
    { 5, 0x08, 2, 2 },  // byte5_bit3
    { 4, 0x40, 2, 3 },  // byte4_bit6
    { 4, 0x10, 3, 0 },  // byte4_bit4
    { 5, 0x10, 3, 1 },  // byte5_bit4
    { 4, 0x04, 3, 2 },  // byte4_bit2
    { 5, 0x04, 3, 3 },  // byte5_bit2
};

// カスタムUSBホストクラス（DOIO KB16専用）
class DOIOKB16UsbHost : public EspUsbHost {
private:
    Adafruit_SSD1306* display;
    String textBuffer;
    bool isDOIOKeyboard = false;
    bool isConnected = false;  // 接続状態を追跡
    bool kb16_key_states[4][4] = {false};
    int lastPressedKeyRow = -1;
    int lastPressedKeyCol = -1;
    
    // 長押し検出用
    unsigned long keyPressTime[4][4] = {0};  // 各キーが押された時刻
    unsigned long lastRepeatTime[4][4] = {0}; // 最後のリピート時刻
    bool keyRepeating[4][4] = {false};        // リピート中フラグ
    
    // 表示の最適化用
    unsigned long lastDisplayUpdate = 0;
    unsigned long lastConnectionUpdate = 0;
    bool displayNeedsUpdate = false;

public:
    DOIOKB16UsbHost(Adafruit_SSD1306* disp) : display(disp) {
        // キーマトリックスの初期化
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                kb16_key_states[i][j] = false;
                keyPressTime[i][j] = 0;
                lastRepeatTime[i][j] = 0;
                keyRepeating[i][j] = false;
            }
        }
        textBuffer = "";
    }
    
    // 接続状態確認メソッド
    bool isDeviceConnected() const {
        return isConnected;
    }
    
    // デバイス接続時の処理（オーバーライド）
    void onNewDevice(const usb_device_info_t &dev_info) override {
        #if DEBUG_ENABLED
        Serial.println("\n=== USB キーボード接続 ===");
        
        // デバイス情報の表示
        String manufacturer = getUsbDescString(dev_info.str_desc_manufacturer);
        String product = getUsbDescString(dev_info.str_desc_product);
        String serialNum = getUsbDescString(dev_info.str_desc_serial_num);
        
        Serial.printf("製造元: %s\n", manufacturer.length() > 0 ? manufacturer.c_str() : "不明");
        Serial.printf("製品名: %s\n", product.length() > 0 ? product.c_str() : "不明");
        Serial.printf("シリアル: %s\n", serialNum.length() > 0 ? serialNum.c_str() : "不明");
        Serial.printf("VID: 0x%04X, PID: 0x%04X\n", device_vendor_id, device_product_id);
        #endif
        
        // DOIO KB16の検出（実際のVID/PID: 53264/5633）
        if (device_vendor_id == DOIO_VID && device_product_id == DOIO_PID) {
            isDOIOKeyboard = true;
            isConnected = true;  // 接続状態を設定
            #if DEBUG_ENABLED
            Serial.println("★ DOIO KB16キーボードを検出しました！");
            Serial.println("→ 専用処理モードで動作します");
            #endif
        } else {
            // DOIO_Bluetoothプロジェクトと同様に、全てのキーボードをDOIO KB16として強制認識
            isDOIOKeyboard = true;
            isConnected = true;  // 強制接続モード
            #if DEBUG_ENABLED
            Serial.println("*** 強制DOIO KB16モード! カスタムキーコードマッピングを使用します ***");
            Serial.printf("  - 実際のVID/PID: 0x%04X/0x%04X\n", device_vendor_id, device_product_id);
            Serial.println("  - 16バイト固定レポートサイズ");
            Serial.println("  - 改良されたキーマッピング適用");
            #endif
        }
        
        // ディスプレイに接続状況を表示（安定化）
        displayNeedsUpdate = true;
        
        #if DEBUG_ENABLED
        Serial.println("=========================\n");
        #endif
    }
    
    // デバイス切断時の処理（オーバーライド）
    void onGone(const usb_host_client_event_msg_t *eventMsg) override {
        Serial.println("キーボードが切断されました");
        isDOIOKeyboard = false;
        isConnected = false;  // 接続状態を解除
        
        // キーマトリックス状態をリセット
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                kb16_key_states[i][j] = false;
            }
        }
        lastPressedKeyRow = -1;
        lastPressedKeyCol = -1;
        
        // ディスプレイ更新（安定化）
        displayNeedsUpdate = true;
    }

    // キーボード入力処理（オーバーライド）
    void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) override {
        // DOIO KB16専用処理
        if (isDOIOKeyboard) {
            processDOIOKB16Report(report, last_report);
            return;
        }
        
        // 標準キーボード処理
        EspUsbHost::onKeyboard(report, last_report);
    }
    
    // DOIO KB16専用レポート処理
    void processDOIOKB16Report(hid_keyboard_report_t report, hid_keyboard_report_t last_report) {
        // DOIO_Bluetoothプロジェクトと同様のアプローチ: reserved フィールドをチェック
        // ただし、強制モードでは全てのレポートを処理する
        bool is_valid_doio_report = (report.reserved == 0xAA);
        
        #if VERBOSE_DEBUG
        if (!is_valid_doio_report) {
            Serial.printf("DOIO KB16: 非標準レポート形式 (reserved=0x%02X) - 強制処理モード\n", report.reserved);
        } else {
            Serial.println("DOIO KB16: 有効なレポート検出（0xAA形式）");
        }
        #endif
        
        // キーコードフィールドからデータを取得
        uint8_t kb16_data[6] = {0};
        uint8_t kb16_last_data[6] = {0};
        
        for (int i = 0; i < 6; i++) {
            kb16_data[i] = report.keycode[i];
            kb16_last_data[i] = last_report.keycode[i];
        }
        
        // デバッグ用: 初回レポート時は全データを表示
        #if VERBOSE_DEBUG
        static bool first_report = true;
        if (first_report) {
            Serial.printf("KB16初回レポート: modifier=0x%02X, reserved=0x%02X\n", report.modifier, report.reserved);
            for (int i = 0; i < 6; i++) {
                Serial.printf("  keycode[%d]=0x%02X\n", i, report.keycode[i]);
            }
            first_report = false;
        }
        #endif
        
        bool key_state_changed = false;
        unsigned long currentTime = millis();
        
        // 各キーマッピングをチェック
        for (int i = 0; i < sizeof(kb16_key_map) / sizeof(KeyMapping); i++) {
            const KeyMapping& mapping = kb16_key_map[i];
            
            if (mapping.byte_idx < 6) {
                uint8_t current_byte = kb16_data[mapping.byte_idx];
                uint8_t last_byte = kb16_last_data[mapping.byte_idx];
                
                bool current_state = (current_byte & mapping.bit_mask) != 0;
                bool last_state = (last_byte & mapping.bit_mask) != 0;
                
                // キー状態に変化があった場合
                if (current_state != last_state) {
                    kb16_key_states[mapping.row][mapping.col] = current_state;
                    
                    #if DEBUG_ENABLED
                    Serial.printf("Key (%d,%d) %s\n", 
                                mapping.row, mapping.col, 
                                current_state ? "PRESS" : "RELEASE");
                    #endif
                    
                    if (current_state) { // キーが押された
                        lastPressedKeyRow = mapping.row;
                        lastPressedKeyCol = mapping.col;
                        keyPressTime[mapping.row][mapping.col] = currentTime;
                        keyRepeating[mapping.row][mapping.col] = false;
                        
                        processKeyPress(mapping.row, mapping.col);
                    } else { // キーが離された
                        keyRepeating[mapping.row][mapping.col] = false;
                        keyPressTime[mapping.row][mapping.col] = 0;
                    }
                    
                    key_state_changed = true;
                }
                
                // 長押し検出とリピート処理
                if (current_state && keyPressTime[mapping.row][mapping.col] > 0) {
                    unsigned long pressedDuration = currentTime - keyPressTime[mapping.row][mapping.col];
                    
                    if (!keyRepeating[mapping.row][mapping.col] && pressedDuration >= KEY_REPEAT_DELAY) {
                        // 長押し開始
                        keyRepeating[mapping.row][mapping.col] = true;
                        lastRepeatTime[mapping.row][mapping.col] = currentTime;
                        #if DEBUG_ENABLED
                        Serial.printf("Key (%d,%d) repeat start\n", mapping.row, mapping.col);
                        #endif
                    } else if (keyRepeating[mapping.row][mapping.col] && 
                             (currentTime - lastRepeatTime[mapping.row][mapping.col]) >= KEY_REPEAT_RATE) {
                        // リピート実行
                        lastRepeatTime[mapping.row][mapping.col] = currentTime;
                        processKeyPress(mapping.row, mapping.col);
                        #if VERBOSE_DEBUG
                        Serial.printf("Key (%d,%d) repeat\n", mapping.row, mapping.col);
                        #endif
                    }
                }
            }
        }
        
        // キー状態が変化した場合のみディスプレイを更新
        if (key_state_changed) {
            updateKeyMatrixDisplay();
        }
    }
    
    void updateKeyMatrixDisplay() {
        if (!display) return;
        
        unsigned long now = millis();
        if (now - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) {
            return; // 更新頻度制限
        }
        lastDisplayUpdate = now;
        
        display->clearDisplay();
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);
        
        // ヘッダー
        display->setCursor(0, 0);
        display->println("DOIO KB16 Monitor");
        
        // 最後に押されたキー情報
        display->setCursor(0, 10);
        if (lastPressedKeyRow >= 0 && lastPressedKeyCol >= 0) {
            display->printf("Last: (%d,%d)", lastPressedKeyRow, lastPressedKeyCol);
        } else {
            display->print("Last: None");
        }
        
        // キーマトリックス表示（4x4）- コンパクト表示
        display->setCursor(0, 20);
        display->println("Keys:");
        
        for (int row = 0; row < 4; row++) {
            display->setCursor(0, 30 + row * 8);
            for (int col = 0; col < 4; col++) {
                display->print(kb16_key_states[row][col] ? "*" : ".");
            }
        }
        
        // テキストバッファ表示（短縮）
        if (textBuffer.length() > 0) {
            display->setCursor(0, 56);
            String shortText = textBuffer;
            if (shortText.length() > 16) {
                shortText = shortText.substring(shortText.length() - 16);
            }
            display->printf("Text:%s", shortText.c_str());
        }
        
        display->display();
    }
    
    // 接続状態表示の更新
    void updateConnectionDisplay() {
        static bool lastConnectionState = false;
        bool currentConnectionState = isDeviceConnected();
        
        // 接続状態に変化があった場合のみ更新
        if (currentConnectionState != lastConnectionState) {
            display->clearDisplay();
            display->setTextSize(1);
            display->setTextColor(SSD1306_WHITE);
            
            if (currentConnectionState) {
                // デバイス接続時の表示
                display->setCursor(10, 0);
                display->println("DOIO KB16");
                display->setCursor(15, 15);
                display->println("Connected!");
                
                if (isDOIOKeyboard) {
                    display->setCursor(5, 30);
                    display->println("Special Mode");
                    display->setCursor(0, 45);
                    display->println("Keys: Ready");
                } else {
                    display->setCursor(5, 30);
                    display->println("Standard HID");
                    display->setCursor(0, 45);
                    display->println("Mode: Active");
                }
            } else {
                // デバイス未接続時の表示
                display->setCursor(15, 0);
                display->println("USB Host Mode");
                display->setCursor(25, 15);
                display->println("Activated");
                display->setCursor(5, 30);
                display->println("Waiting for");
                display->setCursor(5, 40);
                display->println("DOIO KB16...");
                display->setCursor(0, 55);
                display->println("USB: Ready");
            }
            
            display->display();
            lastConnectionState = currentConnectionState;
            Serial.printf("Display updated - Connection: %s\n", 
                         currentConnectionState ? "Connected" : "Waiting");
        }
    }
    
    // 定期的な表示更新処理（制御強化・点滅完全防止）
    void periodicDisplayUpdate() {
        static unsigned long lastUpdateTime = 0;
        static bool lastDisplayUpdateFlag = false;
        unsigned long currentTime = millis();
        
        // displayNeedsUpdateフラグがtrueの場合のみ、かつ最低1秒間隔で更新
        if (displayNeedsUpdate && !lastDisplayUpdateFlag && 
            (currentTime - lastUpdateTime) > 1000) {
            
            updateConnectionDisplay();
            displayNeedsUpdate = false;
            lastDisplayUpdateFlag = true;
            lastUpdateTime = currentTime;
            
            Serial.println("Display updated due to connection state change");
        } else if (!displayNeedsUpdate) {
            lastDisplayUpdateFlag = false;  // フラグリセット
        }
    }
    
    // 生のUSBデータを表示するためのオーバーライド（DOIO_Bluetoothプロジェクトから移植）
    void onReceive(const usb_transfer_t *transfer) override {
        // 親クラスの処理を呼び出す
        EspUsbHost::onReceive(transfer);
        
        if (!isDOIOKeyboard) return;
        
        #if VERBOSE_DEBUG
        // DOIO KB16のデータを詳細に解析（デバッグ時のみ）
        if (transfer->actual_num_bytes > 0) {
            // データを16進数表示
            String hex_data = "";
            for (int i = 0; i < transfer->actual_num_bytes; i++) {
                if (transfer->data_buffer[i] < 16) hex_data += "0";
                hex_data += String(transfer->data_buffer[i], HEX) + " ";
            }
            hex_data.trim();
            hex_data.toUpperCase();
            
            Serial.printf("DOIO KB16 Raw Data [%dバイト]: %s\n", transfer->actual_num_bytes, hex_data.c_str());
            
            // レポートの構造を解析（DOIO_Bluetoothプロジェクトの実装）
            if (transfer->actual_num_bytes >= 8) {
                hid_keyboard_report_t report = {};
                
                // HIDレポート構造に変換
                report.modifier = transfer->data_buffer[0];
                report.reserved = transfer->data_buffer[1];
                
                // キーコードをレポート構造にコピー
                for (int i = 0; i < 6 && i + 2 < transfer->actual_num_bytes; i++) {
                    report.keycode[i] = transfer->data_buffer[i + 2];
                }
                
                // 非ゼロのデータを探す（可能なキーコードを特定）
                for (int i = 0; i < transfer->actual_num_bytes; i++) {
                    if (transfer->data_buffer[i] != 0 && i >= 2) {  // 先頭の2バイトは通常制御情報
                        uint8_t possibleKeycode = transfer->data_buffer[i];
                        
                        // 一般的なキーコードの範囲内かチェック
                        if (possibleKeycode >= 0x04 && possibleKeycode <= 0xE7 && 
                            possibleKeycode != 0x00 && possibleKeycode != 0x01 && 
                            possibleKeycode != 0xFF) {
                            Serial.printf("  潜在的なキーコード検出: 0x%02X at position %d\n", possibleKeycode, i);
                        }
                    }
                }
            }
        }
        #endif
    }
    
    // キー押下処理の統一メソッド
    void processKeyPress(int row, int col) {
        // DOIO_Bluetoothプロジェクトから移植した正しいHIDキーコードマッピング
        uint8_t hid_keycode = 0;
        char keyChar = '?';
        
        // 修正されたHIDキーコードマッピング（実際のDOIO KB16配置に合わせて）
        if (row == 0 && col == 0) {
            hid_keycode = 0x22; keyChar = '1'; // 1キー
        } else if (row == 0 && col == 1) {
            hid_keycode = 0x23; keyChar = '2'; // 2キー
        } else if (row == 0 && col == 2) {
            hid_keycode = 0x24; keyChar = '3'; // 3キー
        } else if (row == 0 && col == 3) {
            hid_keycode = 0x25; keyChar = '4'; // 4キー
        } else if (row == 1 && col == 0) {
            hid_keycode = 0x26; keyChar = '5'; // 5キー
        } else if (row == 1 && col == 1) {
            hid_keycode = 0x27; keyChar = '6'; // 6キー
        } else if (row == 1 && col == 2) {
            hid_keycode = 0x30; keyChar = '7'; // 7キー（修正）
        } else if (row == 1 && col == 3) {
            hid_keycode = 0x31; keyChar = '8'; // 8キー（修正）
        } else if (row == 2 && col == 0) {
            hid_keycode = 0x32; keyChar = '9'; // 9キー（修正）
        } else if (row == 2 && col == 1) {
            hid_keycode = 0x33; keyChar = '0'; // 0キー（修正）
        } else if (row == 2 && col == 2) {
            hid_keycode = 0x28; keyChar = 'E'; // Enterキー
        } else if (row == 2 && col == 3) {
            hid_keycode = 0x29; keyChar = 'X'; // Escキー
        } else if (row == 3 && col == 0) {
            hid_keycode = 0x2A; keyChar = 'B'; // Backspaceキー
        } else if (row == 3 && col == 1) {
            hid_keycode = 0x08; keyChar = 'A'; // Aキー（実際のユーザーキー配置）
        } else if (row == 3 && col == 2) {
            hid_keycode = 0x2C; keyChar = ' '; // Spaceキー
        } else if (row == 3 && col == 3) {
            hid_keycode = 0x2B; keyChar = 'T'; // Tabキー
        }
        
        #if VERBOSE_DEBUG
        Serial.printf("  HIDキーコード: 0x%02X, 表示文字: '%c'\n", hid_keycode, keyChar);
        #endif
        
        // テキストバッファに追加
        if (keyChar == 'B' && textBuffer.length() > 0) {
            // Backspace処理
            textBuffer = textBuffer.substring(0, textBuffer.length() - 1);
        } else if (keyChar != 'B' && keyChar != 'E' && keyChar != 'X' && keyChar != 'T') {
            textBuffer += keyChar;
            
            // 行の長さ制限
            if (textBuffer.length() >= MAX_TEXT_LENGTH) {
                textBuffer = textBuffer.substring(1);
            }
        }
        
        // ここでBLEキーボードへの送信や、シリアル出力などの実際の処理を行う
        // 現在はディスプレイ表示とテキストバッファ更新のみ
        #if DEBUG_ENABLED
        Serial.printf("Key processed: (%d,%d) -> '%c'\n", row, col, keyChar);
        #endif
    }
};

// グローバル変数
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DOIOKB16UsbHost* usbHost;

// 関数宣言
void initDisplay();
void showWriteMode();
void showUSBHostMode();

void setup() {
    // CPU周波数を240MHzに設定（パフォーマンス向上）
    setCpuFrequencyMhz(240);
    
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("=================================");
    Serial.println("DOIO KB16 USB-SSD1306 Display System");
    Serial.println("Based on KEYBOARD_BLE project");
    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    Serial.println("=================================");
    
    // I2C初期化（電力とクロック設定を最適化）
    Wire.begin();
    Wire.setClock(100000); // 100kHzに設定（電力消費を抑制）
    
    // SSD1306ディスプレイ初期化
    Serial.println("Initializing display...");
    initDisplay();
    
    // 電力状態をチェック
    Serial.printf("Available heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
    
    // 書き込みモード表示（5秒間）
    showWriteMode();
    
    // USBHostクラスのインスタンス作成
    usbHost = new DOIOKB16UsbHost(&display);
    
    // USBHost初期化
    Serial.println("Initializing USB Host...");
    usbHost->begin();
    
    Serial.println("USB Host initialized successfully!");
    
    // USBHostモード表示
    showUSBHostMode();
}

void loop() {
    // USBHostタスク処理（応答性重視：5ms間隔で高頻度処理）
    static unsigned long lastUsbTask = 0;
    static unsigned long lastDisplayCheck = 0;
    unsigned long now = millis();
    
    // USB処理の応答性を最大化（10ms -> 5ms）
    if (usbHost && (now - lastUsbTask) > USB_TASK_INTERVAL) { 
        usbHost->task();
        lastUsbTask = now;
    }
    
    // 表示更新は必要時のみ実行（1秒間隔に増加）
    if (usbHost && (now - lastDisplayCheck) > 1000) { 
        ((DOIOKB16UsbHost*)usbHost)->periodicDisplayUpdate();
        lastDisplayCheck = now;
    }
    
    // CPU負荷軽減のため最小限の待機（5ms -> 2ms：さらなる応答性向上）
    delay(2);
}

void initDisplay() {
    // 電源安定化のため少し待機
    delay(100);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed - retrying...");
        
        // リトライ機構（バッテリー電源時の不安定性対策）
        delay(500);
        if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            Serial.println("SSD1306 allocation failed - second attempt");
            delay(1000);
            if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
                Serial.println("SSD1306 allocation failed - final attempt failed");
                for(;;) {
                    Serial.println("Display initialization failed - check power/connections");
                    delay(5000);
                }
            }
        }
    }
    
    // 表示設定を調整（電力消費を抑制）
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Display initialized");
    display.display();
    delay(1000);
    
    Serial.println("SSD1306 display initialized successfully");
}

void showWriteMode() {
    Serial.println("Entering 5-second write mode...");
    
    for (int i = 5; i > 0; i--) {
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        
        // タイトル表示
        display.setCursor(10, 5);
        display.println("USB Write");
        display.setCursor(30, 25);
        display.println("Mode");
        
        // カウントダウン表示
        display.setTextSize(1);
        display.setCursor(15, 45);
        display.printf("Starting in %d sec", i);
        
        display.display();
        
        Serial.printf("Write mode countdown: %d\n", i);
        delay(1000);
    }
    
    Serial.println("Write mode ended. Switching to USB Host mode.");
}

void showUSBHostMode() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // ヘッダー
    display.setCursor(15, 0);
    display.println("USB Host Mode");
    
    // 状態表示
    display.setCursor(25, 15);
    display.println("Activated");
    
    // 待機メッセージ
    display.setCursor(5, 30);
    display.println("Waiting for");
    display.setCursor(5, 40);
    display.println("DOIO KB16...");
    
    // ステータス
    display.setCursor(0, 55);
    display.println("USB: Ready");
    
    display.display();
    
    Serial.println("USB Host mode activated. Waiting for DOIO KB16...");
}