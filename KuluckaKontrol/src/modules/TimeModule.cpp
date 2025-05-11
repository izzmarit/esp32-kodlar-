/**
 * Kuluçka Makinesi Kontrol Sistemi - Zaman Modülü
 */

 #include "TimeModule.h"
 #include <Wire.h>
 #include <RTClib.h>
 #include <WiFi.h>
 #include <time.h>
 #include "DataModule.h"
 
 static const char* TAG = "TIME";

 // External variables
 extern PowerOutage powerOutages[MAX_POWER_OUTAGES];
 extern int powerOutageCount;
 
 // RTC nesnesi
 static RTC_DS3231 rtc;
 
 // Zaman bilgisi
 static TimeInfo currentTime;
 
 // Son güç açılış zamanı
 static uint32_t lastPowerOnTime = 0;
 
 // TimeInfo yapısını Unix zamanına çeviren yardımcı fonksiyon
 TimeInfo UnixTimeToTimeInfo(uint32_t unixTime) {
     TimeInfo time;
     
     // Unix zamanından DateTime nesnesine dönüştür
     DateTime dt(unixTime);
     
     time.year = dt.year();
     time.month = dt.month();
     time.day = dt.day();
     time.hour = dt.hour();
     time.minute = dt.minute();
     time.second = dt.second();
     time.timestamp = unixTime;
     
     return time;
 }
 
 bool TimeInit() {
     // I2C'yi başlatma burada yapılmıyor, ana programda yapılıyor
     
     // RTC'yi başlat
     if (!rtc.begin()) {
         LOG_ERROR(TAG, "DS3231 RTC bulunamadı! I2C bağlantısını kontrol edin.");
         return false;
     }
     
     // RTC'nin çalışıp çalışmadığını kontrol et
     if (rtc.lostPower()) {
         LOG_WARNING(TAG, "RTC güç kaybetti, zaman ayarlanıyor!");
         // RTC zamanını Arduino'nun derleme zamanına ayarla
         rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
     }
     
     // Mevcut zamanı oku
     TimeUpdate();
     
     // Güç kesintilerini kontrol et
     lastPowerOnTime = GetUnixTime();
     CheckPowerOutage();
     
     LOG_INFO(TAG, "Zaman modülü başlatıldı, şu anki zaman: %04d-%02d-%02d %02d:%02d:%02d", 
             currentTime.year, currentTime.month, currentTime.day, 
             currentTime.hour, currentTime.minute, currentTime.second);
     
     return true;
 }
 
 void TimeUpdate() {
     DateTime now = rtc.now();
     
     currentTime.year = now.year();
     currentTime.month = now.month();
     currentTime.day = now.day();
     currentTime.hour = now.hour();
     currentTime.minute = now.minute();
     currentTime.second = now.second();
     currentTime.timestamp = now.unixtime();
 }
 
 TimeInfo GetCurrentTime() {
     TimeUpdate();
     return currentTime;
 }
 
 uint32_t GetUnixTime() {
     TimeUpdate();
     return currentTime.timestamp;
 }
 
 bool SetTime(TimeInfo timeInfo) {
     rtc.adjust(DateTime(timeInfo.year, timeInfo.month, timeInfo.day, 
                        timeInfo.hour, timeInfo.minute, timeInfo.second));
     TimeUpdate();
     
     LOG_INFO(TAG, "Zaman ayarlandı: %04d-%02d-%02d %02d:%02d:%02d", 
             timeInfo.year, timeInfo.month, timeInfo.day, 
             timeInfo.hour, timeInfo.minute, timeInfo.second);
     
     return true;
 }
 
 bool SetUnixTime(uint32_t unixTime) {
     rtc.adjust(DateTime(unixTime));
     TimeUpdate();
     
     LOG_INFO(TAG, "Unix zamanı ayarlandı: %u", unixTime);
     return true;
 }
 
 void CheckPowerOutage() {
     // Son kaydedilen güç zamanını al
     uint32_t lastStoredTime = GetLastPowerOnTime();
     
     // Mevcut zamanı al
     uint32_t currentUnixTime = GetUnixTime();
     
     // Eğer son kaydedilen zaman varsa ve şimdiki zamandan küçükse
     if (lastStoredTime > 0 && lastStoredTime < currentUnixTime) {
         // 5 saniyeden uzun süre fark varsa, elektrik kesintisi olarak değerlendir
         uint32_t diff = currentUnixTime - lastStoredTime;
         if (diff > 5) {
             // Elektrik kesintisi kaydı oluştur
             LogPowerOutage(lastStoredTime, currentUnixTime);
             
             LOG_WARNING(TAG, "Elektrik kesintisi algılandı: %u saniye", diff);
         } else {
             LOG_INFO(TAG, "Kısa elektrik kesintisi veya yeniden başlatma: %u saniye", diff);
         }
     }
     
     // Yeni güç açılış zamanını kaydet
     SetPowerOnTime(currentUnixTime);
 }
 
 int GetPowerOutageCount() {
     return powerOutageCount; // Global değişken
 }
 
 PowerOutage GetPowerOutage(int index) {
     if (index >= 0 && index < powerOutageCount) {
         return powerOutages[index]; // Global değişken
     }
     
     // Geçersiz indeks
     PowerOutage emptyOutage = {0, 0, 0};
     return emptyOutage;
 }
 
 uint32_t GetIncubationDay(uint32_t startTime) {
     uint32_t currentTime = GetUnixTime();
     
     if (startTime == 0 || startTime > currentTime) {
         return 0;
     }
     
     // Günün tamamlanması için 24 saat geçmesi gerekir
     // Unix timestamp saniye cinsinden olduğu için 86400 (24*60*60) saniye = 1 gün
     uint32_t elapsedDays = (currentTime - startTime) / 86400;
     return elapsedDays + 1; // İlk günü 1 olarak say
 }
 
 uint32_t CalculateEndTime(uint32_t startTime, int durationDays) {
     if (startTime == 0 || durationDays <= 0) {
         return 0;
     }
     
     // Kuluçka süresi sonunda tahmin edilen bitiş zamanı
     return startTime + (durationDays * 86400);
 }
 
 // NTP sunucuları
 const char* ntpServer1 = "pool.ntp.org";
 const char* ntpServer2 = "time.nist.gov";
 const char* ntpServer3 = "europe.pool.ntp.org"; // Avrupa için
 
 bool SyncTimeWithNTP() {
     if (WiFi.status() != WL_CONNECTED) {
         LOG_ERROR(TAG, "NTP senkronizasyonu için WiFi bağlantısı gerekli");
         return false;
     }
     
     LOG_INFO(TAG, "NTP üzerinden zaman senkronize ediliyor...");
     
     // Maximum 3 NTP server
     configTime(0, 0, ntpServer1, ntpServer2, ntpServer3);
     
     // Daha anlaşılır bir minimum tarih kullanın
     const time_t MINIMUM_VALID_TIME = 1577836800; // 2020-01-01 00:00:00 UTC
     
     // Senkronizasyon için bekle
     time_t nowSecs = time(nullptr);
     int retries = 0;
     
     while (nowSecs < MINIMUM_VALID_TIME && retries < 10) {
         delay(500);
         nowSecs = time(nullptr);
         retries++;
     }
     
     if (nowSecs < MINIMUM_VALID_TIME) {
         LOG_ERROR(TAG, "NTP senkronizasyonu başarısız oldu");
         return false;
     }
     
     struct tm timeinfo;
     gmtime_r(&nowSecs, &timeinfo);
     
     // RTC'yi ayarla
     rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, 
                          timeinfo.tm_mday, timeinfo.tm_hour, 
                          timeinfo.tm_min, timeinfo.tm_sec));
     
     // Mevcut zamanı güncelle
     TimeUpdate();
     
     LOG_INFO(TAG, "RTC saat NTP ile senkronize edildi: %04d-%02d-%02d %02d:%02d:%02d", 
             currentTime.year, currentTime.month, currentTime.day, 
             currentTime.hour, currentTime.minute, currentTime.second);
     
     return true;
 }