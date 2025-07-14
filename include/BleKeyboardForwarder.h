#ifndef BLE_KEYBOARD_FORWARDER_H
#define BLE_KEYBOARD_FORWARDER_H

#include <Arduino.h>
#include "BleKeyboard.h"

// BLEキーボード転送クラス
class BleKeyboardForwarder {
private:
    BleKeyboard* bleKeyboard;
    bool bleEnabled = false;
    unsigned long lastSendTime = 0;
    static const unsigned long SEND_INTERVAL = 10; // 10ms間隔で送信

public:
    BleKeyboardForwarder();
    ~BleKeyboardForwarder();
    
    // 初期化・終了処理
    bool begin(const String& deviceName = "DOIO KB16 Bridge");
    void end();
    
    // 接続状態
    bool isConnected();
    bool isEnabled() { return bleEnabled; }
    
    // USB HIDレポートからBLEキーボードへの転送
    void forwardHidReport(const uint8_t* report_data, int data_size);
    void forwardStandardKeyboard(const uint8_t modifier, const uint8_t* keycodes, int keycode_count);
    
    // 代替のBLE転送方法（press/release使用）
    void forwardUsingPressRelease(const uint8_t modifier, const uint8_t* keycodes, int keycode_count);
    
    // 16バイトのUSB HIDレポートを直接BLEに転送
    void forwardRawHidReport(const uint8_t* report_data, int data_size);
    
    // キーリリース
    void releaseAllKeys();
    
    // HIDキーコードからBLEキーコードへの変換
    uint8_t hidToBleKeycode(uint8_t hidKeycode);
    
    // デバッグ情報
    void printStatus();
};

#endif // BLE_KEYBOARD_FORWARDER_H
