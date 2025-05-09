/**
 * Kuluçka Makinesi Kontrol Sistemi - Modül Yöneticisi
 * 
 * Bu modül, tüm alt modüllerin başlatılma ve güncelleme sıralarını yönetir.
 */

 #ifndef MODULE_MANAGER_H
 #define MODULE_MANAGER_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 
 // Fonksiyon prototipleri
 ModuleErrorCode ModuleManagerInit();
 void ModuleManagerUpdate();
 ModuleErrorCode ModuleManagerGetLastError();
 String ModuleManagerGetErrorMessage(ModuleErrorCode code);
 
 // Modül durumu dış değişkeni
 extern ModuleStatus moduleStatus;
 
 #endif // MODULE_MANAGER_H