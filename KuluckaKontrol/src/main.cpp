/**
 * ESP32 Kuluçka Makinesi Kontrol Sistemi
 * 
 * Tarih: 17.05.2025
 * Versiyon: 5.0.0
 * 
 * Bu program, ESP32 mikrodenetleyici kullanarak kuluçka makinesi kontrolü sağlar.
 * SHT31 sıcaklık ve nem sensörlerini, TFT ekranı, joystick'i ve röleleri kontrol eder.
 */

 #include <Arduino.h>
 #include <Wire.h>
 #include <SPI.h>
 #include "esp_task_wdt.h"
 #include "esp_system.h"
 #include "esp_log.h"
 #include "esp_ota_ops.h"
 #include "esp_sleep.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "esp_pm.h"
 #include <Preferences.h>
  
 // Proje konfigürasyonu
 #include "AppConfig.h"
 #include "config/pins.h"
 #include "config/GlobalDefinitions.h"
 #include "modules/SensorModule.h"
 #include "modules/ControlModule.h"
 #include "modules/WifiModule.h"
 #include "modules/UIModule.h"
 #include "modules/DataModule.h"
 #include "modules/TimeModule.h"
  
 // Modül yöneticisi
 #include "modules/ModuleManager.h"
  
 // Ekran modülü için global değişkenler
 #include "modules/DisplayModule.h"
 #include "modules/PowerSaveModule.h"
 #include <Adafruit_GFX.h>
 #include <Adafruit_ST7735.h>
  
 // Global değişkenler
 IncubationSettings settings;
 MotorSettings motorSettings;
 AlarmThresholds alarmThresholds;
 ModuleStatus moduleStatus = {false};
 bool systemInitialized = false;
 bool displayInitialized = false;
 WifiStatus wifiStatus = WIFI_STATUS_OFF;
 Preferences preferences;
 
 // Global ayar önbelleği
 IncubationSettings globalCachedSettings;
 unsigned long globalLastSettingsUpdate = 0;
  
 // Menü navigasyonu için
 int menuSelection = 0;
 int profileMenuCurrentPage = 0;
  
 // DisplayModule için
 bool notificationActive = false;
 unsigned long notificationStartTime = 0;
 String activeNotificationMessage = "";
 DisplayMode previousDisplayMode = DISPLAY_WELCOME;
 Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
  
 // Aktif profil
 Profile currentProfile;
 Profile predefinedProfiles[MAX_PROFILES];
  
 // Elektrik kesintileri
 PowerOutage powerOutages[MAX_POWER_OUTAGES];
 int powerOutageCount = 0;
  
 // OTA değişkenleri
 #ifdef ENABLE_OTA
 String otaMqttServer = "mqtt.example.com";
 int otaMqttPort = 1883;
 String otaMqttUser = "otauser";
 String otaMqttPassword = "otapassword";
 #endif
 
 // Sistem kararlılık değişkenleri
 unsigned long lastResetCheck = 0;
 const unsigned long RESET_CHECK_INTERVAL = 300000; // 5 dakikada bir kontrol
 const unsigned long MIN_UPTIME = 10000; // 10 saniye minimum çalışma süresi
 unsigned long systemStartTime = 0;
 int resetCount = 0;
 
 // FreeRTOS görev tanımlamaları
 #if USE_MULTITASKING
 TaskHandle_t sensorTaskHandle = NULL;
 TaskHandle_t displayTaskHandle = NULL;
 TaskHandle_t controlTaskHandle = NULL;
 TaskHandle_t wifiTaskHandle = NULL;
 
  
 // Sensör okuma görevi
 void sensorTask(void *pvParameters) {
     const TickType_t xDelay = pdMS_TO_TICKS(SENSOR_READ_INTERVAL);
     
     while (true) {
         if (systemInitialized && moduleStatus.sensorInitialized) {
             SensorUpdate();
         }
         vTaskDelay(xDelay);
     }
 }
  
 // Ekran güncelleme görevi
 void displayTask(void *pvParameters) {
    const TickType_t xDelay = pdMS_TO_TICKS(500); // 100ms'den 500ms'ye çıkarıldı
    
    while (true) {
        if (systemInitialized && moduleStatus.displayInitialized) {
            DisplayUpdate();
        }
        vTaskDelay(xDelay);
    }
 }
  
 // Kontrol görevi
 void controlTask(void *pvParameters) {
     const TickType_t xDelay = pdMS_TO_TICKS(1000); // 1 saniye aralık
     
     while (true) {
         if (systemInitialized && moduleStatus.controlInitialized) {
             ControlUpdate();
         }
         vTaskDelay(xDelay);
     }
 }
  
 // WiFi görevi
 void wifiTask(void *pvParameters) {
     const TickType_t xDelay = pdMS_TO_TICKS(1000); // 1 saniye aralık
     
     while (true) {
         if (systemInitialized && moduleStatus.wifiInitialized) {
             WifiUpdate();
         }
         vTaskDelay(xDelay);
     }
 }
 #endif
  
 // Sistem belleği kontrolü
 void printMemoryStats() {
  LOG_INFO("MEM", "Free Heap: %d bytes", ESP.getFreeHeap());
  
  if (psramFound()) {
      LOG_INFO("MEM", "Free PSRAM: %d bytes", ESP.getFreePsram());
  } else {
      LOG_INFO("MEM", "PSRAM not found or not enabled");
  }
  
  LOG_INFO("MEM", "Min Free Heap: %d bytes", ESP.getMinFreeHeap());
  LOG_INFO("MEM", "Max Alloc Heap: %d bytes", ESP.getMaxAllocHeap());
 }
 
 // Watchdog timer beslemesi
 void feedWatchdog() {
  esp_task_wdt_reset();
 }
 
 // Kullanıcı aktivitesi tespiti
 void registerActivity() {
  static unsigned long lastActivityTime = 0;
  lastActivityTime = millis();
  
  if (systemInitialized && moduleStatus.powerSaveInitialized) {
      PowerSaveResetScreenSaverTimer();
  }
 }
 
 // Sistem kararlılığını arttırmak için hata kontrolü ve otomatik reset fonksiyonu
 void checkSystemStability() {
     unsigned long currentTime = millis();
     
     // Sistem çalışma süresi çok kısaysa, hızlı ardışık resetleri engelle
     if (currentTime < MIN_UPTIME) {
         resetCount++;
         if (resetCount > 3) {
             // Çok sık reset oluyorsa, daha yavaş bir şekilde başlat
             delay(5000); // 5 saniye bekle
         }
     }
     
     // Periyodik stabilite kontrolü
     if (currentTime - lastResetCheck > RESET_CHECK_INTERVAL) {
         lastResetCheck = currentTime;
         
         // Heap boyutu tehlikeli derecede düşükse
         if (ESP.getFreeHeap() < 20000) {
             LOG_ERROR("SYSTEM", "Heap belleği kritik seviyede düşük! (%d bytes)", ESP.getFreeHeap());
             
             // Mümkünse bazı kaynakları serbest bırak
             if (moduleStatus.wifiInitialized) {
                 WifiStop(); // WiFi'yi kapat
                 LOG_INFO("SYSTEM", "WiFi kapatıldı - bellek tasarrufu için");
             }
             
             delay(1000);
             
             // Kritik durumsa cihazı yeniden başlat
             if (ESP.getFreeHeap() < 10000) {
                 LOG_ERROR("SYSTEM", "Bellek çok düşük, sistem yeniden başlatılıyor...");
                 
                 // Çalışma süresini ve reset sayısını kaydet
                 preferences.begin("system", false);
                 preferences.putUInt("uptime", (currentTime - systemStartTime) / 1000);
                 preferences.putUInt("reset_count", resetCount);
                 preferences.end();
                 
                 ESP.restart();
             }
         }
     }
 }
 
 void setup() {
     // Serial port başlatma
     Serial.begin(115200);
     delay(100); // Serial portun açılması için kısa bir bekleme
     
     // Sistem başlangıç zamanını kaydet
     systemStartTime = millis();
     
     LOG_INFO("BOOT", "ESP32 Kuluçka Makinesi Kontrol Sistemi Başlatılıyor");
     LOG_INFO("BOOT", "Versiyon: %s", APP_VERSION);
     
     // Pin modlarını ayarla
     setupPins();
     
     // Joystick test ekleme
     LOG_INFO("JOYSTICK", "Joystick test başlatılıyor");
     for (int i = 0; i < 5; i++) {
         int xValue = analogRead(JOYSTICK_X);
         int yValue = analogRead(JOYSTICK_Y);
         int btnValue = digitalRead(JOYSTICK_BTN);
         LOG_INFO("JOYSTICK", "Test #%d - X: %d, Y: %d, BTN: %d", i+1, xValue, yValue, btnValue);
         delay(200);
     }
     
     // Bellek durumunu yazdır
     printMemoryStats();
     
     // PSRAM varsa etkinleştir
     if (ENABLE_PSRAM && psramFound()) {
         LOG_INFO("BOOT", "PSRAM etkinleştiriliyor");
         heap_caps_malloc_extmem_enable(20000); // 20KB limit
     }
     
     // Watchdog timer'ı başlat - daha uzun bir süre ile
     #if defined(CONFIG_ESP_TASK_WDT_SUPPORT)
     // ESP-IDF 4.4 ve üzeri için
     esp_task_wdt_config_t wdtConfig;
     wdtConfig.timeout_ms = WDT_TIMEOUT_SECONDS * 2000; // 2x timeout süresi
     wdtConfig.idle_core_mask = 0;
     wdtConfig.trigger_panic = true;
     esp_task_wdt_init(&wdtConfig);
     #else
     // Önceki versiyonlar için
     esp_task_wdt_init(WDT_TIMEOUT_SECONDS * 2, true);
     #endif
     feedWatchdog();
     
     // I2C ve SPI başlatma
     Wire.begin(SDA_PIN, SCL_PIN);
     SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
     
     // Tüm modülleri başlat
     ModuleErrorCode initResult = ModuleManagerInit();
     if (initResult != MODULE_OK) {
         LOG_ERROR("BOOT", "Modül başlatma hatası: %s", 
                   ModuleManagerGetErrorMessage(initResult).c_str());
     } else {
         LOG_INFO("BOOT", "Tüm modüller başarıyla başlatıldı");
     }
     
     // Watchdog timer'a görevi ekle
     esp_task_wdt_add(NULL);
     
     // Sistem başlatma durumunu işaretle
     systemInitialized = true;
     LOG_INFO("BOOT", "Sistem başlatma tamamlandı");
     printMemoryStats();
     
     // Ekran başlatıldıysa hoş geldiniz ekranı göster
     if (moduleStatus.displayInitialized) {
         DisplayShowWelcome();
         delay(2000); // Logo ekranını 2 saniye göster
         UIBackToMain();
     }
     
     // FreeRTOS görevlerini başlat
     #if USE_MULTITASKING
     if (systemInitialized) {
         // Görevler burada tanımlanır...
     }
     #endif
     
     // Reset sayacını sıfırla
     resetCount = 0;
 }
 
 void loop() {
     // Watchdog'u besle
     feedWatchdog();
     
     // Sistem kararlılık kontrolü
     checkSystemStability();
     
     // Periyodik bellek kontrolü
     static unsigned long lastMemCheck = 0;
     unsigned long currentTime = millis();
     
     if (currentTime - lastMemCheck > 60000) { // 1 dakikada bir
         lastMemCheck = currentTime;
         LOG_INFO("LOOP", "Periyodik bellek kontrolü");
         printMemoryStats();
     }
     
     // Ana sistem başlatılmadıysa hata göster
     if (!systemInitialized) {
         static unsigned long lastErrorFlash = 0;
         
         if (currentTime - lastErrorFlash > 1000) {  // 1 saniyelik aralık
             lastErrorFlash = currentTime;
             static bool errorState = false;
             errorState = !errorState;
             
             if (errorState && moduleStatus.displayInitialized) {
                 DisplayShowError("Sistem Baslatilmadi!");
             } else if (moduleStatus.displayInitialized) {
                 tft.fillScreen(ST7735_BLACK);
             }
         }
         
         feedWatchdog();
         delay(50);
         return;
     }
     
     // *** YENİ EKLENEN KOD - UI Modülünü doğrudan güncelle ***
     if (moduleStatus.uiInitialized) {
         UIUpdate();
     }
     
     #if !USE_MULTITASKING
     // Çoklu görev kullanılmıyorsa, sırayla tüm modülleri güncelle
     ModuleManagerUpdate();
     #else
     // Çoklu görev kullanılıyorsa, sadece ModuleManager'ı güncelle
     // Diğer modüller kendi görevlerinde güncelleniyor
     #endif
     
     // Periyodik ekran yenileme - ana ekran verilerinin sürekli korunması
     static unsigned long lastDisplayRefresh = 0;
     if (currentTime - lastDisplayRefresh > 5000 && UIGetCurrentMenu() == MENU_NONE) { // 5 saniyede bir ve ana ekrandaysa
         lastDisplayRefresh = currentTime;
         
         if (systemInitialized && moduleStatus.displayInitialized) {
             // Ana ekran verilerini tazele, verileri korunması için
             SensorData sensorData = GetSensorData();
             RelayState relayState = ControlGetRelayState();
             IncubationSettings settings = LoadSettings();
             
             // Güncel kuluçka bilgisini hesapla
             int currentDay = 0;
             int totalDays = settings.totalDays;
             
             if (settings.startTime > 0) {
                 currentDay = GetIncubationDay(settings.startTime);
             }
             
             // Ana ekranı güncelle
             DisplayShowMain(
                 sensorData.avgTemperature,
                 sensorData.avgHumidity,
                 currentDay,
                 totalDays,
                 relayState.heater,
                 relayState.humidifier,
                 relayState.motor
             );
         }
     }
     
     // Ana döngü düzenlemesi - daha uzun bekleme
     static unsigned long loopStartTime = 0;
     unsigned long loopDuration = millis() - loopStartTime;
     
     if (loopDuration < MAIN_LOOP_DELAY) {
         // CPU kullanımını azaltmak için kısa bir bekleme
         delay(MAIN_LOOP_DELAY - loopDuration);
     }
     
     loopStartTime = millis();
 }