/**
 * Kuluçka Makinesi Kontrol Sistemi - WiFi Modülü
 * 
 * Bu modül, ESP32'nin WiFi bağlantısını ve TCP/IP sunucusunu yönetir.
 * Android uygulaması ile iletişim için gereken fonksiyonları içerir.
 */

 #ifndef WIFI_MODULE_H
 #define WIFI_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 #include <WiFi.h>
 #include <ArduinoJson.h>
 
 // Fonksiyon prototipleri
 bool WifiInit();
 void WifiUpdate();
 bool WifiStartAP();
 bool WifiStartSTA(const char* ssid, const char* password);
 bool WifiStop();
 WifiStatus WifiGetStatus();
 String WifiGetIP();
 int WifiGetClientCount();
 bool WifiSetAPCredentials(const char* ssid, const char* password);
 
 // İstemciye özel veri gönderme
 bool WifiSendData(const String& data, WiFiClient& targetClient);
 
 // Tüm istemcilere veri gönderme
 bool WifiSendDataToAll(const String& data);
 
 // Geriye uyumluluk için eski fonksiyon (tüm istemcilere gönderir)
 bool WifiSendData(const String& data);
 
 String WifiReceiveData();
 bool WifiIsClientConnected();
 
 // Android iletişim fonksiyonları
 void ProcessJsonCommand(ArduinoJson::JsonDocument& doc, WiFiClient& client, int clientIndex);
 void SendSensorData(WiFiClient& client);
 void SendSettingsData(WiFiClient& client);
 void SendProfileData(WiFiClient& client);
 void SendAlarmData(WiFiClient& client);
 void SendAllProfilesData(WiFiClient& client);
 void SendSystemStatus(WiFiClient& client);
 bool HandleSettingsUpdate(ArduinoJson::JsonDocument& doc);
 
 // Düşük güç modu desteği
 void WifiEnterLowPowerMode();
 void WifiExitLowPowerMode();
 bool WifiIsInLowPowerMode();
 
 #endif // WIFI_MODULE_H