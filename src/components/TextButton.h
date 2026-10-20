#ifndef TextButton_h
#define TextButton_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSans9pt7b.h>  // Match genericPages button font
#include "constants/Colors.h"
#include "DisplayObject.h"
#include "../services/display.h"
#include "text_fit_logic.h"

// Special constant to indicate no pin assignment (visual-only button)
#define NO_PIN 255

class TextButton : public DisplayObject
{
private:
    bool lastButtonState = false;
    String buttonText;
    const uint8_t buttonPin;
    
    // Dynamic styling properties
    uint16_t textColor = Colors::black;
    uint16_t backgroundColor = Colors::textBackground;
    uint16_t pressedTextColor = Colors::black;
    uint16_t pressedBackgroundColor = Colors::white;
    
    // Temporary label shown by flashText() until flashUntilMs
    String restoreText;
    uint32_t flashUntilMs = 0;

    // Track changes for redraw
    String lastText;
    uint16_t lastTextColor;
    uint16_t lastBackgroundColor;

public:
    TextButton(const String &text, uint8_t pin, int16_t x, int16_t y, int16_t width = 70, int16_t height = 35)
        : DisplayObject(x, y, width, height), buttonText(text), buttonPin(pin)
    {
        // Initialize tracking variables
        lastText = text;
        lastTextColor = textColor;
        lastBackgroundColor = backgroundColor;
    }

    // Methods to dynamically change button appearance
    void setText(const String &text) {
        buttonText = text;
        flashUntilMs = 0;
    }

    // Shows `text` for `durationMs`, then restores the current label.
    // Calling it again restarts the timer.
    void flashText(const String &text, uint32_t durationMs) {
        if (flashUntilMs == 0) restoreText = buttonText;
        buttonText = text;
        flashUntilMs = millis() + durationMs;
        if (flashUntilMs == 0) flashUntilMs = 1;
    }

    // Restores the label replaced by flashText() straight away.
    void clearFlash() {
        if (flashUntilMs == 0) return;
        buttonText = restoreText;
        flashUntilMs = 0;
    }

    void setColors(uint16_t backgroundColor = Colors::textBackground, uint16_t textColor = Colors::white) {
        this->textColor = textColor;
        this->backgroundColor = backgroundColor;
    }

    bool shouldDraw() override
    {
        if (flashUntilMs != 0 &&
            static_cast<int32_t>(millis() - flashUntilMs) >= 0) {
            clearFlash();
        }

        bool currentState = false;
        bool stateChanged = false;
        
        // Only check pin state if a valid pin is assigned
        if (buttonPin != NO_PIN) {
            currentState = digitalRead(buttonPin) == LOW;
            stateChanged = currentState != lastButtonState;
        }
        
        bool textChanged = buttonText != lastText;
        bool colorsChanged = (textColor != lastTextColor ||
                             backgroundColor != lastBackgroundColor);
        
        return stateChanged || textChanged || colorsChanged || isFirstDraw;
    }

    void draw() override
    {
        bool currentState = false;
        
        // Only check pin state if a valid pin is assigned
        if (buttonPin != NO_PIN) {
            currentState = digitalRead(buttonPin) == LOW;
        }
        
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Clear the button area
            tft.fillRect(x, y, width, height, ST77XX_BLACK);
            
            tft.setFont(&FreeSans9pt7b);
            
            if (!currentState) {
                // Use filled rectangle for improved visuals 
                //(border-only near physical screen border can cause visual artifacts)
                tft.fillRoundRect(x, y, width, height, 5, backgroundColor);
                tft.setTextColor(textColor);
            } else {                
                tft.fillRoundRect(x, y, width, height, 5, pressedBackgroundColor);
                tft.setTextColor(pressedTextColor);
            }
            
            // Calculate text position for proper centering (matching genericPages.cpp style)
            int16_t x1, y1;
            uint16_t textWidth, textHeight;
            tft.getTextBounds(buttonText.c_str(), 0, 0, &x1, &y1, &textWidth, &textHeight);

            if (text_fit::fitsButton(textWidth, width)) {
                // Horizontal centering
                int16_t textX = x + (width - textWidth) / 2;

                // Vertical centering - using same formula as Device Stopped screen buttons
                int16_t textY = y + (height + textHeight) / 2;

                tft.setCursor(textX, textY);
                tft.print(buttonText);
            } else {
                // Too wide: use the smaller built-in font, whose cursor is
                // the top-left corner rather than the baseline.
                tft.setFont();
                tft.setTextSize(1);
                int16_t textX = x + (width - text_fit::builtinTextWidth(buttonText.length())) / 2;
                tft.setCursor(textX, text_fit::builtinTextTop(y, height));
                tft.print(buttonText);
                tft.setFont(&FreeSans9pt7b);
            }
            
            xSemaphoreGive(displayMutex);
        }
        
        // Update tracking variables
        lastButtonState = currentState;
        lastText = buttonText;
        lastTextColor = textColor;
        lastBackgroundColor = backgroundColor;
        isFirstDraw = false;
    }
};

#endif
