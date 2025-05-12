/**
 * Kuluçka Makinesi Kontrol Sistemi - Kullanıcı Arayüzü Modülü
 * 
 * Bu modül, joystick ile kullanıcı etkileşimini ve menü sistemini yönetir.
 */

 #ifndef UI_MODULE_H
 #define UI_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 
 // Fonksiyon prototipleri
 bool UIInit();
 void UIUpdate();
 MenuState UIGetCurrentMenu();
 JoystickDirection UIReadJoystick();
 bool UIHandleJoystick(JoystickDirection direction);
 
 // Menü işleme fonksiyonları
 bool handleMainMenu(JoystickDirection direction);
 bool handleGeneralSettingsMenu(JoystickDirection direction);
 bool handleTempHumidityMenu(JoystickDirection direction);
 bool handleMotorMenu(JoystickDirection direction);
 bool handleProfileMenu(JoystickDirection direction);
 bool handleGraphMenu(JoystickDirection direction);
 bool handlePowerSaveMenu(JoystickDirection direction);
 bool handleCalibrationMenu(JoystickDirection direction);
 bool handleTimeDateMenu(JoystickDirection direction);
 bool handleWifiMenu(JoystickDirection direction);
 bool handleBackupMenu(JoystickDirection direction);
 bool handleAlarmMenu(JoystickDirection direction);
 
 // UI gösterim fonksiyonları
 void showSystemInfo();
 bool waitForButton();
 int waitForSelection(int maxOption);
 void UIShowMainMenu();
 void UIShowGeneralSettingsMenu();
 void UIShowTempHumidityMenu();
 void UIShowMotorMenu();
 void UIShowCalibrationMenu();
 void UIShowGraphMenu();
 void UIShowPowerSaveMenu();
 void UIShowAlarmMenu();
 void UIShowProfileMenu();
 void UIShowWifiMenu();
 void UIShowBackupMenu();
 void UIShowTimeDateMenu();
 void UISetDate();
 void UISetTime();
 void UIBackToMain();
 void UINavigateToMenu(MenuState menu);
 void UIShowGraphSelection(int graphType);
 void UIShowPowerOutages();
 void CalibrateJoystick();
 void showStatistics();
 void UIShowSavedAndReturnToMenu(const char* message, MenuState returnMenu);
 
 // Yeni fonksiyonlar
 float UISingleNumericInput(const char* title, float minValue, float maxValue, float defaultValue);
 
 // Global değişkenler
 extern bool editMode; // DisplayModule.cpp için export edilen düzenleme modu bayrağı
 
 // UI durumu izleme ve kontrol
 extern int menuSelection;
 extern int profileMenuCurrentPage;
 
 #endif // UI_MODULE_H