// OTAUpdateModule.h 
#ifndef OTA_UPDATE_MODULE_H
#define OTA_UPDATE_MODULE_H

#include <Arduino.h>
#include "../include/AppConfig.h"
#include <WiFi.h>
#include <Update.h>

// Fonksiyon prototipleri
bool OTAInit();
void OTAUpdate();
bool OTAStartUpdate();
void OTASetProgress(int progress);
int OTAGetProgress();
bool OTAIsUpdating();
bool testOTAServer();
void OTADownloadTask(void * parameter);

#endif // OTA_UPDATE_MODULE_H