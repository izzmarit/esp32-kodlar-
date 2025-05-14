/**
 * Kuluçka Makinesi Kontrol Sistemi - Pin Tanımlamaları
 */

 #ifndef PINS_H
 #define PINS_H
 
 #include <Arduino.h>
 
 // TFT Display Pinleri - ST7735
 #define TFT_CS    5   // Chip Select
 #define TFT_DC    2   // Data/Command (A0)
 #define TFT_RST   4   // Reset
 #define TFT_MOSI  23  // SDA (MOSI)
 #define TFT_SCLK  18  // SCK (Clock)
 #define TFT_MISO  19  // Kullanılmıyor ancak SPI için tanımlandı
 #define TFT_LED   15  // Backlight kontrolü
 
 // I2C Pinleri
 #define SDA_PIN   21  // I2C SDA
 #define SCL_PIN   22  // I2C SCL
 
 // Röle Pinleri
 #define RELAY_HEATER     25  // Isıtıcı rölesi
 #define RELAY_HUMIDIFIER 26  // Nemlendirici rölesi
 #define RELAY_MOTOR      27  // Motor rölesi
 
 // Joystick Pinleri
 #define JOYSTICK_X       34  // X ekseni (ADC1_CH6)
 #define JOYSTICK_Y       35  // Y ekseni (ADC1_CH7)
 #define JOYSTICK_BTN     32  // Buton (interrupt uyumlu)
 
 // Sensör Adresleri
 #define SHT31_1_ADDR 0x44  // İlk SHT31 sensörü
 #define SHT31_2_ADDR 0x45  // İkinci SHT31 sensörü

 #define SSR_RELAY_ACTIVE_HIGH  // SSR röle kullanıldığında bu satırı ekleyin
 
 // Pin modlarını ayarlayan fonksiyon
 inline void setupPins() {
    // Röle pinleri
    pinMode(RELAY_HEATER, OUTPUT);
    pinMode(RELAY_HUMIDIFIER, OUTPUT);
    pinMode(RELAY_MOTOR, OUTPUT);
    
    // Başlangıçta tüm röleleri kapat
    #ifdef SSR_RELAY_ACTIVE_HIGH
    digitalWrite(RELAY_HEATER, LOW);      // SSR için LOW = kapalı
    digitalWrite(RELAY_HUMIDIFIER, LOW);  // SSR için LOW = kapalı
    digitalWrite(RELAY_MOTOR, LOW);       // SSR için LOW = kapalı
    #else
    digitalWrite(RELAY_HEATER, HIGH);     // Standart röle için HIGH = kapalı
    digitalWrite(RELAY_HUMIDIFIER, HIGH); // Standart röle için HIGH = kapalı
    digitalWrite(RELAY_MOTOR, HIGH);      // Standart röle için HIGH = kapalı
    #endif
     
     // Ekran arka ışık pini
     pinMode(TFT_LED, OUTPUT);
     digitalWrite(TFT_LED, HIGH);  // Arka ışık açık
     
     // Joystick buton pini
     pinMode(JOYSTICK_BTN, INPUT_PULLUP);
     
     // Joystick analog pinleri açıkça tanımla
     pinMode(JOYSTICK_X, INPUT);
     pinMode(JOYSTICK_Y, INPUT);
     
     // ADC çözünürlük ayarı (ESP32 için)
     analogReadResolution(12);
 }
 
 #endif // PINS_H