/**
 * Kuluçka Makinesi Kontrol Sistemi - Profil Modülü
 * 
 * Bu dosya, genişletilmiş kuş türleri için profil tanımlamalarını içerir.
 */

 #include "ProfileModule.h"
 #include "ControlModule.h"
 #include "DisplayModule.h" // DisplayShowNotification için
 #include "WifiModule.h"    // WiFi fonksiyonları için
 #include <LittleFS.h>      // SPIFFS yerine LittleFS
 #include <FS.h>            // File sınıfı için
 #include <ArduinoJson.h>   // JSON işlemleri için
 #include "DataModule.h"    // LoadSettings/SaveSettings fonksiyonları için
 #include "TimeModule.h"    // GetUnixTime ve GetIncubationDay fonksiyonları için
 
 // Mevcut profil
 extern Profile currentProfile;

 Profile GetProfileByType(ProfileType type) {
  if (type >= PROFILE_CHICKEN && type <= PROFILE_PHEASANT) {
      return predefinedProfiles[type];
  }
  
  // Geçersiz profil türü, varsayılan profili döndür
  return predefinedProfiles[PROFILE_CHICKEN];
}
 
 // Önceden tanımlanmış profiller - genişletilmiş kuş türleri ile
 // Tavuk, Kaz, Bıldırcın, Ördek, Manuel, Hindi, Keklik, Güvercin, Sülün
 extern Profile predefinedProfiles[MAX_PROFILES];
 
 // Aşama geçiş takibi için global değişkenler
 static ProfileStage lastActiveStage = STAGE_EARLY;
 static bool stageNotificationEnabled = true;
 
 // Profil tanımlamalarını JSON dosyasından yükleyen fonksiyon
 bool LoadProfilesFromFile() {
  if (!LittleFS.exists("/profiles.json")) {
      Serial.println("Profil dosyası bulunamadı, varsayılanlar kullanılacak");
      return false;
  }
  
  File file = LittleFS.open("/profiles.json", "r");
  if (!file) {
      Serial.println("Profil dosyası açılamadı!");
      return false;
  }
  
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
      Serial.print("Profil JSON ayrıştırma hatası: ");
      Serial.println(error.c_str());
      return false;
  }
  
  // Profil sayısını kontrol et
  if (!doc.is<JsonArray>() || doc.size() == 0) {
      Serial.println("Profil dosyası geçersiz format!");
      return false;
  }
  
  // Profilleri yükle
  int profileCount = min((int)doc.size(), 9); // Maksimum 9 profil
  
  for (int i = 0; i < profileCount; i++) {
      JsonObject profileObj = doc[i].as<JsonObject>();
      
      if (profileObj.containsKey("type") && profileObj.containsKey("name") && 
         profileObj.containsKey("total_days") && profileObj.containsKey("stages")) {
          
          int type = profileObj["type"];
          if (type >= 0 && type < 9) {
              // Temel profil bilgilerini oku
              predefinedProfiles[type].type = (ProfileType)type;
              predefinedProfiles[type].name = profileObj["name"].as<String>();
              predefinedProfiles[type].totalDays = profileObj["total_days"];
              JsonArray stagesArray = profileObj["stages"].as<JsonArray>();
              
              predefinedProfiles[type].stageCount = min((int)stagesArray.size(), 4);
              
              for (int j = 0; j < predefinedProfiles[type].stageCount; j++) {

                  JsonObject stageObj = stagesArray[j].as<JsonObject>();
                  
                  predefinedProfiles[type].stages[j].temperature = stageObj["temp"];
                  predefinedProfiles[type].stages[j].humidity = stageObj["humidity"];
                  predefinedProfiles[type].stages[j].motorActive = stageObj["motor_active"];
                  predefinedProfiles[type].stages[j].startDay = stageObj["start_day"];
                  predefinedProfiles[type].stages[j].endDay = stageObj["end_day"];
              }
              
              Serial.print("Profil yüklendi: ");
              Serial.println(predefinedProfiles[type].name);
          }
      }
  }
  
  return true;
}
 
 // JSON yapılandırma dosyasını oluşturan fonksiyon (ilk çalıştırma için)
 bool CreateDefaultProfilesFile() {
  StaticJsonDocument<4096> doc;
  JsonArray profilesArray = doc.to<JsonArray>();
  
  // Varsayılan profil tanımlamalarını JSON yapısına dönüştür
  for (int i = 0; i <= PROFILE_PHEASANT; i++) {
    JsonObject profileObj = profilesArray.createNestedObject();
    
    profileObj["type"] = i;
    profileObj["name"] = predefinedProfiles[i].name;
    profileObj["total_days"] = predefinedProfiles[i].totalDays;
    
    JsonArray stagesArray = profileObj.createNestedArray("stages");
    
    for (int j = 0; j < predefinedProfiles[i].stageCount; j++) {
      JsonObject stageObj = stagesArray.createNestedObject();
      
      stageObj["temp"] = predefinedProfiles[i].stages[j].temperature;
      stageObj["humidity"] = predefinedProfiles[i].stages[j].humidity;
      stageObj["motor_active"] = predefinedProfiles[i].stages[j].motorActive;
      stageObj["start_day"] = predefinedProfiles[i].stages[j].startDay;
      stageObj["end_day"] = predefinedProfiles[i].stages[j].endDay;
    }
  }
  
  // Dosyaya kaydet
  File file = LittleFS.open("/profiles.json", "w");
  if (!file) {
    Serial.println("Profil dosyası oluşturulamadı!");
    return false;
  }
  
  if (serializeJson(doc, file) == 0) {
    Serial.println("Profil verisi yazılamadı!");
    file.close();
    return false;
  }
  
  file.close();
  Serial.println("Varsayılan profil dosyası oluşturuldu");
  return true;
 }
 
 bool ProfileInit() {
  // Önce varsayılan profilleri tanımla (ilk çalıştırma için)
  // (Mevcut profil tanımlamaları kodu aşağıya eklenecek)
  
  // Tavuk profili
  predefinedProfiles[PROFILE_CHICKEN].type = PROFILE_CHICKEN;
  predefinedProfiles[PROFILE_CHICKEN].name = "Tavuk";
  predefinedProfiles[PROFILE_CHICKEN].totalDays = 21;
  predefinedProfiles[PROFILE_CHICKEN].stageCount = 3;
  
  // Erken aşama (1-9 gün)
  predefinedProfiles[PROFILE_CHICKEN].stages[0].temperature = 37.8;
  predefinedProfiles[PROFILE_CHICKEN].stages[0].humidity = 55.0;
  predefinedProfiles[PROFILE_CHICKEN].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_CHICKEN].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_CHICKEN].stages[0].endDay = 9;
  
  // Orta aşama (10-17 gün)
  predefinedProfiles[PROFILE_CHICKEN].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_CHICKEN].stages[1].humidity = 60.0;
  predefinedProfiles[PROFILE_CHICKEN].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_CHICKEN].stages[1].startDay = 10;
  predefinedProfiles[PROFILE_CHICKEN].stages[1].endDay = 17;
  
  // Son aşama (18-21 gün)
  predefinedProfiles[PROFILE_CHICKEN].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_CHICKEN].stages[2].humidity = 70.0;
  predefinedProfiles[PROFILE_CHICKEN].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_CHICKEN].stages[2].startDay = 18;
  predefinedProfiles[PROFILE_CHICKEN].stages[2].endDay = 21;
  
  // Kaz profili
  predefinedProfiles[PROFILE_GOOSE].type = PROFILE_GOOSE;
  predefinedProfiles[PROFILE_GOOSE].name = "Kaz";
  predefinedProfiles[PROFILE_GOOSE].totalDays = 30;
  predefinedProfiles[PROFILE_GOOSE].stageCount = 3;
  
  // Erken aşama (1-14 gün)
  predefinedProfiles[PROFILE_GOOSE].stages[0].temperature = 37.7;
  predefinedProfiles[PROFILE_GOOSE].stages[0].humidity = 60.0;
  predefinedProfiles[PROFILE_GOOSE].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_GOOSE].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_GOOSE].stages[0].endDay = 14;
  
  // Orta aşama (15-24 gün)
  predefinedProfiles[PROFILE_GOOSE].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_GOOSE].stages[1].humidity = 65.0;
  predefinedProfiles[PROFILE_GOOSE].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_GOOSE].stages[1].startDay = 15;
  predefinedProfiles[PROFILE_GOOSE].stages[1].endDay = 24;
  
  // Son aşama (25-30 gün)
  predefinedProfiles[PROFILE_GOOSE].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_GOOSE].stages[2].humidity = 75.0;
  predefinedProfiles[PROFILE_GOOSE].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_GOOSE].stages[2].startDay = 25;
  predefinedProfiles[PROFILE_GOOSE].stages[2].endDay = 30;
  
  // Bıldırcın profili
  predefinedProfiles[PROFILE_QUAIL].type = PROFILE_QUAIL;
  predefinedProfiles[PROFILE_QUAIL].name = "Bildircin";
  predefinedProfiles[PROFILE_QUAIL].totalDays = 18;
  predefinedProfiles[PROFILE_QUAIL].stageCount = 3;
  
  // Erken aşama (1-6 gün)
  predefinedProfiles[PROFILE_QUAIL].stages[0].temperature = 37.7;
  predefinedProfiles[PROFILE_QUAIL].stages[0].humidity = 60.0;
  predefinedProfiles[PROFILE_QUAIL].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_QUAIL].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_QUAIL].stages[0].endDay = 6;
  
  // Orta aşama (7-14 gün)
  predefinedProfiles[PROFILE_QUAIL].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_QUAIL].stages[1].humidity = 65.0;
  predefinedProfiles[PROFILE_QUAIL].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_QUAIL].stages[1].startDay = 7;
  predefinedProfiles[PROFILE_QUAIL].stages[1].endDay = 14;
  
  // Son aşama (15-18 gün)
  predefinedProfiles[PROFILE_QUAIL].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_QUAIL].stages[2].humidity = 75.0;
  predefinedProfiles[PROFILE_QUAIL].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_QUAIL].stages[2].startDay = 15;
  predefinedProfiles[PROFILE_QUAIL].stages[2].endDay = 18;
  
  // Ördek profili
  predefinedProfiles[PROFILE_DUCK].type = PROFILE_DUCK;
  predefinedProfiles[PROFILE_DUCK].name = "Ordek";
  predefinedProfiles[PROFILE_DUCK].totalDays = 28;
  predefinedProfiles[PROFILE_DUCK].stageCount = 3;
  
  // Erken aşama (1-9 gün)
  predefinedProfiles[PROFILE_DUCK].stages[0].temperature = 37.7;
  predefinedProfiles[PROFILE_DUCK].stages[0].humidity = 65.0;
  predefinedProfiles[PROFILE_DUCK].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_DUCK].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_DUCK].stages[0].endDay = 9;
  
  // Orta aşama (10-24 gün)
  predefinedProfiles[PROFILE_DUCK].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_DUCK].stages[1].humidity = 70.0;
  predefinedProfiles[PROFILE_DUCK].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_DUCK].stages[1].startDay = 10;
  predefinedProfiles[PROFILE_DUCK].stages[1].endDay = 24;
  
  // Son aşama (25-28 gün)
  predefinedProfiles[PROFILE_DUCK].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_DUCK].stages[2].humidity = 80.0;
  predefinedProfiles[PROFILE_DUCK].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_DUCK].stages[2].startDay = 25;
  predefinedProfiles[PROFILE_DUCK].stages[2].endDay = 28;
  
  // Manuel profil (varsayılan ayarlar)
  predefinedProfiles[PROFILE_MANUAL].type = PROFILE_MANUAL;
  predefinedProfiles[PROFILE_MANUAL].name = "Manuel";
  predefinedProfiles[PROFILE_MANUAL].totalDays = 21;
  predefinedProfiles[PROFILE_MANUAL].stageCount = 1;
  
  // Tek aşama (tüm günler için aynı)
  predefinedProfiles[PROFILE_MANUAL].stages[0].temperature = 37.5;
  predefinedProfiles[PROFILE_MANUAL].stages[0].humidity = 65.0;
  predefinedProfiles[PROFILE_MANUAL].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_MANUAL].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_MANUAL].stages[0].endDay = 21;
  
  // YENİ EKLENMİŞ KUŞ TÜRLERİ
  
  // Hindi profili
  predefinedProfiles[PROFILE_TURKEY].type = PROFILE_TURKEY;
  predefinedProfiles[PROFILE_TURKEY].name = "Hindi";
  predefinedProfiles[PROFILE_TURKEY].totalDays = 28;
  predefinedProfiles[PROFILE_TURKEY].stageCount = 3;
  
  // Erken aşama (1-10 gün)
  predefinedProfiles[PROFILE_TURKEY].stages[0].temperature = 37.8;
  predefinedProfiles[PROFILE_TURKEY].stages[0].humidity = 58.0;
  predefinedProfiles[PROFILE_TURKEY].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_TURKEY].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_TURKEY].stages[0].endDay = 10;
  
  // Orta aşama (11-24 gün)
  predefinedProfiles[PROFILE_TURKEY].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_TURKEY].stages[1].humidity = 63.0;
  predefinedProfiles[PROFILE_TURKEY].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_TURKEY].stages[1].startDay = 11;
  predefinedProfiles[PROFILE_TURKEY].stages[1].endDay = 24;
  
  // Son aşama (25-28 gün)
  predefinedProfiles[PROFILE_TURKEY].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_TURKEY].stages[2].humidity = 75.0;
  predefinedProfiles[PROFILE_TURKEY].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_TURKEY].stages[2].startDay = 25;
  predefinedProfiles[PROFILE_TURKEY].stages[2].endDay = 28;
  
  // Keklik profili
  predefinedProfiles[PROFILE_PARTRIDGE].type = PROFILE_PARTRIDGE;
  predefinedProfiles[PROFILE_PARTRIDGE].name = "Keklik";
  predefinedProfiles[PROFILE_PARTRIDGE].totalDays = 24;
  predefinedProfiles[PROFILE_PARTRIDGE].stageCount = 3;
  
  // Erken aşama (1-8 gün)
  predefinedProfiles[PROFILE_PARTRIDGE].stages[0].temperature = 37.7;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[0].humidity = 62.0;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[0].endDay = 8;
  
  // Orta aşama (9-20 gün)
  predefinedProfiles[PROFILE_PARTRIDGE].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[1].humidity = 65.0;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[1].startDay = 9;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[1].endDay = 20;
  
  // Son aşama (21-24 gün)
  predefinedProfiles[PROFILE_PARTRIDGE].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[2].humidity = 72.0;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[2].startDay = 21;
  predefinedProfiles[PROFILE_PARTRIDGE].stages[2].endDay = 24;
  
  // Güvercin profili
  predefinedProfiles[PROFILE_PIGEON].type = PROFILE_PIGEON;
  predefinedProfiles[PROFILE_PIGEON].name = "Guvercin";
  predefinedProfiles[PROFILE_PIGEON].totalDays = 18;
  predefinedProfiles[PROFILE_PIGEON].stageCount = 3;
 
  // Erken aşama (1-5 gün)
  predefinedProfiles[PROFILE_PIGEON].stages[0].temperature = 37.8;
  predefinedProfiles[PROFILE_PIGEON].stages[0].humidity = 65.0;
  predefinedProfiles[PROFILE_PIGEON].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_PIGEON].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_PIGEON].stages[0].endDay = 5;
 
  // Orta aşama (6-14 gün)
  predefinedProfiles[PROFILE_PIGEON].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_PIGEON].stages[1].humidity = 60.0;
  predefinedProfiles[PROFILE_PIGEON].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_PIGEON].stages[1].startDay = 6;
  predefinedProfiles[PROFILE_PIGEON].stages[1].endDay = 14;
 
  // Son aşama (15-18 gün)
  predefinedProfiles[PROFILE_PIGEON].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_PIGEON].stages[2].humidity = 70.0;
  predefinedProfiles[PROFILE_PIGEON].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_PIGEON].stages[2].startDay = 15;
  predefinedProfiles[PROFILE_PIGEON].stages[2].endDay = 18;
 
  // Sülün profili
  predefinedProfiles[PROFILE_PHEASANT].type = PROFILE_PHEASANT;
  predefinedProfiles[PROFILE_PHEASANT].name = "Sulun";
  predefinedProfiles[PROFILE_PHEASANT].totalDays = 25;
  predefinedProfiles[PROFILE_PHEASANT].stageCount = 3;
 
  // Erken aşama (1-8 gün)
  predefinedProfiles[PROFILE_PHEASANT].stages[0].temperature = 37.7;
  predefinedProfiles[PROFILE_PHEASANT].stages[0].humidity = 60.0;
  predefinedProfiles[PROFILE_PHEASANT].stages[0].motorActive = true;
  predefinedProfiles[PROFILE_PHEASANT].stages[0].startDay = 1;
  predefinedProfiles[PROFILE_PHEASANT].stages[0].endDay = 8;
 
  // Orta aşama (9-21 gün)
  predefinedProfiles[PROFILE_PHEASANT].stages[1].temperature = 37.5;
  predefinedProfiles[PROFILE_PHEASANT].stages[1].humidity = 55.0;
  predefinedProfiles[PROFILE_PHEASANT].stages[1].motorActive = true;
  predefinedProfiles[PROFILE_PHEASANT].stages[1].startDay = 9;
  predefinedProfiles[PROFILE_PHEASANT].stages[1].endDay = 21;
 
  // Son aşama (22-25 gün)
  predefinedProfiles[PROFILE_PHEASANT].stages[2].temperature = 37.2;
  predefinedProfiles[PROFILE_PHEASANT].stages[2].humidity = 70.0;
  predefinedProfiles[PROFILE_PHEASANT].stages[2].motorActive = false;
  predefinedProfiles[PROFILE_PHEASANT].stages[2].startDay = 22;
  predefinedProfiles[PROFILE_PHEASANT].stages[2].endDay = 25;
  
  // Ardından SPIFFS'ten profilleri yüklemeyi dene
  bool profilesLoaded = LoadProfilesFromFile();
  
  // Eğer profil dosyası yoksa oluştur (ilk çalıştırma)
  if (!profilesLoaded) {
    CreateDefaultProfilesFile();
  }
  
  // Mevcut profili ayarla (kaydedilmiş ayarlardan)
  IncubationSettings settings = LoadSettings();
  if (settings.type >= 0 && settings.type < 9) {
    currentProfile = predefinedProfiles[settings.type];
  } else {
    currentProfile = predefinedProfiles[PROFILE_NONE];
  }
  
  Serial.println("Profil modülü başlatıldı");
  return true;
 }
 
 void ProfileUpdate() {
 // Aktif kuluçka varsa, mevcut günü ve aşamayı kontrol et
 if (ProfileIsActive()) {
   int currentDay = ProfileGetCurrentDay();
   
   if (currentDay > 0 && currentDay <= currentProfile.totalDays) {
     ProfileStage currentStage = ProfileGetCurrentStage(currentDay);
     
     // Aşama değişikliği olup olmadığını kontrol et
     if (currentStage != lastActiveStage && stageNotificationEnabled) {
       // Aşama değişikliği bildirimi
       String stageName;
       switch (currentStage) {
         case STAGE_EARLY:
           stageName = "Erken";
           break;
         case STAGE_MIDDLE:
           stageName = "Orta";
           break;
         case STAGE_LATE:
           stageName = "Son";
           break;
         case STAGE_HATCHING:
           stageName = "Çıkış";
           break;
       }
       
       // Bildirim metni oluştur
       String message = "Kuluçka Aşaması: " + stageName;
       
       // Ekranda göster
       DisplayShowNotification(message);
       
       // Eğer WiFi bağlantısı varsa Android uygulamasına da bildir
       if (WifiIsClientConnected()) {
         String notificationJson = "{\"notification\":\"stage_change\",\"stage\":" + 
                            String((int)currentStage) + ",\"stage_name\":\"" + 
                            stageName + "\",\"day\":" + String(currentDay) + "}";
         WifiSendData(notificationJson);
       }
       
       lastActiveStage = currentStage;
     }
     
     ProfileStageSettings stageSettings = ProfileGetStageSettings(currentStage);
     
     // Kontrol modülünü güncelle
     ControlSetTargets(stageSettings.temperature, stageSettings.humidity);
     
     // Motor durumunu güncelle
     // Not: Motor kontrol modülü aracılığıyla otomatik dönüşleri yönetir
   }
 }
 }
 
 Profile ProfileGetCurrent() {
 return currentProfile;
 }
 
 ProfileType ProfileGetType() {
 return currentProfile.type;
 }
 
 bool ProfileSetType(ProfileType type) {
 if (type >= PROFILE_CHICKEN && type <= PROFILE_MANUAL) {
   currentProfile = predefinedProfiles[type];
   
   // Ayarları güncelle ama kuluçkayı başlatma
   IncubationSettings settings = LoadSettings();
   settings.type = type;
   settings.totalDays = currentProfile.totalDays;
   SaveSettings(settings);
   
   return true;
 }
 return false;
 }
 
 ProfileStage ProfileGetCurrentStage(int currentDay) {
 for (int i = 0; i < currentProfile.stageCount; i++) {
   if (currentDay >= currentProfile.stages[i].startDay && 
       currentDay <= currentProfile.stages[i].endDay) {
     return (ProfileStage)i;
   }
 }
 
 // Varsayılan olarak ilk aşamayı döndür
 return STAGE_EARLY;
 }
 
 ProfileStageSettings ProfileGetStageSettings(ProfileStage stage) {
 if (stage >= 0 && stage < currentProfile.stageCount) {
   return currentProfile.stages[stage];
 }
 
 // Geçersiz aşama, ilk aşama ayarlarını döndür
 return currentProfile.stages[0];
 }
 
 bool ProfileStartIncubation(ProfileType type) {
  if (type >= PROFILE_CHICKEN && type <= PROFILE_PHEASANT) {
      currentProfile = predefinedProfiles[type];
      
      // Ayarları güncelle ve kuluçkayı başlat
      IncubationSettings settings = LoadSettings();
      settings.type = type;
      settings.targetTemp = currentProfile.stages[0].temperature;
      settings.targetHumidity = currentProfile.stages[0].humidity;
      settings.totalDays = currentProfile.totalDays;
      settings.startTime = GetUnixTime();  // Mevcut zamanı başlangıç zamanı olarak ayarla
      settings.motorEnabled = currentProfile.stages[0].motorActive;
      SaveSettings(settings);
      
      // Kontrol modülünü güncelle
      ControlSetTargets(settings.targetTemp, settings.targetHumidity);
      
      return true;
  }
  return false;
}
 
 bool ProfileEndIncubation() {
 // Kuluçkayı sonlandır
 IncubationSettings settings = LoadSettings();
 settings.type = PROFILE_NONE;
 settings.startTime = 0;
 SaveSettings(settings);
 
 currentProfile = predefinedProfiles[PROFILE_NONE];
 
 return true;
 }
 
 int ProfileGetRemainingDays() {
 int currentDay = ProfileGetCurrentDay();
 
 if (currentDay > 0 && currentDay <= currentProfile.totalDays) {
   return currentProfile.totalDays - currentDay + 1;
 }
 
 return 0;
 }
 
 int ProfileGetCurrentDay() {
 IncubationSettings settings = LoadSettings();
 
 if (settings.startTime > 0) {
   return GetIncubationDay(settings.startTime);
 }
 
 return 0;
 }
 
 bool ProfileIsActive() {
 IncubationSettings settings = LoadSettings();
 return (settings.startTime > 0 && settings.type != PROFILE_NONE);
 }
 
 int ProfileGetTotalDays(ProfileType type) {
 if (type >= PROFILE_CHICKEN && type <= PROFILE_MANUAL) {
   return predefinedProfiles[type].totalDays;
 }
 return 0;
 }
 
 String ProfileGetTypeName(ProfileType type) {
  if (type >= PROFILE_CHICKEN && type <= PROFILE_PHEASANT) {
      return predefinedProfiles[type].name;
  }
  return "Bilinmeyen";
}
 
 void ProfileUpdateSettings(ProfileType type, int stage, ProfileStageSettings settings) {
 if (type >= PROFILE_CHICKEN && type <= PROFILE_MANUAL && 
     stage >= 0 && stage < predefinedProfiles[type].stageCount) {
   predefinedProfiles[type].stages[stage] = settings;
   
   // Eğer bu aktif profil ve aşama ise, kontrol değerlerini güncelle
   if (currentProfile.type == type && ProfileGetCurrentStage(ProfileGetCurrentDay()) == stage) {
     ControlSetTargets(settings.temperature, settings.humidity);
   }
 }
 }
 
 void ProfileEnableStageNotifications(bool enable) {
 stageNotificationEnabled = enable;
 }