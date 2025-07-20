#include "SpecialKeyHandler.h"
#include "BitmapImages.h"

// 汎用：画面中央にビットマップを表示
void drawCenteredBitmap(U8G2* display, int bmp_w, int bmp_h, const unsigned char* bitmap) {
    int xPos = (128 - bmp_w) / 2; // 128: 画面幅
    int yPos = (64 - bmp_h) / 2;  // 64: 画面高さ
    display->drawXBMP(xPos, yPos, bmp_w, bmp_h, bitmap);
}

// 画面中央にテキストを表示する関数
void drawCenteredText(U8G2* display, const char* text, const uint8_t* font) {
    display->setFont(font);
    int textWidth = display->getStrWidth(text);
    int xPos = (128 - textWidth) / 2; // 128: 画面幅
    int fontHeight = display->getFontAscent() - display->getFontDescent();
    int yPos = (64 + fontHeight) / 2.4 - display->getFontDescent(); // 64: 画面高さ
    display->drawStr(xPos, yPos, text);
}

// 顔画像を上下にぴょんぴょん揺らすアニメーション
void jumpBitmapAnimation(U8G2* display, const unsigned char* bitmap, int bmp_w, int bmp_h, int jumpHeight = 4, int frames = 6, int frameDelay = 60) {
    int baseY = 0;
    for (int i = 0; i < frames; ++i) {
        int yOffset = baseY;
        if (i % 2 == 1) yOffset -= jumpHeight; // 奇数フレームで上に移動
        display->clearBuffer();
        display->drawXBMP(0, yOffset, bmp_w, bmp_h, bitmap);
        display->sendBuffer();
        delay(frameDelay);
    }
    // 最後は通常位置で表示
    display->clearBuffer();
    display->drawXBMP(0, baseY, bmp_w, bmp_h, bitmap);
    display->sendBuffer();
}

// 前回のキーも受け取る
bool handleSpecialKeyDisplay(U8G2* display, const String& characters, const String& prevCharacters) {
    if (!display) return false;

    if (prevCharacters == "s" && characters != "s") {
        if (characters == "3") {
            jumpBitmapAnimation(display, epd_bitmap_faces_3_5, 128, 64);
            return true; 
        } else {
        }
    }

    if (characters == "1") {
        jumpBitmapAnimation(display, epd_bitmap_faces_1, 128, 64);
        return true;
    } else if (characters == "2") {
        jumpBitmapAnimation(display, epd_bitmap_faces_2, 128, 64);
        return true;
    } else if (characters == "3") {
        jumpBitmapAnimation(display, epd_bitmap_faces_3, 128, 64);
        return true;
    } else if (characters == "4") {
        jumpBitmapAnimation(display, epd_bitmap_faces_4, 128, 64);
        return true;
    } else if (characters == "s") {
        display->clearBuffer();
        display->setFont(u8g2_font_fub14_tr);
        drawCenteredText(display, "SHIFT ON!!", u8g2_font_fub14_tr);
        display->sendBuffer();
        delay(1000); 
        return true;
    }
    // 残り，e,b,h,t,Up,Down,Left,Right,Escape,PrintScreen
    else if (characters == "e") {
        display->clearBuffer();
        display->setFont(u8g2_font_fub14_tr);
        drawCenteredText(display, "ESCAPE!!", u8g2_font_fub14_tr);
        display->sendBuffer();
        delay(1000); // 1秒間表示
        return true; // 特別表示を行った
    }
    // 何も特別な処理がなければ通常表示を行う
    return false; // 通常表示を行う
}