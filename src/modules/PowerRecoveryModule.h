/**
 * Kuluçka Makinesi Kontrol Sistemi - Elektrik Kesintisi Kurtarma Modülü
 * 
 * Bu modül, elektrik kesintileri ve sistem resetleri sonrasında kuluçka sürecinin
 * kaldığı yerden devam etmesini sağlar.
 */

 #ifndef POWER_RECOVERY_MODULE_H
 #define POWER_RECOVERY_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 
 // Fonksiyon prototipleri
 bool PowerRecoveryInit();
 void PowerRecoveryCheckAndRestore();
 void PowerRecoverySaveState();
 bool PowerRecoveryWasPowerOutage();
 unsigned long PowerRecoveryGetOutageDuration();
 void PowerRecoveryDisplayOutageInfo();
 
 #endif // POWER_RECOVERY_MODULE_H