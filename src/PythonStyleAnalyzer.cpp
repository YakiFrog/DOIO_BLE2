#include "PythonStyleAnalyzer.h"

// BLE接続制御関数の前方宣言
void startBleConnection();
void stopBleConnection();

// BLE接続制御フラグの外部宣言
extern bool bleAutoReconnect;
extern bool bleManualConnect;
extern bool bleStackInitialized;

// 特殊キー処理用の変数
static bool ctrlPressed = false;
static bool altPressed = false;

// これは，正しいので変更しない．
const KeycodeMapping KEYCODE_MAP[] = {
    // アルファベット (0x08-0x21: a-z) - Pythonと同じ
    {0x08, "a", "A"}, {0x09, "b", "B"}, {0x0A, "c", "C"}, {0x0B, "d", "D"},
    {0x0C, "e", "E"}, {0x0D, "f", "F"}, {0x0E, "g", "G"}, {0x0F, "h", "H"},
    {0x10, "i", "I"}, {0x11, "j", "J"}, {0x12, "k", "K"}, {0x13, "l", "L"},
    {0x14, "m", "M"}, {0x15, "n", "N"}, {0x16, "o", "O"}, {0x17, "p", "P"},
    {0x18, "q", "Q"}, {0x19, "r", "R"}, {0x1A, "s", "S"}, {0x1B, "t", "T"},
    {0x1C, "u", "U"}, {0x1D, "v", "V"}, {0x1E, "w", "W"}, {0x1F, "x", "X"},
    {0x20, "y", "Y"}, {0x21, "z", "Z"},
    
    // 数字と記号 (0x22-0x2B) - Pythonと同じ
    {0x22, "1", "!"}, {0x23, "2", "@"}, {0x24, "3", "#"}, {0x25, "4", "$"},
    {0x26, "5", "%"}, {0x27, "6", "^"}, {0x28, "7", "&"}, {0x29, "8", "*"},
    {0x2A, "9", "("}, {0x2B, "0", ")"},
    
    // 一般的なキー (0x2C-0x3C) - Pythonと同じ
    {0x2C, "Enter", "\n"},       // Enter
    {0x2D, "Esc", ""},           // Escape
    {0x2E, "Backspace", ""},     // Backspace
    {0x2F, "Tab", "\t"},         // Tab
    {0x30, "Space", " "},        // Space
    {0x31, "-", "_"},
    {0x32, "=", "+"},
    {0x33, "[", "{"},
    {0x34, "]", "}"},
    {0x35, "\\", "|"},
    {0x37, ";", ":"},
    {0x38, "'", "\""},
    {0x39, "`", "~"},
    {0x3A, ",", "<"},
    {0x3B, ".", ">"},
    {0x3C, "/", "?"},
    
    // ファンクションキー (0x3E-0x49) - Pythonと同じ
    {0x3E, "F1", "F1"}, {0x3F, "F2", "F2"}, {0x40, "F3", "F3"},
    {0x41, "F4", "F4"}, {0x42, "F5", "F5"}, {0x43, "F6", "F6"},
    {0x44, "F7", "F7"}, {0x45, "F8", "F8"}, {0x46, "F9", "F9"},
    {0x47, "F10", "F10"}, {0x48, "F11", "F11"}, {0x49, "F12", "F12"},
    
    // 特殊キー (0x4D-0x56) - Pythonと同じ
    {0x4D, "Insert", "Insert"}, {0x4E, "Home", "Home"}, {0x4F, "PageUp", "PageUp"},
    {0x50, "Delete", "Delete"}, {0x51, "End", "End"}, {0x52, "PageDown", "PageDown"},
    {0x53, "Right", "Right"}, {0x54, "Left", "Left"}, 
    {0x55, "Down", "Down"}, {0x56, "Up", "Up"},
    
    // テンキー (0x58-0x67) - Pythonと同じ
    {0x58, "/", "/"}, {0x59, "*", "*"}, {0x5A, "-", "-"}, {0x5B, "+", "+"},
    {0x5C, "Enter", "Enter"}, {0x5D, "1", "1"}, {0x5E, "2", "2"}, {0x5F, "3", "3"},
    {0x60, "4", "4"}, {0x61, "5", "5"}, {0x62, "6", "6"}, {0x63, "7", "7"},
    {0x64, "8", "8"}, {0x65, "9", "9"}, {0x66, "0", "0"}, {0x67, ".", "."},
    
    // 制御キー (0xE0-0xE7) - Pythonと同じ
    {0xE0, "Ctrl", "Ctrl"}, {0xE1, "Shift", "Shift"}, {0xE2, "Alt", "Alt"},
    {0xE3, "GUI", "GUI"}, {0xE4, "右Ctrl", "右Ctrl"}, {0xE5, "右Shift", "右Shift"},
    {0xE6, "右Alt", "右Alt"}, {0xE7, "右GUI", "右GUI"}
};

const int KEYCODE_MAP_SIZE = sizeof(KEYCODE_MAP) / sizeof(KeycodeMapping);

PythonStyleAnalyzer::PythonStyleAnalyzer(Adafruit_SSD1306* disp, BleKeyboard* bleKbd) 
    : display(disp), bleKeyboard(bleKbd) {
    memset(&report_format, 0, sizeof(report_format));
}

// アイドル状態のディスプレイ更新（publicメソッド）
void PythonStyleAnalyzer::updateDisplayIdle() {
    if (!display) return;
    
    // 3秒間キー入力がない場合はアイドル表示
    if (millis() - lastKeyEventTime > 3000) {
        display->clearDisplay();
        display->setTextColor(SSD1306_WHITE);
        
        if (isConnected) {
            // 中央に大きく待機表示
            display->setTextSize(2);
            display->setCursor(30, 10);  // 中央寄せ調整
            display->println("READY");
            
            // 下部に状態情報
            display->setTextSize(1);
            display->setCursor(0, 35);
            display->println("USB: Connected");
            
            display->setCursor(0, 45);
            display->print("BLE:");
            if (bleKeyboard && bleKeyboard->isConnected()) {
                display->print("OK");
            } else {
                display->print("--");
            }
            
            display->setCursor(70, 45);
            display->print("SHIFT:--");
            
            display->setCursor(0, 55);
            display->print("Uptime:");
            display->print(millis() / 1000);
            display->print("s");
        } else {
            // 中央に大きく待機表示
            display->setTextSize(2);
            display->setCursor(40, 10);  // 中央寄せ調整
            display->println("WAIT");
            
            // 下部に状態情報
            display->setTextSize(1);
            display->setCursor(0, 35);
            display->println("USB: Waiting...");
            
            display->setCursor(0, 45);
            display->print("BLE:--");
            
            display->setCursor(70, 45);
            display->print("SHIFT:--");
            
            display->setCursor(0, 55);
            display->print("Connect KB16");
        }
        display->display();
    }
}

// ディスプレイ更新用のヘルパー関数
void PythonStyleAnalyzer::updateDisplayForDevice(const String& deviceType) {
    if (!display) return;
    
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    
    // 中央に大きく接続表示
    display->setTextSize(2);
    display->setCursor(5, 10);  // 中央寄せ調整
    display->println("CONNECT");
    
    // 下部に状態情報
    display->setTextSize(1);
    display->setCursor(0, 35);
    display->print("USB: ");
    display->println(deviceType);
    
    display->setCursor(0, 45);
    display->print("BLE:");
    if (bleKeyboard && bleKeyboard->isConnected()) {
        display->print("OK");
    } else {
        display->print("--");
    }
    
    display->setCursor(70, 45);
    display->print("SHIFT:--");
    
    display->setCursor(0, 55);
    display->print("Initializing...");
    
    display->display();
}

// キー押下時のディスプレイ更新
void PythonStyleAnalyzer::updateDisplayWithKeys(const String& hexData, const String& keyNames, const String& characters, bool shiftPressed) {
    if (!display) return;
    
    // 表示データを更新
    lastHexData = hexData;
    lastKeyPresses = keyNames;
    lastCharacters = characters;
    lastKeyEventTime = millis();
    
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    
    // メイン文字を大きく表示（画面中央上部）
    if (characters.length() > 0 && characters != "None") {
        // 表示する文字を決定
        String displayText = "";
        
        if (characters == "Space" || characters == " ") {
            displayText = "SPC";
        } else if (characters == "Enter" || characters == "\n") {
            displayText = "ENT";
        } else if (characters == "Tab" || characters == "\t") {
            displayText = "TAB";
        } else if (characters == "Backspace") {
            displayText = "BS";
        } else if (characters == "Delete") {
            displayText = "DEL";
        } else if (characters == "Escape") {
            displayText = "ESC";
        } else if (characters.startsWith("F") && characters.length() <= 3) {
            // ファンクションキー (F1-F12)
            displayText = characters;
        } else if (characters.length() == 1) {
            // 単一文字
            displayText = characters;
        } else if (characters.length() <= 8) {
            // 8文字以下はそのまま表示
            displayText = characters;
        } else {
            // 8文字を超える場合のみ省略
            displayText = characters.substring(0, 8);
            displayText += "..";
        }
        
        // 文字数に応じてサイズとポジションを調整（HEX削除により大きく表示）
        if (displayText.length() <= 1) {
            display->setTextSize(4);  // サイズを3から4に拡大
            display->setCursor(50, 2);  // 1文字の場合は真ん中（上部に移動）
        } else if (displayText.length() <= 3) {
            display->setTextSize(3);  // サイズを2から3に拡大
            display->setCursor(25, 5);  // 2-3文字の場合は中央寄り（上部に移動）
        } else if (displayText.length() <= 8) {
            display->setTextSize(2);  // サイズを1から2に拡大
            display->setCursor(10, 8);  // 4-8文字の場合は左寄り（上部に移動）
        } else {
            display->setTextSize(1);  // サイズはそのまま
            display->setCursor(5, 10);  // 9文字以上の場合は更に左寄り（上部に移動）
        }
        
        display->print(displayText);
    } else {
        // キーが離されている場合は「---」を表示
        display->setTextSize(2);
        display->setCursor(45, 8);  // 中央配置（調整）
        display->print("---");
    }
    
    // 下部に情報を小さく表示（HEX削除により位置を上に移動）
    display->setTextSize(1);
    
    // BLE接続状況とSHIFT状況を同じ行に表示
    display->setCursor(0, 35);
    display->print("BLE:");
    if (bleKeyboard && bleKeyboard->isConnected()) {
        display->print("OK");
    } else {
        display->print("--");
    }
    
    // SHIFT状況を右側に表示
    display->setCursor(70, 35);
    display->print("SHIFT:");
    if (shiftPressed) {
        display->print("ON");
    } else {
        display->print("--");
    }
    
    // キー名を最下部に表示（参考情報）
    display->setCursor(0, 45);
    display->print("Key:");
    if (keyNames.length() > 0) {
        String shortKeys = keyNames;
        if (shortKeys.length() > 16) {
            shortKeys = shortKeys.substring(0, 13) + "...";
        }
        display->print(shortKeys);
    } else {
        display->print("None");
    }
    
    display->display();
}

// Pythonのkeycode_to_string関数を完全移植
String PythonStyleAnalyzer::keycodeToString(uint8_t keycode, bool shift) {
    #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
    Serial.printf("    keycodeToString呼び出し: keycode=0x%02X, shift=%s\n", keycode, shift ? "true" : "false");
    #endif
    
    for (int i = 0; i < KEYCODE_MAP_SIZE; i++) {
        if (KEYCODE_MAP[i].keycode == keycode) {
            String result = shift ? KEYCODE_MAP[i].shifted : KEYCODE_MAP[i].normal;
            #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
            Serial.printf("    マッピング発見: 0x%02X -> normal='%s', shifted='%s', result='%s'\n", 
                         keycode, KEYCODE_MAP[i].normal, KEYCODE_MAP[i].shifted, result.c_str());
            #endif
            return result;
        }
    }
    
    String unknown = "不明(0x" + String(keycode, HEX) + ")";
    #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
    Serial.printf("    マッピング未発見: 0x%02X -> %s\n", keycode, unknown.c_str());
    #endif
    return unknown;
}

// Pythonの_analyze_report_format関数を完全移植
ReportFormat PythonStyleAnalyzer::analyzeReportFormat(const uint8_t* report_data, int data_size) {
    if (!report_format_initialized) {
        // 最初のレポートから形式を推測（Pythonと同じロジック）
        report_format.size = data_size;
        
        if (data_size == 8) {
            // 8バイトレポート：標準キーボード
            report_format.modifier_index = 0;  // バイト0が修飾キー
            report_format.reserved_index = 1;  // バイト1は予約
        } else {
            // 16バイト等：DOIO KB16など
            report_format.modifier_index = 1;  // 修飾キーは1バイト目
            report_format.reserved_index = 0;  // 0バイト目は予約または無視
        }
        
        report_format.key_indices[0] = 2;  // 標準的な6KROレイアウト
        report_format.key_indices[1] = 3;
        report_format.key_indices[2] = 4;
        report_format.key_indices[3] = 5;
        report_format.key_indices[4] = 6;
        report_format.key_indices[5] = 7;
        report_format.format = "Standard";
        
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
        Serial.printf("modifier_index=%d, 修飾キー処理: %s\n", 
                     report_format.modifier_index, 
                     (report_format.format == "Standard" || report_format.format == "NKRO") ? "有効" : "無効");
        #endif
    }
    
    return report_format;
}

// Pythonのpretty_print_report関数を完全移植
void PythonStyleAnalyzer::prettyPrintReport(const uint8_t* report_data, int data_size) {
    ReportFormat format = analyzeReportFormat(report_data, data_size);
    
    // バイト列を16進数で表示（Pythonと同一フォーマット）- 16バイト完全対応
    String hex_data = "";
    for (int i = 0; i < data_size; i++) {
        if (report_data[i] < 16) hex_data += "0";
        hex_data += String(report_data[i], HEX) + " ";
    }
    hex_data.trim();
    hex_data.toUpperCase();
    
    #if SERIAL_OUTPUT_ENABLED
    Serial.printf("\nHIDレポート [%dバイト]: %s\n", data_size, hex_data.c_str());
    // 16バイトの場合はバイト位置も表示
    if (data_size == 16) {
        Serial.println("バイト位置: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
    }
    #endif
    
    // 修飾キー（Pythonと同じロジック）
    bool shift_pressed = false;
    ctrlPressed = false;
    altPressed = false;
    
    // Pythonと同じ：StandardとNKROの場合のみ修飾キー処理
    if (format.format == "Standard" || format.format == "NKRO") {
        uint8_t modifier = report_data[format.modifier_index];
        String mod_str = "";
        if (modifier & 0x01) {
            mod_str += "L-Ctrl ";
            ctrlPressed = true;
        }
        if (modifier & 0x02) {
            mod_str += "L-Shift ";
            shift_pressed = true;
        }
        if (modifier & 0x04) {
            mod_str += "L-Alt ";
            altPressed = true;
        }
        if (modifier & 0x08) mod_str += "L-GUI ";
        if (modifier & 0x10) {
            mod_str += "R-Ctrl ";
            ctrlPressed = true;
        }
        if (modifier & 0x20) {
            mod_str += "R-Shift ";
            shift_pressed = true;
        }
        if (modifier & 0x40) {
            mod_str += "R-Alt ";
            altPressed = true;
        }
        if (modifier & 0x80) mod_str += "R-GUI ";
        
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("修飾キー: %s\n", mod_str.length() > 0 ? mod_str.c_str() : "なし");
        Serial.printf("shift_pressed設定: %s\n", shift_pressed ? "true" : "false");
        #endif
    } else {
        // DOIO KB16等では修飾キー処理を完全にスキップ（Pythonと同じ）
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("修飾キー: 処理なし (%s)\n", format.format.c_str());
        Serial.printf("shift_pressed設定: false (固定)\n");
        #endif
    }
    
    #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
    Serial.printf("最終shift_pressed状態: %s\n", shift_pressed ? "true" : "false");
    Serial.printf("レポート形式: %s\n", format.format.c_str());
    #endif
    
    // 押されているキー（Pythonと同じロジック）
    String pressed_keys = "";
    String pressed_chars = "";
    
    if (format.format == "Standard" || format.format == "NKRO") {
        if (format.format == "Standard") {
            // 標準的な6KROレポート（DOIO KB16の場合は16バイトのStandardとして処理）
            #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
            Serial.printf("Standard形式解析 (サイズ=%d, modifier_index=%d):\n", format.size, format.modifier_index);
            #endif
            
            // DOIO KB16の場合は16バイトすべてをチェック
            int max_key_index = (format.size == 16) ? 16 : 8;
            for (int i = 2; i < max_key_index; i++) {
                if (i < format.size && report_data[i] != 0) {
                    uint8_t keycode = report_data[i];
                    if (pressed_keys.length() > 0) pressed_keys += ", ";
                    pressed_keys += "0x" + String(keycode, HEX);
                    
                    // キーコードを文字に変換
                    String key_str = keycodeToString(keycode, shift_pressed);
                    if (pressed_chars.length() > 0) pressed_chars += ", ";
                    pressed_chars += key_str;
                    
                    #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
                    Serial.printf("  バイト%d: 0x%02X -> %s (shift=%s)\n", i, keycode, key_str.c_str(), shift_pressed ? "true" : "false");
                    #endif
                }
            }
        } else {
            // NKRO形式
            #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
            Serial.println("NKRO形式解析:");
            #endif
            
            for (int i = 2; i < format.size; i++) {
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
                        
                        #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
                        Serial.printf("  NKRO バイト%d bit%d: 0x%02X -> %s (shift=%s)\n", i, bit, keycode, key_str.c_str(), shift_pressed ? "true" : "false");
                        #endif
                    }
                }
            }
        }
    } else {
        // 不明な形式の場合は何もしない（Pythonと同じ）
        #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
        Serial.printf("不明な形式: %s - キー処理をスキップ\n", format.format.c_str());
        #endif
    }
    
    #if SERIAL_OUTPUT_ENABLED
    Serial.printf("押されているキー: %s\n", pressed_keys.length() > 0 ? pressed_keys.c_str() : "なし");
    if (pressed_chars.length() > 0) {
        Serial.printf("文字表現: %s\n", pressed_chars.c_str());
    }
    #endif
    
    #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
    Serial.printf("pressed_chars最終値: '%s'\n", pressed_chars.c_str());
    #endif
    
    // 特殊キー組み合わせの検出（Ctrl+Alt+B でBLE接続制御）
    if (ctrlPressed && altPressed && pressed_chars.indexOf("b") >= 0) {
        Serial.println("🔧 Ctrl+Alt+B検出 - BLE接続制御");
        if (bleKeyboard && bleKeyboard->isConnected()) {
            Serial.println("BLE接続を停止します");
            stopBleConnection();
        } else {
            Serial.println("BLE接続を開始します");
            startBleConnection();
        }
        // 特殊キー処理後はBLE送信をスキップ
        return;
    }
    
    // ディスプレイ更新（リアルタイム表示）
    String display_hex = hex_data;
    String display_keys = pressed_keys.length() > 0 ? pressed_keys : "None";
    String display_chars = pressed_chars.length() > 0 ? pressed_chars : "None";
    updateDisplayWithKeys(display_hex, display_keys, display_chars, shift_pressed);
    
    // BLE送信処理（長押し対応版）
    if (bleKeyboard && bleKeyboard->isConnected() && bleStackInitialized) {
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("BLE送信チェック（長押し対応）: 現在='%s'\n", pressed_chars.c_str());
        #endif
        
        // 長押し処理を実行
        processKeyPress(pressed_chars);
        
    } else {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("BLE送信スキップ: BLE未接続またはスタック停止中");
        #endif
    }
    
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

// BLE送信用のヘルパー関数
void PythonStyleAnalyzer::sendSingleCharacter(const String& character) {
    if (!bleKeyboard || !bleKeyboard->isConnected() || !bleStackInitialized) {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("BLE送信エラー: BLE未接続またはスタック停止中");
        #endif
        return;
    }
    
    #if SERIAL_OUTPUT_ENABLED
    Serial.printf("BLE送信開始: '%s'\n", character.c_str());
    #endif
    
    // 特殊文字の処理
    if (character == "Enter" || character == "\n") {
        bleKeyboard->write('\n');
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("  -> Enter送信完了");
        #endif
    } else if (character == "Tab" || character == "\t") {
        bleKeyboard->write('\t');
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("  -> Tab送信完了");
        #endif
    } else if (character == "Space" || character == " ") {
        bleKeyboard->write(' ');
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("  -> Space送信完了");
        #endif
    } else if (character == "Backspace") {
        bleKeyboard->write(8);  // ASCII backspace
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("  -> Backspace送信完了");
        #endif
    } else if (character.length() == 1) {
        // 単一文字の場合はそのまま送信
        char c = character.charAt(0);
        if (c >= 32 && c <= 126) {  // 印刷可能な文字のみ
            bleKeyboard->write(c);
            #if SERIAL_OUTPUT_ENABLED
            Serial.printf("  -> 文字 '%c' (0x%02X) 送信完了\n", c, (uint8_t)c);
            #endif
        } else {
            #if SERIAL_OUTPUT_ENABLED
            Serial.printf("  -> 印刷不可能な文字 0x%02X をスキップ\n", (uint8_t)c);
            #endif
        }
    } else {
        // 複数文字の場合は制御キーなどなのでスキップ
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("  -> 制御キー '%s' をスキップ\n", character.c_str());
        #endif
    }
    
    // 送信後の待機時間を削除（最高速化）
    // delay(1);  // 完全撤廃
}

// 複数文字を効率的に送信する関数（複数キー対応修正版）
void PythonStyleAnalyzer::sendString(const String& chars) {
    if (!bleKeyboard || !bleKeyboard->isConnected() || !bleStackInitialized) {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("BLE送信スキップ: BLE未接続またはスタック停止中");
        #endif
        return;
    }
    
    unsigned long currentTime = millis();
    unsigned long interval = (lastBleTransmissionTime == 0) ? 0 : currentTime - lastBleTransmissionTime;
    
    // 統計情報の更新
    if (interval > 0) {
        if (interval < minTransmissionInterval) minTransmissionInterval = interval;
        if (interval > maxTransmissionInterval) maxTransmissionInterval = interval;
        totalTransmissionInterval += interval;
        intervalCount++;
    }
    
    // 複数キー判定
    bool isMultipleKeys = chars.indexOf(", ") != -1;
    int keyCount = 1;
    if (isMultipleKeys) {
        keyCount = 1;
        for (int i = 0; i < chars.length() - 1; i++) {
            if (chars.substring(i, i + 2) == ", ") {
                keyCount++;
            }
        }
    }
    
    #if SERIAL_OUTPUT_ENABLED
    Serial.printf("BLE送信（複数キー対応）: '%s' (キー数: %d, 前回送信からの経過時間: %lu ms)\n", 
                  chars.c_str(), keyCount, interval);
    #endif
    
    lastBleTransmissionTime = currentTime;
    bleTransmissionCount++;
    
    // 統計レポート（10秒間隔）
    if (currentTime - lastStatsReport >= STATS_REPORT_INTERVAL) {
        reportPerformanceStats();
        lastStatsReport = currentTime;
    }
    
    // カンマ区切りで分割して送信（複数キー時は適切な間隔で）
    int start = 0;
    int comma_pos = 0;
    int sentCount = 0;
    
    while ((comma_pos = chars.indexOf(", ", start)) != -1) {
        String single_char = chars.substring(start, comma_pos);
        single_char.trim();
        if (single_char.length() > 0) {
            sendSingleCharacterFast(single_char);
            sentCount++;
            
            // 複数キー送信時は適切な間隔を空ける
            if (isMultipleKeys && sentCount < keyCount) {
                delayMicroseconds(200);  // 0.2ms間隔
            }
        }
        start = comma_pos + 2;
    }
    
    // 最後の文字を送信
    if (start < chars.length()) {
        String single_char = chars.substring(start);
        single_char.trim();
        if (single_char.length() > 0) {
            sendSingleCharacterFast(single_char);
        }
    }
}

// 高速化された単一文字送信関数（複数キー対応修正版）
void PythonStyleAnalyzer::sendSingleCharacterFast(const String& character) {
    if (!bleKeyboard || !bleKeyboard->isConnected() || !bleStackInitialized) {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("BLE送信スキップ: BLE未接続またはスタック停止中");
        #endif
        return;
    }
    
    unsigned long startTime = millis();
    
    if (character == "Enter") {
        bleKeyboard->write('\n');
    } else if (character == "Tab") {
        bleKeyboard->write('\t');
    } else if (character == "Space" || character == " ") {
        bleKeyboard->write(' ');
    } else if (character == "Backspace") {
        bleKeyboard->write(8);  // ASCII backspace
    } else if (character.length() == 1) {
        char c = character.charAt(0);
        if (c >= 32 && c <= 126) {  // 印刷可能な文字のみ
            bleKeyboard->write(c);
        }
    }
    
    unsigned long endTime = millis();
    unsigned long bleTransmissionTime = endTime - startTime;
    
    #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
    if (bleTransmissionTime > 0) {
        Serial.printf("  BLE送信実行時間: %lu ms (文字: '%s')\n", bleTransmissionTime, character.c_str());
    }
    #endif
    
    // 最小限の安定化待機（複数キー時の安定性向上）
    delayMicroseconds(300);  // 0.3ms（複数キー時の安定性向上）
}

// デバイス接続時の処理（元のプログラムと同じ処理を追加）
void PythonStyleAnalyzer::onNewDevice(const usb_device_info_t &dev_info) {
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
    
    // DOIO KB16の検出（Pythonと同じ処理）
    if (device_vendor_id == DOIO_VID && device_product_id == DOIO_PID) {
        is_doio_kb16 = true;
        isConnected = true;
        report_size = 16;  // DOIO KB16は16バイト固定
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("✓ DOIO KB16を検出: 16バイト固定レポートサイズ");
        Serial.println("✓ Pythonアナライザーと同じ処理：16バイトのStandardフォーマット");
        Serial.println("✓ 修飾キー処理が有効です（バイト1をチェック）");
        Serial.println("✓ BLEキーボード転送を開始します");
        #endif
        
        // DOIO KB16専用の初期化処理
        updateDisplayForDevice("DOIO KB16");
    } else {
        is_doio_kb16 = false;
        isConnected = true;
        report_size = 8;   // 標準キーボードは8バイト
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("標準キーボードとして処理 (8バイトレポート)");
        Serial.println("修飾キー処理が有効です（バイト0をチェック）");
        Serial.println("✓ BLEキーボード転送を開始します");
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
void PythonStyleAnalyzer::onGone(const usb_host_client_event_msg_t *eventMsg) {
    #if SERIAL_OUTPUT_ENABLED
    Serial.println("USBデバイスが切断されました");
    #endif
    is_doio_kb16 = false;
    isConnected = false;
    has_last_report = false;
    report_format_initialized = false;
    
    // BLEキーボードのキーをすべてリリース
    if (bleKeyboard) {
        bleKeyboard->releaseAll();
    }
    
    // ディスプレイを元の状態に戻す
    if (display) {
        display->clearDisplay();
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);
        display->setCursor(0, 0);
        display->println("USB->BLE Bridge");
        display->println("");
        display->println("Device");
        display->println("DISCONNECTED");
        display->println("");
        display->println("BLE still active");
        display->println("Waiting for USB");
        display->println("device...");
        display->display();
    }
}

// 標準キーボード処理（8バイトレポート）
void PythonStyleAnalyzer::onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report) {
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
void PythonStyleAnalyzer::onReceive(const usb_transfer_t *transfer) {
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
void PythonStyleAnalyzer::processRawReport16Bytes(const uint8_t* data) {
    #if SERIAL_OUTPUT_ENABLED && DEBUG_ENABLED
    Serial.println("16バイトDOIO KB16レポート処理");
    #endif
    
    // Pythonと同じ処理フローを呼び出し
    prettyPrintReport(data, 16);
}

// 長押しリピート処理
void PythonStyleAnalyzer::handleKeyRepeat() {
    if (currentPressedChars.length() == 0) {
        // キーが押されていない場合はリピート状態をリセット
        isRepeating = false;
        return;
    }
    
    unsigned long currentTime = millis();
    
    // 複数キー判定
    bool isMultipleKeys = currentPressedChars.indexOf(", ") != -1;
    int keyCount = 1;
    if (isMultipleKeys) {
        keyCount = 1;
        for (int i = 0; i < currentPressedChars.length() - 1; i++) {
            if (currentPressedChars.substring(i, i + 2) == ", ") {
                keyCount++;
            }
        }
    }
    
    // 複数キー時は少し遅延を長くして安定化
    unsigned long effectiveRepeatDelay = isMultipleKeys ? REPEAT_DELAY + (keyCount * 10) : REPEAT_DELAY;
    unsigned long effectiveRepeatRate = isMultipleKeys ? REPEAT_RATE + (keyCount * 5) : REPEAT_RATE;
    
    if (!isRepeating) {
        // 長押し開始判定
        unsigned long elapsed = currentTime - keyPressStartTime;
        if (elapsed >= effectiveRepeatDelay) {
            isRepeating = true;
            lastRepeatTime = currentTime;
            
            #if SERIAL_OUTPUT_ENABLED
            Serial.printf("🔥 長押しリピート開始: '%s' (キー数: %d, 経過時間: %lu ms, 遅延: %lu ms)\n", 
                         currentPressedChars.c_str(), keyCount, elapsed, effectiveRepeatDelay);
            #endif
            
            // 長押し開始時に音を鳴らす
            speakerController.playKeySound();
            
            // 長押し開始時に即座に1回送信
            sendString(currentPressedChars);
        }
    } else {
        // リピート送信
        unsigned long elapsed = currentTime - lastRepeatTime;
        if (elapsed >= effectiveRepeatRate) {
            lastRepeatTime = currentTime;
            
            unsigned long totalElapsed = currentTime - keyPressStartTime;
            
            #if SERIAL_OUTPUT_ENABLED
            Serial.printf("🔥 長押しリピート送信: '%s' (キー数: %d, 間隔: %lu ms, 総経過時間: %lu ms)\n", 
                         currentPressedChars.c_str(), keyCount, elapsed, totalElapsed);
            #endif
            
            // リピート送信時に音を鳴らす
            speakerController.playKeySound();
            
            // 現在押されているキーを送信
            sendString(currentPressedChars);
        }
    }
}

// キー押下処理（長押し対応）
void PythonStyleAnalyzer::processKeyPress(const String& pressed_chars) {
    if (pressed_chars.length() == 0) {
        // キーリリース
        if (currentPressedChars.length() > 0) {
            #if SERIAL_OUTPUT_ENABLED
            Serial.printf("🔑 キーリリース検出: '%s'\n", currentPressedChars.c_str());
            #endif
            currentPressedChars = "";
            isRepeating = false;
            // キーリリース時に即座にlastSentCharsをクリア（高速連続押し対応）
            lastSentChars = "";
        }
        return;
    }
    
    // 新しいキー押下または継続
    if (pressed_chars != currentPressedChars) {
        // 新しいキー押下
        currentPressedChars = pressed_chars;
        keyPressStartTime = millis();
        isRepeating = false;
        
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("🔑 新しいキー押下: '%s' (開始時刻: %lu ms)\n", pressed_chars.c_str(), keyPressStartTime);
        #endif
        
        // LEDとスピーカーを制御
        ledController.keyPressed();
        speakerController.playKeySound();
        
        // 初回送信（常に送信する - 高速化）
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("🔑 初回送信: '%s'\n", pressed_chars.c_str());
        #endif
        sendString(pressed_chars);
        
        // 送信後に即座に履歴を更新（高速連続押し対応）
        lastSentChars = pressed_chars;
    } else {
        // 同じキーの継続 - handleKeyRepeat()で処理される
        #if SERIAL_OUTPUT_ENABLED
        static unsigned long lastSameKeyLog = 0;
        unsigned long currentTime = millis();
        if (currentTime - lastSameKeyLog > 1000) {  // 1秒ごとにログ
            Serial.printf("🔑 同じキー継続中: '%s' (継続時間: %lu ms)\n", 
                         pressed_chars.c_str(), currentTime - keyPressStartTime);
            lastSameKeyLog = currentTime;
        }
        #endif
    }
}

// パフォーマンス統計レポート
void PythonStyleAnalyzer::reportPerformanceStats() {
    if (intervalCount == 0) {
        return;
    }
    
    unsigned long avgInterval = totalTransmissionInterval / intervalCount;
    
    #if SERIAL_OUTPUT_ENABLED
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("【BLE送信パフォーマンス統計】");
    Serial.printf("  総送信回数: %lu 回\n", bleTransmissionCount);
    Serial.printf("  送信間隔統計 (直近 %lu 回):\n", intervalCount);
    Serial.printf("    - 最小間隔: %lu ms\n", minTransmissionInterval);
    Serial.printf("    - 最大間隔: %lu ms\n", maxTransmissionInterval);
    Serial.printf("    - 平均間隔: %lu ms\n", avgInterval);
    Serial.printf("  長押しリピート設定:\n");
    Serial.printf("    - 単一キー初期遅延: %lu ms\n", REPEAT_DELAY);
    Serial.printf("    - 単一キーリピート間隔: %lu ms\n", REPEAT_RATE);
    Serial.printf("    - 複数キー時は追加遅延あり\n");
    Serial.printf("  BLEライブラリ遅延: 1 ms\n");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    #endif
    
    // 統計をリセット
    minTransmissionInterval = 999999;
    maxTransmissionInterval = 0;
    totalTransmissionInterval = 0;
    intervalCount = 0;
}
