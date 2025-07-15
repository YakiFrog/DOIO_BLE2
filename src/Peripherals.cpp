#include "Peripherals.h"

// グローバルインスタンスの定義
LEDController ledController;
SpeakerController speakerController;

// LEDController実装

void LEDController::begin() {
    // 内蔵LEDのピン設定
    pinMode(INTERNAL_LED_PIN, OUTPUT);
    digitalWrite(INTERNAL_LED_PIN, LOW);
    
    // 外部ステータスLEDのピン設定
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH); // 電源投入時に点灯
    
    // 初期状態設定
    bleConnected = false;
    blinkState = false;
    lastBlinkTime = 0;
    lastKeyPressTime = 0;
}

void LEDController::keyPressed() {
    // シリアルにデバッグ出力を追加
    Serial.println("LED: Key pressed!");
    
    // LEDを確実にONにする
    pinMode(INTERNAL_LED_PIN, OUTPUT); // 念のためピンモードを再設定
    digitalWrite(INTERNAL_LED_PIN, HIGH);
    lastKeyPressTime = millis();
}

void LEDController::updateKeyLED() {
    // キーLEDの自動消灯
    if (lastKeyPressTime > 0 && millis() - lastKeyPressTime > keyLedDuration) {
        digitalWrite(INTERNAL_LED_PIN, LOW);
        lastKeyPressTime = 0;
    }
}

void LEDController::setStatusLED(bool on) {
    digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

void LEDController::updateStatusLED() {
    unsigned long currentMillis = millis();

    if (bleConnected) {
        // BLE接続中は常時点灯
        setStatusLED(true);
    } else {
        // BLE未接続時は点滅
        if (currentMillis - lastBlinkTime > BLE_BLINK_INTERVAL) {
            lastBlinkTime = currentMillis;
            blinkState = !blinkState;
            setStatusLED(blinkState);
        }
    }
}

void LEDController::setBleConnected(bool connected) {
    if (bleConnected != connected) {
        bleConnected = connected;
        
        // 接続状態が変わった場合、即時LEDを更新
        if (bleConnected) {
            setStatusLED(true);  // 接続時は点灯
        } else {
            // 切断時は点滅状態にセット
            blinkState = true;
            setStatusLED(true);
            lastBlinkTime = millis();
        }
    }
}

// SpeakerController実装

void SpeakerController::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    noTone();
}

void SpeakerController::tone(unsigned int frequency, unsigned long duration) {
    #if SOUND_ENABLED
    // 指定した周波数で音を鳴らす
    ledcSetup(0, frequency, 8); // チャネル0、8ビット分解能
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 128); // 50%デューティサイクル
    
    // 指定した時間が経過したら音を止める（非ブロッキング版）
    delay(duration);
    noTone();
    #endif
}

void SpeakerController::noTone() {
    #if SOUND_ENABLED
    ledcDetachPin(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
    #endif
}

void SpeakerController::playKeySound() {
    #if SOUND_ENABLED
    // デバッグ出力を追加
    Serial.println("SPEAKER: Playing key sound");
    
    // 確実に音を鳴らすために念のためpinModeを再設定
    pinMode(BUZZER_PIN, OUTPUT);
    tone(KEY_FREQ, KEY_DURATION);
    #endif
}

void SpeakerController::playStartupMelody() {
    #if SOUND_ENABLED
    // 起動音（短めのメロディ）
    tone(NOTE_C5, 100);
    delay(20);
    tone(NOTE_E5, 100);
    delay(20);
    tone(NOTE_G5, 100);
    delay(20);
    tone(NOTE_C6, 200);
    #endif
}

void SpeakerController::playConnectedSound() {
    #if SOUND_ENABLED
    // 接続音（上昇音）
    tone(NOTE_C5, 80);
    delay(50);
    tone(NOTE_G5, 150);
    #endif
}

void SpeakerController::playDisconnectedSound() {
    #if SOUND_ENABLED
    // 切断音（下降音）
    tone(NOTE_G5, 80);
    delay(50);
    tone(NOTE_C5, 150);
    #endif
}
