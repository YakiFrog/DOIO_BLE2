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

// HID関連の定義の競合を防ぐため、再度チェック
#ifdef HID_CLASS
#undef HID_CLASS
#endif
#ifdef HID_SUBCLASS_NONE
#undef HID_SUBCLASS_NONE
#endif

#include "PythonStyleAnalyzer.h"

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
    
    // ディスプレイ初期化
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("エラー: SSD1306初期化失敗");
        while (true) delay(1000);
    }
    
    // ===== 5秒間のプログラミングモード（書き込みモード）開始 =====
    Serial.println("Starting 5-second programming mode...");
    
    // プログラミングモード初期表示
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("USB->BLE Bridge");
    display.println("===============");
    display.println("");
    display.println("Programming Mode");
    display.println("Safe to upload");
    display.println("");
    display.println("Starting in 5s...");
    display.display();
    
    // 5秒カウントダウン
    for (int i = 5; i > 0; i--) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("USB->BLE Bridge");
        display.println("===============");
        display.println("");
        display.println("Programming Mode");
        display.println("Safe to upload");
        display.println("");
        display.print("Starting in ");
        display.print(i);
        display.println("s...");
        display.display();
        
        // 1秒待機
        delay(1000);
        
        Serial.printf("Programming mode countdown: %d seconds remaining\n", i);
    }
    
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
    bleKeyboard.setDelay(5);  // 最大高速化（20ms→5ms）
    Serial.println("✓ BLEキーボードを初期化しました");
    
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
    

    
    // ディスプレイのアイドル状態更新（定期的にチェック）
    static unsigned long lastIdleCheck = 0;
    if (millis() - lastIdleCheck > 1000) {  // 1秒ごとにチェック
        lastIdleCheck = millis();
        analyzer->updateDisplayIdle();
    }
    
    // 最小限の遅延（応答性最優先）
    delayMicroseconds(100);
}
