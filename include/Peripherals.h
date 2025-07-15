#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include <Arduino.h>

// GPIOピンの設定
#define INTERNAL_LED_PIN 21    // 内蔵LED（キー入力表示用）
#define STATUS_LED_PIN 2       // 外部赤色LED（電源/Bluetooth状態表示用）
#define BUZZER_PIN 1           // 圧電スピーカー

// サウンド設定
#define SOUND_ENABLED 1        // サウンド機能の有効/無効
#define KEY_FREQ 800           // キー押下時の周波数 (Hz)
#define KEY_DURATION 10        // キー音の長さ (ms)

// デバッグ出力設定
#define DEBUG_OUTPUT 1         // シリアルデバッグ出力の有効/無効
#define USB_DEBUG_DETAIL 1     // 詳細なUSB情報デバッグの有効/無効

// LED点滅設定
#define BLE_BLINK_INTERVAL 500 // BLE未接続時のLED点滅間隔 (ms)

// 起動音階のノート定義
#define NOTE_C5  523
#define NOTE_E5  659
#define NOTE_G5  784
#define NOTE_C6  1047

// LED制御クラス
class LEDController {
public:
    // 初期化
    void begin();
    
    // キー入力表示用LED制御
    void keyPressed();
    void updateKeyLED();
    
    // ステータスLED制御
    void setStatusLED(bool on);
    void updateStatusLED();
    void setBleConnected(bool connected);
    
private:
    unsigned long lastKeyPressTime = 0;
    const unsigned long keyLedDuration = 100;  // キー入力LEDの点灯時間(ms)
    
    bool bleConnected = false;
    unsigned long lastBlinkTime = 0;
    bool blinkState = false;
};

// スピーカー制御クラス
class SpeakerController {
public:
    // 初期化
    void begin();
    
    // サウンド再生
    void playKeySound();
    void playStartupMelody();
    void playConnectedSound();
    void playDisconnectedSound();
    
private:
    void tone(unsigned int frequency, unsigned long duration);
    void noTone();
};

// グローバルインスタンス
extern LEDController ledController;
extern SpeakerController speakerController;

#endif // PERIPHERALS_H
