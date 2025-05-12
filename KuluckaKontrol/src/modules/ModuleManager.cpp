/**
 * Kuluçka Makinesi Kontrol Sistemi - Modül Yöneticisi
 */

 #include "ModuleManager.h"
 #include "../include/AppConfig.h"
 #include "../config/pins.h"
 #include "DataModule.h"
 #include "TimeModule.h"
 #include "DisplayModule.h"
 #include "SensorModule.h"
 #include "ControlModule.h"
 #include "UIModule.h"
 #include "WifiModule.h"
 #include "ProfileModule.h"
 #include "AlarmModule.h"
 #include "PowerSaveModule.h"
 #include "PowerRecoveryModule.h"
 #include "OTAUpdateModule.h"
 
 // Son hata kodu
 static ModuleErrorCode lastErrorCode = MODULE_OK;
 static const char* TAG = "ModMgr";

 
 ModuleErrorCode ModuleManagerInit() {
    // Başlangıç öncesi durum temizliği
    moduleStatus = {false};
    
    // DATA modülünü başlat
    if (!DataInit()) {
        lastErrorCode = MODULE_ERROR_DATA;
        LOG_ERROR(TAG, "Veri modülü başlatılamadı - varsayılan ayarlarla devam ediliyor");
        // Hata olsa bile devam et
    } else {
        moduleStatus.dataInitialized = true;
        LOG_INFO(TAG, "Veri modülü başlatıldı");
    }
    
    // TIME modülünü başlat
    if (!TimeInit()) {
        lastErrorCode = MODULE_ERROR_TIME;
        LOG_ERROR(TAG, "Zaman modülü başlatılamadı");
        return lastErrorCode;  // Diğer modüllerle tutarlı olması için
    } else {
        moduleStatus.timeInitialized = true;
        LOG_INFO(TAG, "Zaman modülü başlatıldı");
    }
    
    // DISPLAY modülünü başlat
    if (!DisplayInit()) {
        LOG_WARNING(TAG, "Ekran modülü başlatılamadı! Sistem kısıtlı fonksiyonlarla çalışacak");
        // Bu kritik bir hata değil, ekransız devam edebiliriz
    } else {
        moduleStatus.displayInitialized = true;
        LOG_INFO(TAG, "Ekran modülü başlatıldı");
    }
     
     // SENSOR modülünü başlat
     if (!SensorInit()) {
         lastErrorCode = MODULE_ERROR_SENSOR;
         LOG_ERROR(TAG, "Sensör modülü başlatılamadı");
         return lastErrorCode;
     }
     moduleStatus.sensorInitialized = true;
     LOG_INFO(TAG, "Sensör modülü başlatıldı");
     
     // CONTROL modülünü başlat
     if (!ControlInit()) {
         lastErrorCode = MODULE_ERROR_CONTROL;
         LOG_ERROR(TAG, "Kontrol modülü başlatılamadı");
         return lastErrorCode;
     }
     moduleStatus.controlInitialized = true;
     LOG_INFO(TAG, "Kontrol modülü başlatıldı");
     
     // PROFILE modülünü başlat
     if (!ProfileInit()) {
         lastErrorCode = MODULE_ERROR_PROFILE;
         LOG_ERROR(TAG, "Profil modülü başlatılamadı");
         return lastErrorCode;
     }
     moduleStatus.profileInitialized = true;
     LOG_INFO(TAG, "Profil modülü başlatıldı");
     
     // ALARM modülünü başlat
     if (!AlarmInit()) {
         lastErrorCode = MODULE_ERROR_ALARM;
         LOG_ERROR(TAG, "Alarm modülü başlatılamadı");
         return lastErrorCode;
     }
     moduleStatus.alarmInitialized = true;
     LOG_INFO(TAG, "Alarm modülü başlatıldı");
     
     // UI modülünü başlat (ekran yoksa da başlatılabilir)
     if (!UIInit()) {
         lastErrorCode = MODULE_ERROR_UI;
         LOG_ERROR(TAG, "UI modülü başlatılamadı");
         return lastErrorCode;
     }
     moduleStatus.uiInitialized = true;
     LOG_INFO(TAG, "UI modülü başlatıldı");
     
     // POWER_SAVE modülünü başlat
     if (!PowerSaveInit()) {
         lastErrorCode = MODULE_ERROR_POWER_SAVE;
         LOG_ERROR(TAG, "Güç tasarrufu modülü başlatılamadı");
         return lastErrorCode;
     }
     moduleStatus.powerSaveInitialized = true;
     LOG_INFO(TAG, "Güç tasarrufu modülü başlatıldı");
     
     // POWER_RECOVERY modülünü başlat
     
     if (!PowerRecoveryInit()) {
         lastErrorCode = MODULE_ERROR_POWER_RECOVERY;
         LOG_ERROR(TAG, "Elektrik kesintisi kurtarma modülü başlatılamadı");
         return lastErrorCode;
     }
     moduleStatus.powerRecoveryInitialized = true;
     LOG_INFO(TAG, "Elektrik kesintisi kurtarma modülü başlatıldı");
     
     // Elektrik kesintisi olup olmadığını kontrol et
     if (PowerRecoveryWasPowerOutage()) {
         // Kesinti bilgisini göster
         if (moduleStatus.displayInitialized) {
             PowerRecoveryDisplayOutageInfo();
         }
         // Sistemi eski durumuna geri yükle
         PowerRecoveryCheckAndRestore();
     }
     
     // WIFI modülünü başlat (en son, çünkü diğer modüllerin verilerine erişebilmesi gerekiyor)
     if (!WifiInit()) {
         lastErrorCode = MODULE_ERROR_WIFI;
         LOG_ERROR(TAG, "WiFi modülü başlatılamadı");
         // WiFi kritik değil, devam et
     } else {
         moduleStatus.wifiInitialized = true;
         LOG_INFO(TAG, "WiFi modülü başlatıldı");
     }
     
     // OTA modülünü başlat (opsiyonel)
     #if OTA_ENABLED
     if (!OTAInit()) {
         lastErrorCode = MODULE_ERROR_OTA;
         LOG_ERROR(TAG, "OTA modülü başlatılamadı");
         // OTA kritik değil, devam et
     } else {
         moduleStatus.otaInitialized = true;
         LOG_INFO(TAG, "OTA modülü başlatıldı");
     }
     #endif
     
     // Tüm modüller başarıyla başlatıldı
     LOG_INFO(TAG, "Tüm modüller başarıyla başlatıldı");
     return MODULE_OK;
 }
 
 void ModuleManagerUpdate() {
     // Zamanlama kontrolü için statik değişkenler
     static unsigned long lastTimeUpdate = 0;
     static unsigned long lastDataUpdate = 0;
     static unsigned long lastProfileUpdate = 0;
     static unsigned long lastPowerSaveUpdate = 0;
     static unsigned long lastUIUpdate = 0;
     
     #if !USE_MULTITASKING
     // Çoklu görev kullanılmıyorsa, bu fonksiyonda tüm modülleri güncelle
     static unsigned long lastSensorUpdate = 0;
     static unsigned long lastControlUpdate = 0;
     static unsigned long lastDisplayUpdate = 0;
     static unsigned long lastAlarmUpdate = 0;
     static unsigned long lastWifiUpdate = 0;
     #endif
     
     unsigned long currentTime = millis();
     
     // Her modülü kendi periyodunda güncelle
     
     // TIME modülü güncelleme (100ms)
     if (moduleStatus.timeInitialized && currentTime - lastTimeUpdate >= 100) {
         TimeUpdate();
         lastTimeUpdate = currentTime;
     }
     
     #if !USE_MULTITASKING
     // SENSOR modülü güncelleme (2000ms)
     if (moduleStatus.sensorInitialized && currentTime - lastSensorUpdate >= 2000) {
         SensorUpdate();
         lastSensorUpdate = currentTime;
     }
     
     // CONTROL modülü güncelleme (1000ms)
     if (moduleStatus.controlInitialized && currentTime - lastControlUpdate >= 1000) {
         ControlUpdate();
         lastControlUpdate = currentTime;
     }
     
     // DISPLAY modülü güncelleme (500ms)
     if (moduleStatus.displayInitialized && currentTime - lastDisplayUpdate >= 500) {
         DisplayUpdate();
         lastDisplayUpdate = currentTime;
     }
     
     // ALARM modülü güncelleme (3000ms)
     if (moduleStatus.alarmInitialized && currentTime - lastAlarmUpdate >= 3000) {
         AlarmUpdate();
         lastAlarmUpdate = currentTime;
     }
     
     // WIFI modülü güncelleme (1000ms)
     if (moduleStatus.wifiInitialized && currentTime - lastWifiUpdate >= 1000) {
         WifiUpdate();
         lastWifiUpdate = currentTime;
     }
     #endif
     
     // DATA modülü güncelleme (5000ms)
     if (moduleStatus.dataInitialized && currentTime - lastDataUpdate >= 5000) {
         DataUpdate();
         lastDataUpdate = currentTime;
     }
     
     // UI modülü güncelleme (200ms)
     if (moduleStatus.uiInitialized && currentTime - lastUIUpdate >= 200) {
         UIUpdate();
         lastUIUpdate = currentTime;
     }
     
     // PROFILE modülü güncelleme (60000ms - 1 dakika)
     if (moduleStatus.profileInitialized && currentTime - lastProfileUpdate >= 60000) {
         ProfileUpdate();
         lastProfileUpdate = currentTime;
     }
     
     // POWER_SAVE modülü güncelleme (30000ms - 30 saniye)
     if (moduleStatus.powerSaveInitialized && currentTime - lastPowerSaveUpdate >= 30000) {
         PowerSaveUpdate();
         lastPowerSaveUpdate = currentTime;
     }
     
     // OTA modülü güncelleme (kontrol için)
     #if OTA_ENABLED
     if (moduleStatus.otaInitialized) {
         OTAUpdate();
     }
     #endif
     
     // Her 5 dakikada sistem durumunu kaydet
     static unsigned long lastStateSaveTime = 0;
     if (moduleStatus.powerRecoveryInitialized && currentTime - lastStateSaveTime > 300000) {
         lastStateSaveTime = currentTime;
         PowerRecoverySaveState();
     }
 }
 
 ModuleErrorCode ModuleManagerGetLastError() {
     return lastErrorCode;
 }
 
 String ModuleManagerGetErrorMessage(ModuleErrorCode code) {
     switch (code) {
         case MODULE_OK:
             return "Tüm modüller başarıyla başlatıldı";
         case MODULE_ERROR_CONFIG:
             return "Yapılandırma modülü başlatılamadı";
         case MODULE_ERROR_DATA:
             return "Veri modülü başlatılamadı";
         case MODULE_ERROR_TIME:
             return "Zaman modülü başlatılamadı";
         case MODULE_ERROR_DISPLAY:
             return "Ekran modülü başlatılamadı";
         case MODULE_ERROR_SENSOR:
             return "Sensör modülü başlatılamadı";
         case MODULE_ERROR_CONTROL:
             return "Kontrol modülü başlatılamadı";
         case MODULE_ERROR_UI:
             return "Kullanıcı arayüzü modülü başlatılamadı";
         case MODULE_ERROR_WIFI:
             return "WiFi modülü başlatılamadı";
         case MODULE_ERROR_PROFILE:
             return "Profil modülü başlatılamadı";
         case MODULE_ERROR_ALARM:
             return "Alarm modülü başlatılamadı";
         case MODULE_ERROR_POWER_SAVE:
             return "Güç tasarrufu modülü başlatılamadı";
         case MODULE_ERROR_POWER_RECOVERY:
             return "Elektrik kesintisi kurtarma modülü başlatılamadı";
         case MODULE_ERROR_OTA:
             return "OTA güncelleme modülü başlatılamadı";
         default:
             return "Bilinmeyen hata kodu: " + String(code);
     }
 }