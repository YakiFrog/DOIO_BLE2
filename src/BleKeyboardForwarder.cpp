#include "BleKeyboardForwarder.h"

// HIDキーコードからBLEライブラリ用キーコードへの変換テーブル
// DOIO KB16の4つずらしたキーコードに対応
struct HidToBleMapping {
    uint8_t hidKeycode;
    uint8_t bleKeycode;
};

// DOIO KB16のキーコードマッピング（4つずらし対応）
const HidToBleMapping HID_TO_BLE_MAP[] = {
    // DOIO KB16カスタムアルファベット (0x08-0x21) -> 標準HIDキーコード (0x04-0x1D)
    {0x08, 0x04}, // a
    {0x09, 0x05}, // b
    {0x0A, 0x06}, // c
    {0x0B, 0x07}, // d
    {0x0C, 0x08}, // e
    {0x0D, 0x09}, // f
    {0x0E, 0x0A}, // g
    {0x0F, 0x0B}, // h
    {0x10, 0x0C}, // i
    {0x11, 0x0D}, // j
    {0x12, 0x0E}, // k
    {0x13, 0x0F}, // l
    {0x14, 0x10}, // m
    {0x15, 0x11}, // n
    {0x16, 0x12}, // o
    {0x17, 0x13}, // p
    {0x18, 0x14}, // q
    {0x19, 0x15}, // r
    {0x1A, 0x16}, // s
    {0x1B, 0x17}, // t
    {0x1C, 0x18}, // u
    {0x1D, 0x19}, // v
    {0x1E, 0x1A}, // w
    {0x1F, 0x1B}, // x
    {0x20, 0x1C}, // y
    {0x21, 0x1D}, // z
    
    // DOIO KB16カスタム数字 (0x22-0x2B) -> 標準HIDキーコード (0x1E-0x27)
    {0x22, 0x1E}, // 1
    {0x23, 0x1F}, // 2
    {0x24, 0x20}, // 3
    {0x25, 0x21}, // 4
    {0x26, 0x22}, // 5
    {0x27, 0x23}, // 6
    {0x28, 0x24}, // 7
    {0x29, 0x25}, // 8
    {0x2A, 0x26}, // 9
    {0x2B, 0x27}, // 0
    
    // DOIO KB16カスタム制御キー (0x2C-0x3C) -> 標準HIDキーコード (0x28-0x38)
    {0x2C, 0x28}, // Enter
    {0x2D, 0x29}, // Escape
    {0x2E, 0x2A}, // Backspace
    {0x2F, 0x2B}, // Tab
    {0x30, 0x2C}, // Space
    {0x31, 0x2D}, // -
    {0x32, 0x2E}, // =
    {0x33, 0x2F}, // [
    {0x34, 0x30}, // ]
    {0x35, 0x31}, // backslash
    {0x37, 0x33}, // ;
    {0x38, 0x34}, // '
    {0x39, 0x35}, // `
    {0x3A, 0x36}, // ,
    {0x3B, 0x37}, // .
    {0x3C, 0x38}, // /
    
    // ファンクションキー (0x3E-0x49) -> 標準HIDキーコード (0x3A-0x45)
    {0x3E, 0x3A}, // F1
    {0x3F, 0x3B}, // F2
    {0x40, 0x3C}, // F3
    {0x41, 0x3D}, // F4
    {0x42, 0x3E}, // F5
    {0x43, 0x3F}, // F6
    {0x44, 0x40}, // F7
    {0x45, 0x41}, // F8
    {0x46, 0x42}, // F9
    {0x47, 0x43}, // F10
    {0x48, 0x44}, // F11
    {0x49, 0x45}, // F12
    
    // 特殊キー (0x4D-0x56) -> 標準HIDキーコード (0x49-0x52)
    {0x4D, 0x49}, // Insert
    {0x4E, 0x4A}, // Home
    {0x4F, 0x4B}, // Page Up
    {0x50, 0x4C}, // Delete
    {0x51, 0x4D}, // End
    {0x52, 0x4E}, // Page Down
    {0x53, 0x4F}, // Right Arrow
    {0x54, 0x50}, // Left Arrow
    {0x55, 0x51}, // Down Arrow
    {0x56, 0x52}, // Up Arrow
    
    // テンキー (0x58-0x67) -> 標準HIDキーコード (0x54-0x63)
    {0x58, 0x54}, // Keypad /
    {0x59, 0x55}, // Keypad *
    {0x5A, 0x56}, // Keypad -
    {0x5B, 0x57}, // Keypad +
    {0x5C, 0x58}, // Keypad Enter
    {0x5D, 0x59}, // Keypad 1
    {0x5E, 0x5A}, // Keypad 2
    {0x5F, 0x5B}, // Keypad 3
    {0x60, 0x5C}, // Keypad 4
    {0x61, 0x5D}, // Keypad 5
    {0x62, 0x5E}, // Keypad 6
    {0x63, 0x5F}, // Keypad 7
    {0x64, 0x60}, // Keypad 8
    {0x65, 0x61}, // Keypad 9
    {0x66, 0x62}, // Keypad 0
    {0x67, 0x63}, // Keypad .
};

BleKeyboardForwarder::BleKeyboardForwarder() {
    bleKeyboard = nullptr;
}

BleKeyboardForwarder::~BleKeyboardForwarder() {
    end();
}

bool BleKeyboardForwarder::begin(const String& deviceName) {
    Serial.println("BLEキーボード転送機能を初期化中...");
    
    try {
        bleKeyboard = new BleKeyboard(deviceName.c_str(), "DOIO Bridge", 100);
        
        // DOIO KB16として識別されるようにVID/PIDを設定
        bleKeyboard->set_vendor_id(0xD010);  // DOIO VID
        bleKeyboard->set_product_id(0x1601); // DOIO KB16 PID
        bleKeyboard->set_version(0x0100);
        
        Serial.println("DEBUG: BleKeyboard object created");
        
        bleKeyboard->begin();
        Serial.println("DEBUG: bleKeyboard->begin() called");
        
        // 少し待ってから接続状態を確認
        delay(1000);
        
        bleEnabled = true;
        
        Serial.println("✓ BLEキーボード初期化完了");
        Serial.printf("  デバイス名: %s\n", deviceName.c_str());
        Serial.println("  VID: 0xD010, PID: 0x1601 (DOIO KB16)");
        Serial.printf("  初期接続状態: %s\n", bleKeyboard->isConnected() ? "接続済み" : "未接続");
        Serial.println("  BLE接続を待機中...");
        
        return true;
    } catch (const std::exception& e) {
        Serial.printf("✗ BLEキーボード初期化エラー: %s\n", e.what());
        bleEnabled = false;
        return false;
    } catch (...) {
        Serial.println("✗ BLEキーボード初期化で不明なエラーが発生");
        bleEnabled = false;
        return false;
    }
}

void BleKeyboardForwarder::end() {
    if (bleKeyboard) {
        bleKeyboard->end();
        delete bleKeyboard;
        bleKeyboard = nullptr;
    }
    bleEnabled = false;
    Serial.println("BLEキーボード転送機能を終了しました");
}

bool BleKeyboardForwarder::isConnected() {
    bool connected = bleEnabled && bleKeyboard && bleKeyboard->isConnected();
    // デバッグ情報は過度に出力されるのを防ぐため、状態変化時のみ出力
    static bool lastConnectedState = false;
    if (connected != lastConnectedState) {
        Serial.printf("DEBUG: BLE接続状態変化 - %s\n", connected ? "接続" : "切断");
        lastConnectedState = connected;
    }
    return connected;
}

uint8_t BleKeyboardForwarder::hidToBleKeycode(uint8_t hidKeycode) {
    // DOIO KB16のキーコード変換テーブルを検索
    for (size_t i = 0; i < sizeof(HID_TO_BLE_MAP) / sizeof(HidToBleMapping); i++) {
        if (HID_TO_BLE_MAP[i].hidKeycode == hidKeycode) {
            return HID_TO_BLE_MAP[i].bleKeycode;
        }
    }
    
    // 標準HIDキーコードの場合はそのまま使用
    if (hidKeycode >= 0x04 && hidKeycode <= 0x65) {
        return hidKeycode;
    }
    
    // 修飾キーはそのまま
    if (hidKeycode >= 0xE0 && hidKeycode <= 0xE7) {
        return hidKeycode;
    }
    
    // 変換できない場合は0を返す
    return 0;
}

void BleKeyboardForwarder::forwardHidReport(const uint8_t* report_data, int data_size) {
    Serial.printf("DEBUG: forwardHidReport called - connected=%s, enabled=%s\n", 
                 isConnected() ? "true" : "false", 
                 bleEnabled ? "true" : "false");
    
    if (!isConnected()) {
        Serial.println("DEBUG: BLE not connected, skipping");
        return;
    }
    
    // 送信間隔制御
    if (millis() - lastSendTime < SEND_INTERVAL) {
        Serial.println("DEBUG: Too soon to send, skipping");
        return;
    }
    
    Serial.println("\n--- BLE転送処理開始 ---");
    Serial.printf("受信データサイズ: %d bytes\n", data_size);
    
    // 生データを表示
    Serial.print("生データ: ");
    for (int i = 0; i < data_size; i++) {
        Serial.printf("%02X ", report_data[i]);
    }
    Serial.println();
    
    // HIDレポートの解析
    uint8_t modifier = 0;
    uint8_t keycodes[6] = {0};
    int keycount = 0;
    
    if (data_size == 16) {
        // DOIO KB16の16バイトレポート
        modifier = report_data[1]; // バイト1が修飾キー
        
        Serial.printf("修飾キー: 0x%02X\n", modifier);
        Serial.print("キーコード: ");
        
        // バイト2以降からキーコードを抽出
        for (int i = 2; i < data_size && keycount < 6; i++) {
            if (report_data[i] != 0) {
                uint8_t converted = hidToBleKeycode(report_data[i]);
                Serial.printf("バイト%d: 0x%02X->0x%02X ", i, report_data[i], converted);
                if (converted != 0) {
                    keycodes[keycount] = converted;
                    keycount++;
                } else {
                    Serial.print("(無変換) ");
                }
            }
        }
        Serial.println();
        
    } else if (data_size == 8) {
        // 標準8バイトレポート
        modifier = report_data[0];
        
        Serial.printf("修飾キー: 0x%02X\n", modifier);
        Serial.print("キーコード: ");
        
        for (int i = 2; i < 8 && keycount < 6; i++) {
            if (report_data[i] != 0) {
                keycodes[keycount] = report_data[i]; // 標準はそのまま
                Serial.printf("0x%02X ", keycodes[keycount]);
                keycount++;
            }
        }
        Serial.println();
    }
    
    Serial.printf("最終結果: modifier=0x%02X, keycount=%d\n", modifier, keycount);
    
    // BLE転送方法を選択
    if (data_size == 16) {
        // 16バイトデータは直接転送とpress/releaseの両方を試す
        Serial.println("DEBUG: 16バイトデータ - 直接転送");
        forwardRawHidReport(report_data, data_size);
        
        delay(10);
        
        Serial.println("DEBUG: 16バイトデータ - press/release転送");
        forwardUsingPressRelease(modifier, keycodes, keycount);
    } else {
        // 8バイトデータは標準的な方法で転送
        Serial.println("DEBUG: 8バイトデータ - 標準転送");
        forwardStandardKeyboard(modifier, keycodes, keycount);
    }
    
    lastSendTime = millis();
    Serial.println("--- BLE転送処理完了 ---\n");
}

void BleKeyboardForwarder::forwardStandardKeyboard(const uint8_t modifier, const uint8_t* keycodes, int keycode_count) {
    Serial.printf("DEBUG: forwardStandardKeyboard called - connected=%s\n", 
                 isConnected() ? "true" : "false");
    
    if (!isConnected()) {
        Serial.println("DEBUG: BLE not connected in forwardStandardKeyboard");
        return;
    }
    
    // BLEKeyReportを直接作成して送信
    BLEKeyReport keyReport;
    keyReport.modifiers = modifier;
    keyReport.reserved = 0;
    
    // キーコードを配列にコピー（最大6個）
    for (int i = 0; i < 6; i++) {
        if (i < keycode_count && keycodes[i] != 0) {
            keyReport.keys[i] = keycodes[i];
        } else {
            keyReport.keys[i] = 0;
        }
    }
    
    // デバッグ: 送信予定のレポートを表示
    Serial.printf("DEBUG: 送信予定レポート - modifier=0x%02X, keys=[", keyReport.modifiers);
    for (int i = 0; i < 6; i++) {
        Serial.printf("0x%02X ", keyReport.keys[i]);
    }
    Serial.println("]");
    
    // HIDレポートを直接送信
    try {
        bleKeyboard->sendReport(&keyReport);
        Serial.println("DEBUG: sendReport() 呼び出し成功");
    } catch (...) {
        Serial.println("DEBUG: sendReport() 呼び出し失敗");
    }
    
    Serial.printf("BLE送信: modifier=0x%02X, keys=%d個 [", modifier, keycode_count);
    for (int i = 0; i < keycode_count && i < 6; i++) {
        Serial.printf("0x%02X ", keycodes[i]);
    }
    Serial.println("]");
}

// 代替のBLE転送方法（press/release使用）
void BleKeyboardForwarder::forwardUsingPressRelease(const uint8_t modifier, const uint8_t* keycodes, int keycode_count) {
    if (!isConnected()) {
        return;
    }
    
    // すべてのキーをリリース
    bleKeyboard->releaseAll();
    
    // 修飾キーを押す
    if (modifier & 0x01) bleKeyboard->press(KEY_LEFT_CTRL);
    if (modifier & 0x02) bleKeyboard->press(KEY_LEFT_SHIFT);
    if (modifier & 0x04) bleKeyboard->press(KEY_LEFT_ALT);
    if (modifier & 0x08) bleKeyboard->press(KEY_LEFT_GUI);
    if (modifier & 0x10) bleKeyboard->press(KEY_RIGHT_CTRL);
    if (modifier & 0x20) bleKeyboard->press(KEY_RIGHT_SHIFT);
    if (modifier & 0x40) bleKeyboard->press(KEY_RIGHT_ALT);
    if (modifier & 0x80) bleKeyboard->press(KEY_RIGHT_GUI);
    
    // 通常のキーを押す（136を足してBLEライブラリ用にする）
    for (int i = 0; i < keycode_count && i < 6; i++) {
        if (keycodes[i] != 0) {
            // HIDキーコードをBLEライブラリ用に変換（136を足す）
            uint8_t bleKey = keycodes[i] + 136;
            bleKeyboard->press(bleKey);
            Serial.printf("Press: HID 0x%02X -> BLE 0x%02X\n", keycodes[i], bleKey);
        }
    }
    
    Serial.printf("Press/Release送信: modifier=0x%02X, keys=%d個\n", modifier, keycode_count);
}

// 16バイトのUSB HIDレポートを直接BLEに転送
void BleKeyboardForwarder::forwardRawHidReport(const uint8_t* report_data, int data_size) {
    if (!isConnected()) {
        Serial.println("DEBUG: BLE未接続のため16バイト転送をスキップ");
        return;
    }
    
    if (data_size != 16) {
        Serial.printf("DEBUG: 16バイト以外のデータ（%dバイト）は処理しません\n", data_size);
        return;
    }
    
    Serial.println("DEBUG: 16バイト生データをBLEに直接転送");
    
    // 16バイトのデータから標準的な8バイトHIDレポートを作成
    BLEKeyReport keyReport;
    keyReport.modifiers = report_data[1];  // DOIO KB16の修飾キーは1バイト目
    keyReport.reserved = 0;
    
    // キーコードを抽出して変換
    int keyIndex = 0;
    for (int i = 2; i < 16 && keyIndex < 6; i++) {
        if (report_data[i] != 0) {
            // DOIO KB16のキーコードを標準HIDキーコードに変換
            uint8_t converted = hidToBleKeycode(report_data[i]);
            if (converted != 0) {
                keyReport.keys[keyIndex] = converted;
                keyIndex++;
            }
        }
    }
    
    // 残りのキーコードスロットを0で埋める
    for (int i = keyIndex; i < 6; i++) {
        keyReport.keys[i] = 0;
    }
    
    // BLEレポートを送信
    bleKeyboard->sendReport(&keyReport);
    
    Serial.printf("16バイト直接転送: modifier=0x%02X, keys=%d個\n", keyReport.modifiers, keyIndex);
}

void BleKeyboardForwarder::releaseAllKeys() {
    if (bleKeyboard) {
        bleKeyboard->releaseAll();
    }
}

void BleKeyboardForwarder::printStatus() {
    Serial.println("\n=== BLEキーボード転送状態 ===");
    Serial.printf("BLE有効: %s\n", bleEnabled ? "はい" : "いいえ");
    if (bleKeyboard) {
        Serial.printf("BLE接続: %s\n", bleKeyboard->isConnected() ? "接続中" : "切断中");
    } else {
        Serial.println("BLE接続: 初期化されていません");
    }
    Serial.printf("最終送信: %lu ms前\n", millis() - lastSendTime);
    Serial.println("=============================\n");
}
