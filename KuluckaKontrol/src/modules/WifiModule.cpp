/**
 * Kuluçka Makinesi Kontrol Sistemi - WiFi Modülü
 * ArduinoJson 6.19.4 Sürümü ile Uyumlu Güncellenmiş Versiyon
 */

 #include "WifiModule.h"
 #include <WiFi.h>
 #include <WiFiClient.h>
 #include <WiFiAP.h>
 #include <ArduinoJson.h>
 #include "ControlModule.h"
 #include "DataModule.h"
 #include "SensorModule.h"
 #include "TimeModule.h"
 #include "ProfileModule.h"
 #include "AlarmModule.h"
 #include <esp_wifi.h>
 #include <esp_wifi_types.h>
 #include "../include/AppConfig.h"

 // SYSTEM_NAME ve DEFAULT_AP_PASSWORD değerlerini AppConfig.h'den kullanacağız
     
 // WiFi güç tasarrufu sabitleri
 #define WIFI_PS_NONE 0
 #define WIFI_PS_MAX_MODEM 1
     
 static const char* TAG = "WIFI";
     
 // WiFi durumu için statik değişkenler
 static WifiStatus currentWifiStatus = WIFI_STATUS_OFF;
 static bool lowPowerMode = false;

 static unsigned long lastServerCheck = 0;
 static unsigned long lastClientActivity = 0;
 static unsigned long lastCleanupTime = 0;
     
 // TCP sunucu
 #define TCP_SERVER_PORT 80
 static WiFiServer server(TCP_SERVER_PORT);
 
 // WiFi istemcileri
 #define MAX_CLIENTS 3
 static WiFiClient clients[MAX_CLIENTS];
     
 // Bağlı istemci sayacı
 static int clientCount = 0;
     
 // Son istemci bağlantı zamanı
 static unsigned long lastClientConnectTime = 0;
     
 // İleri bildirimler
 static void updateAPMode();
 static void updateSTAMode();  
 static void updateConnecting();
  
 // Public fonksiyonlar - static OLMAYACAK
 void ProcessJsonCommand(ArduinoJson::JsonDocument& doc, WiFiClient& client, int clientIndex);
 void SendSensorData(WiFiClient& client);
 void SendSettingsData(WiFiClient& client);
 void SendProfileData(WiFiClient& client);
 void SendAlarmData(WiFiClient& client);
 void SendAllProfilesData(WiFiClient& client);
 void SendSystemStatus(WiFiClient& client);
 bool HandleSettingsUpdate(ArduinoJson::JsonDocument& doc);
 
 bool WifiInit() {
      // WiFi başlatma
      WiFi.mode(WIFI_OFF);
      delay(100);
      
      // MAC adresini görüntüle (debug için)
      uint8_t mac[6];
      WiFi.macAddress(mac);
      LOG_INFO(TAG, "MAC Adresi: %02X:%02X:%02X:%02X:%02X:%02X", 
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      
      LOG_INFO(TAG, "WiFi modülü başlatıldı");
      return true;
 }
    
 void WifiUpdate() {
      // WiFi durumuna göre işlemler
      switch (currentWifiStatus) {
          case WIFI_STATUS_AP_MODE:
              updateAPMode();
              break;
              
          case WIFI_STATUS_STA_MODE:
              updateSTAMode();
              break;
              
          case WIFI_STATUS_CONNECTING:
              updateConnecting();
              break;
              
          default:
              // WiFi kapalı, bir şey yapma
              break;
      }
 }

 // EKLEME: Soket temizleme - WifiModule.cpp içine eklenecek
void cleanupInactiveSockets() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            // İstemci bağlı mı kontrol et
            if (!clients[i].connected()) {
                clients[i].stop();
                LOG_INFO(TAG, "İstemci #%d bağlantısı kapatıldı (temizleme)", i + 1);
                clientCount = (clientCount > 0) ? clientCount - 1 : 0;
            }
            // Uzun süredir etkinlik yok mu kontrol et
            else if (currentTime - lastClientActivity > 60000) { // 1 dakika
                // İstemciye ping gönder
                bool pingSuccess = false;
                try {
                    StaticJsonDocument<64> pingDoc;
                    pingDoc["cmd"] = "ping_check";
                    
                    String pingJson;
                    serializeJson(pingDoc, pingJson);
                    
                    // Ping göndermeyi dene
                    pingSuccess = clients[i].println(pingJson);
                    
                    if (!pingSuccess) {
                        clients[i].stop();
                        LOG_INFO(TAG, "İstemci #%d bağlantısı kapatıldı (ping başarısız)", i + 1);
                        clientCount = (clientCount > 0) ? clientCount - 1 : 0;
                    }
                } catch (...) {
                    // Herhangi bir hata durumunda bağlantıyı kapat
                    clients[i].stop();
                    LOG_INFO(TAG, "İstemci #%d bağlantısı kapatıldı (hata)", i + 1);
                    clientCount = (clientCount > 0) ? clientCount - 1 : 0;
                }
            }
        }
    }
}
    
void updateAPMode() {
    unsigned long currentTime = millis();
    
    // Periyodik soket temizleme
    if (currentTime - lastCleanupTime > 10000) { // 10 saniyede bir
        lastCleanupTime = currentTime;
        cleanupInactiveSockets();
    }
    
    // Herhangi bir istemci etkinliği olduğunda zamanı güncelle
    if (clientCount > 0) {
        lastClientActivity = currentTime;
    }
    
    // Uzun süredir (5 dakika) hiçbir istemci bağlanmamışsa sunucuyu yeniden başlat
    if (currentTime - lastServerCheck > 30000) { // Her 30 saniyede bir kontrol et
        lastServerCheck = currentTime;
        
        if (clientCount == 0 && currentTime - lastClientActivity > 300000) { // 5 dakika
            server.close();
            server.begin();
            LOG_INFO(TAG, "TCP sunucu yeniden başlatıldı (istemci aktivitesi yok)");
        }
    }
     
    // AP modunda istemci bağlantısı kontrolü
    if (server.hasClient()) {
        // Yeni bir istemci bağlandı
        WiFiClient newClient = server.available();
        
        // Boş bir istemci slotu bul
        bool clientConnected = false;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i] || !clients[i].connected()) {
                // Önceki istemci varsa kapatın
                if (clients[i]) clients[i].stop();
                
                // Yeni istemciyi bu slota ata
                clients[i] = newClient;
                LOG_INFO(TAG, "Yeni istemci #%d bağlandı", i + 1);
                clientCount++;
                clientConnected = true;
                lastClientConnectTime = currentTime;
                break;
            }
        }
        
        // Tüm slotlar doluysa yeni bağlantıyı reddet
        if (!clientConnected) {
            LOG_WARNING(TAG, "İstemci reddedildi - maksimum bağlantı sayısına ulaşıldı (%d)", MAX_CLIENTS);
            newClient.stop();
        }
    }
    
    // Mevcut istemcileri kontrol et
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i].connected()) {
            // İstemciden veri var mı kontrol et
            if (clients[i].available()) {
                // Veriyi oku ve işle
                String data = "";
                unsigned long startTime = millis();
                
                // İYİLEŞTİRME: Veri okuma mantığı geliştirildi
                // En fazla 2000ms (2 saniye) bekle veya tam satır oku
                while ((millis() - startTime < 2000) && clients[i].available()) {
                    char c = clients[i].read();
                    data += c;
                    
                    // Satır sonu kontrolü - hem \r\n hem de \n için kontrol
                    if (c == '\n') {
                        break;
                    }
                    
                    // TCP paketleri arasında kısa bir gecikme
                    delay(2);
                }
                
                // Veriyi temizle - \r ve \n karakterlerini düzgün şekilde ele al
                data.trim();
                // Olası \r karakterlerini temizle (CR+LF durumlarında)
                if (data.endsWith("\r")) {
                    data = data.substring(0, data.length() - 1);
                }
                
                LOG_DEBUG(TAG, "İstemci #%d'den veri alındı (%d byte): %s", 
                        i + 1, data.length(), data.c_str());
                
                if (data.length() > 0) {
                    // JSON verisi olarak ayrıştır - daha büyük bellek kullan
                    DynamicJsonDocument doc(2048);
                    DeserializationError error = deserializeJson(doc, data);
                    
                    if (error == DeserializationError::Ok) {
                        // JSON verisi başarıyla ayrıştırıldı, işle
                        ProcessJsonCommand(doc, clients[i], i);
                    } else {
                        LOG_ERROR(TAG, "JSON ayrıştırma hatası: %s", error.c_str());
                        // Hata mesajını istemciye gönder
                        clients[i].println("{\"error\":\"JSON parsing error\",\"details\":\"" + String(error.c_str()) + "\"}");
                    }
                }
            }
        } else if (clients[i]) {
            // İstemci bağlantısı kopmuş
            clients[i].stop();
            LOG_INFO(TAG, "İstemci #%d bağlantısı kesildi", i + 1);
            clientCount = (clientCount > 0) ? clientCount - 1 : 0;
        }
    }
}
    
 void updateSTAMode() {
     // STA modunda sunucu istemcilerini kontrol et
     if (WiFi.status() != WL_CONNECTED) {
         // WiFi bağlantısı kopmuş
         LOG_WARNING(TAG, "WiFi bağlantısı kesildi, yeniden bağlanmaya çalışılıyor...");
         currentWifiStatus = WIFI_STATUS_CONNECTING;
         return;
     }
     
     // Sunucu istemcilerini kontrol et - AP moduyla aynı
     updateAPMode();
 }
    
 void updateConnecting() {
     // Bağlantı durumunu kontrol et
     if (WiFi.status() == WL_CONNECTED) {
         // Bağlantı başarılı
         LOG_INFO(TAG, "WiFi bağlantısı kuruldu: %s", WiFi.localIP().toString().c_str());
         currentWifiStatus = WIFI_STATUS_STA_MODE;
         server.begin();
     } else {
         // Bağlantı henüz kurulmadı
         static unsigned long lastConnectCheck = 0;
         unsigned long currentTime = millis();
         
         if (currentTime - lastConnectCheck > 5000) {
             lastConnectCheck = currentTime;
             LOG_INFO(TAG, "WiFi bağlantısı bekleniyor...");
         }
         
         // Uzun süre bağlantı kurulamazsa vazgeç
         static unsigned long connectStartTime = 0;
         if (connectStartTime == 0) {
             connectStartTime = currentTime;
         } else if (currentTime - connectStartTime > 30000) {
             // 30 saniye boyunca bağlantı kurulamadı
             LOG_ERROR(TAG, "WiFi bağlantısı kurulamadı, vazgeçiliyor");
             WiFi.disconnect();
             currentWifiStatus = WIFI_STATUS_OFF;
             connectStartTime = 0;
         }
     }
 }
    
 bool WifiStartAP() {
    // AP modunu başlat
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Önceki sunucuyu durdur ve kaynakları temizle
    server.close();
    delay(100);
    
    if (WiFi.softAP(SYSTEM_NAME, DEFAULT_AP_PASSWORD)) {
        LOG_INFO(TAG, "AP modu başlatıldı: %s", SYSTEM_NAME);
        LOG_INFO(TAG, "AP IP adresi: %s", WiFi.softAPIP().toString().c_str());
        
        // Sunucuyu başlat
        server.begin();
        LOG_INFO(TAG, "TCP sunucu port %d üzerinde başlatıldı", TCP_SERVER_PORT);
        currentWifiStatus = WIFI_STATUS_AP_MODE;
        return true;
    } else {
        LOG_ERROR(TAG, "AP modu başlatılamadı!");
        currentWifiStatus = WIFI_STATUS_OFF;
        return false;
    }
}
    
 bool WifiStartSTA(const char* ssid, const char* password) {
     // STA modunu başlat (ağa bağlan)
     WiFi.mode(WIFI_STA);
     delay(100);
     
     LOG_INFO(TAG, "WiFi ağına bağlanılıyor: %s", ssid);
     WiFi.begin(ssid, password);
     
     currentWifiStatus = WIFI_STATUS_CONNECTING;
     return true;
 }
    
 bool WifiStop() {
     // WiFi'ı kapat
     for (int i = 0; i < MAX_CLIENTS; i++) {
         if (clients[i]) {
             clients[i].stop();
         }
     }
     
     server.close();
     WiFi.disconnect(true);
     WiFi.mode(WIFI_OFF);
     delay(100);
     
     currentWifiStatus = WIFI_STATUS_OFF;
     clientCount = 0;
     LOG_INFO(TAG, "WiFi kapatıldı");
     return true;
 }
    
 WifiStatus WifiGetStatus() {
     return currentWifiStatus;
 }
    
 String WifiGetIP() {
     if (currentWifiStatus == WIFI_STATUS_AP_MODE) {
         return WiFi.softAPIP().toString();
     } else if (currentWifiStatus == WIFI_STATUS_STA_MODE) {
         return WiFi.localIP().toString();
     } else {
         return "0.0.0.0";
     }
 }
    
 int WifiGetClientCount() {
     return clientCount;
 }
    
 bool WifiSetAPCredentials(const char* ssid, const char* password) {
     // AP modunda SSID ve şifre değiştirme (yeniden başlatma gerektirir)
     if (currentWifiStatus == WIFI_STATUS_AP_MODE) {
         WiFi.softAPdisconnect(true);
         delay(100);
         
         if (WiFi.softAP(ssid, password)) {
             LOG_INFO(TAG, "AP kimlik bilgileri güncellendi: %s", ssid);
             return true;
         } else {
             LOG_ERROR(TAG, "AP kimlik bilgileri güncellenemedi!");
             // Varsayılan kimlik bilgileriyle tekrar dene
             if (WiFi.softAP(SYSTEM_NAME, DEFAULT_AP_PASSWORD)) {
                 return false;
             } else {
                 return false;
             }
         }
     }
     return false;
 }
 
 // Belirli bir istemciye veri gönderme
bool WifiSendData(const String& data, WiFiClient& targetClient) {
    // Belirli bir istemciye veri gönder
    if (targetClient && targetClient.connected()) {
        // Veriyi gönder - satır sonu karakteri ekleyerek (CR+LF)
        targetClient.println(data);
        targetClient.flush(); // Tüm verinin gönderildiğinden emin ol
        
        // Debug log girdisi ekleyin
        if (data.length() < 200) {
            // Kısa mesajların tamamını göster
            LOG_DEBUG(TAG, "Veri gönderildi: %s", data.c_str());
        } else {
            // Uzun mesajlar için ilk 100 karakteri göster
            LOG_DEBUG(TAG, "Veri gönderildi (ilk 100 karakter): %s...", data.substring(0, 100).c_str());
        }
        
        return true;
    } else {
        LOG_WARNING(TAG, "Veri gönderilemedi: İstemci bağlı değil");
        return false;
    }
}
 
 // Tüm istemcilere veri gönderme
 bool WifiSendDataToAll(const String& data) {
     bool success = false;
     // Bu fonksiyonun uygulanması için clients[] dizisine global erişim gerekiyor
     // Bu örnekte, her istemciye veri göndermeyi deneriz
     for (int i = 0; i < MAX_CLIENTS; i++) {
         if (clients[i] && clients[i].connected()) {
             clients[i].println(data);
             success = true;
         }
     }
     if (success) {
         LOG_DEBUG(TAG, "Veri tüm istemcilere gönderildi: %s", data.c_str());
     } else {
         LOG_WARNING(TAG, "Veri gönderilemedi: Hiçbir istemci bağlı değil");
     }
     return success;
 }
 
 // Geriye uyumluluk için eski fonksiyon (tüm istemcilere gönderir)
 bool WifiSendData(const String& data) {
     return WifiSendDataToAll(data);
 }
    
 String WifiReceiveData() {
     // Tüm istemcilerden veri okuma denenebilir, ilk veri bulunan istemciden alınır
     for (int i = 0; i < MAX_CLIENTS; i++) {
         if (clients[i] && clients[i].connected() && clients[i].available()) {
             String data = clients[i].readStringUntil('\n');
             LOG_DEBUG(TAG, "İstemci #%d'den veri alındı: %s", i + 1, data.c_str());
             return data;
         }
     }
     return "";
 }
    
 bool WifiIsClientConnected() {
     for (int i = 0; i < MAX_CLIENTS; i++) {
         if (clients[i] && clients[i].connected()) {
             return true;
         }
     }
     return false;
 }
    
 void ProcessJsonCommand(ArduinoJson::JsonDocument& doc, WiFiClient& client, int clientIndex) {
     if (!doc.containsKey("cmd")) {
         // Geçersiz komut hatası - istemciye bildir
         StaticJsonDocument<128> responseDoc;
         responseDoc["status"] = "error";
         responseDoc["message"] = "Invalid command format: 'cmd' field is required";
         
         String responseJson;
         serializeJson(responseDoc, responseJson);
         WifiSendData(responseJson, client);
         
         LOG_ERROR(TAG, "Geçersiz komut formatı: 'cmd' alanı yok");
         return;
     }
     
     String cmd = doc["cmd"];
     LOG_INFO(TAG, "Komut alındı [İstemci #%d]: %s", clientIndex + 1, cmd.c_str());
     
     // Yanıt için JSON dokümanı oluştur
     StaticJsonDocument<256> responseDoc;
     responseDoc["cmd"] = cmd; // Yanıtta hangi komuta cevap verildiğini belirt
     bool success = true;
     String message = "OK";
     
     // Komut işleme mantığı
     if (cmd == "get_sensor_data") {
         SendSensorData(client);
     }
     else if (cmd == "get_settings") {
         SendSettingsData(client);
     }
     else if (cmd == "get_profile") {
         SendProfileData(client);
     }
     else if (cmd == "get_alarm_data") {
         SendAlarmData(client);
     }
     else if (cmd == "get_all_profiles") {
         SendAllProfilesData(client);
     }
     else if (cmd == "get_system_status") {
         SendSystemStatus(client);
     }
     else if (cmd == "update_settings") {
         success = HandleSettingsUpdate(doc);
         if (!success) message = "Failed to update settings";
         
         // Ayarları tüm istemcilere bildir
         if (success) {
             SendSettingsData(client);
         }
     }
     else if (cmd == "ping") {
    // Ping komutuna basit bir başarı yanıtı
    responseDoc["status"] = "success";
    responseDoc["message"] = "pong";
    
    String responseJson;
    serializeJson(responseDoc, responseJson);
    WifiSendData(responseJson, client);
}
     else if (cmd == "start_incubation") {
         if (doc.containsKey("profile_type")) {
             int profileType = doc["profile_type"];
             
             if (profileType >= PROFILE_CHICKEN && profileType <= PROFILE_PHEASANT) {
                 success = ProfileStartIncubation(static_cast<ProfileType>(profileType));
                 if (!success) message = "Failed to start incubation";
                 
                 // Değişikliği tüm istemcilere bildir
                 if (success) {
                     SendSettingsData(client);
                     // Önemli değişiklikler tüm istemcilere bildirilmeli
                     for (int i = 0; i < MAX_CLIENTS; i++) {
                         if (i != clientIndex && clients[i] && clients[i].connected()) {
                             SendSettingsData(clients[i]);
                         }
                     }
                 }
             } else {
                 success = false;
                 message = "Invalid profile type";
             }
         } else {
             success = false;
             message = "Missing profile_type parameter";
         }
     }
     else if (cmd == "stop_incubation") {
         success = ProfileEndIncubation();
         if (!success) message = "Failed to stop incubation";
         
         // Değişikliği tüm istemcilere bildir
         if (success) {
             SendSettingsData(client);
             // Önemli değişiklikler tüm istemcilere bildirilmeli
             for (int i = 0; i < MAX_CLIENTS; i++) {
                 if (i != clientIndex && clients[i] && clients[i].connected()) {
                     SendSettingsData(clients[i]);
                 }
             }
         }
     }
     else if (cmd == "set_targets") {
         if (doc.containsKey("temp") && doc.containsKey("humidity")) {
             float temp = doc["temp"];
             float humidity = doc["humidity"];
             
             ControlSetTargets(temp, humidity);
             
             IncubationSettings settings = LoadSettings();
             settings.targetTemp = temp;
             settings.targetHumidity = humidity;
             success = SaveSettings(settings);
             if (!success) message = "Failed to save settings";
             
             // Değişikliği tüm istemcilere bildir
             if (success) {
                 SendSettingsData(client);
                 // Önemli değişiklikler tüm istemcilere bildirilmeli
                 for (int i = 0; i < MAX_CLIENTS; i++) {
                     if (i != clientIndex && clients[i] && clients[i].connected()) {
                         SendSettingsData(clients[i]);
                     }
                 }
             }
         } else {
             success = false;
             message = "Missing temperature or humidity parameters";
         }
     }
     else if (cmd == "set_motor") {
         if (doc.containsKey("duration") && doc.containsKey("interval")) {
             unsigned long duration = doc["duration"];
             unsigned long interval = doc["interval"];
             
             ControlSetMotorSettings(duration, interval);
             
             // Tüm istemcilere bildir
             SendSettingsData(client);
             for (int i = 0; i < MAX_CLIENTS; i++) {
                 if (i != clientIndex && clients[i] && clients[i].connected()) {
                     SendSettingsData(clients[i]);
                 }
             }
         } else {
             success = false;
             message = "Missing duration or interval parameters";
         }
     }
     else if (cmd == "control_motor") {
         if (doc.containsKey("state")) {
             bool state = doc["state"];
             success = ControlManualMotor(state);
             if (!success) message = "Failed to control motor";
             
             // Motor durumunu tüm istemcilere bildir
             if (success) {
                 SendSettingsData(client);
                 for (int i = 0; i < MAX_CLIENTS; i++) {
                     if (i != clientIndex && clients[i] && clients[i].connected()) {
                         SendSettingsData(clients[i]);
                     }
                 }
             }
         } else {
             success = false;
             message = "Missing state parameter";
         }
     }
     else if (cmd == "set_wifi") {
         if (doc.containsKey("mode")) {
             String mode = doc["mode"];
             
             if (mode == "ap") {
                 WifiStop();
                 success = WifiStartAP();
                 if (!success) message = "Failed to start AP mode";
             } 
             else if (mode == "sta" && 
                      doc.containsKey("ssid") && 
                      doc.containsKey("password")) {
                 String ssid = doc["ssid"];
                 String password = doc["password"];
                 
                 WifiStop();
                 success = WifiStartSTA(ssid.c_str(), password.c_str());
                 if (!success) message = "Failed to connect to network";
             }
             else if (mode == "off") {
                 success = WifiStop();
                 if (!success) message = "Failed to stop WiFi";
             } else {
                 success = false;
                 message = "Invalid mode or missing parameters";
             }
         } else {
             success = false;
             message = "Missing mode parameter";
         }
     }
     else if (cmd == "sync_time") {
         if (currentWifiStatus == WIFI_STATUS_STA_MODE) {
             success = SyncTimeWithNTP();
             if (!success) message = "Failed to sync time with NTP server";
         } else {
             success = false;
             message = "WiFi not in STA mode, NTP sync requires internet connection";
         }
     }
     else {
         success = false;
         message = "Unknown command: " + cmd;
         LOG_WARNING(TAG, "Bilinmeyen komut: %s", cmd.c_str());
     }
     
     // Yanıt oluştur ve gönder (komut işleme tamamlandıktan sonra)
     if (cmd != "get_sensor_data" && cmd != "get_settings" && cmd != "get_profile" && 
         cmd != "get_alarm_data" && cmd != "get_all_profiles" && cmd != "get_system_status") {
         responseDoc["status"] = success ? "success" : "error";
         responseDoc["message"] = message;
         
         String responseJson;
         serializeJson(responseDoc, responseJson);
         WifiSendData(responseJson, client);
     }
 }
 
 bool HandleSettingsUpdate(ArduinoJson::JsonDocument& doc) {
     IncubationSettings settings = LoadSettings();
     bool updated = false;
     
     if (doc.containsKey("target_temp")) {
         settings.targetTemp = doc["target_temp"];
         updated = true;
     }
     
     if (doc.containsKey("target_humidity")) {
         settings.targetHumidity = doc["target_humidity"];
         updated = true;
     }
     
     if (doc.containsKey("motor_enabled")) {
         settings.motorEnabled = doc["motor_enabled"];
         updated = true;
     }
     
     if (doc.containsKey("pid_kp") && doc.containsKey("pid_ki") && doc.containsKey("pid_kd")) {
         settings.pidKp = doc["pid_kp"];
         settings.pidKi = doc["pid_ki"];
         settings.pidKd = doc["pid_kd"];
         
         ControlSetPidParams(settings.pidKp, settings.pidKi, settings.pidKd);
         updated = true;
     }
     
     // Alarm ayarları için
     if (doc.containsKey("alarm")) {
         JsonObject alarmObj = doc["alarm"];
         AlarmThresholds thresholds = AlarmGetThresholds();
         
         if (alarmObj.containsKey("enabled")) {
             thresholds.alarmEnabled = alarmObj["enabled"];
         }
         
         if (alarmObj.containsKey("high_temp")) {
             thresholds.highTempThreshold = alarmObj["high_temp"];
         }
         
         if (alarmObj.containsKey("low_temp")) {
             thresholds.lowTempThreshold = alarmObj["low_temp"];
         }
         
         if (alarmObj.containsKey("high_hum")) {
             thresholds.highHumThreshold = alarmObj["high_hum"];
         }
         
         if (alarmObj.containsKey("low_hum")) {
             thresholds.lowHumThreshold = alarmObj["low_hum"];
         }
         
         if (alarmObj.containsKey("temp_diff")) {
             thresholds.tempDiffThreshold = alarmObj["temp_diff"];
         }
         
         if (alarmObj.containsKey("hum_diff")) {
             thresholds.humDiffThreshold = alarmObj["hum_diff"];
         }
         
         AlarmSetThresholds(thresholds);
         updated = true;
     }
     
     if (updated) {
         SaveSettings(settings);
         ControlSetTargets(settings.targetTemp, settings.targetHumidity);
         return true;
     }
     
     return false;
 }
 
 void SendSensorData(WiFiClient& client) {
    // Sensör verilerini JSON formatında hazırla ve gönder
    SensorData sensorData = GetSensorData();
    RelayState relayState = ControlGetRelayState();
    
    // Basit ve tutarlı alanlar kullan
    StaticJsonDocument<512> doc;
    doc["type"] = "sensor_data";
    doc["temp"] = sensorData.avgTemperature;
    doc["humidity"] = sensorData.avgHumidity;
    doc["temp1"] = sensorData.temperature1;
    doc["hum1"] = sensorData.humidity1;
    doc["temp2"] = sensorData.temperature2;
    doc["hum2"] = sensorData.humidity2;
    doc["heater"] = relayState.heater;
    doc["humidifier"] = relayState.humidifier;
    doc["motor"] = relayState.motor;
    doc["motorRemaining"] = GetMotorCountdown();
    
    // PID ve gün bilgileri
    doc["pidOutput"] = ControlGetPidOutput();
    
    // Mevcut kuluçka gününü ekle
    IncubationSettings settings = LoadSettings();
    if (settings.startTime > 0) {
        int currentDay = GetIncubationDay(settings.startTime);
        doc["day"] = currentDay;
    } else {
        doc["day"] = 0;
    }
    
    String json;
    serializeJson(doc, json);
    
    client.println(json);
    LOG_DEBUG(TAG, "Sensör verisi gönderildi");
}
    
 void SendSettingsData(WiFiClient& client) {
     // Sistem ayarlarını JSON formatında hazırla ve gönder
     IncubationSettings settings = LoadSettings();
     MotorSettings motorSettings = LoadMotorSettings();
     RelayState relayState = ControlGetRelayState();
     
     StaticJsonDocument<512> doc;
     
     // Genel ayarlar
     doc["type"] = "settings";
     doc["profile_type"] = settings.type;
     doc["target_temp"] = settings.targetTemp;
     doc["target_humidity"] = settings.targetHumidity;
     doc["total_days"] = settings.totalDays;
     doc["start_time"] = settings.startTime;
     doc["motor_enabled"] = settings.motorEnabled;
     
     // Mevcut gün
     doc["current_day"] = (settings.startTime > 0) ? 
         GetIncubationDay(settings.startTime) : 0;
     
     // Motor ayarları
     JsonObject motor = doc.createNestedObject("motor");
     motor["duration"] = motorSettings.duration;
     motor["interval"] = motorSettings.interval;
     
     // PID ayarları
     JsonObject pid = doc.createNestedObject("pid");
     pid["kp"] = settings.pidKp;
     pid["ki"] = settings.pidKi;
     pid["kd"] = settings.pidKd;
     
     // Röle durumları
     JsonObject relay = doc.createNestedObject("relay");
     relay["heater"] = relayState.heater;
     relay["humidifier"] = relayState.humidifier;
     relay["motor"] = relayState.motor;
     
     String json;
     serializeJson(doc, json);
     WifiSendData(json, client);
 }
    
 void SendProfileData(WiFiClient& client) {
    // Aktif profil verilerini JSON formatında hazırla ve gönder
    Profile currentProfile = ProfileGetCurrent();
    
    StaticJsonDocument<512> doc;
    doc["type"] = "profile";
    doc["profile_type"] = static_cast<int>(currentProfile.type);
    doc["name"] = currentProfile.name;
    doc["total_days"] = currentProfile.totalDays;
    
    // Profil aşamalarını ekle
    JsonArray stages = doc.createNestedArray("stages");
    
    for (int i = 0; i < currentProfile.stageCount; i++) {
        JsonObject stage = stages.createNestedObject();
        stage["temperature"] = currentProfile.stages[i].temperature;
        stage["humidity"] = currentProfile.stages[i].humidity;
        stage["motor_active"] = currentProfile.stages[i].motorActive;
        stage["start_day"] = currentProfile.stages[i].startDay;
        stage["end_day"] = currentProfile.stages[i].endDay;
    }
    
    String json;
    serializeJson(doc, json);
    WifiSendData(json, client);
}
   
void SendAlarmData(WiFiClient& client) {
    AlarmStatus currentAlarm = AlarmGetStatus();
    AlarmThresholds thresholds = AlarmGetThresholds();
    
    StaticJsonDocument<512> doc;
    
    doc["type"] = "alarm_data";
    doc["alarm_active"] = currentAlarm.active;
    doc["alarm_type"] = static_cast<int>(currentAlarm.type);
    doc["alarm_message"] = currentAlarm.message;
    doc["alarm_start_time"] = currentAlarm.startTime;
    
    // Eşik değerlerini ekle
    JsonObject thresholdObj = doc.createNestedObject("thresholds");
    thresholdObj["enabled"] = thresholds.alarmEnabled;
    thresholdObj["high_temp"] = thresholds.highTempThreshold;
    thresholdObj["low_temp"] = thresholds.lowTempThreshold;
    thresholdObj["high_hum"] = thresholds.highHumThreshold;
    thresholdObj["low_hum"] = thresholds.lowHumThreshold;
    thresholdObj["temp_diff"] = thresholds.tempDiffThreshold;
    thresholdObj["hum_diff"] = thresholds.humDiffThreshold;
    
    // Alarm geçmişini ekle
    JsonArray history = doc.createNestedArray("history");
    
    int historyCount = AlarmGetHistoryCount();
    for (int i = 0; i < historyCount && i < 5; i++) {
        AlarmStatus alarmHistory = AlarmGetHistory(i);
        
        JsonObject alarm = history.createNestedObject();
        alarm["type"] = static_cast<int>(alarmHistory.type);
        alarm["message"] = alarmHistory.message;
        alarm["time"] = alarmHistory.startTime;
    }
    
    String json;
    serializeJson(doc, json);
    WifiSendData(json, client);
}
  
void SendAllProfilesData(WiFiClient& client) {
    StaticJsonDocument<2048> doc;
    doc["type"] = "all_profiles";
    
    JsonArray profilesArray = doc.createNestedArray("profiles");
    
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (!predefinedProfiles[i].name.isEmpty()) {
            JsonObject profileObj = profilesArray.createNestedObject();
            profileObj["type"] = static_cast<int>(predefinedProfiles[i].type);
            profileObj["name"] = predefinedProfiles[i].name;
            profileObj["total_days"] = predefinedProfiles[i].totalDays;
            
            JsonArray stagesArray = profileObj.createNestedArray("stages");
            
            for (int j = 0; j < predefinedProfiles[i].stageCount; j++) {
                JsonObject stageObj = stagesArray.createNestedObject();
                stageObj["temperature"] = predefinedProfiles[i].stages[j].temperature;
                stageObj["humidity"] = predefinedProfiles[i].stages[j].humidity;
                stageObj["motor_active"] = predefinedProfiles[i].stages[j].motorActive;
                stageObj["start_day"] = predefinedProfiles[i].stages[j].startDay;
                stageObj["end_day"] = predefinedProfiles[i].stages[j].endDay;
            }
        }
    }
    
    String json;
    serializeJson(doc, json);
    WifiSendData(json, client);
}
  
void SendSystemStatus(WiFiClient& client) {
    // Sistem durumunu JSON formatında hazırla ve gönder
    StaticJsonDocument<512> doc;
    
    doc["type"] = "system_status";
    doc["system_name"] = SYSTEM_NAME;
    doc["version"] = APP_VERSION;
    
    // WiFi durumu
    doc["wifi_status"] = static_cast<int>(currentWifiStatus);
    doc["wifi_ip"] = WifiGetIP();
    doc["client_count"] = clientCount;
    
    // Sensör durumu
    SensorData sensorData = GetSensorData();
    doc["sensor1_valid"] = sensorData.sensor1Valid;
    doc["sensor2_valid"] = sensorData.sensor2Valid;
    
    // İstatistikler
    doc["free_heap"] = ESP.getFreeHeap();
    if (psramFound()) {
        doc["free_psram"] = ESP.getFreePsram();
    }
    
    // Zaman bilgisi
    TimeInfo currentTime = GetCurrentTime();
    JsonObject time = doc.createNestedObject("time");
    time["year"] = currentTime.year;
    time["month"] = currentTime.month;
    time["day"] = currentTime.day;
    time["hour"] = currentTime.hour;
    time["minute"] = currentTime.minute;
    time["second"] = currentTime.second;
    time["timestamp"] = currentTime.timestamp;
    
    String json;
    serializeJson(doc, json);
    WifiSendData(json, client);
}
  
void WifiEnterLowPowerMode() {
    // Güç tasarrufu ayarları
    esp_wifi_set_ps((wifi_ps_type_t)WIFI_PS_MAX_MODEM);
    
    if (currentWifiStatus == WIFI_STATUS_AP_MODE) {
        // AP modunda DTIM aralığını artırarak güç tasarrufu sağla
        esp_wifi_set_inactive_time(WIFI_IF_AP, 300); // 300 saniye
    }
    
    lowPowerMode = true;
    LOG_INFO(TAG, "WiFi düşük güç moduna geçti");
}
  
void WifiExitLowPowerMode() {
    // Normal güç moduna dönüş
    esp_wifi_set_ps((wifi_ps_type_t)WIFI_PS_NONE);
    
    if (currentWifiStatus == WIFI_STATUS_AP_MODE) {
        // AP modunda DTIM aralığını normale döndür
        esp_wifi_set_inactive_time(WIFI_IF_AP, 60); // 60 saniye
    }
    
    lowPowerMode = false;
    LOG_INFO(TAG, "WiFi normal güç moduna döndü");
}
  
bool WifiIsInLowPowerMode() {
    return lowPowerMode;
}