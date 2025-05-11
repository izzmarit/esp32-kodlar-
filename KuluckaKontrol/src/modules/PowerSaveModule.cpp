/**
 * Kuluçka Makinesi Kontrol Sistemi - Güç Tasarrufu Modülü
 * 
 * Bu dosya, güç tasarrufu modülünün uygulamasını içerir.
 */

 #include "PowerSaveModule.h"
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 #include "SensorModule.h"
 #include "ControlModule.h"
 #include "WifiModule.h"
 #include "DisplayModule.h"
 #include "DataModule.h"
 #include <esp_sleep.h>
 #include <esp_wifi.h>
 #include <esp_pm.h>

 static const char* TAG = "POWER_SAVE";
 
 // Geçerli güç tasarrufu seviyesi
 PowerSaveLevel currentPowerSaveLevel = POWER_SAVE_DISABLED;
 
 // Ekran koruyucu durumu
 bool screenSaverEnabled = true;
 ScreenSaverMode screenSaverMode = SCREEN_SAVER_LOGO;
 unsigned long screenSaverTimeout = 180000; // 3 dakika (ms)
 static unsigned long powerSaveLastActivityTime = 0; 
 bool screenSaverActive = false;
 
 // Güç tasarrufu istatistikleri
 unsigned long totalSleepTime = 0;
 unsigned int sleepCount = 0;
 
 bool PowerSaveInit() {
   // Başlangıçta düşük güç modunu devre dışı bırak
   currentPowerSaveLevel = POWER_SAVE_DISABLED;
   
   // Ekran koruyucu için aktivite zamanını sıfırla
   powerSaveLastActivityTime = millis();
   
   Serial.println("Güç tasarrufu modülü başlatıldı");
   return true;
 }
 
 void PowerSaveUpdate() {
   unsigned long currentTime = millis();
   
   // Ekran koruyucu kontrolü
   if (screenSaverEnabled && !screenSaverActive) {
     if (currentTime - powerSaveLastActivityTime > screenSaverTimeout) {
       // Ekran koruyucuyu etkinleştir
       screenSaverActive = true;
       
       // Ekran koruyucu moduna göre ekranı güncelle
       switch (screenSaverMode) {
         case SCREEN_SAVER_LOGO:
           PowerSaveShowLogo();
           break;
         case SCREEN_SAVER_INFO:
           PowerSaveShowInfo();
           break;
         case SCREEN_SAVER_OFF:
           // Ekranı kapat
           DisplaySetBacklight(false);
           break;
         default:
           // Ekran koruyucu devre dışı - hiçbir şey yapma
           break;
       }
     }
   }
   
   // Güç tasarrufu seviyesine göre ek işlemler
   if (currentPowerSaveLevel == POWER_SAVE_MEDIUM || 
       currentPowerSaveLevel == POWER_SAVE_AGGRESSIVE) {
     
     // CPU frekansını azalt (isteğe bağlı)
     // setCpuFrequencyMhz(80); // 240MHz -> 80MHz
     
     // Eğer hiç sensör veya kontrol aktivitesi yoksa, kısa süreli uyku
     if (currentPowerSaveLevel == POWER_SAVE_AGGRESSIVE) {
       // Agresif güç tasarrufu modunda, daha sık derin uyku moduna gir
       // Not: Bu mod, sensör okuma sıklığını etkileyebilir
       static unsigned long lastDeepSleepCheck = 0;
       if (currentTime - lastDeepSleepCheck > 60000) { // 1 dakikada bir kontrol et
         lastDeepSleepCheck = currentTime;
         
         // Isıtıcı veya nemlendirici aktif değilse kısa süreli uyku
         RelayState relayState = ControlGetRelayState();
         if (!relayState.heater && !relayState.humidifier && !relayState.motor) {
           // Kısa süreli uyku (5 saniye)
           PowerSaveEnterLightSleep(5000);
         }
       }
     }
   }
 }
 
 void PowerSaveSetLevel(PowerSaveLevel level) {
   // Eğer seviye değiştiyse işlem yap
   if (level != currentPowerSaveLevel) {
     currentPowerSaveLevel = level;
     
     // Seviyeye göre güç tasarrufu önlemlerini uygula
     switch (level) {
       case POWER_SAVE_DISABLED:
         // Ekran arka ışığını aç
         DisplaySetBacklight(true);
         // CPU frekansını normal seviyeye getir
         setCpuFrequencyMhz(240); // Normal ESP32 frekansı
         break;
         
       case POWER_SAVE_LIGHT:
         // Temel güç tasarrufu - sadece ekran ve WiFi
         if (screenSaverActive) {
           DisplaySetBacklight(false);
         }
         // WiFi'yi düşük güç moduna al
         WifiEnterLowPowerMode();
         break;
         
       case POWER_SAVE_MEDIUM:
         // Orta seviye güç tasarrufu
         // WiFi'yi düşük güç moduna al
         WifiEnterLowPowerMode();
         // Ekran arka ışığını kapat
         DisplaySetBacklight(false);
         // CPU frekansını düşür
         setCpuFrequencyMhz(160); // 240MHz -> 160MHz
         break;
         
       case POWER_SAVE_AGGRESSIVE:
         // Agresif güç tasarrufu
         // WiFi'yi düşük güç moduna al
         WifiEnterLowPowerMode();
         // Ekran arka ışığını kapat
         DisplaySetBacklight(false);
         // CPU frekansını minimuma düşür
         setCpuFrequencyMhz(80); // 240MHz -> 80MHz
         break;
     }
     
     Serial.print("Güç tasarrufu seviyesi ayarlandı: ");
     Serial.println(level);
   }
 }
 
 PowerSaveLevel PowerSaveGetLevel() {
   return currentPowerSaveLevel;
 }
 
 void PowerSaveToggleScreenSaver(bool enabled) {
   screenSaverEnabled = enabled;
   
   if (!enabled && screenSaverActive) {
     // Ekran koruyucuyu devre dışı bırak ve ekranı normal duruma getir
     screenSaverActive = false;
     DisplaySetBacklight(true);
   }
 }
 
 void PowerSaveSetScreenSaverMode(ScreenSaverMode mode) {
   screenSaverMode = mode;
   
   // Eğer ekran koruyucu aktifse, yeni modu hemen uygula
   if (screenSaverActive) {
     switch (mode) {
       case SCREEN_SAVER_LOGO:
         PowerSaveShowLogo();
         break;
       case SCREEN_SAVER_INFO:
         PowerSaveShowInfo();
         break;
       case SCREEN_SAVER_OFF:
         DisplaySetBacklight(false);
         break;
       default:
         // Ekran koruyucu devre dışı - normal ekrana dön
         screenSaverActive = false;
         break;
     }
   }
 }
 
 ScreenSaverMode PowerSaveGetScreenSaverMode() {
   return screenSaverMode;
 }
 
 void PowerSaveSetScreenSaverTimeout(unsigned long timeoutMs) {
   screenSaverTimeout = timeoutMs;
 }
 
 bool PowerSaveIsScreenSaverActive() {
   return screenSaverActive;
 }
 
 void PowerSaveResetScreenSaverTimer() {
   powerSaveLastActivityTime = millis();
   
   // Eğer ekran koruyucu aktifse ve joystick hareketi algılandıysa
   if (screenSaverActive) {
     // Ekran koruyucudan çık
     screenSaverActive = false;
     
     // Ekran arka ışığını aç
     DisplaySetBacklight(true);
   }
 }
 
 void PowerSaveEnterLightSleep(unsigned long sleepTimeMs) {
   Serial.print("Kısa süreli uyku moduna giriliyor: ");
   Serial.print(sleepTimeMs);
   Serial.println(" ms");
   
   // Uyku istatistiklerini güncelle
   totalSleepTime += sleepTimeMs;
   sleepCount++;
   
   // Uyku süresini ayarla
   esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000); // Mikrosaniye cinsinden
   
   // Hafif uyku moduna gir (CPU durur, periferiler çalışmaya devam eder)
   esp_light_sleep_start();
   
   Serial.println("Kısa süreli uykudan uyandı!");
 }
 
 void PowerSaveShowLogo() {
   // Ekran koruyucu modunda logo göster
   if (!displayInitialized) {
     return; // Ekran başlatılmadıysa, hiçbir şey yapma
   }
   
   tft.fillScreen(ST7735_BLACK);
   
   // Logo merkezi - yatay olarak ekranın ortasında
   int centerX = tft.width() / 2;
   int centerY = tft.height() / 2;
   
   // MK-V5.0 yazısını ekranın ortasına yaz
   tft.setTextSize(3);
   tft.setTextColor(ST7735_YELLOW);
   
   // Yazı uzunluğunu hesapla ve merkeze yerleştir
   int textWidth = 6 * 3 * 6; // yaklaşık yazı uzunluğu
   tft.setCursor(centerX - textWidth / 2, centerY - 10);
   tft.print("MK-V5.0");
 }
 
 void PowerSaveShowInfo() {
   // Ekran koruyucu modunda temel bilgileri göster
   if (!displayInitialized) {
     return; // Ekran başlatılmadıysa, hiçbir şey yapma
   }
   
   tft.fillScreen(ST7735_BLACK);
   
   // Sensör verilerini al
   SensorData sensorData = GetSensorData();
   
   // Röle durumlarını al
   RelayState relayState = ControlGetRelayState();
   
   // Basit bilgileri göster
   tft.setTextSize(2);
   tft.setCursor(10, 10);
   tft.setTextColor(ST7735_RED);
   tft.print(sensorData.avgTemperature, 1);
   tft.print("C");
   
   tft.setCursor(10, 40);
   tft.setTextColor(ST7735_BLUE);
   tft.print(sensorData.avgHumidity, 1);
   tft.print("%");
   
   // Röle durumları
   tft.setTextSize(1);
   tft.setCursor(10, 70);
   tft.setTextColor(ST7735_WHITE);
   tft.print("Isitici: ");
   tft.println(relayState.heater ? "ACIK" : "KAPALI");
   
   tft.setCursor(10, 90);
   tft.print("Nemlendirici: ");
   tft.println(relayState.humidifier ? "ACIK" : "KAPALI");
   
   tft.setCursor(10, 110);
   tft.print("Motor: ");
   tft.println(relayState.motor ? "ACIK" : "KAPALI");
 }
 
 void PowerSaveWakeUp() {
   // Tüm düşük güç modu ayarlarını sıfırla
   
   // Ekranı aç
   DisplaySetBacklight(true);
   
   // Ekran koruyucuyu devre dışı bırak
   screenSaverActive = false;
   
   // Aktivite sayacını sıfırla
   powerSaveLastActivityTime = millis();
   
   // CPU frekansını normal seviyeye getir
   setCpuFrequencyMhz(240);
 }