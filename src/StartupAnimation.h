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

    // 5秒間の起動画面アニメーション（KOTACONの文字が順番にホップする）
    void showStartupScreen() {
        const char* title = "KOTACON";
        const int titleLen = 7;
        const int hopHeight = 8; // ホップの高さ
        const int hopDuration = 120; // 1文字あたりのホップ時間(ms)
        const int normalY = 32;
        const int hopY = normalY - hopHeight;

        for (int sec = 5; sec > 0; sec--) {
            unsigned long startMillis = millis();
            while (millis() - startMillis < 1000) {
                for (int idx = 0; idx < titleLen; idx++) {
                    display->clearBuffer();
                    // タイトル（各文字をホップさせる）
                    display->setFont(u8g2_font_fub14_tr);
                    int x = getCenterX(title, u8g2_font_fub14_tr);
                    int charX = x;
                    for (int i = 0; i < titleLen; i++) {
                        int y = (i == idx) ? hopY : normalY;
                        char c[2] = { title[i], '\0' };
                        display->drawStr(charX, y, c);
                        charX += display->getStrWidth(c); // 各文字の幅を加算
                    }
                    // サブタイトル
                    display->setFont(u8g2_font_7x13_tr);
                    display->drawStr(getCenterX("Made by Kotani", u8g2_font_7x13_tr), 50, "Made by Kotani");
                    // カウントダウン
                    char countdown[32];
                    snprintf(countdown, sizeof(countdown), "Starting in %ds...", sec);
                    display->setFont(u8g2_font_6x10_tr);
                    display->drawStr(getCenterX(countdown, u8g2_font_6x10_tr), 62, countdown);
                    display->sendBuffer();
                    delay(hopDuration);
                }
            }
        }
        // 最後に全ての文字を通常の高さで1周描画
        display->clearBuffer();
        display->setFont(u8g2_font_fub14_tr);
        int x = getCenterX(title, u8g2_font_fub14_tr);
        int charX = x;
        for (int i = 0; i < titleLen; i++) {
            int y = normalY;
            char c[2] = { title[i], '\0' };
            display->drawStr(charX, y, c);
            charX += display->getStrWidth(c);
        }
        display->setFont(u8g2_font_7x13_tr);
        display->drawStr(getCenterX("Made by Kotani", u8g2_font_7x13_tr), 50, "Made by Kotani");
        char countdown[32];
        snprintf(countdown, sizeof(countdown), "Starting in 0s...", 0);
        display->setFont(u8g2_font_6x10_tr);
        display->drawStr(getCenterX(countdown, u8g2_font_6x10_tr), 62, countdown);
        display->sendBuffer();
        delay(400); // 少しだけ表示
    }

private:
    U8G2* display;
};