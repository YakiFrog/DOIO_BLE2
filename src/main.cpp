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
#define DEBUG_ENABLED 1

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
    
    // 表示の最適化用
    unsigned long lastDisplayUpdate = 0;
    unsigned long lastConnectionUpdate = 0;
    const unsigned long DISPLAY_UPDATE_INTERVAL = 200; // 200msに変更（点滅防止）
    const unsigned long CONNECTION_UPDATE_INTERVAL = 1000; // 接続状態更新は1秒間隔
    bool displayNeedsUpdate = false;

public:
    DOIOKB16UsbHost(Adafruit_SSD1306* disp) : display(disp) {
        // キーマトリックスの初期化
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                kb16_key_states[i][j] = false;
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
        Serial.println("\n=== USB キーボード接続 ===");
        
        // デバイス情報の表示
        String manufacturer = getUsbDescString(dev_info.str_desc_manufacturer);
        String product = getUsbDescString(dev_info.str_desc_product);
        String serialNum = getUsbDescString(dev_info.str_desc_serial_num);
        
        Serial.printf("製造元: %s\n", manufacturer.length() > 0 ? manufacturer.c_str() : "不明");
        Serial.printf("製品名: %s\n", product.length() > 0 ? product.c_str() : "不明");
        Serial.printf("シリアル: %s\n", serialNum.length() > 0 ? serialNum.c_str() : "不明");
        Serial.printf("VID: 0x%04X, PID: 0x%04X\n", device_vendor_id, device_product_id);
        
        // DOIO KB16の検出
        if (device_vendor_id == DOIO_VID && device_product_id == DOIO_PID) {
            isDOIOKeyboard = true;
            isConnected = true;  // 接続状態を設定
            Serial.println("★ DOIO KB16キーボードを検出しました！");
            Serial.println("→ 専用処理モードで動作します");
        } else {
            isDOIOKeyboard = false;
            isConnected = true;  // 標準HIDキーボードでも接続状態を設定
            Serial.println("→ 標準HIDキーボードとして動作します");
        }
        
        // ディスプレイに接続状況を表示（安定化）
        displayNeedsUpdate = true;
        
        Serial.println("=========================\n");
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
        // DOIO KB16の特殊なレポート形式識別子をチェック
        if (report.reserved != 0xAA) {
            return; // 無効なレポート
        }
        
        // キーコードフィールドからデータを取得
        uint8_t kb16_data[6] = {0};
        uint8_t kb16_last_data[6] = {0};
        
        for (int i = 0; i < 6; i++) {
            kb16_data[i] = report.keycode[i];
            kb16_last_data[i] = last_report.keycode[i];
        }
        
        bool key_state_changed = false;
        
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
                    
                    Serial.printf("DOIO KB16: キー (%d,%d) %s\n", 
                                mapping.row, mapping.col, 
                                current_state ? "押下" : "解放");
                    
                    if (current_state) { // キーが押された
                        lastPressedKeyRow = mapping.row;
                        lastPressedKeyCol = mapping.col;
                        
                        // 簡単なキーマッピング（数字キー）
                        char keyChar = '?';
                        if (mapping.row == 0 && mapping.col == 0) keyChar = '1';
                        else if (mapping.row == 0 && mapping.col == 1) keyChar = '2';
                        else if (mapping.row == 0 && mapping.col == 2) keyChar = '3';
                        else if (mapping.row == 0 && mapping.col == 3) keyChar = '4';
                        else if (mapping.row == 1 && mapping.col == 0) keyChar = '5';
                        else if (mapping.row == 1 && mapping.col == 1) keyChar = '6';
                        else if (mapping.row == 1 && mapping.col == 2) keyChar = '7';
                        else if (mapping.row == 1 && mapping.col == 3) keyChar = '8';
                        else if (mapping.row == 2 && mapping.col == 0) keyChar = '9';
                        else if (mapping.row == 2 && mapping.col == 1) keyChar = '0';
                        else if (mapping.row == 2 && mapping.col == 2) keyChar = 'E'; // Enter
                        else if (mapping.row == 2 && mapping.col == 3) keyChar = 'X'; // Esc
                        else if (mapping.row == 3 && mapping.col == 0) keyChar = 'B'; // Backspace
                        else if (mapping.row == 3 && mapping.col == 1) keyChar = 'T'; // Tab
                        else if (mapping.row == 3 && mapping.col == 2) keyChar = ' '; // Space
                        else if (mapping.row == 3 && mapping.col == 3) keyChar = 'A'; // Alt
                        
                        // テキストバッファに追加
                        if (keyChar == 'B' && textBuffer.length() > 0) {
                            // Backspace処理
                            textBuffer = textBuffer.substring(0, textBuffer.length() - 1);
                        } else if (keyChar != 'B') {
                            textBuffer += keyChar;
                            
                            // 行の長さ制限
                            if (textBuffer.length() >= MAX_TEXT_LENGTH) {
                                textBuffer = textBuffer.substring(1);
                            }
                        }
                    }
                    
                    key_state_changed = true;
                }
            }
        }
        
        // キー状態が変化した場合はディスプレイを更新
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
    // USBHostタスク処理（頻度を下げる）
    static unsigned long lastUsbTask = 0;
    static unsigned long lastDisplayCheck = 0;
    unsigned long now = millis();
    
    if (usbHost && (now - lastUsbTask) > 10) { // 10ms間隔でUSBタスク処理
        usbHost->task();
        lastUsbTask = now;
    }
    
    // 表示更新は別の頻度で制御（点滅防止）
    if (usbHost && (now - lastDisplayCheck) > 100) { // 100ms間隔で表示チェック
        ((DOIOKB16UsbHost*)usbHost)->periodicDisplayUpdate();
        lastDisplayCheck = now;
    }
    
    // CPU負荷軽減のため少し待機
    delay(5);
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