/**
 * Kuluçka Makinesi Kontrol Sistemi - Global Tanımlar
 */

 #ifndef GLOBAL_DEFINITIONS_H
 #define GLOBAL_DEFINITIONS_H
  
 #include <Arduino.h>
 
 // Joystick ayarları
 #define JOYSTICK_DEADZONE 300         // Joystick ölü bölgesi (hiçbir yönün algılanmadığı aralık)
 #define JOYSTICK_REPEAT_DELAY 300     // Joystick tekrarlama gecikmesi (ms)
  
 // Veri yapıları için maksimum limitler
 #define MAX_DATA_RECORDS 144   // 12 saatlik veri (5 dakikada bir kayıt)
 #define MAX_POWER_OUTAGES 10   // Elektrik kesintisi kaydı sayısı
 #define MAX_ALARM_HISTORY 10   // Alarm geçmişi kaydı sayısı
 #define MAX_PROFILES 9         // Maksimum profil sayısı
  
 // Sistem varsayılan değerleri
 #define DEFAULT_TEMP 37.8      // Varsayılan sıcaklık (°C)
 #define DEFAULT_HUM 65.0       // Varsayılan nem (%)
 #define DEFAULT_MOTOR_DURATION 14000     // Motor çalışma süresi (ms)
 #define DEFAULT_MOTOR_INTERVAL 7200000   // Motor çalışma aralığı (ms)
  
 // PID kontrol parametreleri
 #define PID_KP 10.0            // Orantısal katsayı
 #define PID_KI 0.1             // İntegral katsayı
 #define PID_KD 50.0            // Türev katsayı
 #define PID_WINDOW_SIZE 5000   // PWM pencere boyutu (ms)
 #define HUM_HYSTERESIS 2.0     // Nem kontrolü histerezisi (%)
  
 // Dosya sistem yolları
 #define FS_SETTINGS_FILE       "/settings.json"
 #define FS_MOTOR_FILE          "/motor.json"
 #define FS_PROFILES_FILE       "/profiles.json"
 #define FS_CALIBRATION_FILE    "/calibration.json"
 #define FS_TEMP_HUM_FILE       "/temp_hum.csv"
 #define FS_POWER_OUTAGE_FILE   "/power_outage.bin"
 #define FS_POWER_TIME_FILE     "/power_time.txt"
 #define FS_BACKUP_FILE         "/backup.json"
 #define FS_OTA_CONFIG_FILE     "/ota_config.json"
 #define FS_WIFI_CONFIG_FILE    "/wifi_config.json"
 #define FS_RECOVERY_FILE       "/recovery.json"
  
 //============================================================================
 // ENUM TANIMLARI
 //============================================================================
  
 // Alarm türleri
 enum AlarmType {
     ALARM_NONE,
     ALARM_HIGH_TEMP,
     ALARM_LOW_TEMP,
     ALARM_HIGH_HUMIDITY,
     ALARM_LOW_HUMIDITY,
     ALARM_SENSOR_ERROR,
     ALARM_POWER_OUTAGE,
     ALARM_TEMP_DIFF,
     ALARM_HUM_DIFF
 };
  
 // Profil türleri
 enum ProfileType {
     PROFILE_CHICKEN = 0,      // Tavuk
     PROFILE_GOOSE = 1,        // Kaz
     PROFILE_QUAIL = 2,        // Bıldırcın
     PROFILE_DUCK = 3,         // Ördek
     PROFILE_MANUAL = 4,       // Manuel
     PROFILE_TURKEY = 5,       // Hindi
     PROFILE_PARTRIDGE = 6,    // Keklik
     PROFILE_PIGEON = 7,       // Güvercin
     PROFILE_PHEASANT = 8,     // Sülün
     PROFILE_NONE = 255        // Profil seçilmemiş
 };
  
 // Profil aşamaları
 enum ProfileStage {
     STAGE_EARLY,     // Erken aşama
     STAGE_MIDDLE,    // Orta aşama
     STAGE_LATE,      // Son aşama
     STAGE_HATCHING   // Çıkış aşaması
 };
  
 // Ekran gösterim modları
 enum DisplayMode {
     DISPLAY_WELCOME,     // Hoş geldiniz ekranı
     DISPLAY_MAIN,        // Ana ekran
     DISPLAY_MENU,        // Menü ekranı
     DISPLAY_SETTINGS,    // Ayarlar ekranı
     DISPLAY_GRAPH,       // Grafik ekranı
     DISPLAY_CALIBRATION, // Kalibrasyon ekranı
     DISPLAY_ERROR,       // Hata ekranı
     DISPLAY_ALARM        // Alarm ekranı
 };
  
 // Menü durumları
 enum MenuState {
    MENU_NONE,           // Menü yok (ana ekran)
    MENU_MAIN,           // Ana menü
    MENU_TEMP_HUMIDITY,  // Sıcaklık/Nem ayarları
    MENU_MOTOR,          // Motor ayarları
    MENU_PROFILE,        // Profil seçimi
    MENU_GRAPH,          // Grafik görüntüleme
    MENU_POWER_SAVE,     // Enerji tasarruf modu
    MENU_GENERAL_SETTINGS, // Genel Ayarlar (Yeni)
    MENU_TIME_DATE,      // Saat ve tarih ayarı
    MENU_ALARM,          // Alarm ayarları
    MENU_WIFI,           // WiFi ayarları
    MENU_CALIBRATION,    // Kalibrasyon menüsü
    MENU_BACKUP          // Yedekleme/Geri yükleme
};
  
 // Joystick yönleri
 enum JoystickDirection {
     JOY_NONE,
     JOY_UP,
     JOY_DOWN,
     JOY_LEFT,
     JOY_RIGHT,
     JOY_PRESS
 };
  
 // WiFi durumu
 enum WifiStatus {
     WIFI_STATUS_OFF,        // WiFi kapalı
     WIFI_STATUS_AP_MODE,    // Access Point modu
     WIFI_STATUS_STA_MODE,   // Station modu
     WIFI_STATUS_CONNECTING  // Bağlanıyor
 };
  
 // Enerji tasarruf modu
 enum PowerSaveLevel {
     POWER_SAVE_DISABLED,    // Düşük güç modu devre dışı
     POWER_SAVE_LIGHT,       // Hafif güç tasarrufu
     POWER_SAVE_MEDIUM,      // Orta seviye güç tasarrufu
     POWER_SAVE_AGGRESSIVE   // Agresif güç tasarrufu
 };
  
 // Ekran koruyucu modları
 enum ScreenSaverMode {
     SCREEN_SAVER_NONE,     // Ekran koruyucu devre dışı
     SCREEN_SAVER_LOGO,     // MK-V5.0 yazısı
     SCREEN_SAVER_INFO,     // Temel bilgiler
     SCREEN_SAVER_OFF       // Ekran kapalı
 };
  
 // Modül hata kodları
 enum ModuleErrorCode {
     MODULE_OK = 0,
     MODULE_ERROR_CONFIG = 1,
     MODULE_ERROR_DATA = 2,
     MODULE_ERROR_TIME = 3,
     MODULE_ERROR_DISPLAY = 4,
     MODULE_ERROR_SENSOR = 5,
     MODULE_ERROR_CONTROL = 6,
     MODULE_ERROR_UI = 7,
     MODULE_ERROR_WIFI = 8,
     MODULE_ERROR_PROFILE = 9,
     MODULE_ERROR_ALARM = 10,
     MODULE_ERROR_POWER_SAVE = 11,
     MODULE_ERROR_POWER_RECOVERY = 12,
     MODULE_ERROR_OTA = 13
 };
  
 //============================================================================
 // STRUCT TANIMLARI
 //============================================================================
  
 // Veri kayıt yapısı
 struct DataRecord {
     uint32_t timestamp;     // Zaman damgası
     float temperature;      // Sıcaklık
     float humidity;         // Nem
     bool heaterState;       // Isıtıcı durumu
     bool humidifierState;   // Nemlendirici durumu
 };
  
 // Elektrik kesintisi kaydı yapısı
 struct PowerOutage {
     uint32_t startTime;  // Kesinti başlangıç zamanı
     uint32_t endTime;    // Kesinti bitiş zamanı
     uint32_t duration;   // Kesinti süresi (saniye)
 };
  
 // Zaman bilgisi yapısı
 struct TimeInfo {
     uint16_t year;       // Yıl
     uint8_t month;       // Ay (1-12)
     uint8_t day;         // Gün (1-31)
     uint8_t hour;        // Saat (0-23)
     uint8_t minute;      // Dakika (0-59)
     uint8_t second;      // Saniye (0-59)
     uint32_t timestamp;  // Unix timestamp
 };
  
 // Sensör veri yapısı
 struct SensorData {
     float temperature1;     // İlk sensörden sıcaklık (°C)
     float humidity1;        // İlk sensörden nem (%)
     float temperature2;     // İkinci sensörden sıcaklık (°C)
     float humidity2;        // İkinci sensörden nem (%)
     float avgTemperature;   // Ortalama sıcaklık (°C)
     float avgHumidity;      // Ortalama nem (%)
     bool sensor1Valid;      // İlk sensör verileri geçerli mi?
     bool sensor2Valid;      // İkinci sensör verileri geçerli mi?
     unsigned long timestamp; // Okuma zamanı
 };
  
 // Sensör kalibrasyon yapısı
 struct SensorCalibration {
     float tempOffset1;      // 1. sensör sıcaklık offseti
     float humOffset1;       // 1. sensör nem offseti
     float tempOffset2;      // 2. sensör sıcaklık offseti
     float humOffset2;       // 2. sensör nem offseti
 };
  
 // Röle durumu yapısı
 struct RelayState {
     bool heater;                      // Isıtıcı durumu
     bool humidifier;                  // Nemlendirici durumu
     bool motor;                       // Motor durumu
     unsigned long lastMotorActivation; // Son motor çalışma zamanı
     unsigned long motorRunningUntil;   // Motorun çalışacağı son zaman
     unsigned long heaterStartTime;     // Isıtıcının son açılış zamanı
     unsigned long totalHeaterOnTime;   // Toplam ısıtıcı çalışma süresi (ms)
     unsigned long heaterActivationCount; // Isıtıcı açılma sayısı
     unsigned long humidifierStartTime; // Nemlendiricinin son açılış zamanı
     unsigned long totalHumidifierOnTime; // Toplam nemlendirici çalışma süresi (ms)
     unsigned long humidifierActivationCount; // Nemlendirici açılma sayısı
     double lastPidOutput;             // Son PID çıkış değeri
     unsigned long windowStartTime;    // PID pencere başlangıç zamanı
 };
  
 // Kuluçka ayarları
 struct IncubationSettings {
     int type;               // Profil tipi (tavuk, ördek, vb.)
     float targetTemp;       // Hedef sıcaklık
     float targetHumidity;   // Hedef nem
     int totalDays;          // Toplam kuluçka süresi
     unsigned long startTime; // Başlangıç zamanı
     bool motorEnabled;      // Motor etkin mi
     float pidKp;            // PID P katsayısı
     float pidKi;            // PID I katsayısı
     float pidKd;            // PID D katsayısı
     float humHysteresis;    // Nem histerezisi
 };
  
 // Motor ayarları
 struct MotorSettings {
     unsigned long duration;  // Motor çalışma süresi (ms)
     unsigned long interval;  // Motor dönüş aralığı (ms)
 };
  
 // PID Ayarları
 struct PidSettings {
     float Kp;               // Orantısal katsayı
     float Ki;               // İntegral katsayı
     float Kd;               // Türev katsayı
     float windowSize;       // Çıkış pencere boyutu (ms)
 };
  
 // Alarm durumu
 struct AlarmStatus {
     bool active;            // Alarm durumu aktif mi?
     AlarmType type;         // Alarm türü
     unsigned long startTime; // Alarmın başlangıç zamanı
     String message;         // Alarm mesajı
 };
  
 // Alarm eşikleri
 struct AlarmThresholds {
     bool alarmEnabled;      // Alarmlar etkin mi
     float highTempThreshold; // Yüksek sıcaklık alarmı eşiği
     float lowTempThreshold;  // Düşük sıcaklık alarmı eşiği
     float highHumThreshold;  // Yüksek nem alarmı eşiği
     float lowHumThreshold;   // Düşük nem alarmı eşiği
     float tempDiffThreshold; // Sıcaklık farkı eşiği
     float humDiffThreshold;  // Nem farkı eşiği
 };
  
 // Profil aşama ayarları
 struct ProfileStageSettings {
     float temperature;      // Hedef sıcaklık
     float humidity;         // Hedef nem
     bool motorActive;       // Motor aktif mi?
     int startDay;           // Başlangıç günü
     int endDay;             // Bitiş günü
 };
  
 // Profil yapısı
 struct Profile {
     ProfileType type;       // Profil türü
     String name;            // Profil adı
     int totalDays;          // Toplam kuluçka süresi
     ProfileStageSettings stages[4]; // Aşama ayarları (maksimum 4 aşama)
     int stageCount;         // Aşama sayısı
 };
  
 // Motor durumu bilgisi
 struct MotorStatusInfo {
     bool isActive;          // Motor çalışıyor mu?
     int remainingMinutes;   // Sonraki çalışmaya kalan dakika
 };
  
 // Kurtarma verisi yapısı
 struct RecoveryData {
     bool incubationActive;  // Kuluçka etkin mi?
     int profileType;        // Profil türü
     uint32_t startTime;     // Kuluçka başlangıç zamanı
     int totalDays;          // Toplam kuluçka süresi
     int currentDay;         // Mevcut gün (son kaydedilen)
     float targetTemp;       // Hedef sıcaklık
     float targetHumidity;   // Hedef nem
     bool motorEnabled;      // Motor etkin mi?
     uint32_t saveTime;      // Kayıt zamanı
 };
  
 // Modül durumu izleme
 struct ModuleStatus {
     bool configInitialized;
     bool dataInitialized;
     bool timeInitialized;
     bool displayInitialized;
     bool sensorInitialized;
     bool controlInitialized;
     bool uiInitialized;
     bool wifiInitialized;
     bool profileInitialized;
     bool alarmInitialized;
     bool powerSaveInitialized;
     bool powerRecoveryInitialized;
     bool otaInitialized;
 };
  
 // Global değişkenlerin extern bildirimleri
 extern IncubationSettings settings;
 extern MotorSettings motorSettings;
 extern AlarmThresholds alarmThresholds;
 extern ModuleStatus moduleStatus;
 extern bool systemInitialized;
 extern bool displayInitialized;
 extern WifiStatus wifiStatus;
 extern Profile currentProfile;  
 extern Profile predefinedProfiles[MAX_PROFILES];
 extern PowerOutage powerOutages[MAX_POWER_OUTAGES];
 extern int powerOutageCount;
 extern IncubationSettings globalCachedSettings;
extern unsigned long globalLastSettingsUpdate;
 
 #endif // GLOBAL_DEFINITIONS_H