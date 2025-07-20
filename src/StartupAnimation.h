#pragma once
#include <U8g2lib.h>

class StartupAnimation {
public:
    StartupAnimation(U8G2* display)
        : display(display) {}

    // テキストを左右中央に配置するためのX座標計算（U8G2用）
    int getCenterX(const char* text, const uint8_t* font) {
        display->setFont(font);
        int textPixelWidth = display->getStrWidth(text);
        int screenWidth = display->getDisplayWidth();
        return (screenWidth - textPixelWidth) / 2;
    }

    // 5秒間の起動画面アニメーション
    void showStartupScreen() {
        for (int i = 5; i > 0; i--) {
            display->clearBuffer();
            // タイトル
            display->setFont(u8g2_font_fub14_tr); // 少し小さめの太字
            display->drawStr(getCenterX("KOTACON", u8g2_font_fub14_tr), 32, "KOTACON"); 
            // サブタイトル
            display->setFont(u8g2_font_7x13_tr);
            display->drawStr(getCenterX("Made by Kotani", u8g2_font_7x13_tr), 50, "Made by Kotani");
            // カウントダウン
            char countdown[32];
            snprintf(countdown, sizeof(countdown), "Starting in %ds...", i);
            display->drawStr(getCenterX(countdown, u8g2_font_6x10_tr), 62, countdown);
            display->sendBuffer();
            delay(1000);
        }
    }

private:
    U8G2* display;
};