/**
 * Kuluçka Makinesi Kontrol Sistemi - Ana Konfigürasyon
 */


 #ifndef APP_CONFIG_H
 #define APP_CONFIG_H
 
 #include <Arduino.h>
 
 // Sistem bilgileri
 #ifndef SYSTEM_NAME
 #define SYSTEM_NAME "Kuluçka Kontrol"
 #endif

 #ifndef APP_VERSION 
 #define APP_VERSION "5.0.0"
 #endif

 #ifndef APP_BUILD
 #define APP_BUILD 110  // Build numarası
 #endif
 
 // Hata ayıklama seviyeleri
 #define LOG_LEVEL_NONE      0
 #define LOG_LEVEL_ERROR     1
 #define LOG_LEVEL_WARNING   2
 #define LOG_LEVEL_INFO      3
 #define LOG_LEVEL_DEBUG     4
 #define LOG_LEVEL_VERBOSE   5
 
 // Aktif log seviyesini seçin
 #define CURRENT_LOG_LEVEL LOG_LEVEL_DEBUG
 
 // Güvenlik ayarları
 #define DEFAULT_AP_PASSWORD "12345678"
 #define ADMIN_PASSWORD "admin123"  // Yönetici şifresi, daha güvenli bir şey seçebilirsiniz
 
 // Sistem performans ayarları
 #define ENABLE_PSRAM true  // PSRAM kullanımını etkinleştir
 #define USE_MULTITASKING true  // FreeRTOS çoklu görev kullanımı
 #define MAIN_LOOP_DELAY 10  // Ana döngü gecikmesi (ms)
 
 // Sensör yapılandırması
 #define SENSOR_READ_INTERVAL 2000  // Sensör okuma aralığı (ms)
 #define SENSOR_FILTER_SAMPLES 5  // Hareketli ortalama için örnek sayısı
 
 // Watchdog ayarları
 #define WDT_TIMEOUT_SECONDS 30  // Watchdog timer zaman aşımı
 
 // OTA güncelleme ayarları
 #define OTA_ENABLED true  // OTA güncellemelerini etkinleştir
 #define OTA_URL "https://example.com/firmware"  // OTA sunucu URL'si
 #define OTA_CHECK_INTERVAL 86400000  // Güncelleme kontrolü aralığı (ms) - 24 saat
 
 // Loglama Makroları
 #if CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR
 #define LOG_ERROR(tag, format, ...) Serial.printf("[E][%s] " format "\n", tag, ##__VA_ARGS__)
 #else
   #define LOG_ERROR(tag, format, ...)
 #endif
 
 #if CURRENT_LOG_LEVEL >= LOG_LEVEL_WARNING
   #define LOG_WARNING(tag, format, ...) Serial.printf("[W][%s] " format "\n", tag, ##__VA_ARGS__)
 #else
   #define LOG_WARNING(tag, format, ...)
 #endif
 
 #if CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO
   #define LOG_INFO(tag, format, ...) Serial.printf("[I][%s] " format "\n", tag, ##__VA_ARGS__)
 #else
   #define LOG_INFO(tag, format, ...)
 #endif
 
 #if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
   #define LOG_DEBUG(tag, format, ...) Serial.printf("[D][%s] " format "\n", tag, ##__VA_ARGS__)
 #else
   #define LOG_DEBUG(tag, format, ...)
 #endif
 
 #if CURRENT_LOG_LEVEL >= LOG_LEVEL_VERBOSE
   #define LOG_VERBOSE(tag, format, ...) Serial.printf("[V][%s] " format "\n", tag, ##__VA_ARGS__)
 #else
   #define LOG_VERBOSE(tag, format, ...)
 #endif
 
 #endif // APP_CONFIG_H