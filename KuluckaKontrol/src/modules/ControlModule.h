/**
 * Kuluçka Makinesi Kontrol Sistemi - Kontrol Modülü
 * 
 * Bu modül, sıcaklık ve nem kontrolü için PID ve histerezis algoritmalarını
 * kullanarak röleleri kontrol eder.
 */

 #ifndef CONTROL_MODULE_H
 #define CONTROL_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 
 // Fonksiyon prototipleri
 bool ControlInit();
 void ControlUpdate();
 void ControlSetTargets(float targetTemp, float targetHumidity);
 void ControlSetPidParams(float kp, float ki, float kd);
 void ControlSetHumHysteresis(float hysteresis);
 bool ControlManualHeater(bool state);
 bool ControlManualHumidifier(bool state);
 bool ControlManualMotor(bool state);
 bool ControlTurnMotor();
 void ControlSetMotorSettings(unsigned long duration, unsigned long interval);
 void ControlSetPowerSaveMode(PowerSaveLevel mode);
 PowerSaveLevel ControlGetPowerSaveMode();
 RelayState ControlGetRelayState();
 float ControlGetPidOutput();
 float ControlGetTempError();
 float ControlGetHumError();
 void ControlUpdateProfileSettings(int currentDay);
 int GetMotorCountdown();
 PidSettings GetPidSettings();
 void SetPidSettings(PidSettings settings);
 float GetLastOutput();
 float GetLastInput();
 
 #endif // CONTROL_MODULE_H