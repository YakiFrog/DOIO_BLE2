#pragma once
#include <U8g2lib.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// 画面表示タイプ
enum DisplayType {
    DISPLAY_NORMAL,
    DISPLAY_ANIMATION,
    DISPLAY_TEXT
};

// 画面表示要求構造体
struct DisplayRequest {
    DisplayType type;
    U8G2* display;
    String text1; // メイン表示
    String text2; // サブ表示（例：バイトキー名）
    const unsigned char* bitmap;
    int bmp_w;
    int bmp_h;
    int jumpHeight;
    int frames;
    int frameDelay;
    const uint8_t* font;
};

// 画面表示要求をキューに入れる関数
void requestDisplay(DisplayRequest& req);

// ディスプレイ専用タスク
void displayTask(void* pvParameters);

void drawCenteredBitmap(U8G2* display, int bmp_w, int bmp_h, const unsigned char* bitmap);
// 前回のキーも渡す
bool handleSpecialKeyDisplay(U8G2* display, const String& characters, const String& prevCharacters);
void jumpBitmapAnimation(U8G2* display, const unsigned char* bitmap, int bmp_w, int bmp_h, int jumpHeight, int frames, int frameDelay);
void drawCenteredText(U8G2* display, const char* text, const uint8_t* font);