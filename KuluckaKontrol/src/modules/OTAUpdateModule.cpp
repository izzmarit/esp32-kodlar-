// OTAUpdateModule.cpp - MQTT yerine HTTPS kullanacak şekilde güncelle

#include "OTAUpdateModule.h"
#include <LittleFS.h>  // SPIFFS yerine LittleFS kullanın
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "../include/AppConfig.h" // config.h yerine AppConfig.h kullanılmalı

// OTA durum değişkenleri
static bool otaInProgress = false;
static int otaProgress = 0;
static String updateVersion = "";
static String deviceId = "kulucka_" + String(ESP.getEfuseMac(), HEX);

// HTTPS Sunucu bilgileri
static String httpsServer = "https://example.com/ota"; // HTTPS sunucu adresi
static String authToken = ""; // Kimlik doğrulama için
static const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
"hMAtudXH/vTBH1jLuG2cenAKc67gJH3+oBLp1xK0PF/3OY/o9O856wR/Oq03UZ4w\n" \
"bFH/F/vCAyEN334MSbR8xfqWVCilYdeoa2sC/BjXVW6Xkj2lVotsRUelUeVkUU2q\n" \
"gwF1/UJyZxaVXJqLQlW39cR5DKJ+c6+2YmIKRXw+JDKFcmOJ0rzEMjI+f2l53B+6\n" \
"bqi3bkR2EFiXnEWdGCM=\n" \
"-----END CERTIFICATE-----\n";

bool OTAInit() {
  // WiFi bağlantısını kontrol et
  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERROR("OTA", "OTA başlatılamadı: WiFi bağlantısı yok");
    return false;
  }

  // Config dosyasından kimlik doğrulama token'ını al
  if (LittleFS.exists("/ota_token.txt")) {
    File file = LittleFS.open("/ota_token.txt", "r");
    if (file) {
      authToken = file.readStringUntil('\n');
      authToken.trim();
      file.close();
      LOG_INFO("OTA", "OTA kimlik doğrulama token'ı yüklendi");
    }
  }
  
  // OTA sunucuyla bağlantı testi yap
  bool testResult = testOTAServer();
  if (testResult) {
    LOG_INFO("OTA", "OTA sunucu bağlantısı başarılı");
  } else {
    LOG_ERROR("OTA", "OTA sunucu bağlantısı başarısız");
  }
  
  return testResult;
}

bool testOTAServer() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  
  HTTPClient http;
  http.begin(httpsServer + "/check", root_ca); // root_ca sertifikası eklendi
  
  // HTTP üstbilgileri
  http.addHeader("Authorization", "Bearer " + authToken);
  http.addHeader("Device-ID", deviceId);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.GET();
  bool success = (httpCode == 200);
  http.end();
  
  return success;
}

void OTAUpdate() {
  // OTA güncelleme süreci devam ediyorsa ilerlemeyi kontrol et
  if (otaInProgress) {
    // İlerleme kontrolü - gerekli durumlarda ilerlemeyi güncelle
  }
}

bool OTAStartUpdate() {
  if (otaInProgress || WiFi.status() != WL_CONNECTED) {
    return false;
  }
  
  // Sürüm kontrolü yap
  HTTPClient http;
  http.begin(httpsServer + "/version", root_ca); // root_ca sertifikası eklendi
  http.addHeader("Authorization", "Bearer " + authToken);
  http.addHeader("Device-ID", deviceId);
  http.addHeader("Current-Version", APP_VERSION);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    return false;
  }
  
  String response = http.getString();
  http.end();
  
  // Yanıtı ayrıştır
  StaticJsonDocument<256> doc;
DeserializationError error = deserializeJson(doc, response);
if (error) {
    return false;
}

// Güncellemekontrol edildiğinden emin ol
if (doc["update_available"].isNull()) {
    return false; // Geçersiz yanıt
}

bool updateAvailable = doc["update_available"];
if (!updateAvailable) {
    return false; // Güncelleme yok
}

// Version bilgisi var mı kontrol et
if (!doc["version"].isNull()) {
    updateVersion = doc["version"].as<String>();
} else {
    updateVersion = "bilinmeyen";
}
  LOG_INFO("OTA", "Yeni güncelleme bulundu: %s", updateVersion.c_str());
  
  // Güncelleme sürecini başlat
  otaInProgress = true;
  otaProgress = 0;
  
  // Asenkron olarak güncelleme indir
  xTaskCreate(
    OTADownloadTask,       // Görev fonksiyonu
    "OTADownloadTask",     // Görev adı
    8192,                  // Yığın boyutu
    NULL,                  // Parametre
    1,                     // Öncelik
    NULL                   // Görev tanıtıcısı
  );
  
  return true;
}

void OTADownloadTask(void * parameter) {
  HTTPClient http;
  http.begin(httpsServer + "/firmware", root_ca); // root_ca sertifikası eklendi
  http.addHeader("Authorization", "Bearer " + authToken);
  http.addHeader("Device-ID", deviceId);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    otaInProgress = false;
    vTaskDelete(NULL);
    return;
  }
  
  // Firmware boyutunu al
  int contentLength = http.getSize();
  
  // Update başlat
  if (Update.begin(contentLength)) {
    // Veriyi indir ve yazılıma yaz
    WiFiClient * client = http.getStreamPtr();
    size_t written = Update.writeStream(*client);
    
    if (written == contentLength) {
      if (Update.end()) {
        if (Update.isFinished()) {
          LOG_INFO("OTA", "Update tamamlandı. Sistem yeniden başlatılıyor...");
          ESP.restart();
        } else {
          LOG_ERROR("OTA", "Update bitmedi!");
        }
      } else {
        LOG_ERROR("OTA", "Update hatası: %s", String(Update.getError()).c_str());
      }
    } else {
      LOG_ERROR("OTA", "Yazılan ve beklenen boyut uyuşmuyor!");
    }
  } else {
    LOG_ERROR("OTA", "Update başlatılamadı!");
  }
  
  http.end();
  otaInProgress = false;
  vTaskDelete(NULL);
}

void OTASetProgress(int progress) {
  otaProgress = progress;
}

int OTAGetProgress() {
  return otaProgress;
}

bool OTAIsUpdating() {
  return otaInProgress;
}