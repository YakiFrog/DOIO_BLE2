// DOIO KB16 USB-to-BLE Bridge with Python-style HID Analysis
// USBキーボード入力をBLEキーボードに転送する完全なブリッジシステム

// HID定義の競合を回避
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
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>

// HID関連の定義の競合を防ぐため、再度チェック
#ifdef HID_CLASS
#undef HID_CLASS
#endif
#ifdef HID_SUBCLASS_NONE
#undef HID_SUBCLASS_NONE
#endif

#include "PythonStyleAnalyzer.h"
#include "Peripherals.h"
#include "StartupAnimation.h"

// SSD1306ディスプレイ設定
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Seeed XIAO ESP32S3のI2Cピン設定
#define SDA_PIN 5
#define SCL_PIN 6

// グローバル変数
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BleKeyboard bleKeyboard("DOIO KB16 Bridge", "DOIO Bridge", 100);
PythonStyleAnalyzer* analyzer;

// BLE接続制御フラグ
bool bleAutoReconnect = true;   // 起動時は自動接続
bool bleManualConnect = false;  // 手動接続フラグ
bool bleStackInitialized = false;  // BLEスタックの初期化状態

// BLE接続制御関数の前方宣言
void startBleConnection();
void stopBleConnection();

void setup() {
    // CPU最高速度で動作
    setCpuFrequencyMhz(240);
    
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        delay(100);
    }
    
    Serial.println("=======================================");
    Serial.println("DOIO KB16 USB-to-BLE Bridge System");
    Serial.println("Python Compatible HID + BLE Forward");
    Serial.printf("CPU周波数: %d MHz\n", getCpuFrequencyMhz());
    Serial.println("=======================================");
    
    // I2C初期化
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(400000);  // 高速I2C
    delay(100);
    
    // LEDとスピーカーの初期化
    ledController.begin();
    speakerController.begin();
    
    // 起動音を再生
    speakerController.playStartupMelody();
    
    // ディスプレイ初期化
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("エラー: SSD1306初期化失敗");
        while (true) delay(1000);
    }

    // ===== 5秒間の起動画面アニメーション =====
    StartupAnimation startupAnim(&display);
    startupAnim.showStartupScreen();

    Serial.println("Programming mode finished. Starting Bridge mode...");
    
    // プログラミングモード終了の表示
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("USB->BLE Bridge");
    display.println("===============");
    display.println("");
    display.println("Bridge Mode");
    display.println("ACTIVATED!");
    display.println("");
    display.println("Initializing...");
    display.display();
    delay(1000);
    
    // ===== BLEキーボード初期化 =====
    bleKeyboard.begin();
    bleKeyboard.setDelay(1);  // 極限高速化（5ms→1ms）
    
    // 起動時は自動接続を有効にする
    bleAutoReconnect = true;
    bleStackInitialized = true;
    
    Serial.println("✓ BLEキーボードを初期化しました（自動接続モード）");
    Serial.println("  - 起動時は自動接続有効");
    Serial.println("  - Ctrl+Alt+Bで手動制御に切り替え可能");
    
    // 初期化後の安定化待機を短縮
    delay(500);  // 1000ms→500ms
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("USB->BLE Bridge");
    display.println("===============");
    display.println("");
    display.println("BLE: Ready");
    display.println("USB: Waiting...");
    display.println("");
    display.println("Connect keyboard");
    display.display();
    
    delay(1000);
    
    // ===== Pythonスタイルアナライザー初期化 =====
    analyzer = new PythonStyleAnalyzer(&display, &bleKeyboard);
    analyzer->begin();
    
    Serial.println("システム初期化完了");
    Serial.println("USBキーボードを接続してください...");
    Serial.println("すべてのキー入力がBLEキーボードに自動転送されます");
    Serial.println("");
    Serial.println("BLE接続制御:");
    Serial.println("  起動時は自動接続が有効です");
    Serial.println("  Ctrl+Alt+B - BLE接続/切断の切り替え");
    Serial.println("  手動切断後は自動再接続が無効になります");
    
    // 待機状態表示
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("DOIO KB16 Bridge");
    display.println("================");
    display.println("");
    if (bleKeyboard.isConnected()) {
        display.println("BLE: Connected");
    } else {
        display.println("BLE: Waiting");
    }
    display.println("USB: Waiting...");
    display.println("");
    display.println("Connect USB kbd");
    display.println("to start bridge");
    display.display();
}

void loop() {
    // USBタスクの実行（PythonアナライザーとBLE転送処理が内部で行われる）
    analyzer->task();
    
    // 長押しリピート処理を高頻度で実行（重要！）
    analyzer->handleKeyRepeat();
    
    // LEDの更新処理
    ledController.updateKeyLED();
    ledController.updateStatusLED();
    
    // BLE接続状態の監視とLED制御
    static bool lastBleConnected = false;
    bool currentBleConnected = false;
    
    // BLEスタックが無効化されている場合は接続チェックをスキップ
    if (bleStackInitialized) {
        currentBleConnected = bleKeyboard.isConnected();
    }
    
    // BLE接続状態の変化を検出
    if (lastBleConnected != currentBleConnected) {
        lastBleConnected = currentBleConnected;
        ledController.setBleConnected(currentBleConnected);
        
        if (currentBleConnected) {
            Serial.println("BLE接続しました");
            speakerController.playConnectedSound();
            bleManualConnect = true;  // 手動接続成功
        } else {
            Serial.println("BLE切断しました");
            speakerController.playDisconnectedSound();
            
            // 自動再接続の制御
            if (!bleAutoReconnect) {
                Serial.println("自動再接続が無効のため、手動接続を待機しています");
                bleManualConnect = false;  // 手動接続リセット
                
                // 自動再接続を完全に防ぐため、BLEスタックを停止
                if (bleStackInitialized) {
                    bleKeyboard.end();
                    
                    // ESP32のBLEスタックを完全に停止
                    esp_bluedroid_disable();
                    esp_bluedroid_deinit();
                    esp_bt_controller_disable();
                    esp_bt_controller_deinit();
                    
                    bleStackInitialized = false;
                    Serial.println("✓ BLEスタックを完全に停止しました");
                }
            } else {
                Serial.println("自動再接続モードのため、再接続を試行します");
            }
        }
    }
    
    // 手動接続トリガー（例：特定のキー入力で再接続）
    // この部分は後で実装予定
    
    // ディスプレイのアイドル状態更新（定期的にチェック）
    static unsigned long lastIdleCheck = 0;
    if (millis() - lastIdleCheck > 1000) {  // 1秒ごとにチェック
        lastIdleCheck = millis();
        analyzer->updateDisplayIdle();
    }
    
    // 最小限の遅延（応答性最優先）
    delayMicroseconds(100);
}

// BLE接続制御関数
void startBleConnection() {
    if (!bleStackInitialized) {
        Serial.println("手動でBLE接続を開始します...");
        
        // ESP32のBLEスタックを再初期化
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_bt_controller_init(&bt_cfg);
        esp_bt_controller_enable(ESP_BT_MODE_BLE);
        esp_bluedroid_init();
        esp_bluedroid_enable();
        
        // BLEキーボードを開始
        bleKeyboard.begin();
        bleManualConnect = true;
        bleAutoReconnect = true;  // 手動接続時は自動再接続を有効にする
        bleStackInitialized = true;
        
        Serial.println("BLE接続を開始しました（自動再接続有効）");
    } else if (!bleKeyboard.isConnected()) {
        Serial.println("BLE再接続を試行します...");
        bleKeyboard.begin();
        bleManualConnect = true;
        bleAutoReconnect = true;
    }
}

void stopBleConnection() {
    if (bleStackInitialized) {
        Serial.println("BLE接続を手動で停止します...");
        
        // 自動再接続を無効にする
        bleAutoReconnect = false;
        bleManualConnect = false;
        
        // BLEサービスを停止
        bleKeyboard.end();
        
        // ESP32のBLEデバイスを完全に停止
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        
        bleStackInitialized = false;
        
        Serial.println("BLE接続を完全に停止しました");
        Serial.println("再接続するにはCtrl+Alt+Bを押してください");
    }
}
