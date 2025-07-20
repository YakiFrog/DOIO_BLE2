#pragma once
#include <U8g2lib.h>
#include <Arduino.h>

void drawCenteredBitmap(U8G2* display, int bmp_w, int bmp_h, const unsigned char* bitmap);
// 前回のキーも渡す
bool handleSpecialKeyDisplay(U8G2* display, const String& characters, const String& prevCharacters);
void jumpBitmapAnimation(U8G2* display, const unsigned char* bitmap, int bmp_w, int bmp_h, int jumpHeight, int frames, int frameDelay);
void drawCenteredText(U8G2* display, const char* text, const uint8_t* font);