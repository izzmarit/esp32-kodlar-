/**
 * Kuluçka Makinesi Kontrol Sistemi - Ekran Modülü
 */

 #include "DisplayModule.h"
 #include <Adafruit_GFX.h>
 #include <Adafruit_ST7735.h>
 #include <SPI.h>
 #include "TimeModule.h"
 #include "DataModule.h"
 #include "ProfileModule.h"
 #include "ControlModule.h"
 #include "WifiModule.h"    // WiFi durumu gösterimi için
 #include "../config/pins.h"
  
 static const char* TAG = "DISPLAY";
  
 // ST7735 ekran nesnesi
 extern Adafruit_ST7735 tft;
  
 // Ekran başlatılma durumu
 extern bool notificationActive;
 extern unsigned long notificationStartTime;
 extern String activeNotificationMessage;
 extern DisplayMode previousDisplayMode;
 extern bool displayInitialized;
  
 // Global değişkenler bölümüne ekleyin
 static IncubationSettings cachedSettings;
 static unsigned long lastSettingsUpdate = 0;
 static const unsigned long SETTINGS_UPDATE_INTERVAL = 5000; // 5 saniye
  
 // Mevcut ekran modu
 static DisplayMode currentDisplayMode = DISPLAY_WELCOME;
  
 // Son güncelleme zamanı
 static unsigned long lastDisplayUpdate = 0;
  
 // Son değerleri saklayan değişkenler
 static struct {
      float temperature;
      float humidity;
      bool heaterOn;
      bool humidifierOn;
      bool motorOn;
      int day;
      int totalDays;
      int motorCountdown;
      int hour;
      int minute;
      String profileName;
 } lastValues = {0};
  
 // İlk çağrı olup olmadığını kontrol etmek için bayrak
 bool isFirstMainDisplay = true;
  
 bool DisplayInit() {
      // SPI başlatma zaten main.cpp'de yapılıyor
      // TFT nesnesi de main.cpp'de doğru şekilde oluşturulmuş
      
      // Ekran başlatma
      tft.initR(INITR_BLACKTAB);
      tft.setRotation(1);
      
      // SPI hızını artır
      SPI.setFrequency(40000000); // 40 MHz
      
      tft.fillScreen(ST7735_BLACK);
      
      // Arka ışığı aç
      pinMode(TFT_LED, OUTPUT);
      digitalWrite(TFT_LED, HIGH);
      
      // Test gösterimi
      tft.setCursor(10, 10);
      tft.setTextSize(1);
      tft.setTextColor(ST7735_WHITE);
      tft.println("Ekran baslatiliyor...");
      
      displayInitialized = true;
      LOG_INFO(TAG, "Ekran modülü başlatıldı");
      return true;
 }
  
 bool DisplayIsInitialized() {
      return displayInitialized;
 }
  
 void DisplayUpdate() {
      // Mevcut ekran moduna göre ekranı güncelle
      unsigned long currentTime = millis();
      
      // Her 500ms'de bir ekranı güncelle (pil tasarrufu için)
      if (currentTime - lastDisplayUpdate < 500) {
          return;
      }
      
      lastDisplayUpdate = currentTime;
      
      // Bildirim kontrolü
      if (notificationActive) {
          // Buton basıldı mı veya zaman aşımı oldu mu?
          if (digitalRead(JOYSTICK_BTN) == LOW || currentTime - notificationStartTime > 5000) {
              notificationActive = false;
              delay(50); // Debounce
              // Önceki ekran moduna geri dön
              DisplaySetMode(previousDisplayMode);
              LOG_DEBUG(TAG, "Bildirim kapatıldı");
          }
      }
 }
  
 void DisplaySetMode(DisplayMode mode) {
      // Önceki ekran modunu kaydet - bildirim için
      previousDisplayMode = currentDisplayMode;
      
      // Yeni modu ayarla
      currentDisplayMode = mode;
      
      // Yeni moda geçildiğinde ekranı temizle
      if (mode != DISPLAY_MAIN) {
          tft.fillScreen(ST7735_BLACK);
      }
      
      // Ana ekrana geçildiğinde, ilk çağrı olarak işaretle
      if (mode == DISPLAY_MAIN) {
          isFirstMainDisplay = true;
      }
      
      LOG_DEBUG(TAG, "Ekran modu değiştirildi: %d", mode);
 }
  
 DisplayMode DisplayGetMode() {
      return currentDisplayMode;
 }
  
 void DisplayShowWelcome() {
      DisplaySetMode(DISPLAY_WELCOME);
      
      tft.fillScreen(ST7735_BLACK);
      
      // Daha küçük yazı ile sığacak şekilde ayarla
      tft.setCursor(20, 30);
      tft.setTextSize(2);
      tft.setTextColor(ST7735_WHITE);
      tft.println("KULUCKA");
      tft.setCursor(20, 50);
      tft.println("KONTROL");
      
      tft.setCursor(30, 80);
      tft.setTextSize(1);
      tft.setTextColor(ST7735_GREEN);
      tft.print("v");
      tft.println(APP_VERSION);
      
      LOG_INFO(TAG, "Hoş geldiniz ekranı gösteriliyor");
 }
  
 void DisplayShowMain(float temp, float humidity, int day, int totalDays, 
    bool heaterOn, bool humidifierOn, bool motorOn) {
    
    // İlk çağrıysa veya mod değiştiyse tüm ekranı oluştur
    if (isFirstMainDisplay || DisplayGetMode() != DISPLAY_MAIN) {
        DisplaySetMode(DISPLAY_MAIN);
        isFirstMainDisplay = false;
        
        // Ekranı temizle
        tft.fillScreen(ST7735_BLACK);
        
        // ÜST BİLGİ ÇUBUĞU
        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
        
        // Tarih ve Saat
        TimeInfo now = GetCurrentTime();
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", now.hour, now.minute);
        tft.setCursor(5, 5);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_WHITE);
        tft.print(timeStr);
        
        char dateStr[9]; // Format: DD.MM
        sprintf(dateStr, "%02d.%02d", now.day, now.month);
        tft.setCursor((tft.width() - 30) / 2, 5);
        tft.print(dateStr);
        
        // WiFi durumu - metin yerine simge kullan
        if (WifiGetStatus() != WIFI_STATUS_OFF) {
            // Sabit pozisyonda küçük WiFi simgesi çiz
            int iconX = tft.width() - 45;
            int iconY = 4;
            
            // Basit WiFi simgesi çizimi
            tft.fillRect(iconX, iconY+6, 2, 2, ST7735_WHITE); // Merkez nokta
            tft.drawCircle(iconX, iconY+6, 3, ST7735_WHITE);  // İç daire
            tft.drawCircle(iconX, iconY+6, 6, ST7735_WHITE);  // Dış daire
        }
        
        // Menü bilgisi (sabit konum)
        tft.setCursor(tft.width() - 35, 5);
        tft.print("Menu>");
        
        // BÖLÜCÜ ÇİZGİLER (statik)
        tft.drawFastHLine(0, 16, tft.width(), ST7735_DARKGREY);
        
        // Ekranın kalan kısmını 4 eşit bölüme böl
        int mainAreaTop = 17;
        int mainAreaHeight = tft.height() - mainAreaTop;
        int sectionHeight = mainAreaHeight / 2;
        
        // Bölüm çizgileri
        tft.drawFastVLine(tft.width() / 2, mainAreaTop, tft.height() - mainAreaTop, ST7735_DARKGREY);
        tft.drawFastHLine(0, mainAreaTop + sectionHeight, tft.width(), ST7735_DARKGREY);
        
        // Bölüm başlıkları - daha küçük metin boyutu
        // Sıcaklık bölümü başlığı
        tft.setCursor(5, mainAreaTop + 3);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_WHITE);
        tft.print("SICAKLIK");
        
        // Nem bölümü başlığı
        tft.setCursor(tft.width() / 2 + 5, mainAreaTop + 3);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_WHITE);
        tft.print("NEM");

        // YENİ: İlk çizimde durum ikonlarını göster
drawHeaterIndicator(heaterOn);
drawHumidifierIndicator(humidifierOn);
        
        // Motor bölümü başlığı
        tft.setCursor(5, mainAreaTop + sectionHeight + 3);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_WHITE);
        tft.print("MOTOR");
        
        // Kuluçka bölümü başlığı
        tft.setCursor(tft.width() / 2 + 5, mainAreaTop + sectionHeight + 3);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_WHITE);
        tft.print("KULUCKA");
        
        // Son değerleri güncelle - ekranın tamamen yeniden çizilmesi için
        lastValues.temperature = -100; // Geçersiz bir değer
        lastValues.humidity = -100;    // Geçersiz bir değer
        lastValues.heaterOn = !heaterOn; // Zorunlu güncelleme
        lastValues.humidifierOn = !humidifierOn; // Zorunlu güncelleme
        lastValues.motorOn = !motorOn; // Zorunlu güncelleme
        lastValues.day = -1;          // Geçersiz bir değer
        lastValues.totalDays = -1;    // Geçersiz bir değer
        lastValues.motorCountdown = -1; // Geçersiz bir değer
        lastValues.hour = -1;         // Geçersiz bir değer
        lastValues.minute = -1;       // Geçersiz bir değer
        lastValues.profileName = "";  // Zorunlu güncelleme
    }
    
    // Saat ve tarih değişimini kontrol et ve güncelle
    TimeInfo now = GetCurrentTime();
    if (now.hour != lastValues.hour || now.minute != lastValues.minute) {
        // Saat alanını sil
        tft.fillRect(5, 5, 40, 10, ST7735_NAVY);
        
        // Yeni saati çiz
        tft.setCursor(5, 5);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_WHITE);
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", now.hour, now.minute);
        tft.print(timeStr);
        
        // Son değerleri güncelle
        lastValues.hour = now.hour;
        lastValues.minute = now.minute;
    }
    
    // Ekranın kalan kısmını 4 eşit bölüme böl
    int mainAreaTop = 17;
    int mainAreaHeight = tft.height() - mainAreaTop;
    int sectionHeight = mainAreaHeight / 2;
    int sectionWidth = tft.width() / 2;
    
    // Sıcaklık değişimini kontrol et ve güncelle
    if (abs(temp - lastValues.temperature) >= 0.1 || heaterOn != lastValues.heaterOn || isFirstMainDisplay) {
    // Sıcaklık alanını sil
    tft.fillRect(5, mainAreaTop + 15, tft.width()/2 - 10, 30, ST7735_BLACK);
    
    // Yeni sıcaklığı çiz - Daha küçük boyut ve doğru hizalama
    char tempDisplay[8];
    sprintf(tempDisplay, "%.1fC", temp);
    
    // Metin genişliğini hesapla ve ortala
    int tempWidth = strlen(tempDisplay) * 10; // Size 2 için yaklaşık genişlik
    int tempX = 5 + ((tft.width()/2 - 10) - tempWidth) / 2;
    
    tft.setCursor(tempX, mainAreaTop + 20);
    tft.setTextSize(2); // Daha küçük boyut (önce 3 idi)
    tft.setTextColor(ST7735_RED);
    tft.print(temp, 1);
    tft.print("C");
    
    // DEĞIŞTIRILDI: Eski durum gösterge kodu kaldırıldı, yeni fonksiyon çağrıldı
    drawHeaterIndicator(heaterOn);
    
    // Hedef sıcaklık etiketi
    tft.setCursor(5, mainAreaTop + 45);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_YELLOW);
    tft.print("Hedef: ");
    tft.print(settings.targetTemp, 1);
    tft.print("C");
    
    lastValues.temperature = temp;
    lastValues.heaterOn = heaterOn;
}

// Nem değişimini kontrol et ve güncelle
if (abs(humidity - lastValues.humidity) >= 0.5 || humidifierOn != lastValues.humidifierOn || isFirstMainDisplay) {
    // Nem alanını sil
    tft.fillRect(tft.width() / 2 + 5, mainAreaTop + 15, tft.width()/2 - 10, 30, ST7735_BLACK);
    
    // Yeni nemi çiz
    char humDisplay[8];
    sprintf(humDisplay, "%.0f%%", humidity);
    
    // Metin genişliğini hesapla ve ortala
    int humWidth = strlen(humDisplay) * 10;
    int humX = tft.width() / 2 + 5 + ((tft.width()/2 - 10) - humWidth) / 2;
    
    tft.setCursor(humX, mainAreaTop + 20);
    tft.setTextSize(2);
    tft.setTextColor(ST7735_BLUE);
    tft.print(humidity, 0);
    tft.print("%");
    
    // DEĞIŞTIRILDI: Eski durum gösterge kodu kaldırıldı, yeni fonksiyon çağrıldı
    drawHumidifierIndicator(humidifierOn);
    
    // Hedef nem etiketi
    tft.setCursor(tft.width() / 2 + 5, mainAreaTop + 45);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_CYAN);
    tft.print("Hedef: ");
    tft.print(settings.targetHumidity, 0);
    tft.print("%");
    
    lastValues.humidity = humidity;
    lastValues.humidifierOn = humidifierOn;
}
    
    // Motor durumunu kontrol et ve güncelle
    if (motorOn != lastValues.motorOn || isFirstMainDisplay) {
        // Motor durumu alanını sil
        tft.fillRect(5, mainAreaTop + sectionHeight + 20, tft.width()/2 - 10, 20, ST7735_BLACK);
        
        // Motor geri sayım değerini göster - büyütülmüş yazı tipi boyutu
        int motorCountdown = GetMotorCountdown();
        
        // Geri sayımı ortala
        char countStr[10];
        sprintf(countStr, "%d dk", motorCountdown);
        int countWidth = strlen(countStr) * 12; // Size 2 için yaklaşık genişlik
        int countX = 5 + ((tft.width()/2 - 10) - countWidth) / 2;
        
        tft.setCursor(countX, mainAreaTop + sectionHeight + 20);
        tft.setTextSize(2); // Yazı boyutunu 2'ye yükseltiyoruz (önce 1 idi)
        tft.setTextColor(ST7735_YELLOW);
        tft.print(motorCountdown);
        tft.print(" dk");
        
        // Son değeri güncelle
        lastValues.motorOn = motorOn;
    }
    
    // Motor durumu
    tft.setCursor(10, mainAreaTop + sectionHeight + 45); // Pozisyonu aşağı aldık
    tft.setTextSize(1); // Normal boyut
    tft.setTextColor(motorOn ? ST7735_GREEN : ST7735_LIGHTGREY);
    tft.print(motorOn ? "ACIK" : "KAPALI");
    
    // Güncel motor geri sayımını al ve güncelle
    int motorCountdown = GetMotorCountdown();
    if (motorCountdown != lastValues.motorCountdown || isFirstMainDisplay) {
        // Eski konumu sil (eğer geri sayım değişmişse)
        tft.fillRect(5, mainAreaTop + sectionHeight + 20, tft.width()/2 - 10, 24, ST7735_BLACK);
        
        // Geri sayımı ortala
        char countStr[10];
        sprintf(countStr, "%d dk", motorCountdown);
        int countWidth = strlen(countStr) * 12; // Size 2 için yaklaşık genişlik
        int countX = 5 + ((tft.width()/2 - 10) - countWidth) / 2;
        
        // Yeni geri sayımı çiz - büyütülmüş font
        tft.setCursor(countX, mainAreaTop + sectionHeight + 20);
        tft.setTextSize(2); // Büyük boyut
        tft.setTextColor(ST7735_YELLOW);
        tft.print(motorCountdown);
        tft.print(" dk");
        
        // Son değeri güncelle
        lastValues.motorCountdown = motorCountdown;
    }
    
    // Profil adını ve gün bilgisini kontrol et ve güncelle
    String profileName = ProfileGetTypeName((ProfileType)settings.type);
if (day != lastValues.day || totalDays != lastValues.totalDays || 
    profileName != lastValues.profileName || isFirstMainDisplay) {
    
    // Kuluçka bilgisi alanını sil
    tft.fillRect(tft.width() / 2 + 5, mainAreaTop + sectionHeight + 20, 
                tft.width() / 2 - 10, 50, ST7735_BLACK);
    
    // Gün bilgisini çiz ve ortala - büyütülmüş boyut
    char dayStr[10];
    sprintf(dayStr, "%d/%d", day, totalDays);
    int dayWidth = strlen(dayStr) * 12; // Size 2 için yaklaşık genişlik
    int dayX = tft.width() / 2 + 5 + ((tft.width()/2 - 10) - dayWidth) / 2;
    
    tft.setCursor(dayX, mainAreaTop + sectionHeight + 20);
    tft.setTextSize(2); // Büyütülmüş boyut (önce 1 idi)
    tft.setTextColor(ST7735_YELLOW);
    tft.print(day);
    tft.print("/");
    tft.print(totalDays);
    
    // Kuluçka tipini çiz
    tft.setCursor(tft.width() / 2 + 5, mainAreaTop + sectionHeight + 45); // Pozisyonu aşağı kaydırdık
    tft.setTextSize(1);
    tft.setTextColor(ST7735_GREEN);
    tft.print("Tip: ");
    tft.println(profileName);
    
    // Son değerleri güncelle
    lastValues.day = day;
    lastValues.totalDays = totalDays;
    lastValues.profileName = profileName;
}
    
    LOG_DEBUG(TAG, "Ana ekran güncellendi");
}
  
 void DisplayShowMenu(int selectedItem, const char* items[], int itemCount) {
     DisplaySetMode(DISPLAY_MENU);
     
     // Ekranı temizle
     tft.fillScreen(ST7735_BLACK);
     
     // Başlık çubuğu
     tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
     tft.setCursor(5, 5);
     tft.setTextSize(1);
     tft.setTextColor(ST7735_WHITE);
     tft.println("MENU");
     
     // Alt bilgi çubuğu için yükseklik
     int footerHeight = 15;
     
     // Alt bilgi çubuğu
     tft.fillRect(0, tft.height() - footerHeight, tft.width(), footerHeight, ST7735_NAVY);
     tft.setCursor(5, tft.height() - footerHeight + 2);
     tft.setTextColor(ST7735_CYAN);
     tft.println("^v:Secim  >:Sec  <:Geri");
     
     // Menü öğelerini göster - Alt navigasyon satırına kadar
     int displayCount = min(itemCount, 6); // Ekrana sığacak şekilde max 6 öğe göster
     int startItem = max(0, selectedItem - 2); // Seçili öğeyi ortada tutmak için
     
     // Görüntülenen öğelerin startItem'dan fazla olması durumunda ayarla
     if (startItem + displayCount > itemCount) {
         startItem = max(0, itemCount - displayCount);
     }
     
     // Menü öğelerine ayrılabilecek alan hesapla
     int menuAreaHeight = tft.height() - 16 - footerHeight; // Üst başlık ve alt navigasyon alanı çıkarıldı
     int itemHeight = menuAreaHeight / displayCount;
     
     for (int i = 0; i < displayCount; i++) {
         int itemIndex = startItem + i;
         if (itemIndex >= itemCount) break;
         
         bool isSelected = (itemIndex == selectedItem);
         
         // Menü öğesinin y pozisyonu
         int itemY = 16 + i * itemHeight;
         
         // Seçili öğe için çerçeve çiz
         if (isSelected) {
             tft.drawRect(2, itemY, tft.width() - 4, itemHeight, ST7735_YELLOW);
         }
         
         tft.setCursor(5, itemY + (itemHeight - 8) / 2); // Metin y-merkezleme
         tft.setTextColor(isSelected ? ST7735_YELLOW : ST7735_WHITE);
         tft.println(items[itemIndex]);
     }
     
     LOG_DEBUG(TAG, "Menü gösteriliyor, seçili öğe: %d", selectedItem);
 }
  
// Isıtıcı durum göstergesini çiz
void drawHeaterIndicator(bool heaterOn) {
    // Isıtıcı durum göstergesi - başlığın yanında
    int mainAreaTop = 17;
    int indicatorX = 5 + 48 + 5; // "SICAKLIK" yazısı (~48px) + biraz boşluk
    int indicatorY = mainAreaTop + 3; // Başlıkla aynı hizada
    
    if (heaterOn) {
        tft.fillCircle(indicatorX, indicatorY, 4, ST7735_RED);
    } else {
        tft.fillCircle(indicatorX, indicatorY, 4, ST7735_BLACK);
    }
}

// Nemlendirici durum göstergesini çiz
void drawHumidifierIndicator(bool humidifierOn) {
    // Nemlendirici durum göstergesi - başlığın yanında
    int mainAreaTop = 17;
    int indicatorX = (tft.width() / 2 + 5) + 24 + 5; // "NEM" yazısı (~24px) + biraz boşluk 
    int indicatorY = mainAreaTop + 3; // Başlıkla aynı hizada
    
    if (humidifierOn) {
        tft.fillCircle(indicatorX, indicatorY, 4, ST7735_BLUE);
    } else {
        tft.fillCircle(indicatorX, indicatorY, 4, ST7735_BLACK);
    }
}

 void DisplayShowGraph(const char* title, float* data, uint32_t* timestamps, int dataCount, int graphType) {
     DisplaySetMode(DISPLAY_GRAPH);
     
     // Ekranı temizle
     tft.fillScreen(ST7735_BLACK);
     
     // Başlık
     tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
     tft.setCursor(5, 5);
     tft.setTextSize(1);
     tft.setTextColor(ST7735_WHITE);
     tft.println(title);
     
     if (dataCount <= 0) {
         tft.setCursor(10, tft.height() / 2);
         tft.setTextColor(ST7735_RED);
         tft.println("Veri yok!");
         LOG_WARNING(TAG, "Grafik gösterilemiyor: Veri yok");
         return;
     }
     
     // Alt bilgi çubuğu için yükseklik
     int footerHeight = 15;
     
     // Alt bilgi çubuğu
     tft.fillRect(0, tft.height() - footerHeight, tft.width(), footerHeight, ST7735_NAVY);
     tft.setCursor(5, tft.height() - footerHeight + 2);
     tft.setTextColor(ST7735_YELLOW);
     tft.println("Geri donmek icin tiklayiniz");
     
     // Grafik alanını belirle
     int graphX = 30;  // Sol kenar boşluğu
     int graphY = 25;  // Üst kenar boşluğu
     int graphW = tft.width() - 40;  // Genişlik
     int graphH = tft.height() - 50 - footerHeight; // Yükseklik (alt navigasyon çubuğuna kadar)
     
     // Eksenleri çiz
     tft.drawFastHLine(graphX, graphY + graphH, graphW, ST7735_WHITE); // X ekseni
     tft.drawFastVLine(graphX, graphY, graphH, ST7735_WHITE);          // Y ekseni
     
     // Min/Max değerleri belirle
     float minVal = data[0];
     float maxVal = data[0];
     
     for (int i = 1; i < dataCount; i++) {
         if (data[i] < minVal) minVal = data[i];
         if (data[i] > maxVal) maxVal = data[i];
     }
     
     // Değer aralığını biraz genişlet
     float range = maxVal - minVal;
     if (range < 1.0) range = 1.0;
     
     minVal -= range * 0.1;
     maxVal += range * 0.1;
     
     // Y ekseninde değer gösterimi
     tft.setTextSize(1);
     tft.setCursor(5, graphY);
     tft.setTextColor(ST7735_CYAN);
     tft.print(maxVal, 1);
     
     tft.setCursor(5, graphY + graphH - 10);
     tft.print(minVal, 1);
     
     // Veri noktalarını çiz
     for (int i = 0; i < dataCount - 1; i++) {
         int x1 = graphX + i * graphW / (dataCount - 1);
         int y1 = graphY + graphH - (data[i] - minVal) * graphH / (maxVal - minVal);
         
         int x2 = graphX + (i + 1) * graphW / (dataCount - 1);
         int y2 = graphY + graphH - (data[i + 1] - minVal) * graphH / (maxVal - minVal);
         
         // Renk seçimi (sıcaklık: kırmızı, nem: mavi)
         uint16_t lineColor = (graphType == 0) ? ST7735_RED : ST7735_BLUE;
         tft.drawLine(x1, y1, x2, y2, lineColor);
     }
     
     LOG_DEBUG(TAG, "Grafik gösteriliyor: %s (%d veri noktası)", 
              title, dataCount);
 }
  
 void DisplayShowError(const char* errorMessage) {
     DisplaySetMode(DISPLAY_ERROR);
     
     // Ekranı temizle
     tft.fillScreen(ST7735_BLACK);
     
     // Uyarı başlığı
     tft.fillRect(0, 0, tft.width(), 20, ST7735_RED);
     tft.setCursor(5, 5);
     tft.setTextSize(1);
     tft.setTextColor(ST7735_WHITE);
     tft.println("HATA!");
     
     // Hata mesajı
     tft.setCursor(5, 30);
     tft.setTextSize(1);
     tft.setTextColor(ST7735_YELLOW);
     
     // Uzun mesajları birden çok satıra böl
     String msg = String(errorMessage);
     int maxCharsPerLine = 21; // 21 karakter/satır (yaklaşık)
     
     for (int i = 0; i < msg.length(); i += maxCharsPerLine) {
         String line = msg.substring(i, min(i + maxCharsPerLine, (int)msg.length()));
         tft.println(line);
     }
     
     // Alt bilgi çubuğu
     tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
     tft.setCursor(5, tft.height() - 13);
     tft.setTextColor(ST7735_CYAN);
     tft.println("Devam etmek icin tiklayiniz");
     
     LOG_ERROR(TAG, "Hata ekranı gösteriliyor: %s", errorMessage);
 }
  
 void DisplaySetBacklight(bool state) {
     digitalWrite(TFT_LED, state ? HIGH : LOW);
     LOG_DEBUG(TAG, "Ekran arka ışığı: %s", state ? "AÇIK" : "KAPALI");
 }
  
 // Bildirim gösterme fonksiyonu
 void DisplayShowNotification(const String& message) {
     // Ekran başlatılmadıysa bir şey yapma
     if (!displayInitialized) {
         LOG_WARNING(TAG, "Bildirim gösterilemiyor: Ekran başlatılmamış");
         return;
     }
     
     // Mesajın çok uzun olmadığından emin ol
     String msg = message;
     if (msg.length() > 200) {
         msg = msg.substring(0, 197) + "...";
     }
     
     // Mevcut ekran modunu kaydet
     previousDisplayMode = currentDisplayMode;
     activeNotificationMessage = msg;
     
     // Bildirim durumunu başlat
     notificationActive = true;
     notificationStartTime = millis();
     
     // Ekranı temizle
     tft.fillScreen(ST7735_BLACK);
     
     // Başlık
     tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
     tft.setCursor((tft.width() - 50) / 2, 5);
     tft.setTextSize(1);
     tft.setTextColor(ST7735_WHITE);
     tft.println("BILDIRIM");
     
     // Mesaj
     tft.setCursor(10, 40);
     tft.setTextSize(1);
     tft.setTextColor(ST7735_YELLOW);
     
     // Uzun mesajları düzgün şekilde birden fazla satıra böl
     int maxCharsPerLine = 21; // 21 karakter/satır
     int yPos = 40;
     
     while (msg.length() > 0) {
         String line;
         if (msg.length() > maxCharsPerLine) {
             // Satır bölme için kelime sınırı bul
             int lastSpace = msg.substring(0, maxCharsPerLine).lastIndexOf(' ');
             
             if (lastSpace > 0) {
                 line = msg.substring(0, lastSpace);
                 msg = msg.substring(lastSpace + 1);
             } else {
                 // Boşluk bulunamazsa karakterleri böl
                 line = msg.substring(0, maxCharsPerLine);
                 msg = msg.substring(maxCharsPerLine);
             }
         } else {
             line = msg;
             msg = "";
         }
         
         tft.setCursor(10, yPos);
         tft.println(line);
         yPos += 10; // Satır yüksekliği
         
         // Ekran sınırını kontrol et
         if (yPos > tft.height() - 20) break;
     }
     
     // Alt bilgi çubuğu - sabit bir bölüm oluştur
     tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
     tft.setCursor(10, tft.height() - 13);
     tft.setTextColor(ST7735_CYAN);
     tft.println("Devam etmek icin tiklayin");
     
     LOG_INFO(TAG, "Bildirim gösteriliyor: %s", msg.c_str());
 }