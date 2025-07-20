#pragma once
#include <Adafruit_SSD1306.h>

class StartupAnimation {
public:
    StartupAnimation(Adafruit_SSD1306* display)
        : display(display) {}

    // テキストを左右中央に配置するためのX座標計算
    int getCenterX(const char* text, int textSize = 1) {
        int charWidth = 6 * textSize; // Adafruit_GFXの標準幅
        int textPixelWidth = strlen(text) * charWidth;
        int screenWidth = display->width();
        return (screenWidth - textPixelWidth) / 2;
    }

    // 5秒間の起動画面アニメーション
    void showStartupScreen() {
        for (int i = 5; i > 0; i--) {
            display->clearDisplay();
            display->setTextSize(2);
            display->setTextColor(SSD1306_WHITE);
            display->setCursor(getCenterX("KOTACON", 2), 10); // 中央配置
            display->println("KOTACON");
            display->setTextSize(1);
            display->setCursor(getCenterX("Made by Kotani", 1), 40); // 中央配置
            display->println("Made by Kotani");
            // カウントダウン文字列を作成して中央配置
            char countdown[32];
            snprintf(countdown, sizeof(countdown), "Starting in %ds...", i);
            display->setCursor(getCenterX(countdown, 1), 55);
            display->print(countdown);
            display->display();
            delay(1000);
        }
    }

private:
    Adafruit_SSD1306* display;
};
