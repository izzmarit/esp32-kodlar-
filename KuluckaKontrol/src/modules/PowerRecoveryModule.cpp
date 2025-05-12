/**
 * Kuluçka Makinesi Kontrol Sistemi - Elektrik Kesintisi Kurtarma Modülü
 * 
 * Bu dosya, elektrik kesintisi kurtarma modülünün uygulamasını içerir.
 */

 #include "PowerRecoveryModule.h"
 #include <LittleFS.h>  // SPIFFS yerine LittleFS kullanın
 #include <ArduinoJson.h>
 #include <RTClib.h>  // RTC referansı için
 #include "DisplayModule.h" // Ekran için
 #include "DataModule.h"
 #include "TimeModule.h"
 #include "ProfileModule.h"
 #include "ControlModule.h"
  
 // Dosya yolları
 const char* RECOVERY_FILE = "/recovery.json";
 const char* LAST_POWER_ON_FILE = "/last_power.txt";
  
 // Son güç kapatma zamanı
 uint32_t lastPowerOffTime = 0;
 bool wasPowerOutage = false;
 unsigned long outageDuration = 0;
  
 bool PowerRecoveryInit() {
    // LittleFS'i kontrol et
    if (!LittleFS.exists(LAST_POWER_ON_FILE)) {
      // İlk çalıştırma - sadece mevcut zamanı kaydet
      uint32_t currentTime = GetUnixTime();
      File file = LittleFS.open(LAST_POWER_ON_FILE, "w");
      if (file) {
        file.println(currentTime);
        file.close();
        Serial.println("İlk çalıştırma: güç zamanı kaydedildi");
      } else {
        Serial.println("Güç zamanı dosyası oluşturulamadı!");
        // Non-critical error, continue anyway
      }
      return true;
    }
    
    // Önceki güç açılış zamanını oku
    File file = LittleFS.open(LAST_POWER_ON_FILE, "r");
    if (file) {
      String timeStr = file.readStringUntil('\n');
      file.close();
      lastPowerOffTime = timeStr.toInt();
    }
    
    // Mevcut zamanı güncelle
    uint32_t currentTime = GetUnixTime();
    
    // Yeni zamanı kaydet
    file = LittleFS.open(LAST_POWER_ON_FILE, "w");
    if (file) {
      file.println(currentTime);
      file.close();
    }
    
    // Kesinti olup olmadığını kontrol et
    if (lastPowerOffTime > 0 && currentTime > lastPowerOffTime) {
      // Minimum 5 saniyeden fazla fark varsa elektrik kesintisi olarak değerlendir
      outageDuration = currentTime - lastPowerOffTime;
      
      if (outageDuration >= 5) {
        wasPowerOutage = true;
        
        Serial.print("Elektrik kesintisi algılandı. Süre: ");
        if (outageDuration < 60) {
          Serial.print(outageDuration);
          Serial.println(" saniye");
        } else if (outageDuration < 3600) {
          Serial.print(outageDuration / 60);
          Serial.println(" dakika");
        } else {
          Serial.print(outageDuration / 3600);
          Serial.print(" saat ");
          Serial.print((outageDuration % 3600) / 60);
          Serial.println(" dakika");
        }
      } else {
        Serial.println("Kısa kesinti veya yeniden başlatma algılandı (< 5 sn)");
      }
    }
    
    return true;
 }
 
 void PowerRecoveryCheckAndRestore() {
   Serial.println("Sistem durumu kontrol ediliyor...");
   
   // Kurtarma dosyasını kontrol et
   if (LittleFS.exists(RECOVERY_FILE)) {
     File file = LittleFS.open(RECOVERY_FILE, "r");
     if (!file) {
       Serial.println("Kurtarma dosyası açılamadı!");
       return;
     }
     
     // JSON verisini ayrıştır
     StaticJsonDocument<512> doc;
     DeserializationError error = deserializeJson(doc, file);
     file.close();
     
     if (error) {
       Serial.print("Kurtarma verisi ayrıştırma hatası: ");
       Serial.println(error.c_str());
       return;
     }
     
     // Kurtarma verisini çıkar ve varsa sistem durumunu geri yükle
     RecoveryData recovery;
     recovery.incubationActive = doc["incubation_active"] | false;
     recovery.profileType = doc["profile_type"] | -1;
     recovery.startTime = doc["start_time"] | 0;
     recovery.totalDays = doc["total_days"] | 21;
     recovery.currentDay = doc["current_day"] | 0;
     recovery.targetTemp = doc["target_temp"] | DEFAULT_TEMP;
     recovery.targetHumidity = doc["target_humidity"] | DEFAULT_HUM;
     recovery.motorEnabled = doc["motor_enabled"] | true;
     recovery.saveTime = doc["save_time"] | 0;
    
     // Kuluçka işleminin devam etmesi gerekip gerekmediğini kontrol et
     if (recovery.incubationActive && recovery.profileType >= 0) {
       // Mevcut ayarları yükle
       IncubationSettings settings = LoadSettings();
       bool needsRestore = false;
      
       // Kuluçka durumu karşılaştırması
       if (settings.startTime == 0 || settings.type != recovery.profileType) {
         needsRestore = true;
       }
      
       // Hedef değerler karşılaştırması
       if (abs(settings.targetTemp - recovery.targetTemp) > 0.1 || 
           abs(settings.targetHumidity - recovery.targetHumidity) > 1.0) {
         needsRestore = true;
       }
      
       if (needsRestore) {
         Serial.println("Kuluçka durumu geri yükleniyor...");
        
         // Kuluçka ayarlarını geri yükle
         settings.type = recovery.profileType;
         settings.targetTemp = recovery.targetTemp;
         settings.targetHumidity = recovery.targetHumidity;
         settings.totalDays = recovery.totalDays;
         settings.motorEnabled = recovery.motorEnabled;
        
         // Başlangıç zamanını kontrol et ve güncelle
         if (settings.startTime == 0 || settings.startTime != recovery.startTime) {
           settings.startTime = recovery.startTime;
           Serial.print("Kuluçka başlangıç zamanı geri yüklendi: ");
           Serial.println(settings.startTime);
         }
        
         // Ayarları kaydet
         SaveSettings(settings);
        
         // Profil türünü ayarla
         ProfileSetType((ProfileType)settings.type);
        
         // Kontrol hedeflerini ayarla
         ControlSetTargets(settings.targetTemp, settings.targetHumidity);
        
         Serial.println("Kuluçka süreci kurtarıldı!");
         Serial.print("Profil: ");
         Serial.print(ProfileGetTypeName((ProfileType)settings.type));
         Serial.print(", Gün: ");
         Serial.print(GetIncubationDay(settings.startTime));
         Serial.print("/");
         Serial.println(settings.totalDays);
        } else {
          Serial.println("Kuluçka bilgileri güncel, kurtarma gerekmedi");
        }
      } else {
        Serial.println("Aktif kuluçka süreci bulunamadı");
      }
    } else {
      Serial.println("Kurtarma dosyası bulunamadı");
    }
  }
    
  void PowerRecoverySaveState() {
    // Mevcut sistem durumunu periyodik olarak kaydet
    IncubationSettings settings = LoadSettings();
    bool isIncubationActive = ProfileIsActive();
    uint32_t currentTime = GetUnixTime();
    
    // Kurtarma dosyasını oluştur
    StaticJsonDocument<512> doc;
    
    // Kuluçka durumu
    doc["incubation_active"] = isIncubationActive;
    doc["profile_type"] = settings.type;
    doc["start_time"] = settings.startTime;
    doc["total_days"] = settings.totalDays;
    doc["current_day"] = GetIncubationDay(settings.startTime);
    
    // Hedef değerler
    doc["target_temp"] = settings.targetTemp;
    doc["target_humidity"] = settings.targetHumidity;
    
    // Diğer ayarlar
    doc["motor_enabled"] = settings.motorEnabled;
    doc["save_time"] = currentTime;
    
    // Dosyaya kaydet
    File file = LittleFS.open(RECOVERY_FILE, "w");
    if (file) {
      size_t bytesWritten = serializeJson(doc, file);
      file.close();
      
      if (bytesWritten > 0) {
        Serial.println("Sistem durumu kaydedildi");
      } else {
        Serial.println("Sistem durumu kaydedilemedi!");
      }
    } else {
      Serial.println("Kurtarma dosyası oluşturulamadı!");
    }
  }
   
  bool PowerRecoveryWasPowerOutage() {
    return wasPowerOutage;
  }
   
  unsigned long PowerRecoveryGetOutageDuration() {
    return outageDuration;
  }
   
  void PowerRecoveryDisplayOutageInfo() {
    if (!wasPowerOutage) {
      return;
    }
    
    // Ekranda elektrik kesintisi bilgisini göster
    tft.fillScreen(ST7735_BLACK);
    
    tft.fillRect(0, 0, tft.width(), 20, ST7735_RED);
    tft.setCursor(15, 5);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.print("ELEKTRiK KESiNTiSi");
    
    tft.setCursor(10, 30);
    tft.setTextColor(ST7735_YELLOW);
    tft.print("Sistem yeniden baslatildi");
    
    tft.setCursor(10, 50);
    tft.setTextColor(ST7735_WHITE);
    tft.print("Kesinti suresi:");
    
    tft.setCursor(10, 65);
    tft.setTextSize(2);
    
    if (outageDuration < 60) {
      tft.print(outageDuration);
      tft.print(" sn");
    } else if (outageDuration < 3600) {
      tft.print(outageDuration / 60);
      tft.print(" dk");
    } else {
      tft.print(outageDuration / 3600);
      tft.print(" sa ");
      tft.print((outageDuration % 3600) / 60);
      tft.print(" dk");
    }
    
    tft.setTextSize(1);
    tft.setCursor(10, 90);
    tft.print("Kulucka surecine");
    tft.setCursor(10, 105);
    tft.print("devam ediliyor...");
    
    tft.setCursor(10, tft.height() - 20);
    tft.setTextColor(ST7735_CYAN);
    tft.print("Devam etmek icin tiklayin");
    
    // 5 saniye bekle veya joystick butonu basılınca çık
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) {
      if (digitalRead(JOYSTICK_BTN) == LOW) {
        break;
      }
      delay(100);
    }
  }