#include "SpecialKeyHandler.h"
#include "BitmapImages.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// 画面表示キュー（定義はmain.cppのみ、ここではextern宣言）
extern QueueHandle_t displayQueue;

// ディスプレイ専用タスク
void displayTask(void* pvParameters) {
    DisplayRequest req;
    static int lastDisplayType = -1; // 直前の表示タイプを記憶
    static String lastText1 = "";    // 前回のtext1を記憶
    static String lastText2 = "";    // 前回のtext2を記憶
    static unsigned long lastTextChangeTime = 0; // 最後にキーが変わった時刻
    const unsigned long TEXT_CHANGE_INTERVAL = 500; // 500ms以上変化がなければdelay

    for (;;) {
        if (xQueueReceive(displayQueue, &req, portMAX_DELAY) == pdTRUE) {
            // 特別キー表示の直後はDISPLAY_NORMALをスキップ
            if (req.type == DISPLAY_NORMAL && 
                (lastDisplayType == DISPLAY_TEXT || lastDisplayType == DISPLAY_ANIMATION)) {
                // DISPLAY_NORMAL表示要求を無視して特別表示を維持
                // 必要なら一定時間後に解除するロジックも追加可能
                continue;
            }
            if (req.type == DISPLAY_NORMAL) {
                // 2行表示（メイン＋サブ）
                req.display->clearBuffer();
                req.display->setFont(req.font);
                // メイン文字（中央上部）
                int textWidth1 = req.display->getStrWidth(req.text1.c_str());
                int xPos1 = (128 - textWidth1) / 2;
                int fontHeight1 = req.display->getFontAscent() - req.display->getFontDescent();
                int yPos1 = 16 + fontHeight1 / 2;
                req.display->drawStr(xPos1, yPos1, req.text1.c_str());
                req.display->setFont(u8g2_font_6x10_tr);
                // 下部情報（BLE/SHIFT/Key名）
                req.display->drawStr(0, 52, "BLE: --");
                req.display->drawStr(70, 52, "SHIFT: --");
                req.display->drawStr(0, 62, "Key:");
                req.display->drawStr(30, 62, req.text2.c_str());
                req.display->sendBuffer();
            } else if (req.type == DISPLAY_TEXT) {
                req.display->clearBuffer();
                req.display->setFont(req.font);
                drawCenteredText(req.display, req.text1.c_str(), req.font);
                if (!req.text2.isEmpty()) {
                    req.display->setFont(u8g2_font_6x10_tr);
                    int textWidth2 = req.display->getStrWidth(req.text2.c_str());
                    int xPos2 = (128 - textWidth2) / 2;
                    int fontHeight2 = req.display->getFontAscent() - req.display->getFontDescent();
                    int yPos2 = 52 + fontHeight2 / 1.5; // 少し下に配置
                    req.display->drawStr(xPos2, yPos2, req.text2.c_str());
                }
                req.display->sendBuffer();

                unsigned long now = millis();
                // キーが切り替わった場合はdelayなし
                if (req.text1 != lastText1 || req.text2 != lastText2) {
                    lastTextChangeTime = now;
                    // 上書き表示（delayなし）
                } else {
                    // 前回の表示から一定時間以上経過していればdelay
                    if (now - lastTextChangeTime > TEXT_CHANGE_INTERVAL) {
                        vTaskDelay(1000 / portTICK_PERIOD_MS); // 1秒間表示
                        lastTextChangeTime = now;
                    }
                }
                lastText1 = req.text1;
                lastText2 = req.text2;
            } else if (req.type == DISPLAY_ANIMATION) {
                int baseY = 0;
                for (int i = 0; i < req.frames; ++i) {
                    int yOffset = baseY;
                    if (i % 2 == 1) yOffset -= req.jumpHeight;
                    req.display->clearBuffer();
                    req.display->drawXBMP(0, yOffset, req.bmp_w, req.bmp_h, req.bitmap);
                    req.display->sendBuffer();
                    vTaskDelay(req.frameDelay / portTICK_PERIOD_MS); // フレーム間の遅延
                }
                req.display->clearBuffer();
                req.display->drawXBMP(0, baseY, req.bmp_w, req.bmp_h, req.bitmap);
                req.display->sendBuffer();
            }
            lastDisplayType = req.type; // 表示タイプを記憶
        }
        vTaskDelay(1); // 負荷軽減のための短い待機
    }
}

// 画面表示要求をキューに入れる関数
void requestDisplay(DisplayRequest& req) {
    if (displayQueue != NULL) {
        xQueueSend(displayQueue, &req, 0);
    }
}

// 画面中央にテキストを表示する関数（既存のまま）
void drawCenteredText(U8G2* display, const char* text, const uint8_t* font) {
    display->setFont(font);
    int textWidth = display->getStrWidth(text);
    int xPos = (128 - textWidth) / 2;
    int fontHeight = display->getFontAscent() - display->getFontDescent();
    int yPos = (64 + fontHeight) / 2.4 - display->getFontDescent();
    display->drawStr(xPos, yPos, text);
}

// 画面表示要求を統一的に使う
bool handleSpecialKeyDisplay(U8G2* display, const String& characters, const String& prevCharacters) {
    if (!display) return false;

    DisplayRequest req;
    req.display = display;
    // デフォルト値をまとめて設定
    req.jumpHeight = 4;
    req.frames = 6;
    req.frameDelay = 60;
    req.bmp_w = 128;
    req.bmp_h = 64;

    if (prevCharacters == "s" && characters != "s") {
        if (characters == "3") {
            req.type = DISPLAY_ANIMATION;
            req.bitmap = epd_bitmap_faces_3_5;
            requestDisplay(req);
            return true;
        }
    }

    if (characters == "1") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_1;
        requestDisplay(req);
        return true;
    } else if (characters == "2") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_2;
        requestDisplay(req);
        return true;
    } else if (characters == "3") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_3;
        requestDisplay(req);
        return true;
    } else if (characters == "4") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_4;
        requestDisplay(req);
        return true;
    } else if (characters == "s") {
        req.type = DISPLAY_TEXT;
        req.text1 = "SHIFT ON!!";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
        return true;
    } else if (characters == "e") {
        req.type = DISPLAY_TEXT;
        req.text1 = "INTRO";
        req.text2 = "Japanese or English";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
        return true;
    } else if (characters == "b") {
        req.type = DISPLAY_TEXT;
        req.text1 = "BARK";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
        return true;
    } else if (characters == "h") {
        req.type = DISPLAY_TEXT;
        req.text1 = "HAZARD";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
        return true;
    } else if (characters == "t") {
        req.type = DISPLAY_TEXT;
        req.text1 = "AT/MT";
        req.text2 = "TOGGLE";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
        return true;
    } else if (characters == "Up") {
        req.type = DISPLAY_TEXT;
        req.text1 = "MOVE FWD";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    } else if (characters == "Down") {
        req.type = DISPLAY_TEXT;
        req.text1 = "MOVE BKWD";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    } else if (characters == "Left") {
        req.type = DISPLAY_TEXT;
        req.text1 = "TURN LEFT";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    } else if (characters == "Right") {
        req.type = DISPLAY_TEXT;
        req.text1 = "TURN RIGHT";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    } else if (characters == "Esc") {
        req.type = DISPLAY_TEXT;
        req.text1 = "ESCAPE";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    } else if (characters == "PrintScreen") {
        req.type = DISPLAY_TEXT;
        req.text1 = "PRTSC";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    } else if (characters == ",") {
        req.type = DISPLAY_TEXT;
        req.text1 = "STOP";
        req.text2 = "LINEAR SPEED";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    } else if (characters == ".") {
        req.type = DISPLAY_TEXT;
        req.text1 = "STOP";
        req.text2 = "ANGULAR SPEED";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
    }    
    return false;
}

// setup()でディスプレイタスクとキューを初期化してください
// displayQueue = xQueueCreate(4, sizeof(DisplayRequest));
// xTaskCreatePinnedToCore(displayTask, "displayTask", 4096, NULL, 1, NULL, 0);