#include "PythonStyleAnalyzer.h"

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
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);
        display->setCursor(0, 0);
        
        if (isConnected) {
            display->println("USB->BLE Bridge");
            
            // BLE接続状態を表示
            if (bleKeyboard && bleKeyboard->isConnected()) {
                display->println("BLE: Connected");
            } else {
                display->println("BLE: Disconnected");
            }
            
            display->println("");
            display->println("Ready for keys");
            display->print("Uptime: ");
            display->print(millis() / 1000);
            display->println("s");
        } else {
            display->println("DOIO KB16 Bridge");
            display->println("================");
            display->println("");
            display->println("Waiting for USB");
            display->println("keyboard...");
            display->println("");
            display->println("BLE will auto-");
            display->println("forward keys");
        }
        display->display();
    }
}

// ディスプレイ更新用のヘルパー関数
void PythonStyleAnalyzer::updateDisplayForDevice(const String& deviceType) {
    if (!display) return;
    
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 0);
    display->println("USB->BLE Bridge");
    display->println(deviceType);
    display->println("Connected!");
    display->println("");
    
    // BLE状態を表示
    if (bleKeyboard && bleKeyboard->isConnected()) {
        display->println("BLE: Ready");
    } else {
        display->println("BLE: Waiting");
    }
    
    display->println("Forwarding keys...");
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
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 0);
    
    // HEXデータを連続表示（16バイト完全表示）
    if (hexData.length() > 0) {
        // バイト単位で分割して表示
        String bytes[16];
        int byteCount = 0;
        String currentByte = "";
        
        // HEXデータをバイト単位で分割
        for (int i = 0; i < hexData.length() && byteCount < 16; i++) {
            char c = hexData.charAt(i);
            if (c == ' ') {
                if (currentByte.length() > 0) {
                    bytes[byteCount] = currentByte;
                    byteCount++;
                    currentByte = "";
                }
            } else {
                currentByte += c;
            }
        }
        // 最後のバイトを処理
        if (currentByte.length() > 0 && byteCount < 16) {
            bytes[byteCount] = currentByte;
            byteCount++;
        }
        
        // 7バイトずつ表示（画面幅に最適化）
        String line1 = "";
        String line2 = "";
        String line3 = "";
        
        for (int i = 0; i < byteCount; i++) {
            if (i < 7) {
                if (line1.length() > 0) line1 += " ";
                line1 += bytes[i];
            } else if (i < 14) {
                if (line2.length() > 0) line2 += " ";
                line2 += bytes[i];
            } else {
                if (line3.length() > 0) line3 += " ";
                line3 += bytes[i];
            }
        }
        
        display->println(line1);
        if (line2.length() > 0) {
            display->println(line2);
        }
        if (line3.length() > 0) {
            display->println(line3);
        }
    } else {
        display->println("No HEX data");
    }
    
    // キー名（短縮表示）
    display->print("Key:");
    if (keyNames.length() > 0) {
        String shortKeys = keyNames;
        if (shortKeys.length() > 20) {
            shortKeys = shortKeys.substring(0, 17) + "...";
        }
        display->println(shortKeys);
    } else {
        display->println("None");
    }
    
    // 文字表示を追加
    display->print("Char:");
    if (characters.length() > 0) {
        String shortChars = characters;
        if (shortChars.length() > 19) {
            shortChars = shortChars.substring(0, 16) + "...";
        }
        display->println(shortChars);
    } else {
        display->println("None");
    }
    
    // BLE転送状態を表示
    display->print("BLE:");
    if (bleKeyboard && bleKeyboard->isConnected()) {
        display->println("Sent");
    } else {
        display->println("Wait");
    }
    
    // Shiftキー状態を右下に表示
    display->setCursor(105, 48);  // 位置を少し上に調整
    if (shiftPressed) {
        display->print("SH");
    } else {
        display->print("--");
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
    
    // Pythonと同じ：StandardとNKROの場合のみ修飾キー処理
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
    
    // ディスプレイ更新（リアルタイム表示）
    String display_hex = hex_data;
    String display_keys = pressed_keys.length() > 0 ? pressed_keys : "None";
    String display_chars = pressed_chars.length() > 0 ? pressed_chars : "None";
    updateDisplayWithKeys(display_hex, display_keys, display_chars, shift_pressed);
    
    // BLE送信処理（改良版）
    if (bleKeyboard && bleKeyboard->isConnected()) {
        #if SERIAL_OUTPUT_ENABLED
        Serial.printf("BLE送信チェック: 前回='%s', 今回='%s'\n", 
                     lastSentCharacters.c_str(), pressed_chars.c_str());
        #endif
        
        // 文字列の変化またはキーが新しく押された場合に送信
        bool should_send = false;
        
        if (pressed_chars.length() > 0) {
            // 新しい文字が押された場合
            if (lastSentCharacters != pressed_chars) {
                should_send = true;
                #if SERIAL_OUTPUT_ENABLED
                Serial.println("BLE送信理由: 新しい文字または文字の変化");
                #endif
            }
        } else if (lastSentCharacters.length() > 0) {
            // すべてのキーが離された場合
            should_send = false;  // キーリリースは送信しない
            #if SERIAL_OUTPUT_ENABLED
            Serial.println("BLE送信スキップ: すべてのキーが離された");
            #endif
        }
        
        if (should_send) {
            #if SERIAL_OUTPUT_ENABLED
            Serial.printf("BLE送信開始: '%s'\n", pressed_chars.c_str());
            #endif
            
            // 文字列を個別に送信
            int start = 0;
            int comma_pos = 0;
            
            while ((comma_pos = pressed_chars.indexOf(", ", start)) != -1) {
                String single_char = pressed_chars.substring(start, comma_pos);
                single_char.trim();
                
                if (single_char.length() > 0) {
                    sendSingleCharacter(single_char);
                }
                
                start = comma_pos + 2;
            }
            
            // 最後の文字を処理
            if (start < pressed_chars.length()) {
                String single_char = pressed_chars.substring(start);
                single_char.trim();
                
                if (single_char.length() > 0) {
                    sendSingleCharacter(single_char);
                }
            }
            
            // 送信済み文字列を更新
            lastSentCharacters = pressed_chars;
            
            #if SERIAL_OUTPUT_ENABLED
            Serial.println("BLE送信完了");
            #endif
        } else {
            #if SERIAL_OUTPUT_ENABLED
            Serial.println("BLE送信スキップ: 文字の変化なし");
            #endif
        }
        
        // キーが離された場合は送信済み文字列をクリア
        if (pressed_chars.length() == 0) {
            lastSentCharacters = "";
        }
    } else {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("BLE送信スキップ: BLE未接続");
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
    if (!bleKeyboard || !bleKeyboard->isConnected()) {
        #if SERIAL_OUTPUT_ENABLED
        Serial.println("BLE送信エラー: BLE未接続");
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
    
    // 送信後の短い待機時間
    delay(10);
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
