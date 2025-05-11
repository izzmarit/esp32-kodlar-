/**
 * Kuluçka Makinesi Kontrol Sistemi - Profil Modülü
 */

 #ifndef PROFILE_MODULE_H
 #define PROFILE_MODULE_H
 
 #include <Arduino.h>
 #include "../config/GlobalDefinitions.h"
 #include "../include/AppConfig.h"
 
 // Global değişken bildirimi
 extern Profile currentProfile;
 extern Profile predefinedProfiles[MAX_PROFILES];
 
 // Fonksiyon prototipleri
 bool ProfileInit();
 void ProfileUpdate();
 Profile ProfileGetCurrent();
 ProfileType ProfileGetType();
 bool ProfileSetType(ProfileType type);
 ProfileStage ProfileGetCurrentStage(int currentDay);
 ProfileStageSettings ProfileGetStageSettings(ProfileStage stage);
 bool ProfileStartIncubation(ProfileType type);
 bool ProfileEndIncubation();
 int ProfileGetRemainingDays();
 int ProfileGetCurrentDay();
 bool ProfileIsActive();
 int ProfileGetTotalDays(ProfileType type);
 String ProfileGetTypeName(ProfileType type);
 void ProfileUpdateSettings(ProfileType type, int stage, ProfileStageSettings settings);
 void ProfileEnableStageNotifications(bool enable);
 Profile GetProfileByType(ProfileType type);  // Eksik olan fonksiyon prototipi
 bool LoadProfilesFromFile();
 bool CreateDefaultProfilesFile();
 
 #endif // PROFILE_MODULE_H