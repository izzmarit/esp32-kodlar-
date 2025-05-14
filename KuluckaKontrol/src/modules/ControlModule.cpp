/**
 * Kuluçka Makinesi Kontrol Sistemi - Kontrol Modülü
 * SSR Röle Versiyonu
 */

 #include "ControlModule.h"
 #include <PID_v1.h>
 #include "SensorModule.h"
 #include "DataModule.h"
 #include "AlarmModule.h"
 #include "WifiModule.h"
 #include "../config/pins.h"
 
 // Dış fonksiyon prototipi
 Profile GetProfileByType(ProfileType type);
  
 static const char* TAG = "CONTROL"; // static eklendi
  
 // Hedef değerler
 static float targetTemperature = DEFAULT_TEMP;
 static float targetHumidity = DEFAULT_HUM;
  
 // PID değişkenleri
 static double pidInput, pidOutput, pidSetpoint;
 static PID tempPID(&pidInput, &pidOutput, &pidSetpoint, PID_KP, PID_KI, PID_KD, DIRECT);
  
 // PID ayarları
 static PidSettings pidSettings = {PID_KP, PID_KI, PID_KD, PID_WINDOW_SIZE};
  
 // Nem kontrolü için histerezis
 static float humidityHysteresis = HUM_HYSTERESIS;
  
 // Röle durumları
 static RelayState relayState = {false};
  
 // Enerji tasarruf modu
 static PowerSaveLevel currentPowerSaveMode = POWER_SAVE_DISABLED;
  
 // Motor ayarları
 static unsigned long motorDuration = DEFAULT_MOTOR_DURATION;  // 14 saniye
 static unsigned long motorInterval = DEFAULT_MOTOR_INTERVAL;  // 2 saat
  
 // Son güncelleme zamanları
 static unsigned long lastHeaterUpdate = 0;
 static unsigned long lastHumidifierUpdate = 0;
 static unsigned long lastMotorCheck = 0;
 static unsigned long windowStartTime = 0;
  
 // PID çalışma aralığı (ms)
 static const unsigned long PID_INTERVAL = 1000;
  
 bool ControlInit() {
    // Röle pinleri zaten pins.h içerisindeki setupPins() fonksiyonunda ayarlanıyor
    
    // PID kontrolörünü ayarla
    pidSetpoint = targetTemperature;
    tempPID.SetMode(AUTOMATIC);
    tempPID.SetSampleTime(PID_INTERVAL);
    tempPID.SetOutputLimits(0, pidSettings.windowSize);  // 0-WindowSize aralığında çıkış
    
    // Relay state yapısını tam olarak ilklendir
    relayState = {
        false,                      // heater
        false,                      // humidifier
        false,                      // motor
        millis(),                   // lastMotorActivation
        0,                          // motorRunningUntil
        0,                          // heaterStartTime
        0,                          // totalHeaterOnTime
        0,                          // heaterActivationCount
        0,                          // humidifierStartTime
        0,                          // totalHumidifierOnTime
        0,                          // humidifierActivationCount
        0.0,                        // lastPidOutput
        millis()                    // windowStartTime
    };
    
    // Ayarları veri modülünden yükle
    IncubationSettings settings = LoadSettings();
    targetTemperature = settings.targetTemp;
    targetHumidity = settings.targetHumidity;
    
    // PID parametrelerini ayarla
    tempPID.SetTunings(pidSettings.Kp, pidSettings.Ki, pidSettings.Kd);
    
    windowStartTime = millis();
    
    LOG_INFO(TAG, "Kontrol modülü başlatıldı");
    LOG_INFO(TAG, "Hedef sıcaklık: %.1f°C", targetTemperature);
    LOG_INFO(TAG, "Hedef nem: %.1f%%", targetHumidity);
    
    // Başlangıçta sensör verilerini kontrol et ve röleleri ayarla
    SensorData sensorData = GetSensorData();
    if (sensorData.sensor1Valid || sensorData.sensor2Valid) {
        // Sıcaklık kontrolü - hedeften düşükse ısıtıcıyı çalıştır
        if (sensorData.avgTemperature < targetTemperature - 0.3) {
            relayState.heater = true;
            relayState.heaterStartTime = millis();
            digitalWrite(RELAY_HEATER, HIGH);
            LOG_INFO(TAG, "Başlangıçta ısıtıcı otomatik açıldı");
        }
        
        // Nem kontrolü - hedeften düşükse nemlendiriciyi çalıştır
        if (sensorData.avgHumidity < targetHumidity - humidityHysteresis) {
            relayState.humidifier = true;
            relayState.humidifierStartTime = millis();
            digitalWrite(RELAY_HUMIDIFIER, HIGH);
            LOG_INFO(TAG, "Başlangıçta nemlendirici otomatik açıldı");
        }
    }
    
    return true;
}
  
 // PID denetleyicisi için adaptif aralık ekleme
 #define PID_INTERVAL_MIN 500   // Minimum güncelleme aralığı (ms)
 #define PID_INTERVAL_MAX 2000  // Maksimum güncelleme aralığı (ms)
  
 void ControlUpdate() {
    unsigned long currentTime = millis();
    
    // Sensör verilerini oku
    SensorData sensorData = GetSensorData();
    if (!sensorData.sensor1Valid && !sensorData.sensor2Valid) {
        // Sensörler çalışmıyor, güvenlik için röleleri kapat
        ControlManualHeater(false);
        ControlManualHumidifier(false);
        LOG_ERROR(TAG, "Sensör verileri geçersiz, güvenlik için tüm röleleri kapattım!");
        return;
    }
    
    float currentTemp = sensorData.avgTemperature;
    float currentHumidity = sensorData.avgHumidity;
    
    // Adaptif PID aralığı hesaplama
    float tempDiff = abs(targetTemperature - currentTemp);
    unsigned long adaptivePidInterval;
    
    if (tempDiff > 1.0) {
        // Büyük sıcaklık farkında daha hızlı tepki
        adaptivePidInterval = PID_INTERVAL_MIN;
    } else {
        // Hedef sıcaklığa yakınsa daha yavaş tepki
        adaptivePidInterval = map(tempDiff * 100, 0, 100, PID_INTERVAL_MAX, PID_INTERVAL_MIN);
    }
    
    // PID kontrolünü güncelle
    if (currentTime - lastHeaterUpdate >= adaptivePidInterval) {
        lastHeaterUpdate = currentTime;
        
        // PID giriş değerlerini güncelle
        pidInput = currentTemp;
        pidSetpoint = targetTemperature;
        tempPID.Compute();
        
        // PID çıkışını sınırla - daha stabil kontrol için kademeli değişim
        double maxChange = 500.0; // Maksimum değişim miktarı
        
        if (pidOutput > relayState.lastPidOutput + maxChange) {
            pidOutput = relayState.lastPidOutput + maxChange;
        } else if (pidOutput < relayState.lastPidOutput - maxChange) {
            pidOutput = relayState.lastPidOutput - maxChange;
        }
        
        relayState.lastPidOutput = pidOutput;
            
        // PWM mantığı: Daha yumuşak geçişler için dinamik pencere boyutu
        unsigned long effectiveWindowSize = pidSettings.windowSize;
        
        // Küçük sıcaklık farkları için daha hassas çalışma
        if (tempDiff < 0.5) {
            // Eğer hedef sıcaklığa yakınsak, daha kısa PWM döngüsü kullan
            effectiveWindowSize = pidSettings.windowSize / 2;
        }
        
        // Her zaman pencere hesaplamasını yap
        if(currentTime - windowStartTime > effectiveWindowSize) {
            // Yeni pencere başlatılıyor
            windowStartTime += effectiveWindowSize;
        }
        
        // PID çıkışına göre röle kontrolü - PWM yaklaşımı
        if(currentTime - windowStartTime < pidOutput) {
            // Isıtıcıyı aç
            if(!relayState.heater) {
                relayState.heater = true;
                relayState.heaterStartTime = currentTime; // Açılış zamanını kaydet
                digitalWrite(RELAY_HEATER, HIGH);  // HIGH = Açık (SSR röle için)
                LOG_DEBUG(TAG, "Isıtıcı AÇIK");
            }
        } else {
            // Isıtıcıyı kapat
            if(relayState.heater) {
                relayState.heater = false;
                digitalWrite(RELAY_HEATER, LOW);  // LOW = Kapalı (SSR röle için)
                
                // Röle kullanım istatistiği güncelleme
                unsigned long heaterOnTime = currentTime - relayState.heaterStartTime;
                relayState.totalHeaterOnTime += heaterOnTime;
                relayState.heaterActivationCount++;
                
                LOG_DEBUG(TAG, "Isıtıcı KAPALI");
            }
        }
        
        // Güvenlik kontrolleri
        
        // Aşırı yüksek sıcaklık kontrolü - kritik limit 
        if(currentTemp > targetTemperature + 1.0) {
            // Sıcaklık kritik derecede yüksek, acil kapat ve alarm oluştur
            if(relayState.heater) {
                relayState.heater = false;
                digitalWrite(RELAY_HEATER, LOW);  // LOW = Kapalı (SSR röle için)
                LOG_ERROR(TAG, "Isıtıcı ACİL KAPATILDI (Kritik üst limit)");
                
                // Kritik sıcaklık alarmı oluştur
                AlarmLog(ALARM_HIGH_TEMP, "KRİTİK YÜKSEK SICAKLIK: " + String(currentTemp, 1) + "C");
            }
        }
        // Normal sıcaklık üst limit kontrolü
        else if(currentTemp > targetTemperature + 0.2) {
            // Sıcaklık çok yüksek, zorla kapat
            if(relayState.heater) {
                relayState.heater = false;
                digitalWrite(RELAY_HEATER, LOW);  // LOW = Kapalı (SSR röle için)
                LOG_DEBUG(TAG, "Isıtıcı KAPALI (Üst limit)");
            }
        } 
        // Alt limit kontrolü
        else if(currentTemp < targetTemperature - 0.3) {
            // Sıcaklık çok düşük, zorla aç
            if(!relayState.heater) {
                relayState.heater = true;
                relayState.heaterStartTime = currentTime; // Açılış zamanını kaydet
                digitalWrite(RELAY_HEATER, HIGH);  // HIGH = Açık (SSR röle için)
                LOG_DEBUG(TAG, "Isıtıcı AÇIK (Alt limit)");
            }
        }
        
        // Enerji tasarrufu moduna bağlı maksimum çalışma süresi
        unsigned long maxHeaterTime = 1800000; // varsayılan 30 dakika
        
        // Enerji tasarrufu moduna göre maksimum çalışma süresini ayarla
        switch (currentPowerSaveMode) {
            case POWER_SAVE_LIGHT:
                maxHeaterTime = 1500000; // 25 dakika
                break;
            case POWER_SAVE_MEDIUM:
                maxHeaterTime = 1200000; // 20 dakika
                break;
            case POWER_SAVE_AGGRESSIVE:
                maxHeaterTime = 900000;  // 15 dakika
                break;
            default:
                maxHeaterTime = 1800000; // 30 dakika
                break;
        }
        
        // Isıtıcı açıkken maksimum çalışma süresi kontrolü - güvenlik için
        if(relayState.heater && (currentTime - relayState.heaterStartTime > maxHeaterTime)) {
            // Isıtıcı maksimum süreden fazla sürekli çalışıyorsa, kısa bir süre kapat
            relayState.heater = false;
            digitalWrite(RELAY_HEATER, LOW);  // LOW = Kapalı (SSR röle için)
            LOG_WARNING(TAG, "Isıtıcı KAPALI (Maksimum çalışma süresi aşıldı: %u dakika)", 
                        maxHeaterTime / 60000);
            
            // İstatistikleri güncelle
            relayState.totalHeaterOnTime += (currentTime - relayState.heaterStartTime);
            
            // 10 saniye sonra tekrar değerlendirmek için zaman ayarla
            windowStartTime = currentTime - pidSettings.windowSize + 10000;
        }
        
        // Enerji tasarruf modunu kontrol et
        if (currentPowerSaveMode == POWER_SAVE_MEDIUM && currentTemp > (targetTemperature - 0.2)) {
            // Yüksek enerji tasarrufu modunda ve sıcaklık hedefe yakınsa
            if(relayState.heater) {
                relayState.heater = false;
                digitalWrite(RELAY_HEATER, LOW);  // LOW = Kapalı (SSR röle için)
                LOG_DEBUG(TAG, "Isıtıcı KAPALI (Enerji tasarruf)");
            }
        }
        
        // Debug bilgileri yazdır
        LOG_VERBOSE(TAG, "PID: Set:%.1f In:%.1f Out:%.1f Heater:%d", 
                pidSetpoint, pidInput, pidOutput, relayState.heater);
    }
    
    // Nem kontrolü (Histerezis kullanarak)
    if (currentTime - lastHumidifierUpdate >= 5000) {  // 5 saniye aralıklarla
        lastHumidifierUpdate = currentTime;
        
        bool humidifierState = relayState.humidifier;
        
        if (currentHumidity < (targetHumidity - humidityHysteresis)) {
            humidifierState = true;  // Nem düşükse, nemlendiriciyi aç
        } 
        else if (currentHumidity > (targetHumidity + humidityHysteresis)) {
            humidifierState = false; // Nem yüksekse, nemlendiriciyi kapat
        }
        
        // Enerji tasarruf modunu kontrol et
        if (currentPowerSaveMode == POWER_SAVE_MEDIUM && currentHumidity > (targetHumidity - 5)) {
            // Yüksek enerji tasarrufu modunda ve nem hedefe yakınsa
            humidifierState = false;
        }
        
        // Nemlendirici rölesini kontrol et
        if (humidifierState != relayState.humidifier) {
            relayState.humidifier = humidifierState;
            
            if (humidifierState) {
                // Nemlendiriciyi aç
                digitalWrite(RELAY_HUMIDIFIER, HIGH);  // HIGH = Açık (SSR röle için)
                relayState.humidifierStartTime = currentTime;
                LOG_DEBUG(TAG, "Nemlendirici AÇIK");
            } else {
                // Nemlendiriciyi kapat
                digitalWrite(RELAY_HUMIDIFIER, LOW);  // LOW = Kapalı (SSR röle için)
                
                // İstatistikleri güncelle
                unsigned long humidifierOnTime = currentTime - relayState.humidifierStartTime;
                relayState.totalHumidifierOnTime += humidifierOnTime;
                relayState.humidifierActivationCount++;
                
                LOG_DEBUG(TAG, "Nemlendirici KAPALI");
            }
        }
    }
    
    // Motor kontrolü
    if (relayState.motor && currentTime >= relayState.motorRunningUntil) {
        // Motor çalışma süresi doldu, durdur
        relayState.motor = false;
        digitalWrite(RELAY_MOTOR, LOW);  // LOW = Kapalı (SSR röle için)
        LOG_INFO(TAG, "Motor durduruldu");
        
        // Yeni bir interval başlat
        relayState.lastMotorActivation = currentTime;
        LOG_INFO(TAG, "Geri sayim yeniden baslatildi: %d dk", (int)(motorInterval / 60000));
    }
    
    // Motor periyodik çalışma kontrolü
    if (!relayState.motor && (currentTime - relayState.lastMotorActivation) >= motorInterval) {
        // Motor çalışma zamanı geldi, çalıştır
        if (ControlTurnMotor()) {
            LOG_INFO(TAG, "Motor periyodik olarak çalıştırıldı");
        }
    }
}
 
 void ControlSetTargets(float targetTemp, float targetHum) {
     targetTemperature = targetTemp;
     targetHumidity = targetHum;
     
     pidSetpoint = targetTemperature;
     
     LOG_INFO(TAG, "Hedef değerler: Sıcaklık=%.1f°C, Nem=%.1f%%", targetTemp, targetHum);
 }
 
 void ControlSetPidParams(float kp, float ki, float kd) {
     pidSettings.Kp = kp;
     pidSettings.Ki = ki;
     pidSettings.Kd = kd;
     tempPID.SetTunings(kp, ki, kd);
     
     LOG_INFO(TAG, "PID parametreleri güncellendi: Kp=%.2f Ki=%.4f Kd=%.2f", kp, ki, kd);
 }
 
 void ControlSetHumHysteresis(float hysteresis) {
     humidityHysteresis = hysteresis;
     LOG_INFO(TAG, "Nem histerezisi güncellendi: %.1f%%", hysteresis);
 }
 
 bool ControlManualHeater(bool state) {
     relayState.heater = state;
     digitalWrite(RELAY_HEATER, state ? HIGH : LOW);  // HIGH = Açık (SSR röle için)
     
     if (state) {
         relayState.heaterStartTime = millis();
         LOG_INFO(TAG, "Isıtıcı manuel olarak AÇILDI");
     } else {
         LOG_INFO(TAG, "Isıtıcı manuel olarak KAPATILDI");
     }
     
     return true;
 }
 
 bool ControlManualHumidifier(bool state) {
     relayState.humidifier = state;
     digitalWrite(RELAY_HUMIDIFIER, state ? HIGH : LOW);  // HIGH = Açık (SSR röle için)
     
     if (state) {
         relayState.humidifierStartTime = millis();
         LOG_INFO(TAG, "Nemlendirici manuel olarak AÇILDI");
     } else {
         LOG_INFO(TAG, "Nemlendirici manuel olarak KAPATILDI");
     }
     
     return true;
 }
 
 bool ControlManualMotor(bool state) {
    // Eğer motoru açmak istiyorsak
    if (state) {
        return ControlTurnMotor();  // Motor çevirme fonksiyonunu çağır
    } else {
        // Motoru kapat
        relayState.motor = false;
        digitalWrite(RELAY_MOTOR, LOW);  // LOW = Kapalı (SSR röle için)
        LOG_INFO(TAG, "Motor manuel olarak KAPATILDI");
        
        // EKLENEN KOD: Manuel kapatma sonrası lastMotorActivation'ı güncelle
        relayState.lastMotorActivation = millis();
        return true;
    }
}
 
 bool ControlTurnMotor() {
     unsigned long currentTime = millis();
     
     // Motoru çalıştır
     relayState.motor = true;
     relayState.lastMotorActivation = currentTime;
     relayState.motorRunningUntil = currentTime + motorDuration;
     
     digitalWrite(RELAY_MOTOR, HIGH);  // HIGH = Açık (SSR röle için)
     LOG_INFO(TAG, "Motor çalıştırıldı, %d saniye sonra duracak", motorDuration / 1000);
     
     return true;
 }
 
 void ControlSetMotorSettings(unsigned long duration, unsigned long interval) {
     motorDuration = duration;
     motorInterval = interval;
     
     // Ayarları kaydet
     MotorSettings settings;
     settings.duration = duration;
     settings.interval = interval;
     SaveMotorSettings(settings);
     
     LOG_INFO(TAG, "Motor ayarları: Süre=%.1f sn, Aralık=%.1f dk", 
             duration / 1000.0f, interval / 60000.0f);
 }
 
 void ControlSetPowerSaveMode(PowerSaveLevel mode) {
     currentPowerSaveMode = mode;
     
     // Enerji tasarruf moduna göre PID parametrelerini ayarla
     switch (mode) {
         case POWER_SAVE_LIGHT:
             // Düşük güç tüketimi için PID tepkisini azalt
             tempPID.SetTunings(pidSettings.Kp * 0.8, pidSettings.Ki * 0.5, pidSettings.Kd * 0.8);
             LOG_INFO(TAG, "Hafif enerji tasarruf modu aktif");
             break;
             
         case POWER_SAVE_MEDIUM:
             // Yüksek güç tasarrufu için PID tepkisini minimuma indir
             tempPID.SetTunings(pidSettings.Kp * 0.5, pidSettings.Ki * 0.3, pidSettings.Kd * 0.5);
             LOG_INFO(TAG, "Orta seviye enerji tasarruf modu aktif");
             break;
             
         case POWER_SAVE_AGGRESSIVE:
             // Agresif güç tasarrufu - minimum kontrol
             tempPID.SetTunings(pidSettings.Kp * 0.3, pidSettings.Ki * 0.1, pidSettings.Kd * 0.3);
             LOG_INFO(TAG, "Agresif enerji tasarruf modu aktif");
             break;
             
         case POWER_SAVE_DISABLED:
         default:
             // Normal mod - varsayılan PID parametreleri
             tempPID.SetTunings(pidSettings.Kp, pidSettings.Ki, pidSettings.Kd);
             LOG_INFO(TAG, "Enerji tasarruf modu devre dışı");
             break;
     }
 }
 
 PowerSaveLevel ControlGetPowerSaveMode() {
     return currentPowerSaveMode;
 }
 
 RelayState ControlGetRelayState() {
     return relayState;
 }
 
 float ControlGetPidOutput() {
     return pidOutput;
 }
 
 float ControlGetTempError() {
     return targetTemperature - pidInput;
 }
 
 float ControlGetHumError() {
     SensorData sensorData = GetSensorData();
     return targetHumidity - sensorData.avgHumidity;
 }
 
 PidSettings GetPidSettings() {
     return pidSettings;
 }
 
 void SetPidSettings(PidSettings settings) {
     pidSettings = settings;
     tempPID.SetTunings(settings.Kp, settings.Ki, settings.Kd);
 }
 
 float GetLastOutput() {
     return pidOutput;
 }
 
 float GetLastInput() {
     return pidInput;
 }
 
 int GetMotorCountdown() {
     unsigned long currentTime = millis();
     unsigned long timeSinceLastMotor = currentTime - relayState.lastMotorActivation;
     
     if (timeSinceLastMotor >= motorInterval) {
         return 0; // Motor dönüşü zamanı gelmiş veya geçmiş
     }
     
     // Kalan dakika hesabı
     return (int)((motorInterval - timeSinceLastMotor) / 60000); // Milisaniyeden dakikaya çevirme
 }
 
 void ControlUpdateProfileSettings(int currentDay) {
     // Mevcut kuluçka profiline göre ayarları güncelle
     IncubationSettings settings = LoadSettings();
     
     // Kuluçka türüne göre gün ilerlemesine dayalı ayarları değiştir
     if (settings.type >= PROFILE_CHICKEN && settings.type <= PROFILE_PHEASANT) {
         // Profil bilgilerini al
         Profile profile = GetProfileByType((ProfileType)settings.type);
         
         // Mevcut aşamayı bul
         ProfileStageSettings stageSettings;
         bool stageFound = false;
         
         for (int i = 0; i < profile.stageCount; i++) {
             if (currentDay >= profile.stages[i].startDay && currentDay <= profile.stages[i].endDay) {
                 stageSettings = profile.stages[i];
                 stageFound = true;
                 LOG_INFO(TAG, "Profil aşaması güncellendi: %s Gün %d (%d-%d)", 
                         profile.name.c_str(), currentDay, 
                         profile.stages[i].startDay, profile.stages[i].endDay);
                 break;
             }
         }
         
         if (stageFound) {
             // Hedef değerleri güncelle
             targetTemperature = stageSettings.temperature;
             targetHumidity = stageSettings.humidity;
             pidSetpoint = targetTemperature;
             
             // Ayarları kaydet
             settings.targetTemp = targetTemperature;
             settings.targetHumidity = targetHumidity;
             SaveSettings(settings);
             
             LOG_INFO(TAG, "Yeni hedefler: Sıcaklık=%.1f°C, Nem=%.1f%%", 
                     targetTemperature, targetHumidity);
         }
     }
 }