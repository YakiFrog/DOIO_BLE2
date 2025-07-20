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
    for (;;) {
        if (xQueueReceive(displayQueue, &req, portMAX_DELAY) == pdTRUE) {
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
                req.display->sendBuffer();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            } else if (req.type == DISPLAY_ANIMATION) {
                int baseY = 0;
                for (int i = 0; i < req.frames; ++i) {
                    int yOffset = baseY;
                    if (i % 2 == 1) yOffset -= req.jumpHeight;
                    req.display->clearBuffer();
                    req.display->drawXBMP(0, yOffset, req.bmp_w, req.bmp_h, req.bitmap);
                    req.display->sendBuffer();
                    vTaskDelay(req.frameDelay / portTICK_PERIOD_MS);
                }
                req.display->clearBuffer();
                req.display->drawXBMP(0, baseY, req.bmp_w, req.bmp_h, req.bitmap);
                req.display->sendBuffer();
            }
        }
        vTaskDelay(1);
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

    if (prevCharacters == "s" && characters != "s") {
        if (characters == "3") {
            req.type = DISPLAY_ANIMATION;
            req.bitmap = epd_bitmap_faces_3_5;
            req.bmp_w = 128;
            req.bmp_h = 64;
            req.jumpHeight = 4;
            req.frames = 6;
            req.frameDelay = 60;
            requestDisplay(req);
            return true;
        }
    }

    if (characters == "1") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_1;
        req.bmp_w = 128;
        req.bmp_h = 64;
        req.jumpHeight = 4;
        req.frames = 6;
        req.frameDelay = 60;
        requestDisplay(req);
        return true;
    } else if (characters == "2") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_2;
        req.bmp_w = 128;
        req.bmp_h = 64;
        req.jumpHeight = 4;
        req.frames = 6;
        req.frameDelay = 60;
        requestDisplay(req);
        return true;
    } else if (characters == "3") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_3;
        req.bmp_w = 128;
        req.bmp_h = 64;
        req.jumpHeight = 4;
        req.frames = 6;
        req.frameDelay = 60;
        requestDisplay(req);
        return true;
    } else if (characters == "4") {
        req.type = DISPLAY_ANIMATION;
        req.bitmap = epd_bitmap_faces_4;
        req.bmp_w = 128;
        req.bmp_h = 64;
        req.jumpHeight = 4;
        req.frames = 6;
        req.frameDelay = 60;
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
        req.text1 = "ESCAPE!!";
        req.font = u8g2_font_fub14_tr;
        requestDisplay(req);
        return true;
    }
    return false;
}

// setup()でディスプレイタスクとキューを初期化してください
// displayQueue = xQueueCreate(4, sizeof(DisplayRequest));
// xTaskCreatePinnedToCore(displayTask, "displayTask", 4096, NULL, 1, NULL, 0);