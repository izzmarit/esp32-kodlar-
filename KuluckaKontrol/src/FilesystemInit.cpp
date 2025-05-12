/**
 * Kuluçka Makinesi Kontrol Sistemi - Dosya Sistemi Başlatma
 */

 #include <Arduino.h>
 #include <LittleFS.h>  // SPIFFS yerine LittleFS
 #include "../include/AppConfig.h"
  
 const char* FS_TAG = "FS";
  
// LittleFS dosya sistemi başlatma fonksiyonu
bool InitializeFilesystem() {
    // Önce formatlamadan başlatmayı dene
if (!LittleFS.begin(false)) {
    LOG_WARNING(FS_TAG, "LittleFS başlatılamadı, formatlamayı deniyorum...");
    
    // Formatlamayı dene
    if (!LittleFS.format()) {
        LOG_ERROR(FS_TAG, "LittleFS formatlanamadı!");
        return false;
    }
    
    // Format sonrası tekrar başlat
    if (!LittleFS.begin(false)) {
        LOG_ERROR(FS_TAG, "LittleFS format sonrası başlatılamadı!");
        return false;
    }
    
    LOG_INFO(FS_TAG, "LittleFS formatlandı ve başlatıldı");
} else {
    LOG_INFO(FS_TAG, "LittleFS başarıyla başlatıldı");
}
    
    // Dosya sistemi bilgilerini görüntüle
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    
    LOG_INFO(FS_TAG, "LittleFS Bilgisi:");
    LOG_INFO(FS_TAG, "  Toplam: %lu KB", totalBytes / 1024);
    LOG_INFO(FS_TAG, "  Kullanılan: %lu KB", usedBytes / 1024);
    LOG_INFO(FS_TAG, "  Boş: %lu KB", (totalBytes - usedBytes) / 1024);
    
    return true;
}
  
 // Dosya sisteminin doğru şekilde başlatılmasını sağlamak için statik sınıf
 class FileSystemInitializer {
 public:
     FileSystemInitializer() {
         // Sınıf ilk oluşturulduğunda dosya sistemini başlat
         initialized = InitializeFilesystem();
     }
     
     bool isInitialized() const {
         return initialized;
     }
     
 private:
     bool initialized;
 };
  