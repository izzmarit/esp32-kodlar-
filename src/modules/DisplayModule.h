/**
 * Kuluçka Makinesi Kontrol Sistemi - Ekran Modülü
 * 
 * Bu modül, TFT ekranın kontrolü ve kullanıcı arayüzünün görüntülenmesini yönetir.
 */

 #ifndef DISPLAY_MODULE_H
 #define DISPLAY_MODULE_H
 
 #include <Arduino.h>
 #include <Adafruit_GFX.h>
 #include <Adafruit_ST7735.h>
 #include "../config/GlobalDefinitions.h"
 #include "../config/pins.h"
 
 // Renk tanımları
 #define ST7735_BLACK   0x0000
 #define ST7735_BLUE    0x001F
 #define ST7735_RED     0xF800
 #define ST7735_GREEN   0x07E0
 #define ST7735_CYAN    0x07FF
 #define ST7735_MAGENTA 0xF81F
 #define ST7735_YELLOW  0xFFE0
 #define ST7735_WHITE   0xFFFF
 #define ST7735_NAVY    0x000F
 #define ST7735_DARKGREY 0x4A49
 #define ST7735_LIGHTGREY 0xB596
 #define ST7735_ORANGE  0xFD20
 
 // Bildirim değişkenleri
 extern bool notificationActive;
 extern unsigned long notificationStartTime;
 extern String activeNotificationMessage;
 extern DisplayMode previousDisplayMode;
 extern Adafruit_ST7735 tft;
 extern bool isFirstMainDisplay; // UIModule.cpp'de kullanmak için dışa açıldı
 
 // Fonksiyon prototipleri
 bool DisplayIsInitialized();
 bool DisplayInit();
 void DisplayUpdate();
 void DisplaySetMode(DisplayMode mode);
 DisplayMode DisplayGetMode();
 void DisplayShowWelcome();
 void DisplayShowMain(float temp, float humidity, int day, int totalDays, bool heaterOn, bool humidifierOn, bool motorOn);
 void DisplayShowMenu(int selectedItem, const char* items[], int itemCount);
 void DisplayShowGraph(const char* title, float* data, uint32_t* timestamps, int dataCount, int graphType);
 void DisplayShowError(const char* errorMessage);
 void DisplaySetBacklight(bool state);
 void DisplayShowNotification(const String& message);
 
 #endif // DISPLAY_MODULE_H