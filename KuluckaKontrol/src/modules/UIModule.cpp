/**
 * Kuluçka Makinesi Kontrol Sistemi - Kullanıcı Arayüzü Modülü
 */

 #include "UIModule.h"
 #include "DisplayModule.h"
 #include "TimeModule.h"
 #include "SensorModule.h"
 #include "ControlModule.h"
 #include "DataModule.h"
 #include "ProfileModule.h"
 #include "AlarmModule.h"
 #include "PowerSaveModule.h"
 #include "WifiModule.h"
 #include "../config/pins.h"
 #include "esp_task_wdt.h" // Eklenen watchdog başlığı
    
 static const char* TAG = "UI";
   
 // Fonksiyon prototipleri
 bool handleMainMenu(JoystickDirection direction);
 bool handleGeneralSettingsMenu(JoystickDirection direction);
 bool handleTempHumidityMenu(JoystickDirection direction);
 bool handleMotorMenu(JoystickDirection direction);
 bool handleProfileMenu(JoystickDirection direction);
 bool handleGraphMenu(JoystickDirection direction);
 bool handlePowerSaveMenu(JoystickDirection direction);
 bool handleCalibrationMenu(JoystickDirection direction);
 bool handleTimeDateMenu(JoystickDirection direction);
 bool handleAlarmMenu(JoystickDirection direction);
 bool handleWifiMenu(JoystickDirection direction);
 bool handleBackupMenu(JoystickDirection direction);
 
   
 // Mevcut menü durumu
 static MenuState currentMenu = MENU_NONE;
   
 // Seçilen menü öğesi
 extern int menuSelection;
   
 // Profil menüsü sayfa kontrolü
 extern int profileMenuCurrentPage;
   
 // Joystick değişkenleri
 static int joystickCenterX = 2048;
 static int joystickCenterY = 2048;
 static unsigned long lastJoystickRead = 0;
 static unsigned long lastJoystickAction = 0;
 static JoystickDirection lastJoystickDirection = JOY_NONE;
 static bool lastButtonState = HIGH;
  
 static bool inTransition = false;
 static unsigned long transitionStartTime = 0;
 static const unsigned long TRANSITION_DURATION = 100; // ms

 // Değer düzenleme modu - extern olarak tanımlanmış, burada sadece başlatılıyor
 bool editMode = false;
 static int editCursor = 0; // İmleç pozisyonu (tarih/saat düzenleme için)
  
 // Menü zamanaşımı
 static unsigned long lastMenuActivity = 0;
 static const unsigned long MENU_TIMEOUT = 30000; // 30 saniye
    
 // Ana menü öğeleri
 static const char* mainMenuItems[] = {
     "Sicaklik ve Nem",
     "Motor Ayarlari",
     "Profil Secimi",
     "Grafikleri Goster",
     "Enerji Tasarruf Modu",
     "Genel Ayarlar"
 };
 static const int mainMenuItemCount = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);
   
 // Genel Ayarlar menü öğeleri
 static const char* generalSettingsMenuItems[] = {
     "Tarih ve Saat",
     "Alarm Ayarlari",
     "WiFi Ayarlari",
     "Kalibrasyon",
     "Yedekleme ve Geri Yukleme",
     "Sistem Bilgisi"
 };
 static const int generalSettingsMenuItemCount = sizeof(generalSettingsMenuItems) / sizeof(generalSettingsMenuItems[0]);
   
 bool UIInit() {
     // Joystick pin modları pins.h içinde tanımlanıyor
     menuSelection = 0;
     currentMenu = MENU_NONE;
     editMode = false;
     
     // Joystick kalibrasyonunu çalıştır
     CalibrateJoystick();
     
     LOG_INFO(TAG, "UI modülü başlatıldı");
     return true;
 }
   
 void CalibrateJoystick() {
     LOG_INFO(TAG, "===============================");
     LOG_INFO(TAG, "JOYSTICK KALIBRASYON BAŞLIYOR");
     LOG_INFO(TAG, "Joystick'i ortada tutun");
     LOG_INFO(TAG, "===============================");
     delay(1000);
     
     int xSum = 0, ySum = 0;
     const int samples = 20;
     
     for (int i = 0; i < samples; i++) {
         int xVal = analogRead(JOYSTICK_X);
         int yVal = analogRead(JOYSTICK_Y);
         xSum += xVal;
         ySum += yVal;
         LOG_INFO(TAG, "Kalibrasyon örnekleme #%d - X: %d, Y: %d", i+1, xVal, yVal);
         delay(50);
     }
     
     joystickCenterX = xSum / samples;
     joystickCenterY = ySum / samples;
     
     // Merkez değerleri limitlerini kontrol et
     if (joystickCenterX < 500 || joystickCenterX > 3500 || 
         joystickCenterY < 500 || joystickCenterY > 3500) {
         // Beklenmedik değer, varsayılan merkezi kullan
         joystickCenterX = 2048;
         joystickCenterY = 2048;
         LOG_WARNING(TAG, "Joystick kalibrasyon değerleri sınır dışı! Varsayılanlar kullanılıyor.");
     }
     
     LOG_INFO(TAG, "Joystick kalibrasyon tamamlandı - Merkez X: %d, Y: %d", joystickCenterX, joystickCenterY);
 }
   
// Tamamen yeni bir fonksiyon
void UIHandleTransition() {
    if (inTransition) {
        unsigned long currentTime = millis();
        if (currentTime - transitionStartTime >= TRANSITION_DURATION) {
            inTransition = false;
            // Mevcut ekranı yeniden çiz
            switch (currentMenu) {
                case MENU_NONE:
                    UIBackToMain();
                    break;
                case MENU_MAIN:
                    UIShowMainMenu();
                    break;
                // Diğer case ifadeleri...
            }
        }
    }
}

// Tamamen yeni bir fonksiyon
void UIShowSavedNotification(const char* setting) {
    char message[40];
    sprintf(message, "%s kaydedildi!", setting);
    DisplayShowNotification(message);
}

 void UIUpdate() {
    // Geçişleri işle - YENİ EKLENEN KOD
    UIHandleTransition();
     // Joystick okuması ve işlenmesi
     JoystickDirection direction = UIReadJoystick();
     
     if (direction != JOY_NONE) {
         // Joystick hareketi algılandı
         bool handled = UIHandleJoystick(direction);
         LOG_INFO(TAG, "Joystick hareketi işlendi: %s", handled ? "evet" : "hayır");
         
         // Güç tasarruf modülüne aktivite bildirimi
         PowerSaveResetScreenSaverTimer();
         
         // Menü zamanaşımını sıfırla
         lastMenuActivity = millis();
     }
     
     // Menü zamanaşımı kontrolü - yalnızca düzenleme modunda değilse
     if (!editMode && currentMenu != MENU_NONE && millis() - lastMenuActivity > MENU_TIMEOUT) {
         LOG_INFO(TAG, "Menü zaman aşımı, ana ekrana dönülüyor");
         UIBackToMain();
     }
     
     // Watchdog'u besle
     esp_task_wdt_reset();
 }
   
 MenuState UIGetCurrentMenu() {
     return currentMenu;
 }
   
 JoystickDirection UIReadJoystick() {
     unsigned long currentTime = millis();
     
     // Debounce kontrolü
     if (currentTime - lastJoystickRead < 50) {
         return JOY_NONE;
     }
     
     lastJoystickRead = currentTime;
     
     // Buton kontrolü
     bool buttonState = digitalRead(JOYSTICK_BTN) == LOW;
     
     // Buton basılırsa
     if (buttonState && !lastButtonState && 
         currentTime - lastJoystickAction > 200) {
         lastButtonState = true;
         lastJoystickAction = currentTime;
         LOG_INFO(TAG, "Joystick BUTON basıldı");
         return JOY_PRESS;
     }
     
     // Buton bırakıldığında durumu güncelle
     if (!buttonState) {
         lastButtonState = false;
     }
     
     // ADC değerlerini oku
     int xValue = analogRead(JOYSTICK_X);
     int yValue = analogRead(JOYSTICK_Y);
     
     // Ölü bölge
     const int deadZone = 500;
     
     int xDiff = xValue - joystickCenterX;
     int yDiff = yValue - joystickCenterY;
     
     // Yönü belirle
     JoystickDirection direction = JOY_NONE;
     
     if (abs(xDiff) > abs(yDiff) && abs(xDiff) > deadZone) {
         if (xDiff > deadZone) {
             direction = JOY_RIGHT;
         } else if (xDiff < -deadZone) {
             direction = JOY_LEFT;
         }
     } else if (abs(yDiff) > abs(xDiff) && abs(yDiff) > deadZone) {
         if (yDiff > deadZone) {
             direction = JOY_DOWN;
         } else if (yDiff < -deadZone) {
             direction = JOY_UP;
         }
     }
     
     // Yön değişimini veya tekrarlamayı kontrol et
     if (direction != JOY_NONE) {
         if (direction != lastJoystickDirection || 
             currentTime - lastJoystickAction > JOYSTICK_REPEAT_DELAY) {
             lastJoystickDirection = direction;
             lastJoystickAction = currentTime;
             return direction;
         }
     } else {
         // Merkezde ise durumu sıfırla
         if (abs(xDiff) < deadZone/2 && abs(yDiff) < deadZone/2) {
             lastJoystickDirection = JOY_NONE;
         }
     }
     
     return JOY_NONE;
 }
  
 bool UIHandleJoystick(JoystickDirection direction) {
     if (direction == JOY_NONE) {
         return false;
     }
     
     // Bildirim aktifse, herhangi bir joystick hareketi bildirimi kapatır
     if (notificationActive) {
         notificationActive = false;
         DisplaySetMode(previousDisplayMode);
         return true;
     }
     
     // Mevcut menü durumuna göre işlem yap
     switch (currentMenu) {
         case MENU_NONE:
             // Ana ekranda sağ tuşla ana menüye gir
             if (direction == JOY_RIGHT || direction == JOY_PRESS) {
                 UIShowMainMenu();
                 lastMenuActivity = millis(); // Menü zamanaşımını sıfırla
                 return true;
             }
             break;
             
         case MENU_MAIN:
             return handleMainMenu(direction);
             
         case MENU_GENERAL_SETTINGS:
             return handleGeneralSettingsMenu(direction);
             
         case MENU_TEMP_HUMIDITY:
             return handleTempHumidityMenu(direction);
             
         case MENU_MOTOR:
             return handleMotorMenu(direction);
             
         case MENU_PROFILE:
             return handleProfileMenu(direction);
             
         case MENU_GRAPH:
             return handleGraphMenu(direction);
             
         case MENU_POWER_SAVE:
             return handlePowerSaveMenu(direction);
             
         case MENU_CALIBRATION:
             return handleCalibrationMenu(direction);
             
         case MENU_TIME_DATE:
             return handleTimeDateMenu(direction);
             
         case MENU_ALARM:
             return handleAlarmMenu(direction);
             
         case MENU_WIFI:
             return handleWifiMenu(direction);
             
         case MENU_BACKUP:
             return handleBackupMenu(direction);
             
         default:
             LOG_WARNING(TAG, "Bilinmeyen menü durumu: %d", currentMenu);
             break;
     }
     
     return false;
 }
  
   // Grafik Menüsü İşleme
bool handleGraphMenu(JoystickDirection direction) {
    static const char* graphMenuItems[] = {
        "Sicaklik Grafigi",
        "Nem Grafigi",
        "Istatistikler",
        "Elektrik Kesintileri"
    };
    static const int itemCount = sizeof(graphMenuItems) / sizeof(graphMenuItems[0]);
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, graphMenuItems, itemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < itemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, graphMenuItems, itemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_MAIN);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS:
            // Seçilen grafiği göster
            switch (menuSelection) {
                case 0: // Sıcaklık grafiği
                    UIShowGraphSelection(0);
                    break;
                    
                case 1: // Nem grafiği
                    UIShowGraphSelection(1);
                    break;
                    
                case 2: // İstatistikler
                    showStatistics();
                    break;
                    
                case 3: // Elektrik kesintileri
                    UIShowPowerOutages();
                    break;
            }
            return true;
            
        default:
            return false;
    }
}

// Yedekleme Menüsü İşleme
bool handleBackupMenu(JoystickDirection direction) {
    static const char* backupMenuItems[] = {
        "Verileri Yedekle",
        "Yedekten Geri Yukle",
        "Tum Verileri Temizle"
    };
    static const int itemCount = sizeof(backupMenuItems) / sizeof(backupMenuItems[0]);
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, backupMenuItems, itemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < itemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, backupMenuItems, itemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_GENERAL_SETTINGS);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS:
            switch (menuSelection) {
                case 0: // Verileri Yedekle
                    {
                        // Ekranda işlem bilgisi göster
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("YEDEKLEME");
                        
                        tft.setCursor(5, 40);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Veriler yedekleniyor...");
                        
                        // Yedekleme işlemini gerçekleştir
                        bool success = BackupData();
                        
                        if (success) {
                            DisplayShowNotification("Veriler basariyla yedeklendi!");
                        } else {
                            DisplayShowNotification("Yedekleme hatasi!");
                        }
                        
                        UINavigateToMenu(MENU_BACKUP);
                    }
                    break;
                    
                case 1: // Yedekten Geri Yükle
                    {
                        // Onay mesajı göster
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("GERI YUKLEME");
                        
                        tft.setCursor(5, 40);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_RED);
                        tft.println("Mevcut veriler silinecek!");
                        
                        tft.setCursor(5, 55);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Devam etmek istiyor musunuz?");
                        
                        tft.setCursor(5, 80);
                        tft.setTextColor(ST7735_GREEN);
                        tft.println("EVET - Joyistik SAG");
                        
                        tft.setCursor(5, 95);
                        tft.setTextColor(ST7735_RED);
                        tft.println("HAYIR - Joyistik SOL");
                        
                        // Kullanıcı seçimini bekle
                        JoystickDirection selection = JOY_NONE;
                        while (selection != JOY_LEFT && selection != JOY_RIGHT) {
                            selection = UIReadJoystick();
                            esp_task_wdt_reset();
                            delay(50);
                        }
                        
                        if (selection == JOY_RIGHT) {
                            // Geri yükleme işlemini gerçekleştir
                            tft.fillRect(0, 40, tft.width(), 60, ST7735_BLACK);
                            tft.setCursor(5, 40);
                            tft.setTextColor(ST7735_YELLOW);
                            tft.println("Veriler geri yukleniyor...");
                            
                            bool success = RestoreData();
                            
                            if (success) {
                                DisplayShowNotification("Veriler basariyla geri yuklendi!");
                            } else {
                                DisplayShowNotification("Geri yukleme hatasi!");
                            }
                        }
                        
                        UINavigateToMenu(MENU_BACKUP);
                    }
                    break;
                    
                case 2: // Tüm Verileri Temizle
                    {
                        // Onay mesajı göster
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("VERI TEMIZLEME");
                        
                        tft.setCursor(5, 40);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_RED);
                        tft.println("TUM VERILER SILINECEK!");
                        
                        tft.setCursor(5, 55);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Devam etmek istiyor musunuz?");
                        
                        tft.setCursor(5, 80);
                        tft.setTextColor(ST7735_GREEN);
                        tft.println("EVET - Joyistik SAG");
                        
                        tft.setCursor(5, 95);
                        tft.setTextColor(ST7735_RED);
                        tft.println("HAYIR - Joyistik SOL");
                        
                        // Kullanıcı seçimini bekle
                        JoystickDirection selection = JOY_NONE;
                        while (selection != JOY_LEFT && selection != JOY_RIGHT) {
                            selection = UIReadJoystick();
                            esp_task_wdt_reset();
                            delay(50);
                        }
                        
                        if (selection == JOY_RIGHT) {
                            // Temizleme işlemini gerçekleştir
                            tft.fillRect(0, 40, tft.width(), 60, ST7735_BLACK);
                            tft.setCursor(5, 40);
                            tft.setTextColor(ST7735_YELLOW);
                            tft.println("Veriler temizleniyor...");
                            
                            bool success = ClearAllData();
                            
                            if (success) {
                                DisplayShowNotification("Tum veriler temizlendi!");
                            } else {
                                DisplayShowNotification("Veri temizleme hatasi!");
                            }
                        }
                        
                        UINavigateToMenu(MENU_BACKUP);
                    }
                    break;
            }
            return true;
            
        default:
            return false;
    }
}

// Güç Tasarrufu Menüsü İşleme
bool handlePowerSaveMenu(JoystickDirection direction) {
    static const char* powerSaveMenuItems[] = {
        "Guc Tasarruf Modu",
        "Ekran Koruyucu",
        "Ekran Zaman Asimi"
    };
    static const int itemCount = sizeof(powerSaveMenuItems) / sizeof(powerSaveMenuItems[0]);
    
    // PowerSaveModule'den mevcut değerleri al
    PowerSaveLevel currentMode = PowerSaveGetLevel();
    ScreenSaverMode currentScreenSaverMode = PowerSaveGetScreenSaverMode();
    // screenSaverTimeout doğrudan erişilemediği için 3 dakika (180000ms) varsayılan değer olarak kullanılacak
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, powerSaveMenuItems, itemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < itemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, powerSaveMenuItems, itemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_MAIN);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS:
            switch (menuSelection) {
                case 0: { // Güç Tasarruf Modu
                    // Güç tasarruf modları
                    const char* powerModes[] = {
                        "Devre Disi",
                        "Hafif",
                        "Orta",
                        "Agresif"
                    };
                    int modeCount = 4;
                    
                    // Mevcut modu seç
                    int currentModeIndex = (int)currentMode;
                    
                    // Mod seçim ekranını göster
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("GUC TASARRUF MODU");
                    
                    // Modları listele ve şu an aktif olanı işaretle
                    for (int i = 0; i < modeCount; i++) {
                        tft.setCursor(10, 30 + i * 20);
                        tft.setTextSize(1);
                        
                        // Aktif modu farklı renkte göster
                        if (i == currentModeIndex) {
                            tft.setTextColor(ST7735_GREEN);
                            tft.print("> ");
                        } else {
                            tft.setTextColor(ST7735_WHITE);
                            tft.print("  ");
                        }
                        
                        tft.println(powerModes[i]);
                    }
                    
                    // Alt bilgi
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("^v:Secim >:Onay <:Iptal");
                    
                    // Kullanıcı seçimini bekle
                    int selection = currentModeIndex;
                    bool exitMenu = false;
                    
                    while (!exitMenu) {
                        JoystickDirection dir = UIReadJoystick();
                        esp_task_wdt_reset();
                        
                        if (dir != JOY_NONE) {
                            switch (dir) {
                                case JOY_UP:
                                    if (selection > 0) {
                                        // Önceki seçimi normal renkte göster
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_WHITE);
                                        tft.print("  ");
                                        tft.println(powerModes[selection]);
                                        
                                        // Yeni seçimi vurgula
                                        selection--;
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("> ");
                                        tft.println(powerModes[selection]);
                                    }
                                    break;
                                    
                                case JOY_DOWN:
                                    if (selection < modeCount - 1) {
                                        // Önceki seçimi normal renkte göster
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_WHITE);
                                        tft.print("  ");
                                        tft.println(powerModes[selection]);
                                        
                                        // Yeni seçimi vurgula
                                        selection++;
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("> ");
                                        tft.println(powerModes[selection]);
                                    }
                                    break;
                                    
                                case JOY_LEFT:
                                    // İptal et
                                    exitMenu = true;
                                    break;
                                    
                                case JOY_RIGHT:
                                case JOY_PRESS:
                                    // Güç tasarruf modunu değiştir
                                    PowerSaveSetLevel((PowerSaveLevel)selection);
                                    
                                    // Bildirim göster
                                    DisplayShowNotification("Guc tasarruf modu değiştirildi");
                                    
                                    exitMenu = true;
                                    break;
                                    
                                default:
                                    break;
                            }
                            
                            delay(200); // Debounce
                        }
                    }
                    
                    UINavigateToMenu(MENU_POWER_SAVE);
                    break;
                }
                
                case 1: { // Ekran Koruyucu
                    // Ekran koruyucu modları
                    const char* screenSaverModes[] = {
                        "Devre Disi",
                        "Logo",
                        "Bilgi",
                        "Ekran Kapali"
                    };
                    int modeCount = 4;
                    
                    // Mevcut modu seç
                    int currentModeIndex = (int)currentScreenSaverMode;
                    
                    // Mod seçim ekranını göster
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("EKRAN KORUYUCU");
                    
                    // Modları listele ve şu an aktif olanı işaretle
                    for (int i = 0; i < modeCount; i++) {
                        tft.setCursor(10, 30 + i * 20);
                        tft.setTextSize(1);
                        
                        // Aktif modu farklı renkte göster
                        if (i == currentModeIndex) {
                            tft.setTextColor(ST7735_GREEN);
                            tft.print("> ");
                        } else {
                            tft.setTextColor(ST7735_WHITE);
                            tft.print("  ");
                        }
                        
                        tft.println(screenSaverModes[i]);
                    }
                    
                    // Alt bilgi
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("^v:Secim >:Onay <:Iptal");
                    
                    // Kullanıcı seçimini bekle
                    int selection = currentModeIndex;
                    bool exitMenu = false;
                    
                    while (!exitMenu) {
                        JoystickDirection dir = UIReadJoystick();
                        esp_task_wdt_reset();
                        
                        if (dir != JOY_NONE) {
                            switch (dir) {
                                case JOY_UP:
                                    if (selection > 0) {
                                        // Önceki seçimi normal renkte göster
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_WHITE);
                                        tft.print("  ");
                                        tft.println(screenSaverModes[selection]);
                                        
                                        // Yeni seçimi vurgula
                                        selection--;
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("> ");
                                        tft.println(screenSaverModes[selection]);
                                    }
                                    break;
                                    
                                case JOY_DOWN:
                                    if (selection < modeCount - 1) {
                                        // Önceki seçimi normal renkte göster
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_WHITE);
                                        tft.print("  ");
                                        tft.println(screenSaverModes[selection]);
                                        
                                        // Yeni seçimi vurgula
                                        selection++;
                                        tft.setCursor(10, 30 + selection * 20);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("> ");
                                        tft.println(screenSaverModes[selection]);
                                    }
                                    break;
                                    
                                case JOY_LEFT:
                                    // İptal et
                                    exitMenu = true;
                                    break;
                                    
                                case JOY_RIGHT:
                                case JOY_PRESS:
                                    // Ekran koruyucu modunu değiştir
                                    PowerSaveSetScreenSaverMode((ScreenSaverMode)selection);
                                    
                                    // Ekran koruyucuyu devre dışı bırakma kontrolü
                                    PowerSaveToggleScreenSaver(selection != SCREEN_SAVER_NONE);
                                    
                                    // Bildirim göster
                                    DisplayShowNotification("Ekran koruyucu modu değiştirildi");
                                    
                                    exitMenu = true;
                                    break;
                                    
                                default:
                                    break;
                            }
                            
                            delay(200); // Debounce
                        }
                    }
                    
                    UINavigateToMenu(MENU_POWER_SAVE);
                    break;
                }
                
                case 2: { // Ekran Zaman Aşımı
                    // Zaman aşımı seçenekleri (dakika cinsinden)
                    const int timeoutOptions[] = {1, 3, 5, 10, 15, 30};
                    const int optionCount = sizeof(timeoutOptions) / sizeof(timeoutOptions[0]);
                    
                    // Mevcut zaman aşımı değerini varsayılan 3 dakika olarak alıyoruz
                    // (doğrudan screenSaverTimeout'a erişemediğimiz için)
                    int currentTimeoutMinutes = 3; // Varsayılan değer
                    
                    // En yakın seçeneği bul
                    int currentSelection = 0;
                    for (int i = 0; i < optionCount; i++) {
                        if (abs(timeoutOptions[i] - currentTimeoutMinutes) < 
                            abs(timeoutOptions[currentSelection] - currentTimeoutMinutes)) {
                            currentSelection = i;
                        }
                    }
                    
                    // Seçim ekranını göster
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("EKRAN ZAMAN ASIMI");
                    
                    // Süreleri listele ve şu an aktif olanı işaretle
                    for (int i = 0; i < optionCount; i++) {
                        tft.setCursor(10, 30 + i * 15);
                        tft.setTextSize(1);
                        
                        // Aktif süreyi farklı renkte göster
                        if (i == currentSelection) {
                            tft.setTextColor(ST7735_GREEN);
                            tft.print("> ");
                        } else {
                            tft.setTextColor(ST7735_WHITE);
                            tft.print("  ");
                        }
                        
                        tft.print(timeoutOptions[i]);
                        tft.println(" dakika");
                    }
                    
                    // Alt bilgi
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("^v:Secim >:Onay <:Iptal");
                    
                    // Kullanıcı seçimini bekle
                    int selection = currentSelection;
                    bool exitMenu = false;
                    
                    while (!exitMenu) {
                        JoystickDirection dir = UIReadJoystick();
                        esp_task_wdt_reset();
                        
                        if (dir != JOY_NONE) {
                            switch (dir) {
                                case JOY_UP:
                                    if (selection > 0) {
                                        // Önceki seçimi normal renkte göster
                                        tft.setCursor(10, 30 + selection * 15);
                                        tft.setTextColor(ST7735_WHITE);
                                        tft.print("  ");
                                        tft.print(timeoutOptions[selection]);
                                        tft.println(" dakika");
                                        
                                        // Yeni seçimi vurgula
                                        selection--;
                                        tft.setCursor(10, 30 + selection * 15);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("> ");
                                        tft.print(timeoutOptions[selection]);
                                        tft.println(" dakika");
                                    }
                                    break;
                                    
                                case JOY_DOWN:
                                    if (selection < optionCount - 1) {
                                        // Önceki seçimi normal renkte göster
                                        tft.setCursor(10, 30 + selection * 15);
                                        tft.setTextColor(ST7735_WHITE);
                                        tft.print("  ");
                                        tft.print(timeoutOptions[selection]);
                                        tft.println(" dakika");
                                        
                                        // Yeni seçimi vurgula
                                        selection++;
                                        tft.setCursor(10, 30 + selection * 15);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("> ");
                                        tft.print(timeoutOptions[selection]);
                                        tft.println(" dakika");
                                    }
                                    break;
                                    
                                case JOY_LEFT:
                                    // İptal et
                                    exitMenu = true;
                                    break;
                                    
                                case JOY_RIGHT:
                                case JOY_PRESS:
                                    // Ekran zaman aşımını değiştir (milisaniyeye çevir)
                                    unsigned long timeoutMs = timeoutOptions[selection] * 60000;
                                    PowerSaveSetScreenSaverTimeout(timeoutMs);
                                    
                                    // Bildirim göster
                                    DisplayShowNotification("Ekran zaman asimi degistirildi");
                                    
                                    exitMenu = true;
                                    break;
                            }
                            
                            delay(200); // Debounce
                        }
                    }
                    
                    UINavigateToMenu(MENU_POWER_SAVE);
                    break;
                }
            }
            return true;
            
        default:
            return false;
    }
}

// Kalibrasyon Menüsü İşleme
bool handleCalibrationMenu(JoystickDirection direction) {
    static const char* calibrationMenuItems[] = {
        "Sicaklik Kalibrasyonu",
        "Nem Kalibrasyonu",
        "Sensör Testi",
        "Joystick Kalibrasyonu"
    };
    static const int itemCount = sizeof(calibrationMenuItems) / sizeof(calibrationMenuItems[0]);
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, calibrationMenuItems, itemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < itemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, calibrationMenuItems, itemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_GENERAL_SETTINGS);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS:
            switch (menuSelection) {
                case 0: // Sıcaklık Kalibrasyonu
                    {
                        // Mevcut kalibrasyonu al
                        SensorCalibration calibration = GetCalibration();
                        
                        // Sensör verilerini al
                        SensorData sensorData = GetSensorData();
                        
                        // Sıcaklık kalibrasyonu yapılacak sensörü seç
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("SICAKLIK KALIBRASYONU");
                        
                        tft.setCursor(5, 25);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Kalibre edilecek sensoru sec:");
                        
                        // Sensör 1
                        tft.setCursor(10, 45);
                        tft.setTextColor(ST7735_RED);
                        tft.print("1. Sensor: ");
                        if (sensorData.sensor1Valid) {
                            tft.print(sensorData.temperature1, 1);
                            tft.print("C (");
                            tft.print(calibration.tempOffset1, 1);
                            tft.println(")");
                        } else {
                            tft.println("HATA");
                        }
                        
                        // Sensör 2
                        tft.setCursor(10, 60);
                        tft.print("2. Sensor: ");
                        if (sensorData.sensor2Valid) {
                            tft.print(sensorData.temperature2, 1);
                            tft.print("C (");
                            tft.print(calibration.tempOffset2, 1);
                            tft.println(")");
                        } else {
                            tft.println("HATA");
                        }
                        
                        // Alt bilgi
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("1-2:Secim <:Iptal");
                        
                        // Kullanıcı seçimini bekle
                        bool exitMenu = false;
                        int selectedSensor = 0;
                        
                        while (!exitMenu) {
                            JoystickDirection dir = UIReadJoystick();
                            esp_task_wdt_reset();
                            
                            if (dir != JOY_NONE) {
                                switch (dir) {
                                    case JOY_UP:
                                        selectedSensor = 0; // Sensör 1
                                        
                                        // Seçilen sensörü vurgula
                                        tft.setCursor(10, 45);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("1. Sensor: ");
                                        if (sensorData.sensor1Valid) {
                                            tft.print(sensorData.temperature1, 1);
                                            tft.print("C (");
                                            tft.print(calibration.tempOffset1, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        
                                        tft.setCursor(10, 60);
                                        tft.setTextColor(ST7735_RED);
                                        tft.print("2. Sensor: ");
                                        if (sensorData.sensor2Valid) {
                                            tft.print(sensorData.temperature2, 1);
                                            tft.print("C (");
                                            tft.print(calibration.tempOffset2, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        break;
                                        
                                    case JOY_DOWN:
                                        selectedSensor = 1; // Sensör 2
                                        
                                        // Seçilen sensörü vurgula
                                        tft.setCursor(10, 45);
                                        tft.setTextColor(ST7735_RED);
                                        tft.print("1. Sensor: ");
                                        if (sensorData.sensor1Valid) {
                                            tft.print(sensorData.temperature1, 1);
                                            tft.print("C (");
                                            tft.print(calibration.tempOffset1, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        
                                        tft.setCursor(10, 60);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("2. Sensor: ");
                                        if (sensorData.sensor2Valid) {
                                            tft.print(sensorData.temperature2, 1);
                                            tft.print("C (");
                                            tft.print(calibration.tempOffset2, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        break;
                                        
                                    case JOY_LEFT:
                                        // İptal et
                                        exitMenu = true;
                                        break;
                                        
                                    case JOY_RIGHT:
                                    case JOY_PRESS:
                                        // Seçili sensör kalibrasyonuna devam et
                                        if ((selectedSensor == 0 && sensorData.sensor1Valid) ||
                                            (selectedSensor == 1 && sensorData.sensor2Valid)) {
                                            
                                            // Referans değeri girme ekranı
                                            float refTemp = UISingleNumericInput("Referans Sicaklik", 20.0, 50.0, sensorData.avgTemperature);
                                            
                                            // Kalibrasyonu uygula
                                            if (selectedSensor == 0) {
                                                // Sensör 1 kalibrasyonu
                                                calibration.tempOffset1 = refTemp - (sensorData.temperature1 - calibration.tempOffset1);
                                            } else {
                                                // Sensör 2 kalibrasyonu
                                                calibration.tempOffset2 = refTemp - (sensorData.temperature2 - calibration.tempOffset2);
                                            }
                                            
                                            // Kalibrasyonu kaydet
                                            SetCalibration(calibration);
                                            
                                            // Bildirim göster
                                            DisplayShowNotification("Sicaklik kalibrasyonu tamamlandi");
                                        } else {
                                            // Seçilen sensör hatalı
                                            DisplayShowNotification("Secilen sensör calişmiyor!");
                                        }
                                        
                                        exitMenu = true;
                                        break;
                                        
                                    default:
                                        break;
                                }
                                
                                delay(200); // Debounce
                            }
                        }
                        
                        UINavigateToMenu(MENU_CALIBRATION);
                    }
                    break;
                    
                case 1: // Nem Kalibrasyonu
                    {
                        // Mevcut kalibrasyonu al
                        SensorCalibration calibration = GetCalibration();
                        
                        // Sensör verilerini al
                        SensorData sensorData = GetSensorData();
                        
                        // Nem kalibrasyonu yapılacak sensörü seç
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("NEM KALIBRASYONU");
                        
                        tft.setCursor(5, 25);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Kalibre edilecek sensoru sec:");
                        
                        // Sensör 1
                        tft.setCursor(10, 45);
                        tft.setTextColor(ST7735_GREEN);
                        tft.print("1. Sensor: ");
                        if (sensorData.sensor1Valid) {
                            tft.print(sensorData.humidity1, 1);
                            tft.print("% (");
                            tft.print(calibration.humOffset1, 1);
                            tft.println(")");
                        } else {
                            tft.println("HATA");
                        }
                        
                        // Sensör 2
                        tft.setCursor(10, 60);
                        tft.setTextColor(ST7735_RED);
                        tft.print("2. Sensor: ");
                        if (sensorData.sensor2Valid) {
                            tft.print(sensorData.humidity2, 1);
                            tft.print("% (");
                            tft.print(calibration.humOffset2, 1);
                            tft.println(")");
                        } else {
                            tft.println("HATA");
                        }
                        
                        // Alt bilgi
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("^v:Secim >:Onay <:Iptal");
                        
                        // Kullanıcı seçimini bekle
                        bool exitMenu = false;
                        int selectedSensor = 0;
                        
                        while (!exitMenu) {
                            JoystickDirection dir = UIReadJoystick();
                            esp_task_wdt_reset();
                            
                            if (dir != JOY_NONE) {
                                switch (dir) {
                                    case JOY_UP:
                                        selectedSensor = 0; // Sensör 1
                                        
                                        // Seçilen sensörü vurgula
                                        tft.setCursor(10, 45);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("1. Sensor: ");
                                        if (sensorData.sensor1Valid) {
                                            tft.print(sensorData.humidity1, 1);
                                            tft.print("% (");
                                            tft.print(calibration.humOffset1, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        
                                        tft.setCursor(10, 60);
                                        tft.setTextColor(ST7735_RED);
                                        tft.print("2. Sensor: ");
                                        if (sensorData.sensor2Valid) {
                                            tft.print(sensorData.humidity2, 1);
                                            tft.print("% (");
                                            tft.print(calibration.humOffset2, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        break;
                                        
                                    case JOY_DOWN:
                                        selectedSensor = 1; // Sensör 2
                                        
                                        // Seçilen sensörü vurgula
                                        tft.setCursor(10, 45);
                                        tft.setTextColor(ST7735_RED);
                                        tft.print("1. Sensor: ");
                                        if (sensorData.sensor1Valid) {
                                            tft.print(sensorData.humidity1, 1);
                                            tft.print("% (");
                                            tft.print(calibration.humOffset1, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        
                                        tft.setCursor(10, 60);
                                        tft.setTextColor(ST7735_GREEN);
                                        tft.print("2. Sensor: ");
                                        if (sensorData.sensor2Valid) {
                                            tft.print(sensorData.humidity2, 1);
                                            tft.print("% (");
                                            tft.print(calibration.humOffset2, 1);
                                            tft.println(")");
                                        } else {
                                            tft.println("HATA");
                                        }
                                        break;
                                        
                                    case JOY_LEFT:
                                        // İptal et
                                        exitMenu = true;
                                        break;
                                        
                                    case JOY_RIGHT:
                                    case JOY_PRESS:
                                        // Seçili sensör kalibrasyonuna devam et
                                        if ((selectedSensor == 0 && sensorData.sensor1Valid) ||
                                            (selectedSensor == 1 && sensorData.sensor2Valid)) {
                                            
                                            // Referans değeri girme ekranı
                                            float refHum = UISingleNumericInput("Referans Nem", 10.0, 95.0, sensorData.avgHumidity);
                                            
                                            // Kalibrasyonu uygula
                                            if (selectedSensor == 0) {
                                                // Sensör 1 kalibrasyonu
                                                calibration.humOffset1 = refHum - (sensorData.humidity1 - calibration.humOffset1);
                                            } else {
                                                // Sensör 2 kalibrasyonu
                                                calibration.humOffset2 = refHum - (sensorData.humidity2 - calibration.humOffset2);
                                            }
                                            
                                            // Kalibrasyonu kaydet
                                            SetCalibration(calibration);
                                            
                                            // Bildirim göster
                                            DisplayShowNotification("Nem kalibrasyonu tamamlandi");
                                        } else {
                                            // Seçilen sensör hatalı
                                            DisplayShowNotification("Secilen sensör calişmiyor!");
                                        }
                                        
                                        exitMenu = true;
                                        break;
                                        
                                    default:
                                        break;
                                }
                                
                                delay(200); // Debounce
                            }
                        }
                        
                        UINavigateToMenu(MENU_CALIBRATION);
                    }
                    break;
                    
                case 2: // Sensör Testi
                    {
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("SENSOR TESTI");
                        
                        tft.setCursor(5, 25);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Sensor butuenluek testi basladi");
                        
                        // Sensör bütünlük kontrolü
                        bool testResult = CheckSensorsIntegrity();
                        
                        tft.setCursor(5, 45);
                        tft.setTextColor(testResult ? ST7735_GREEN : ST7735_RED);
                        tft.println(testResult ? "Sensor testi basarili!" : "Sensor testi basarisiz!");
                        
                        // Sensör durumları
                        SensorData sensorData = GetSensorData();
                        
                        tft.setCursor(5, 65);
                        tft.setTextColor(ST7735_WHITE);
                        tft.print("Sensor 1: ");
                        tft.setTextColor(sensorData.sensor1Valid ? ST7735_GREEN : ST7735_RED);
                        tft.println(sensorData.sensor1Valid ? "CALISIYOR" : "HATA");
                        
                        tft.setCursor(5, 80);
                        tft.setTextColor(ST7735_WHITE);
                        tft.print("Sensor 2: ");
                        tft.setTextColor(sensorData.sensor2Valid ? ST7735_GREEN : ST7735_RED);
                        tft.println(sensorData.sensor2Valid ? "CALISIYOR" : "HATA");
                        
                        // Hata sayaçları
                        tft.setCursor(5, 100);
                        tft.setTextColor(ST7735_WHITE);
                        tft.print("Hata 1: ");
                        tft.println(GetSensor1ErrorCount());
                        
                        tft.setCursor(5, 115);
                        tft.print("Hata 2: ");
                        tft.println(GetSensor2ErrorCount());
                        
                        // Alt bilgi çubuğu
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("Geri donmek icin tiklayin");
                        
                        // Buton basılmasını bekle
                        waitForButton();
                        
                        UINavigateToMenu(MENU_CALIBRATION);
                    }
                    break;
                    
                case 3: // Joystick Kalibrasyonu
                    {
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("JOYSTICK KALIBRASYONU");
                        
                        tft.setCursor(5, 30);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Joystick'i merkeze getirin");
                        tft.setCursor(5, 45);
                        tft.println("ve tiklayin.");
                        
                        // Buton bekle
                        waitForButton();
                        
                        tft.setCursor(5, 65);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("Kalibrasyon basliyor...");
                        
                        // Kalibrasyonu gerçekleştir
                        CalibrateJoystick();
                        
                        tft.setCursor(5, 85);
                        tft.setTextColor(ST7735_GREEN);
                        tft.println("Kalibrasyon tamamlandi!");
                        
                        // Alt bilgi çubuğu
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("Geri donmek icin tiklayin");
                        
                        // Buton basılmasını bekle
                        waitForButton();
                        
                        UINavigateToMenu(MENU_CALIBRATION);
                    }
                    break;
            }
            return true;
            
        default:
            return false;
    }
}

// Güç Tasarruf Menüsünü Göster
void UIShowPowerSaveMenu() {
    static const char* powerSaveMenuItems[] = {
        "Guc Tasarruf Modu",
        "Ekran Koruyucu",
        "Ekran Zaman Asimi"
    };
    static const int itemCount = sizeof(powerSaveMenuItems) / sizeof(powerSaveMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_POWER_SAVE;
    DisplayShowMenu(menuSelection, powerSaveMenuItems, itemCount);
    LOG_INFO(TAG, "Güç tasarruf menüsü gösteriliyor");
}

// Kalibrasyon Menüsünü Göster
void UIShowCalibrationMenu() {
    static const char* calibrationMenuItems[] = {
        "Sicaklik Kalibrasyonu",
        "Nem Kalibrasyonu",
        "Sensör Testi",
        "Joystick Kalibrasyonu"
    };
    static const int itemCount = sizeof(calibrationMenuItems) / sizeof(calibrationMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_CALIBRATION;
    DisplayShowMenu(menuSelection, calibrationMenuItems, itemCount);
    LOG_INFO(TAG, "Kalibrasyon menüsü gösteriliyor");
}

// Alarm Menüsünü Göster
void UIShowAlarmMenu() {
    static const char* alarmMenuItems[] = {
        "Alarm Durum",
        "Alarm Esikleri",
        "Alarm Gecmisi",
        "Alarm Testi"
    };
    static const int itemCount = sizeof(alarmMenuItems) / sizeof(alarmMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_ALARM;
    DisplayShowMenu(menuSelection, alarmMenuItems, itemCount);
    LOG_INFO(TAG, "Alarm menüsü gösteriliyor");
}

// WiFi Menüsünü Göster
void UIShowWifiMenu() {
    static const char* wifiMenuItems[] = {
        "WiFi Durum",
        "Erisim Noktasi Modu",
        "Aga Baglan",
        "WiFi Kapat"
    };
    static const int itemCount = sizeof(wifiMenuItems) / sizeof(wifiMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_WIFI;
    DisplayShowMenu(menuSelection, wifiMenuItems, itemCount);
    LOG_INFO(TAG, "WiFi menüsü gösteriliyor");
}

// Yedekleme Menüsünü Göster
void UIShowBackupMenu() {
    static const char* backupMenuItems[] = {
        "Verileri Yedekle",
        "Yedekten Geri Yukle",
        "Tum Verileri Temizle"
    };
    static const int itemCount = sizeof(backupMenuItems) / sizeof(backupMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_BACKUP;
    DisplayShowMenu(menuSelection, backupMenuItems, itemCount);
    LOG_INFO(TAG, "Yedekleme menüsü gösteriliyor");
}

// Grafik Menüsünü Göster
void UIShowGraphMenu() {
    static const char* graphMenuItems[] = {
        "Sicaklik Grafigi",
        "Nem Grafigi",
        "Istatistikler",
        "Elektrik Kesintileri"
    };
    static const int itemCount = sizeof(graphMenuItems) / sizeof(graphMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_GRAPH;
    DisplayShowMenu(menuSelection, graphMenuItems, itemCount);
    LOG_INFO(TAG, "Grafik menüsü gösteriliyor");
}

// Grafik tipine göre grafiği gösterme
void UIShowGraphSelection(int graphType) {
    // Daha makul bir veri boyutu - ekrana sığacak kadar
    const int MAX_DISPLAY_POINTS = 20; 
    
    // Verileri hazırla
    float data[MAX_DISPLAY_POINTS];
    uint32_t timestamps[MAX_DISPLAY_POINTS];
    int dataCount = 0;
    
    // Grafik tipine göre verileri al
    if (graphType == 0) { // Sıcaklık grafiği
        GetTempGraphData(data, timestamps, MAX_DISPLAY_POINTS, &dataCount);
        
        if (dataCount > 0) {
            DisplayShowGraph("SICAKLIK GRAFIGI", data, timestamps, dataCount, 0);
            LOG_INFO(TAG, "Sicaklik grafigi gösteriliyor (%d veri noktasi)", dataCount);
        } else {
            // Veri yoksa bilgilendirme mesajı göster
            tft.fillScreen(ST7735_BLACK);
            tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
            tft.setCursor(5, 5);
            tft.setTextSize(1);
            tft.setTextColor(ST7735_WHITE);
            tft.println("SICAKLIK GRAFIGI");
            
            tft.setCursor(20, tft.height() / 2);
            tft.setTextColor(ST7735_RED);
            tft.println("Veri bulunamadi!");
            
            tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
            tft.setCursor(5, tft.height() - 13);
            tft.setTextSize(1);
            tft.setTextColor(ST7735_CYAN);
            tft.println("Geri donmek icin tiklayin");
            
            waitForButton();
            UINavigateToMenu(MENU_GRAPH);
            return;
        }
    } else { // Nem grafiği
        GetHumGraphData(data, timestamps, MAX_DISPLAY_POINTS, &dataCount);
        
        if (dataCount > 0) {
            DisplayShowGraph("NEM GRAFIGI", data, timestamps, dataCount, 1);
            LOG_INFO(TAG, "Nem grafigi gösteriliyor (%d veri noktasi)", dataCount);
        } else {
            // Veri yoksa bilgilendirme mesajı göster
            tft.fillScreen(ST7735_BLACK);
            tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
            tft.setCursor(5, 5);
            tft.setTextSize(1);
            tft.setTextColor(ST7735_WHITE);
            tft.println("NEM GRAFIGI");
            
            tft.setCursor(20, tft.height() / 2);
            tft.setTextColor(ST7735_RED);
            tft.println("Veri bulunamadi!");
            
            tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
            tft.setCursor(5, tft.height() - 13);
            tft.setTextSize(1);
            tft.setTextColor(ST7735_CYAN);
            tft.println("Geri donmek icin tiklayin");
            
            waitForButton();
            UINavigateToMenu(MENU_GRAPH);
            return;
        }
    }
    
    // Kullanıcının butona basmasını bekle
    waitForButton();
    
    // Grafik menüsüne geri dön
    UINavigateToMenu(MENU_GRAPH);
}

// İstatistikleri göster
void showStatistics() {
    float minTemp, maxTemp, avgTemp;
    float minHum, maxHum, avgHum;
    
    tft.fillScreen(ST7735_BLACK);
    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
    tft.setCursor(5, 5);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.println("ISTATISTIKLER");
    
    if (GetTempHumidityStats(&minTemp, &maxTemp, &avgTemp, &minHum, &maxHum, &avgHum)) {
        // Sıcaklık istatistikleri
        tft.setCursor(5, 25);
        tft.setTextColor(ST7735_RED);
        tft.println("SICAKLIK:");
        
        tft.setCursor(5, 40);
        tft.setTextColor(ST7735_WHITE);
        tft.print("Min: ");
        tft.print(minTemp, 1);
        tft.println(" C");
        
        tft.setCursor(5, 55);
        tft.print("Maks: ");
        tft.print(maxTemp, 1);
        tft.println(" C");
        
        tft.setCursor(5, 70);
        tft.print("Ort: ");
        tft.print(avgTemp, 1);
        tft.println(" C");
        
        // Nem istatistikleri
        tft.setCursor(5, 90);
        tft.setTextColor(ST7735_BLUE);
        tft.println("NEM:");
        
        tft.setCursor(5, 105);
        tft.setTextColor(ST7735_WHITE);
        tft.print("Min: %");
        tft.println(minHum, 0);
        
        tft.setCursor(5, 120);
        tft.print("Maks: %");
        tft.println(maxHum, 0);
        
        tft.setCursor(5, 135);
        tft.print("Ort: %");
        tft.println(avgHum, 0);
    } else {
        tft.setCursor(10, tft.height() / 2);
        tft.setTextColor(ST7735_RED);
        tft.println("Veri yok!");
    }
    
    // Buton bekleme
    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
    tft.setCursor(5, tft.height() - 13);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_CYAN);
    tft.println("Geri donmek icin tiklayin");
    
    waitForButton();
    UINavigateToMenu(MENU_GRAPH);
}

// Elektrik kesintilerini göster
void UIShowPowerOutages() {
    PowerOutage outages[MAX_POWER_OUTAGES];
    int outageCount = 0;
    
    tft.fillScreen(ST7735_BLACK);
    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
    tft.setCursor(5, 5);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.println("ELEKTRIK KESINTILERI");
    
    if (GetPowerOutages(outages, MAX_POWER_OUTAGES, &outageCount) && outageCount > 0) {
        for (int i = 0; i < outageCount && i < 5; i++) { // En son 5 kesinti
            // Kesinti başlangıç zamanını göster
            TimeInfo time = UnixTimeToTimeInfo(outages[i].startTime);
            
            tft.setCursor(5, 25 + i * 25);
            tft.setTextColor(ST7735_YELLOW);
            char dateStr[20];
            sprintf(dateStr, "%02d.%02d.%04d %02d:%02d", 
                    time.day, time.month, time.year, time.hour, time.minute);
            tft.print(dateStr);
            
            // Kesinti süresini göster
            tft.setCursor(5, 25 + i * 25 + 12);
            tft.setTextColor(ST7735_GREEN);
            
            if (outages[i].duration < 60) {
                tft.print("Sure: ");
                tft.print(outages[i].duration);
                tft.println(" sn");
            } else if (outages[i].duration < 3600) {
                tft.print("Sure: ");
                tft.print(outages[i].duration / 60);
                tft.println(" dk");
            } else {
                tft.print("Sure: ");
                tft.print(outages[i].duration / 3600);
                tft.print(" sa ");
                tft.print((outages[i].duration % 3600) / 60);
                tft.println(" dk");
            }
        }
    } else {
        tft.setCursor(10, tft.height() / 2);
        tft.setTextColor(ST7735_RED);
        tft.println("Kesinti kaydı yok!");
    }
    
    // Buton bekleme
    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
    tft.setCursor(5, tft.height() - 13);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_CYAN);
    tft.println("Geri donmek icin tiklayin");
    
    waitForButton();
    UINavigateToMenu(MENU_GRAPH);
}

// Profil Menüsü İşleme
bool handleProfileMenu(JoystickDirection direction) {
    // Profil adlarını ve tiplerini saklayacak diziler
    static const char* profileItems[MAX_PROFILES];
    static int profileTypes[MAX_PROFILES];
    static int profileCount = 0;
    
    // Profil listesini ilk kez yükle veya yenile
    if (profileCount == 0) {
        profileCount = 0;
        // Tüm geçerli profilleri ekle
        for (int i = 0; i < MAX_PROFILES; i++) {
            if (predefinedProfiles[i].name.length() > 0) {
                profileItems[profileCount] = predefinedProfiles[i].name.c_str();
                profileTypes[profileCount] = i;
                profileCount++;
            }
        }
        
        // Profilleri göster
        DisplayShowMenu(menuSelection, profileItems, profileCount);
    }
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, profileItems, profileCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < profileCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, profileItems, profileCount);
            }
            return true;
            
        case JOY_LEFT:
            // Ana menüye dön
            UINavigateToMenu(MENU_MAIN);
            profileCount = 0; // Sonraki sefer listeyi yeniden yükle
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS: {
            // Seçilen profilin indeksini al
            int selectedType = profileTypes[menuSelection];
            
            // Onay ekranı
            tft.fillScreen(ST7735_BLACK);
            tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
            tft.setCursor(5, 5);
            tft.setTextSize(1);
            tft.setTextColor(ST7735_WHITE);
            tft.println("PROFIL SECIMI");
            
            // Profil detaylarını göster
            Profile selectedProfile = predefinedProfiles[selectedType];
            
            tft.setCursor(5, 25);
            tft.setTextColor(ST7735_YELLOW);
            tft.print("Profil: ");
            tft.println(selectedProfile.name);
            
            tft.setCursor(5, 40);
            tft.setTextColor(ST7735_WHITE);
            tft.print("Toplam sure: ");
            tft.print(selectedProfile.totalDays);
            tft.println(" gun");
            
            tft.setCursor(5, 60);
            tft.setTextColor(ST7735_GREEN);
            tft.println("Kulucka baslatilsin mi?");
            
            tft.setCursor(5, 80);
            tft.setTextColor(ST7735_WHITE);
            tft.println("EVET - Joyistik SAG");
            
            tft.setCursor(5, 95);
            tft.println("HAYIR - Joyistik SOL");
            
            // Kullanıcı onayını bekle
            JoystickDirection selection = JOY_NONE;
            while (selection != JOY_LEFT && selection != JOY_RIGHT && selection != JOY_PRESS) {
                selection = UIReadJoystick();
                esp_task_wdt_reset(); // Watchdog'u besle
                delay(50);
            }
            
            if (selection == JOY_RIGHT || selection == JOY_PRESS) {
                // Seçilen profil ile kuluçka işlemini başlat
                bool success = ProfileStartIncubation((ProfileType)selectedType);
                
                if (success) {
                    DisplayShowNotification("Kulucka islemi baslatildi!");
                    delay(1500); // Bildirimi görmesi için biraz bekle
                    UIBackToMain(); // Ana ekrana dön ve güncel verileri göster
                } else {
                    DisplayShowNotification("Kulucka baslatilamadi!");
                    delay(1500);
                }
            }
            
            // Profil menüsüne dön - başarılı olup olmadığına bakılmaksızın
            if (selection != JOY_RIGHT && selection != JOY_PRESS) {
                profileCount = 0; // Sonraki sefer listeyi yeniden yükle
                UINavigateToMenu(MENU_PROFILE);
            }
            return true;
        }
        
        default:
            return false;
    }
}

// Alarm Menüsü İşleme
bool handleAlarmMenu(JoystickDirection direction) {
    static const char* alarmMenuItems[] = {
        "Alarm Durum",
        "Alarm Esikleri",
        "Alarm Gecmisi",
        "Alarm Testi"
    };
    static const int itemCount = sizeof(alarmMenuItems) / sizeof(alarmMenuItems[0]);
    
    // Alt menü işleme için
    static bool inThresholdSubmenu = false;
    static int thresholdSubmenuSelection = 0;
    static const char* thresholdMenuItems[] = {
        "Alarmlar Acik/Kapali",
        "Yuksek Sicaklik Esigi",
        "Dusuk Sicaklik Esigi",
        "Yuksek Nem Esigi",
        "Dusuk Nem Esigi",
        "Sicaklik Fark Esigi",
        "Nem Fark Esigi"
    };
    static const int thresholdCount = sizeof(thresholdMenuItems) / sizeof(thresholdMenuItems[0]);
    
    if (inThresholdSubmenu) {
        // Alt menü navigasyon işleme
        switch (direction) {
            case JOY_UP: {
                if (thresholdSubmenuSelection > 0) {
                    thresholdSubmenuSelection--;
                    // Alt menüyü göster
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("ALARM ESIKLERI");
                    
                    // Menü öğelerini göster
                    int displayCount = min(thresholdCount, 6);
                    for (int i = 0; i < displayCount; i++) {
                        tft.setCursor(5, 25 + i * 18);
                        tft.setTextColor((i == thresholdSubmenuSelection) ? ST7735_YELLOW : ST7735_WHITE);
                        tft.print(i + 1);
                        tft.print(". ");
                        tft.println(thresholdMenuItems[i]);
                    }
                    
                    // Alt bilgi çubuğu
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("^v:Secim >:Sec <:Geri");
                }
                return true;
            }
                
            case JOY_DOWN: {
                if (thresholdSubmenuSelection < thresholdCount - 1) {
                    thresholdSubmenuSelection++;
                    // Yeni seçimle menüyü yeniden çiz
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("ALARM ESIKLERI");
                    
                    // Menü öğelerini göster
                    int displayCount = min(thresholdCount, 6);
                    for (int i = 0; i < displayCount; i++) {
                        tft.setCursor(5, 25 + i * 18);
                        tft.setTextColor((i == thresholdSubmenuSelection) ? ST7735_YELLOW : ST7735_WHITE);
                        tft.print(i + 1);
                        tft.print(". ");
                        tft.println(thresholdMenuItems[i]);
                    }
                    
                    // Alt bilgi çubuğu
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("^v:Secim >:Sec <:Geri");
                }
                return true;
            }
                
            case JOY_LEFT: {
                // Alt menüden çık
                inThresholdSubmenu = false;
                UINavigateToMenu(MENU_ALARM);
                return true;
            }
                
            case JOY_RIGHT:
            case JOY_PRESS: {
                // Seçilen eşik seçeneğini işle
                AlarmThresholds thresholds = AlarmGetThresholds();
                
                switch (thresholdSubmenuSelection) {
                    case 0: { // Alarmlar Açık/Kapalı
                        thresholds.alarmEnabled = !thresholds.alarmEnabled;
                        AlarmSetEnabled(thresholds.alarmEnabled);
                        DisplayShowNotification(thresholds.alarmEnabled ? 
                            "Alarmlar etkinlestirildi!" : 
                            "Alarmlar devre disi birakildi!");
                        break;
                    }
                        
                    case 1: { // Yüksek Sıcaklık Eşiği
                        float newValue = UISingleNumericInput("Yuksek Sicaklik Esigi", 
                                                            thresholds.lowTempThreshold + 0.5, 
                                                            50.0, 
                                                            thresholds.highTempThreshold);
                        thresholds.highTempThreshold = newValue;
                        AlarmSetThresholds(thresholds);
                        DisplayShowNotification("Yuksek sicaklik esigi kaydedildi!");
                        break;
                    }
                    
                    case 2: { // Düşük Sıcaklık Eşiği
                        float newValue = UISingleNumericInput("Dusuk Sicaklik Esigi", 
                                                            25.0, 
                                                            thresholds.highTempThreshold - 0.5, 
                                                            thresholds.lowTempThreshold);
                        thresholds.lowTempThreshold = newValue;
                        AlarmSetThresholds(thresholds);
                        DisplayShowNotification("Dusuk sicaklik esigi kaydedildi!");
                        break;
                    }
                        
                    case 3: { // Yüksek Nem Eşiği
                        float newValue = UISingleNumericInput("Yuksek Nem Esigi", 
                                                            thresholds.lowHumThreshold + 5.0, 
                                                            100.0, 
                                                            thresholds.highHumThreshold);
                        thresholds.highHumThreshold = newValue;
                        AlarmSetThresholds(thresholds);
                        DisplayShowNotification("Yuksek nem esigi kaydedildi!");
                        break;
                    }
                        
                    case 4: { // Düşük Nem Eşiği
                        float newValue = UISingleNumericInput("Dusuk Nem Esigi", 
                                                            10.0, 
                                                            thresholds.highHumThreshold - 5.0, 
                                                            thresholds.lowHumThreshold);
                        thresholds.lowHumThreshold = newValue;
                        AlarmSetThresholds(thresholds);
                        DisplayShowNotification("Dusuk nem esigi kaydedildi!");
                        break;
                    }
                        
                    case 5: { // Sıcaklık Fark Eşiği
                        float newValue = UISingleNumericInput("Sicaklik Fark Esigi", 
                                                            0.5, 
                                                            5.0, 
                                                            thresholds.tempDiffThreshold);
                        thresholds.tempDiffThreshold = newValue;
                        AlarmSetThresholds(thresholds);
                        DisplayShowNotification("Sicaklik fark esigi kaydedildi!");
                        break;
                    }
                        
                    case 6: { // Nem Fark Eşiği
                        float newValue = UISingleNumericInput("Nem Fark Esigi", 
                                                            2.0, 
                                                            20.0, 
                                                            thresholds.humDiffThreshold);
                        thresholds.humDiffThreshold = newValue;
                        AlarmSetThresholds(thresholds);
                        DisplayShowNotification("Nem fark esigi kaydedildi!");
                        break;
                    }
                }
                
                // Ayar sonrası alt menüye dön
                inThresholdSubmenu = true;
                
                // Eşik alt menüsünü yeniden çiz
                tft.fillScreen(ST7735_BLACK);
                tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                tft.setCursor(5, 5);
                tft.setTextSize(1);
                tft.setTextColor(ST7735_WHITE);
                tft.println("ALARM ESIKLERI");
                
                // Menü öğelerini göster
                int displayCount = min(thresholdCount, 6);
                for (int i = 0; i < displayCount; i++) {
                    tft.setCursor(5, 25 + i * 18);
                    tft.setTextColor((i == thresholdSubmenuSelection) ? ST7735_YELLOW : ST7735_WHITE);
                    tft.print(i + 1);
                    tft.print(". ");
                    tft.println(thresholdMenuItems[i]);
                }
                
                // Alt bilgi çubuğu
                tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                tft.setCursor(5, tft.height() - 13);
                tft.setTextSize(1);
                tft.setTextColor(ST7735_CYAN);
                tft.println("^v:Secim >:Sec <:Geri");
                return true;
            }
            
            default:
                return false;
        }
    } else {
        // Ana alarm menüsü işleme
        switch (direction) {
            case JOY_UP: {
                if (menuSelection > 0) {
                    menuSelection--;
                    DisplayShowMenu(menuSelection, alarmMenuItems, itemCount);
                }
                return true;
            }
                
            case JOY_DOWN: {
                if (menuSelection < itemCount - 1) {
                    menuSelection++;
                    DisplayShowMenu(menuSelection, alarmMenuItems, itemCount);
                }
                return true;
            }
                
            case JOY_LEFT: {
                UINavigateToMenu(MENU_GENERAL_SETTINGS);
                return true;
            }
                
            case JOY_RIGHT:
            case JOY_PRESS: {
                switch (menuSelection) {
                    case 0: { // Alarm Durum
                        // Mevcut alarm durumunu göster
                        AlarmStatus currentAlarm = AlarmGetStatus();
                        AlarmThresholds thresholds = AlarmGetThresholds();
                        
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("ALARM DURUM");
                        
                        // Alarm durumu
                        tft.setCursor(5, 25);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.print("Alarmlar: ");
                        tft.setTextColor(thresholds.alarmEnabled ? ST7735_GREEN : ST7735_RED);
                        tft.println(thresholds.alarmEnabled ? "ACIK" : "KAPALI");
                        
                        // Aktif alarm bilgisi
                        tft.setCursor(5, 45);
                        tft.setTextColor(ST7735_WHITE);
                        tft.print("Aktif Alarm: ");
                        
                        if (currentAlarm.active) {
                            tft.setTextColor(ST7735_RED);
                            tft.println("VAR");
                            
                            // Alarm türü ve mesajı
                            tft.setCursor(5, 65);
                            tft.setTextColor(ST7735_WHITE);
                            
                            // Alarm türü
                            const char* alarmTypes[] = {
                                "Bilinmeyen",
                                "Yuksek Sicaklik",
                                "Dusuk Sicaklik",
                                "Yuksek Nem",
                                "Dusuk Nem",
                                "Sensor Hatasi",
                                "Elektrik Kesintisi",
                                "Sicaklik Farki",
                                "Nem Farki"
                            };
                            
                            int type = (int)currentAlarm.type;
                            if (type >= 0 && type < 9) {
                                tft.print("Tur: ");
                                tft.println(alarmTypes[type]);
                            }
                            
                            // Alarm mesajı
                            tft.setCursor(5, 80);
                            tft.setTextColor(ST7735_YELLOW);
                            
                            // Uzun mesajları birden çok satıra böl
                            String msg = currentAlarm.message;
                            int maxCharsPerLine = 21; // 21 karakter/satır (yaklaşık)
                            
                            for (int i = 0; i < msg.length(); i += maxCharsPerLine) {
                                int endPos = min(i + maxCharsPerLine, (int)msg.length());
                                String line = msg.substring(i, endPos);
                                tft.setCursor(5, 80 + (i / maxCharsPerLine) * 12);
                                tft.println(line);
                                
                                // Ekran sınırlarını aşarsa sonlandır
                                if (80 + (i / maxCharsPerLine) * 12 > tft.height() - 30) {
                                    break;
                                }
                            }
                        } else {
                            tft.setTextColor(ST7735_GREEN);
                            tft.println("YOK");
                        }
                        
                        // Alt bilgi çubuğu
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("Geri donmek icin tiklayin");
                        
                        waitForButton();
                        UINavigateToMenu(MENU_ALARM);
                        break;
                    }
                    
                    case 1: { // Alarm Eşikleri
                        // Alt menü moduna geç
                        inThresholdSubmenu = true;
                        thresholdSubmenuSelection = 0;
                        
                        // Eşik alt menüsünü çiz
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("ALARM ESIKLERI");
                        
                        // Menü öğelerini göster
                        int displayCount = min(thresholdCount, 6);
                        for (int i = 0; i < displayCount; i++) {
                            tft.setCursor(5, 25 + i * 18);
                            tft.setTextColor((i == thresholdSubmenuSelection) ? ST7735_YELLOW : ST7735_WHITE);
                            tft.print(i + 1);
                            tft.print(". ");
                            tft.println(thresholdMenuItems[i]);
                        }
                        
                        // Alt bilgi çubuğu
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("^v:Secim >:Sec <:Geri");
                        return true;
                    }
                    
                    case 2: { // Alarm Geçmişi
                        // Alarm geçmişini göster
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("ALARM GECMISI");
                        
                        int historyCount = AlarmGetHistoryCount();
                        
                        if (historyCount > 0) {
                            for (int i = 0; i < historyCount && i < 5; i++) { // En son 5 alarm
                                AlarmStatus alarmHistory = AlarmGetHistory(i);
                                
                                // Alarm türü
                                const char* alarmTypes[] = {
                                    "Bilinmeyen",
                                    "Yuksek Sicaklik",
                                    "Dusuk Sicaklik",
                                    "Yuksek Nem",
                                    "Dusuk Nem",
                                    "Sensor Hatasi",
                                    "Elektrik Kesintisi",
                                    "Sicaklik Farki",
                                    "Nem Farki"
                                };
                                
                                int type = (int)alarmHistory.type;
                                if (type >= 0 && type < 9) {
                                    tft.setCursor(5, 25 + i * 25);
                                    tft.setTextColor(ST7735_YELLOW);
                                    tft.print(i + 1);
                                    tft.print(". ");
                                    tft.println(alarmTypes[type]);
                                }
                                
                                // Alarm mesajı - sınırlandırılmış boyutla
                                tft.setCursor(5, 25 + i * 25 + 12);
                                tft.setTextColor(ST7735_WHITE);
                                
                                // Mesajı kısalt
                                String msg = alarmHistory.message;
                                if (msg.length() > 25) {
                                    msg = msg.substring(0, 22) + "...";
                                }
                                tft.println(msg);
                            }
                        } else {
                            tft.setCursor(10, tft.height() / 2);
                            tft.setTextColor(ST7735_YELLOW);
                            tft.println("Alarm kaydı yok!");
                        }
                        
                        // Alt bilgi çubuğu
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("Geri donmek icin tiklayin");
                        
                        waitForButton();
                        UINavigateToMenu(MENU_ALARM);
                        break;
                    }
                    
                    case 3: { // Alarm Testi
                        // Alarm testi yap
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("ALARM TESTI");
                        
                        tft.setCursor(20, tft.height() / 2 - 20);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.println("Test alarmı başlatılıyor...");
                        
                        // Test alarm çağrısı
                        AlarmTest();
                        
                        delay(2000); // Alarmın gösterilmesi için 2 saniye bekle
                        
                        UINavigateToMenu(MENU_ALARM);
                        break;
                    }
                }
                return true;
            }
            
            default:
                return false;
        }
    }
    
    return false;
}

// WiFi Menüsü İşleme
bool handleWifiMenu(JoystickDirection direction) {
    static const char* wifiMenuItems[] = {
        "WiFi Durum",
        "Erisim Noktasi Modu",
        "Aga Baglan",
        "WiFi Kapat"
    };
    static const int itemCount = sizeof(wifiMenuItems) / sizeof(wifiMenuItems[0]);
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, wifiMenuItems, itemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < itemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, wifiMenuItems, itemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_GENERAL_SETTINGS);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS:
            switch (menuSelection) {
                case 0: { // WiFi Durum
                    // WiFi durumunu göster
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("WIFI DURUM");
                    
                    WifiStatus status = WifiGetStatus();
                    
                    tft.setCursor(5, 30);
                    tft.setTextColor(ST7735_YELLOW);
                    tft.print("Durum: ");
                    
                    switch (status) {
                        case WIFI_STATUS_OFF:
                            tft.setTextColor(ST7735_RED);
                            tft.println("KAPALI");
                            break;
                            
                        case WIFI_STATUS_AP_MODE:
                            tft.setTextColor(ST7735_GREEN);
                            tft.println("AP MODU");
                            
                            tft.setCursor(5, 45);
                            tft.setTextColor(ST7735_WHITE);
                            tft.print("SSID: ");
                            tft.println(SYSTEM_NAME); // WIFI_AP_SSID_STR yerine SYSTEM_NAME kullanıldı
                            
                            tft.setCursor(5, 60);
                            tft.print("IP: ");
                            tft.println(WifiGetIP());
                            
                            tft.setCursor(5, 75);
                            tft.print("Bağlı İstemci: ");
                            tft.println(WifiGetClientCount());
                            break;
                            
                        case WIFI_STATUS_STA_MODE:
                            tft.setTextColor(ST7735_GREEN);
                            tft.println("STA MODU (BAGLI)");
                            
                            tft.setCursor(5, 45);
                            tft.setTextColor(ST7735_WHITE);
                            tft.print("IP: ");
                            tft.println(WifiGetIP());
                            break;
                            
                        case WIFI_STATUS_CONNECTING:
                            tft.setTextColor(ST7735_YELLOW);
                            tft.println("BAGLANIYOR...");
                            break;
                    }
                    
                    // Alt bilgi çubuğu
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("Geri donmek icin tiklayin");
                    
                    waitForButton();
                    UINavigateToMenu(MENU_WIFI);
                    break;
                }
                
                case 1: { // Erişim Noktası Modu
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("ERISIM NOKTASI MODU");
                    
                    tft.setCursor(5, 30);
                    tft.setTextColor(ST7735_YELLOW);
                    tft.println("WiFi AP modu baslatilacak");
                    
                    tft.setCursor(5, 50);
                    tft.setTextColor(ST7735_WHITE);
                    tft.print("SSID: ");
                    tft.println(SYSTEM_NAME); // WIFI_AP_SSID_STR yerine SYSTEM_NAME kullanıldı
                    
                    tft.setCursor(5, 65);
                    tft.setCursor(5, 65);
                    tft.print("Sifre: ");
                    tft.println(DEFAULT_AP_PASSWORD); // WIFI_AP_PASSWORD_STR yerine DEFAULT_AP_PASSWORD kullanıldı
                    
                    tft.setCursor(5, 85);
                    tft.setTextColor(ST7735_GREEN);
                    tft.println("Onayliyor musunuz?");
                    
                    tft.setCursor(5, 100);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("EVET - Joyistik SAG");
                    
                    tft.setCursor(5, 115);
                    tft.println("HAYIR - Joyistik SOL");
                    
                    // Kullanıcının seçimini bekle
                    JoystickDirection selection = JOY_NONE;
                    while (selection != JOY_LEFT && selection != JOY_RIGHT) {
                        selection = UIReadJoystick();
                        esp_task_wdt_reset();
                        delay(50);
                    }
                    
                    if (selection == JOY_RIGHT) {
                        // WiFi'yi kapat ve AP modunu başlat
                        WifiStop();
                        
                        if (WifiStartAP()) {
                            DisplayShowNotification("AP modu baslatildi!\nSSID: "
                                                  + String(SYSTEM_NAME)); // WIFI_AP_SSID_STR yerine SYSTEM_NAME kullanıldı
                        } else {
                            DisplayShowNotification("AP modu baslatma hatasi!");
                        }
                    }
                    
                    UINavigateToMenu(MENU_WIFI);
                    break;
                }
                
                case 2: { // Ağa Bağlan
                    // Bu fonksiyon, kullanıcıdan SSID ve şifre almak için ekranda birer menü sunar.
                    // Ekran ve arayüz kısıtlamaları nedeniyle, basitleştirilmiş bir yaklaşım kullanılır:
                    
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("AGA BAGLAN");
                    
                    tft.setCursor(5, 30);
                    tft.setTextColor(ST7735_YELLOW);
                    tft.println("Bu ozellik joyistik ile");
                    tft.setCursor(5, 42);
                    tft.println("kullanılamaz.");
                    
                    tft.setCursor(5, 60);
                    tft.setTextColor(ST7735_GREEN);
                    tft.println("Baglanmak icin lutfen");
                    tft.setCursor(5, 72);
                    tft.println("Android uygulamasini");
                    tft.setCursor(5, 84);
                    tft.println("kullanin.");
                    
                    // Alt bilgi çubuğu
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("Geri donmek icin tiklayin");
                    
                    waitForButton();
                    UINavigateToMenu(MENU_WIFI);
                    break;
                }
                
                case 3: { // WiFi Kapat
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("WIFI KAPAT");
                    
                    tft.setCursor(5, 30);
                    tft.setTextColor(ST7735_YELLOW);
                    tft.println("WiFi kapatilacak.");
                    tft.setCursor(5, 45);
                    tft.println("Tum baglantilar kesilecek.");
                    
                    tft.setCursor(5, 70);
                    tft.setTextColor(ST7735_GREEN);
                    tft.println("Onayliyor musunuz?");
                    
                    tft.setCursor(5, 90);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("EVET - Joyistik SAG");
                    
                    tft.setCursor(5, 105);
                    tft.println("HAYIR - Joyistik SOL");
                    
                    // Kullanıcının seçimini bekle
                    JoystickDirection selection = JOY_NONE;
                    while (selection != JOY_LEFT && selection != JOY_RIGHT) {
                        selection = UIReadJoystick();
                        esp_task_wdt_reset();
                        delay(50);
                    }
                    
                    if (selection == JOY_RIGHT) {
                        // WiFi'yi kapat
                        if (WifiStop()) {
                            DisplayShowNotification("WiFi kapatildi!");
                        } else {
                            DisplayShowNotification("WiFi kapatma hatasi!");
                        }
                    }
                    
                    UINavigateToMenu(MENU_WIFI);
                    break;
                }
            }
            return true;
            
        default:
            return false;
    }
}

// Seçim bekleme fonksiyonu - bir numara seçilmesini bekler
int waitForSelection(int maxOption) {
    int selection = -1;
    
    while (selection < 0) {
        JoystickDirection dir = UIReadJoystick();
        
        // Watchdog'u besle
        esp_task_wdt_reset();
        
        if (dir == JOY_PRESS) {
            // Joystick'e basıldığında seçim numarası 1 olarak kabul edilir
            selection = 0;
        } else if (dir == JOY_LEFT) {
            // Geri düğmesine basıldığında iptal et
            return -1;
        } else if (dir >= JOY_UP && dir <= JOY_RIGHT) {
            // Joystick yönü seçim olarak kullanılabilir
            int joyValue = (int)dir - (int)JOY_UP;
            if (joyValue >= 0 && joyValue < maxOption) {
                selection = joyValue;
            }
        }
        
        // Menü zamanaşımı kontrolü
        if (millis() - lastMenuActivity > MENU_TIMEOUT) {
            LOG_INFO(TAG, "Beklerken zaman aşımı, ana ekrana dönülüyor");
            UIBackToMain();
            return -1;
        }
        
        delay(50);
    }
    
    return selection;
}

 // Tarih ve Saat Menüsü İşleme - Yeni Düzenleme
 bool handleTimeDateMenu(JoystickDirection direction) {
    static enum TimeDateState {
        TD_MENU,
        TD_EDIT_DATE,
        TD_EDIT_TIME,
        TD_NTP_SYNC
    } timeDateState = TD_MENU;
    
    static TimeInfo editingTimeInfo;
    static int cursorPos = 0; // 0: gün, 1: ay, 2: yıl | 0: saat, 1: dakika, 2: saniye
    
    if (timeDateState == TD_MENU) {
        // Menü görünümü
        static const char* timeDateMenuItems[] = {
            "Tarihi Ayarla",
            "Saati Ayarla",
            "NTP ile Senkronize Et"
        };
        static const int itemCount = 3;
        
        switch (direction) {
            case JOY_UP:
                if (menuSelection > 0) {
                    menuSelection--;
                    DisplayShowMenu(menuSelection, timeDateMenuItems, itemCount);
                }
                return true;
                
            case JOY_DOWN:
                if (menuSelection < itemCount - 1) {
                    menuSelection++;
                    DisplayShowMenu(menuSelection, timeDateMenuItems, itemCount);
                }
                return true;
                
            case JOY_LEFT:
                UINavigateToMenu(MENU_GENERAL_SETTINGS);
                return true;
                
            case JOY_RIGHT:
            case JOY_PRESS: {
                int cursorX;
                
                switch (menuSelection) {
                    case 0: // Tarihi Ayarla
                        timeDateState = TD_EDIT_DATE;
                        editingTimeInfo = GetCurrentTime();
                        cursorPos = 0; // Gün seçili
                        
                        // Tarih düzenleme ekranını göster
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("TARIH AYARLA");
                        
                        // Tarih değerini göster
                        char dateStr[11];
                        sprintf(dateStr, "%02d.%02d.%04d", 
                                editingTimeInfo.day, 
                                editingTimeInfo.month, 
                                editingTimeInfo.year);
                        
                        cursorX = (tft.width() - 10*12) / 2;
                        tft.setCursor(cursorX, tft.height() / 2 - 10);
                        tft.setTextSize(2);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.print(dateStr);
                        
                        // İlk imleç pozisyonunu göster (gün)
                        tft.drawLine(cursorX, tft.height() / 2 + 10, 
                                   cursorX + 24, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                        
                        // Alt bilgi çubuğu
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("<>:Secim ^v:Degistir >:Kaydet");
                        
                        editMode = true;
                        break;
                        
                    case 1: // Saati Ayarla
                        timeDateState = TD_EDIT_TIME;
                        editingTimeInfo = GetCurrentTime();
                        cursorPos = 0; // Saat seçili
                        
                        // Saat düzenleme ekranını göster
                        tft.fillScreen(ST7735_BLACK);
                        tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                        tft.setCursor(5, 5);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_WHITE);
                        tft.println("SAAT AYARLA");
                        
                        // Saat değerini göster
                        char timeStr[9];
                        sprintf(timeStr, "%02d:%02d:%02d", 
                                editingTimeInfo.hour, 
                                editingTimeInfo.minute, 
                                editingTimeInfo.second);
                        
                        cursorX = (tft.width() - 8*12) / 2;
                        tft.setCursor(cursorX, tft.height() / 2 - 10);
                        tft.setTextSize(2);
                        tft.setTextColor(ST7735_YELLOW);
                        tft.print(timeStr);
                        
                        // İlk imleç pozisyonunu göster (saat)
                        tft.drawLine(cursorX, tft.height() / 2 + 10, 
                                   cursorX + 24, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                        
                        // Alt bilgi çubuğu
                        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                        tft.setCursor(5, tft.height() - 13);
                        tft.setTextSize(1);
                        tft.setTextColor(ST7735_CYAN);
                        tft.println("<>:Secim ^v:Degistir >:Kaydet");
                        
                        editMode = true;
                        break;
                        
                    case 2: // NTP ile Senkronize Et
                        timeDateState = TD_NTP_SYNC;
                        
                        if (WifiGetStatus() == WIFI_STATUS_STA_MODE) {
                            if (SyncTimeWithNTP()) {
                                DisplayShowNotification("Zaman NTP ile senkronize edildi");
                            } else {
                                DisplayShowNotification("NTP senkronizasyonu başarısız!");
                            }
                        } else {
                            DisplayShowNotification("NTP senkronizasyonu için\nWiFi bağlantısı gerekli!");
                        }
                        
                        timeDateState = TD_MENU;
                        UINavigateToMenu(MENU_TIME_DATE);
                        break;
                }
                return true;
            }
                
            default:
                return false;
        }
    }
    else if (timeDateState == TD_EDIT_DATE) {
        // Tarih düzenleme modu
        int cursorX = (tft.width() - 10*12) / 2;
        
        switch (direction) {
            case JOY_UP:
// Değeri artır
                switch (cursorPos) {
                    case 0: // Gün
                        editingTimeInfo.day++;
                        if (editingTimeInfo.day > 31) editingTimeInfo.day = 1;
                        break;
                    case 1: // Ay
                        editingTimeInfo.month++;
                        if (editingTimeInfo.month > 12) editingTimeInfo.month = 1;
                        break;
                    case 2: // Yıl
                        editingTimeInfo.year++;
                        if (editingTimeInfo.year > 2100) editingTimeInfo.year = 2000;
                        break;
                }
                
                // Değeri güncelle
                char dateStr[11];
                sprintf(dateStr, "%02d.%02d.%04d", 
                        editingTimeInfo.day, 
                        editingTimeInfo.month, 
                        editingTimeInfo.year);
                
                tft.fillRect(cursorX, tft.height() / 2 - 10, 10*12, 20, ST7735_BLACK);
                tft.setCursor(cursorX, tft.height() / 2 - 10);
                tft.setTextSize(2);
                tft.setTextColor(ST7735_YELLOW);
                tft.print(dateStr);
                
                // İmleci göster
                tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                if (cursorPos == 0) { // Gün
                    tft.drawLine(cursorX, tft.height() / 2 + 10, 
                               cursorX + 24, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 1) { // Ay
                    tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                               cursorX + 60, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 2) { // Yıl
                    tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                               cursorX + 120, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                }
                return true;
                
            case JOY_DOWN:
                // Değeri azalt
                switch (cursorPos) {
                    case 0: // Gün
                        editingTimeInfo.day--;
                        if (editingTimeInfo.day < 1) editingTimeInfo.day = 31;
                        break;
                    case 1: // Ay
                        editingTimeInfo.month--;
                        if (editingTimeInfo.month < 1) editingTimeInfo.month = 12;
                        break;
                    case 2: // Yıl
                        editingTimeInfo.year--;
                        if (editingTimeInfo.year < 2000) editingTimeInfo.year = 2100;
                        break;
                }
                
                // Değeri güncelle
                sprintf(dateStr, "%02d.%02d.%04d", 
                        editingTimeInfo.day, 
                        editingTimeInfo.month, 
                        editingTimeInfo.year);
                
                tft.fillRect(cursorX, tft.height() / 2 - 10, 10*12, 20, ST7735_BLACK);
                tft.setCursor(cursorX, tft.height() / 2 - 10);
                tft.setTextSize(2);
                tft.setTextColor(ST7735_YELLOW);
                tft.print(dateStr);
                
                // İmleci göster
                tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                if (cursorPos == 0) { // Gün
                    tft.drawLine(cursorX, tft.height() / 2 + 10, 
                               cursorX + 24, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 1) { // Ay
                    tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                               cursorX + 60, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 2) { // Yıl
                    tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                               cursorX + 120, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                }
                return true;
                
            case JOY_LEFT:
                // İmleci sola kaydır
                cursorPos--;
                if (cursorPos < 0) cursorPos = 2;
                
                // İmleci göster
                tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                if (cursorPos == 0) { // Gün
                    tft.drawLine(cursorX, tft.height() / 2 + 10, 
                               cursorX + 24, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 1) { // Ay
                    tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                               cursorX + 60, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 2) { // Yıl
                    tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                               cursorX + 120, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                }
                return true;
                
            case JOY_RIGHT:
                if (cursorPos < 2) {
                    // İmleci sağa kaydır
                    cursorPos++;
                    
                    // İmleci göster
                    tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                    if (cursorPos == 0) { // Gün
                        tft.drawLine(cursorX, tft.height() / 2 + 10, 
                                   cursorX + 24, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                    } else if (cursorPos == 1) { // Ay
                        tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                                   cursorX + 60, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                    } else if (cursorPos == 2) { // Yıl
                        tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                                   cursorX + 120, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                    }
                    return true;
                }
                // En sağdaysa kaydet
                // Bilinçli olarak JOY_PRESS durumuna düşüyor
                
            case JOY_PRESS: {
                // Değişiklikleri kaydet
                TimeInfo currentTime = GetCurrentTime();
                
                // Sadece tarih değerlerini güncelle, saat değerlerini mevcut zamandan al
                editingTimeInfo.hour = currentTime.hour;
                editingTimeInfo.minute = currentTime.minute;
                editingTimeInfo.second = currentTime.second;
                
                // Tarihi ayarla
                SetTime(editingTimeInfo);
                
                // Düzenleme modundan çık
                editMode = false;
                timeDateState = TD_MENU;
                
                // Bildirim göster
                DisplayShowNotification("Tarih kaydedildi!");
                
                // Menüye dön
                UINavigateToMenu(MENU_TIME_DATE);
                return true;
            }
                
            default:
                return false;
        }
    }
    else if (timeDateState == TD_EDIT_TIME) {
        // Saat düzenleme modu
        int cursorX = (tft.width() - 8*12) / 2;
        
        switch (direction) {
            case JOY_UP:
                // Değeri artır
                switch (cursorPos) {
                    case 0: // Saat
                        editingTimeInfo.hour++;
                        if (editingTimeInfo.hour > 23) editingTimeInfo.hour = 0;
                        break;
                    case 1: // Dakika
                        editingTimeInfo.minute++;
                        if (editingTimeInfo.minute > 59) editingTimeInfo.minute = 0;
                        break;
                    case 2: // Saniye
                        editingTimeInfo.second++;
                        if (editingTimeInfo.second > 59) editingTimeInfo.second = 0;
                        break;
                }
                
                // Değeri güncelle
                char timeStr[9];
                sprintf(timeStr, "%02d:%02d:%02d", 
                        editingTimeInfo.hour, 
                        editingTimeInfo.minute, 
                        editingTimeInfo.second);
                
                tft.fillRect(cursorX, tft.height() / 2 - 10, 8*12, 20, ST7735_BLACK);
                tft.setCursor(cursorX, tft.height() / 2 - 10);
                tft.setTextSize(2);
                tft.setTextColor(ST7735_YELLOW);
                tft.print(timeStr);
                
                // İmleci göster
                tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                if (cursorPos == 0) { // Saat
                    tft.drawLine(cursorX, tft.height() / 2 + 10, 
                               cursorX + 24, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 1) { // Dakika
                    tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                               cursorX + 60, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 2) { // Saniye
                    tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                               cursorX + 96, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                }
                return true;
                
            case JOY_DOWN:
                // Değeri azalt
                switch (cursorPos) {
                    case 0: // Saat
                        if (editingTimeInfo.hour == 0) {
                            editingTimeInfo.hour = 23;
                        } else {
                            editingTimeInfo.hour--;
                        }
                        break;
                    case 1: // Dakika
                        if (editingTimeInfo.minute == 0) {
                            editingTimeInfo.minute = 59;
                        } else {
                            editingTimeInfo.minute--;
                        }
                        break;
                    case 2: // Saniye
                        if (editingTimeInfo.second == 0) {
                            editingTimeInfo.second = 59;
                        } else {
                            editingTimeInfo.second--;
                        }
                        break;
                }
                
                // Değeri güncelle
                sprintf(timeStr, "%02d:%02d:%02d", 
                        editingTimeInfo.hour, 
                        editingTimeInfo.minute, 
                        editingTimeInfo.second);
                
                tft.fillRect(cursorX, tft.height() / 2 - 10, 8*12, 20, ST7735_BLACK);
                tft.setCursor(cursorX, tft.height() / 2 - 10);
                tft.setTextSize(2);
                tft.setTextColor(ST7735_YELLOW);
                tft.print(timeStr);
                
                // İmleci göster
                tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                if (cursorPos == 0) { // Saat
                    tft.drawLine(cursorX, tft.height() / 2 + 10, 
                               cursorX + 24, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 1) { // Dakika
                    tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                               cursorX + 60, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 2) { // Saniye
                    tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                               cursorX + 96, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                }
                return true;
                
            case JOY_LEFT:
                // İmleci sola kaydır
                cursorPos--;
                if (cursorPos < 0) cursorPos = 2;
                
                // İmleci göster
                tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                if (cursorPos == 0) { // Saat
                    tft.drawLine(cursorX, tft.height() / 2 + 10, 
                               cursorX + 24, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 1) { // Dakika
                    tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                               cursorX + 60, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                } else if (cursorPos == 2) { // Saniye
                    tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                               cursorX + 96, tft.height() / 2 + 10, 
                               ST7735_CYAN);
                }
                return true;
                
            case JOY_RIGHT:
                if (cursorPos < 2) {
                    // İmleci sağa kaydır
                    cursorPos++;
                    
                    // İmleci göster
                    tft.fillRect(0, tft.height() / 2 + 10, tft.width(), 2, ST7735_BLACK);
                    if (cursorPos == 0) { // Saat
                        tft.drawLine(cursorX, tft.height() / 2 + 10, 
                                   cursorX + 24, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                    } else if (cursorPos == 1) { // Dakika
                        tft.drawLine(cursorX + 36, tft.height() / 2 + 10, 
                                   cursorX + 60, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                    } else if (cursorPos == 2) { // Saniye
                        tft.drawLine(cursorX + 72, tft.height() / 2 + 10, 
                                   cursorX + 96, tft.height() / 2 + 10, 
                                   ST7735_CYAN);
                    }
                    return true;
                }
                // En sağdaysa kaydet
                // Bilinçli olarak JOY_PRESS durumuna düşüyor
                
            case JOY_PRESS: {
                // Değişiklikleri kaydet
                TimeInfo currentTime = GetCurrentTime();
                
                // Sadece saat değerlerini güncelle, tarih değerlerini mevcut zamandan al
                editingTimeInfo.year = currentTime.year;
                editingTimeInfo.month = currentTime.month;
                editingTimeInfo.day = currentTime.day;
                
                // Saati ayarla
                SetTime(editingTimeInfo);
                
                // Düzenleme modundan çık
                editMode = false;
                timeDateState = TD_MENU;
                
                // Bildirim göster
                DisplayShowNotification("Saat kaydedildi!");
                
                // Menüye dön
                UINavigateToMenu(MENU_TIME_DATE);
                return true;
            }
                
            default:
                return false;
        }
    }
    
    return false;
}

// Sıcaklık ve Nem Menüsü İşleme - Yeni Düzenleme
bool handleTempHumidityMenu(JoystickDirection direction) {
    static const char* tempHumidityMenuItems[] = {
        "Hedef Sicaklik",
        "Hedef Nem",
        "PID Parametreleri",
        "Histerezis Ayari"
    };
    static const int itemCount = sizeof(tempHumidityMenuItems) / sizeof(tempHumidityMenuItems[0]);
    
    IncubationSettings settings = LoadSettings();
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, tempHumidityMenuItems, itemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < itemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, tempHumidityMenuItems, itemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_MAIN);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS: {
            switch(menuSelection) {
                case 0: { // Hedef Sıcaklık
                    // Sıcaklık değeri için UISingleNumericInput kullan
                    float newTemp = UISingleNumericInput("Hedef Sicaklik", 30.0, 45.0, settings.targetTemp);
                    
                    // Değer değiştiyse kaydet ve uygula
                    if (newTemp != settings.targetTemp) {
                        settings.targetTemp = newTemp;
                        SaveSettings(settings);
                        ControlSetTargets(settings.targetTemp, settings.targetHumidity);
                        
                        DisplayShowNotification("Hedef sicaklik kaydedildi!");
                        delay(1500);
                        UIBackToMain(); // Ana ekrana dön ve yeni değerleri göster
                    } else {
                        UINavigateToMenu(MENU_TEMP_HUMIDITY);
                    }
                    return true;
                }
                
                case 1: { // Hedef Nem
                    // Nem değeri için UISingleNumericInput kullan
                    float newHum = UISingleNumericInput("Hedef Nem", 30.0, 90.0, settings.targetHumidity);
                    
                    // Değer değiştiyse kaydet ve uygula
                    if (newHum != settings.targetHumidity) {
                        settings.targetHumidity = newHum;
                        SaveSettings(settings);
                        ControlSetTargets(settings.targetTemp, settings.targetHumidity);
                        
                        DisplayShowNotification("Hedef nem kaydedildi!");
                        delay(1500);
                        UIBackToMain(); // Ana ekrana dön ve yeni değerleri göster
                    } else {
                        UINavigateToMenu(MENU_TEMP_HUMIDITY);
                    }
                    return true;
                }
                    
                case 2: { // PID Parametreleri
                    // Basitleştirilmiş PID parametre ayarı
                    float kp = UISingleNumericInput("PID Kp", 0.0, 100.0, settings.pidKp);
                    float ki = UISingleNumericInput("PID Ki", 0.0, 10.0, settings.pidKi);
                    float kd = UISingleNumericInput("PID Kd", 0.0, 500.0, settings.pidKd);
                    
                    // Değerleri güncelle
                    if (kp != settings.pidKp || ki != settings.pidKi || kd != settings.pidKd) {
                        settings.pidKp = kp;
                        settings.pidKi = ki;
                        settings.pidKd = kd;
                        SaveSettings(settings);
                        
                        // PID kontrolörünü güncelle
                        ControlSetPidParams(kp, ki, kd);
                        
                        DisplayShowNotification("PID parametreleri kaydedildi!");
                        delay(1500);
                    }
                    
                    UINavigateToMenu(MENU_TEMP_HUMIDITY);
                    return true;
                }
                
                case 3: { // Histerezis Ayarı
                    float hysteresis = UISingleNumericInput("Nem Histerezisi", 0.5, 10.0, settings.humHysteresis);
                    
                    // Değeri güncelle
                    if (hysteresis != settings.humHysteresis) {
                        settings.humHysteresis = hysteresis;
                        SaveSettings(settings);
                        
                        // Kontrol modülünü güncelle
                        ControlSetHumHysteresis(hysteresis);
                        
                        DisplayShowNotification("Histerezis kaydedildi!");
                        delay(1500);
                    }
                    
                    UINavigateToMenu(MENU_TEMP_HUMIDITY);
                    return true;
                }
            }
            return true;
        }
            
        default:
            return false;
    }
}

// Sadeleştirilmiş sayısal değer giriş fonksiyonu
float UISingleNumericInput(const char* title, float minValue, float maxValue, float defaultValue) {
    float value = defaultValue;
    float step = 0.1; // Varsayılan adım boyutu - sonradan değişecek
    bool largeStep = false; // Adım boyutu değiştirme
    
    // Ekranı temizle
    tft.fillScreen(ST7735_BLACK);
    
    // Başlık
    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
    tft.setCursor(5, 5);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.println(title);
    
    // Düzenleme moduna gir
    editMode = true;
    
    // Değer aralığını göster
    tft.setCursor(5, 25);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_CYAN);
    tft.print("Min: ");
    tft.print(minValue, (minValue == (int)minValue) ? 0 : 1);
    tft.print(" Max: ");
    tft.print(maxValue, (maxValue == (int)maxValue) ? 0 : 1);
    
    // Sıcaklık değeri için her zaman ondalık kullan
    bool isTempSetting = (title && strstr(title, "Sicaklik"));
    bool integerOnly = !isTempSetting && (minValue == (int)minValue && maxValue == (int)maxValue);
    
    // Tam sayı değerler için varsayılan adımı 1.0 yapalım (sıcaklık hariç)
    if (integerOnly) {
        step = 1.0;
    } else if (isTempSetting) {
        // Sıcaklık için her zaman 0.1 adım
        step = 0.1;
    }
    
    while (true) {
        // Adım boyutuna göre değer metnini formatla
        char valueStr[12];
        if (!integerOnly || isTempSetting) {
            sprintf(valueStr, "%.1f", value); // Ondalıklı değer
        } else {
            sprintf(valueStr, "%.0f", value); // Tam sayı
        }
        
        // Metin genişliğini ve merkez konumunu hesapla
        int textWidth = strlen(valueStr) * 18; // Size 3 fontunun her karakteri yaklaşık 18px
        int xPos = (tft.width() - textWidth) / 2;
        
        // Değer alanını temizle
        tft.fillRect(0, 50, tft.width(), 30, ST7735_BLACK);
        
        // Ortalanmış değeri göster
        tft.setCursor(xPos, 50);
        tft.setTextSize(3);
        tft.setTextColor(ST7735_YELLOW);
        tft.print(valueStr);
        
        // Adım boyutu bilgisi
        tft.fillRect(5, 90, tft.width() - 10, 10, ST7735_BLACK);
        tft.setCursor(5, 90);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_GREEN);
        tft.print("Adim: ");
        
        // Adım değerini belirle ve göster
        if (largeStep) {
            // Büyük adım için değer aralığına göre ayarla (sıcaklık hariç)
            if (isTempSetting) {
                // Sıcaklık için büyük adım da 0.1 olsun
                step = 0.1;
                tft.print("0.1");
            } else {
                float range = maxValue - minValue;
                if (range > 100) {
                    step = 10.0;
                    tft.print("10");
                } else if (range > 30) {
                    step = 5.0;
                    tft.print("5");
                } else {
                    step = 1.0;
                    tft.print("1");
                }
            }
        } else {
            // Küçük adım - tam sayı mı ondalıklı mı?
            if (integerOnly && !isTempSetting) {
                step = 1.0;
                tft.print("1");
            } else {
                step = 0.1;
                tft.print("0.1");
            }
        }
        
        // Adımdan sonra gerekiyorsa birim göster
        if (isTempSetting) {
            tft.println("C");
        } else if (title && strstr(title, "Nem")) {
            tft.println("%");
        } else if (title && strstr(title, "Suresi") && step >= 1) {
            tft.println(" sn");
        } else if (title && strstr(title, "Arasi") && step >= 1) {
            tft.println(" dk");
        } else {
            tft.println("");
        }
        
        // Alt bilgi çubuğu
        tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
        tft.setCursor(5, tft.height() - 13);
        tft.setTextSize(1);
        tft.setTextColor(ST7735_CYAN);
        tft.println("^v:Degistir <>:Adim >:Kaydet");
        
        // Joystick girişini bekle
        JoystickDirection dir;
        do {
            dir = UIReadJoystick();
            // Watchdog'u besle
            esp_task_wdt_reset();
            delay(10);
        } while (dir == JOY_NONE);
        
        // Girişi işle
        switch (dir) {
            case JOY_UP:
                // Değeri artır
                value += step;
                if (value > maxValue) value = maxValue;
                
                // Tam sayı değer ise, yuvarlayalım (sıcaklık hariç)
                if (integerOnly && !isTempSetting) {
                    value = round(value);
                }
                break;
                
            case JOY_DOWN:
                // Değeri azalt
                value -= step;
                if (value < minValue) value = minValue;
                
                // Tam sayı değer ise, yuvarlayalım (sıcaklık hariç)
                if (integerOnly && !isTempSetting) {
                    value = round(value);
                }
                break;
                
            case JOY_LEFT:
                // Küçük adım moduna geç
                largeStep = false;
                break;
                
            case JOY_RIGHT:
                // Büyük adım moduna geç veya değeri kaydet
                if (largeStep) {
                    editMode = false;
                    return value;
                } else {
                    largeStep = true;
                }
                break;
                
            case JOY_PRESS:
                // Değeri kaydet
                editMode = false;
                return value;
                
            default:
                break;
        }
        
        // Debounce gecikmesi
        delay(100);
    }
}

// Motor Menüsü İşleme - Yeni Düzenleme
bool handleMotorMenu(JoystickDirection direction) {
    static const char* motorMenuItems[] = {
        "Donus Suresi",
        "Donus Arasi",
        "Motor Testi",
        "Motor Durum"
    };
    static const int itemCount = sizeof(motorMenuItems) / sizeof(motorMenuItems[0]);
    
    MotorSettings motorSettings = LoadMotorSettings();
    
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, motorMenuItems, itemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < itemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, motorMenuItems, itemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_MAIN);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS: {
            switch(menuSelection) {
                case 0: { // Dönüş Süresi
                    // Dönüş süresini saniye cinsinden al ve göster
                    int currentSeconds = motorSettings.duration / 1000;
                    int newSeconds = (int)UISingleNumericInput("Donus Suresi (sn)", 1.0, 60.0, currentSeconds);
                    
                    if (newSeconds != currentSeconds) {
                        motorSettings.duration = newSeconds * 1000;
                        SaveMotorSettings(motorSettings);
                        ControlSetMotorSettings(motorSettings.duration, motorSettings.interval);
                        
                        DisplayShowNotification("Motor suresi kaydedildi!");
                        delay(1500);
                    }
                    
                    UINavigateToMenu(MENU_MOTOR);
                    return true;
                }
                
                case 1: { // Dönüş Aralığı
                    // Dönüş aralığını dakika cinsinden al ve göster
                    int currentMinutes = motorSettings.interval / 60000;
                    int newMinutes = (int)UISingleNumericInput("Donus Arasi (dk)", 5.0, 240.0, currentMinutes);
                    
                    if (newMinutes != currentMinutes) {
                        motorSettings.interval = newMinutes * 60000;
                        SaveMotorSettings(motorSettings);
                        ControlSetMotorSettings(motorSettings.duration, motorSettings.interval);
                        
                        DisplayShowNotification("Motor araligi kaydedildi!");
                        delay(1500);
                    }
                    
                    UINavigateToMenu(MENU_MOTOR);
                    return true;
                }
                    
                case 2: { // Motor Testi
                    DisplayShowNotification("Motor testi baslatiliyor...");
                    ControlTurnMotor();
                    delay(500);
                    
                    // Kısa bir gecikme sonrası bildirim
                    DisplayShowNotification("Motor calisiyor, " + 
                                         String(motorSettings.duration / 1000) + 
                                         " sn sonra duracak");
                    
                    delay(1500);
                    UINavigateToMenu(MENU_MOTOR);
                    return true;
                }
                
                case 3: { // Motor Durumu
                    // Motor durum ekranını göster
                    tft.fillScreen(ST7735_BLACK);
                    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
                    tft.setCursor(5, 5);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.println("MOTOR DURUMU");
                    
                    RelayState relayState = ControlGetRelayState();
                    int motorCountdown = GetMotorCountdown();
                    
                    // Motor durumunu ortala ve göster
                    tft.setCursor((tft.width() - 8*12) / 2, tft.height() / 2 - 40);
                    tft.setTextSize(2);
                    tft.setTextColor(relayState.motor ? ST7735_GREEN : ST7735_LIGHTGREY);
                    tft.print(relayState.motor ? "ACIK" : "KAPALI");
                    
                    // Geri sayımı ortala ve göster
                    char countStr[8];
                    sprintf(countStr, "%d dk", motorCountdown);
                    tft.setCursor((tft.width() - strlen(countStr)*12) / 2, tft.height() / 2 - 10);
                    tft.setTextColor(ST7735_YELLOW);
                    tft.print(motorCountdown);
                    tft.print(" dk");
                    
                    // Süre ve aralık bilgisi
                    tft.setCursor(5, tft.height() / 2 + 20);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_WHITE);
                    tft.print("Donus Suresi: ");
                    tft.print(motorSettings.duration / 1000);
                    tft.println(" sn");
                    
                    tft.setCursor(5, tft.height() / 2 + 35);
                    tft.print("Donus Arasi: ");
                    tft.print(motorSettings.interval / 60000);
                    tft.println(" dk");
                    
                    // Alt bilgi çubuğu
                    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
                    tft.setCursor(5, tft.height() - 13);
                    tft.setTextSize(1);
                    tft.setTextColor(ST7735_CYAN);
                    tft.println("Geri donmek icin tiklayin");
                    
                    // Joystick butonu basılana kadar bekle
                    waitForButton();
                    UINavigateToMenu(MENU_MOTOR);
                    return true;
                }
            }
            return true;
        }
            
        default:
            return false;
    }
}


// Ana menü işlemleri
bool handleMainMenu(JoystickDirection direction) {
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, mainMenuItems, mainMenuItemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < mainMenuItemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, mainMenuItems, mainMenuItemCount);
            }
            return true;
            
        case JOY_LEFT:
            // Ana ekrana dön
            UIBackToMain();
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS:
            // Seçilen alt menüye git
            switch (menuSelection) {
                case 0: UINavigateToMenu(MENU_TEMP_HUMIDITY); break;
                case 1: UINavigateToMenu(MENU_MOTOR); break;
                case 2: UINavigateToMenu(MENU_PROFILE); break;
                case 3: UINavigateToMenu(MENU_GRAPH); break;
                case 4: UINavigateToMenu(MENU_POWER_SAVE); break;
                case 5: UINavigateToMenu(MENU_GENERAL_SETTINGS); break;
            }
            return true;
            
        default:
            return false;
    }
}

// Genel Ayarlar menüsü işlemleri
bool handleGeneralSettingsMenu(JoystickDirection direction) {
    switch (direction) {
        case JOY_UP:
            if (menuSelection > 0) {
                menuSelection--;
                DisplayShowMenu(menuSelection, generalSettingsMenuItems, generalSettingsMenuItemCount);
            }
            return true;
            
        case JOY_DOWN:
            if (menuSelection < generalSettingsMenuItemCount - 1) {
                menuSelection++;
                DisplayShowMenu(menuSelection, generalSettingsMenuItems, generalSettingsMenuItemCount);
            }
            return true;
            
        case JOY_LEFT:
            UINavigateToMenu(MENU_MAIN);
            return true;
            
        case JOY_RIGHT:
        case JOY_PRESS:
            // Seçilen alt menüye git
            switch (menuSelection) {
                case 0: UINavigateToMenu(MENU_TIME_DATE); break;
                case 1: UINavigateToMenu(MENU_ALARM); break;
                case 2: UINavigateToMenu(MENU_WIFI); break;
                case 3: UINavigateToMenu(MENU_CALIBRATION); break;
                case 4: UINavigateToMenu(MENU_BACKUP); break;
                case 5: showSystemInfo(); break;
            }
            return true;
            
        default:
            return false;
    }
}

// Ana ekrana dön
void UIBackToMain() {
    currentMenu = MENU_NONE;
    editMode = false;
    
    // Ayarları doğrudan yükle
    IncubationSettings settings = LoadSettings();
    
    // Sensör verilerini al
    SensorData sensorData = GetSensorData();
    RelayState relayState = ControlGetRelayState();
    
    // Kuluçka günü hesaplama
    int currentDay = 0;
    int totalDays = settings.totalDays;
    
    if (settings.startTime > 0) {
        currentDay = GetIncubationDay(settings.startTime);
        LOG_INFO(TAG, "Kuluçka günü hesaplandı: %d/%d", currentDay, totalDays);
    }
    
    // Ana ekranı tam olarak yeniden çizmek için isFirstMainDisplay = true olarak ayarla
    isFirstMainDisplay = true;
    
    // Ana ekranı göster
    DisplayShowMain(
        sensorData.avgTemperature,
        sensorData.avgHumidity,
        currentDay,
        totalDays,
        relayState.heater,
        relayState.humidifier,
        relayState.motor
    );
    
    LOG_INFO(TAG, "Ana ekrana dönüldü");
}

// Belirli bir menüye git
void UINavigateToMenu(MenuState menu) {
    menuSelection = 0;
    currentMenu = menu;
    lastMenuActivity = millis(); // Menü zamanaşımını sıfırla
    
    // Ekranı temizle
    tft.fillScreen(ST7735_BLACK);
    
    // Menüye göre uygun ekranı göster
    switch (menu) {
        case MENU_MAIN:
            UIShowMainMenu();
            break;
        case MENU_GENERAL_SETTINGS:
            UIShowGeneralSettingsMenu();
            break;
        case MENU_TEMP_HUMIDITY:
            UIShowTempHumidityMenu();
            break;
        case MENU_MOTOR:
            UIShowMotorMenu();
            break;
        case MENU_PROFILE:
            UIShowProfileMenu();
            break;
        case MENU_GRAPH:
            UIShowGraphMenu();
            break;
        case MENU_POWER_SAVE:
            UIShowPowerSaveMenu();
            break;
        case MENU_TIME_DATE:
            UIShowTimeDateMenu();
            break;
        case MENU_ALARM:
            UIShowAlarmMenu();
            break;
        case MENU_WIFI:
            UIShowWifiMenu();
            break;
        case MENU_CALIBRATION:
            UIShowCalibrationMenu();
            break;
        case MENU_BACKUP:
            UIShowBackupMenu();
            break;
        case MENU_NONE:
            UIBackToMain();
            break;
    }
}

void UIShowSavedAndReturnToMenu(const char* message, MenuState returnMenu) {
    // Kaydedildi mesajını göster
    DisplayShowNotification(message);
    
    // Kısa bir gecikme sonrası menüye dön
    delay(1500);
    
    // Menüye dönüş
    UINavigateToMenu(returnMenu);
}

// Menü ekranlarını oluşturan temel fonksiyonlar
void UIShowMainMenu() {
    menuSelection = 0;
    currentMenu = MENU_MAIN;
    DisplayShowMenu(menuSelection, mainMenuItems, mainMenuItemCount);
    LOG_INFO(TAG, "Ana menü gösteriliyor");
}

void UIShowGeneralSettingsMenu() {
    menuSelection = 0;
    currentMenu = MENU_GENERAL_SETTINGS;
    DisplayShowMenu(menuSelection, generalSettingsMenuItems, generalSettingsMenuItemCount);
    LOG_INFO(TAG, "Genel ayarlar menüsü gösteriliyor");
}

void UIShowTempHumidityMenu() {
    static const char* tempHumidityMenuItems[] = {
        "Hedef Sicaklik",
        "Hedef Nem",
        "PID Parametreleri",
        "Histerezis Ayari"
    };
    static const int itemCount = sizeof(tempHumidityMenuItems) / sizeof(tempHumidityMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_TEMP_HUMIDITY;
    DisplayShowMenu(menuSelection, tempHumidityMenuItems, itemCount);
    LOG_INFO(TAG, "Sıcaklık ve Nem menüsü gösteriliyor");
}

void UIShowMotorMenu() {
    static const char* motorMenuItems[] = {
        "Donus Suresi",
        "Donus Arasi",
        "Motor Testi",
        "Motor Durum"
    };
    static const int itemCount = sizeof(motorMenuItems) / sizeof(motorMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_MOTOR;
    DisplayShowMenu(menuSelection, motorMenuItems, itemCount);
    LOG_INFO(TAG, "Motor ayarları menüsü gösteriliyor");
}

void UIShowProfileMenu() {
    // Profil adlarını ve tiplerini saklayacak diziler
    static const char* profileItems[MAX_PROFILES];
    static int profileTypes[MAX_PROFILES];
    static int profileCount = 0;
    
    // Profil listesini yükle
    profileCount = 0;
    // Tüm geçerli profilleri ekle
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (predefinedProfiles[i].name.length() > 0) {
            profileItems[profileCount] = predefinedProfiles[i].name.c_str();
            profileTypes[profileCount] = i;
            profileCount++;
        }
    }
    
    menuSelection = 0;
    currentMenu = MENU_PROFILE;
    
    // Profilleri göster
    DisplayShowMenu(menuSelection, profileItems, profileCount);
    
    LOG_INFO(TAG, "Profil menüsü gösteriliyor");
}

void UIShowTimeDateMenu() {
    static const char* timeDateMenuItems[] = {
        "Tarihi Ayarla",
        "Saati Ayarla",
        "NTP ile Senkronize Et"
    };
    static const int itemCount = sizeof(timeDateMenuItems) / sizeof(timeDateMenuItems[0]);
    
    menuSelection = 0;
    currentMenu = MENU_TIME_DATE;
    DisplayShowMenu(menuSelection, timeDateMenuItems, itemCount);
    
    LOG_INFO(TAG, "Tarih ve Saat menüsü gösteriliyor");
}

// Sistem bilgilerini göster
void showSystemInfo() {
    // Ekranı temizle
    tft.fillScreen(ST7735_BLACK);
    
    // Başlık
    tft.fillRect(0, 0, tft.width(), 16, ST7735_NAVY);
    tft.setCursor(5, 5);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.println("SISTEM BILGISI");
    
    // Versiyon
    tft.setCursor(5, 20);
    tft.setTextColor(ST7735_YELLOW);
    tft.print("Sistem: ");
    tft.println(SYSTEM_NAME);
    
    tft.setCursor(5, 30);
    tft.print("Versiyon: ");
    tft.println(APP_VERSION);
    
    // Bellek kullanımı
    tft.setCursor(5, 45);
    tft.setTextColor(ST7735_GREEN);
    tft.print("Bellek: ");
    tft.print(ESP.getFreeHeap() / 1024);
    tft.println(" KB");
    
    // Sensör durumu
    tft.setCursor(5, 60);
    tft.setTextColor(ST7735_CYAN);
    tft.print("Sensor1: ");
    tft.println(GetSensorData().sensor1Valid ? "OK" : "HATA");
    
    tft.setCursor(5, 70);
    tft.print("Sensor2: ");
    tft.println(GetSensorData().sensor2Valid ? "OK" : "HATA");
    
    // WiFi durumu
    tft.setCursor(5, 85);
    tft.setTextColor(ST7735_MAGENTA);
    tft.print("WiFi: ");
    
    switch (WifiGetStatus()) {
        case WIFI_STATUS_AP_MODE:
            tft.println("AP Modu");
            tft.setCursor(5, 95);
            tft.print("IP: ");
            tft.println(WifiGetIP());
            break;
        case WIFI_STATUS_STA_MODE:
            tft.println("Bagli");
            tft.setCursor(5, 95);
            tft.print("IP: ");
            tft.println(WifiGetIP());
            break;
        default:
            tft.println("Kapali");
            break;
    }
    
    // Alt bilgi çubuğu
    tft.fillRect(0, tft.height() - 15, tft.width(), 15, ST7735_NAVY);
    tft.setCursor(5, tft.height() - 13);
    tft.setTextColor(ST7735_LIGHTGREY);
    tft.println("Geri donmek icin tiklayin");
    
    // Joystick butonu basılana kadar bekle
    waitForButton();
    
    // Genel Ayarlar menüsüne dön
    UINavigateToMenu(MENU_GENERAL_SETTINGS);
}

// Basit buton bekleme fonksiyonu
bool waitForButton() {
    // Joystick butonuna basılmasını bekle
    while (digitalRead(JOYSTICK_BTN) != LOW) {
        // Watchdog'u besle
        esp_task_wdt_reset();
        delay(50);
        
        // Menü zamanaşımı kontrolü
        if (millis() - lastMenuActivity > MENU_TIMEOUT) {
            LOG_INFO(TAG, "Bekleme zaman aşımı, ana ekrana dönülüyor");
            UIBackToMain();
            return false;
        }
    }
    
    // Debounce
    delay(50);
    while (digitalRead(JOYSTICK_BTN) == LOW) {
        delay(10);
    }
    delay(50);
    
    return true;
}