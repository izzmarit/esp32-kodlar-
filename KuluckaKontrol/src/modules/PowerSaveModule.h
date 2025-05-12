/**
 * Kuluçka Makinesi Kontrol Sistemi - Güç Tasarrufu Modülü
 * 
 * Bu modül, ESP32'nin düşük güç modlarını ve güç tasarrufu özelliklerini yönetir.
 */

 #ifndef POWER_SAVE_MODULE_H
 #define POWER_SAVE_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 #include "DisplayModule.h"

 extern unsigned long screenSaverTimeout;
 
 // Fonksiyon prototipleri
 bool PowerSaveInit();
 void PowerSaveUpdate();
 void PowerSaveSetLevel(PowerSaveLevel level);
 PowerSaveLevel PowerSaveGetLevel();
 void PowerSaveToggleScreenSaver(bool enabled);
 void PowerSaveSetScreenSaverMode(ScreenSaverMode mode);
 ScreenSaverMode PowerSaveGetScreenSaverMode();
 void PowerSaveSetScreenSaverTimeout(unsigned long timeoutMs);
 bool PowerSaveIsScreenSaverActive();
 void PowerSaveResetScreenSaverTimer();
 void PowerSaveEnterLightSleep(unsigned long sleepTimeMs);
 void PowerSaveShowLogo();
 void PowerSaveShowInfo();
 void PowerSaveWakeUp();
 
 #endif // POWER_SAVE_MODULE_H