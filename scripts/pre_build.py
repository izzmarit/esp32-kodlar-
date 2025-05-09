#!/usr/bin/env python3

import os
import shutil
import datetime

Import("env")

# Proje kök dizinini al
project_dir = env.subst("$PROJECT_DIR")

# Proje bilgilerini içeren etiket dosyası
# PlatformIO'ya uygun yolu belirtin
version_file = os.path.join(project_dir, "src", "include", "build_info.h")

# Eğer klasör yoksa oluştur
if not os.path.exists(os.path.dirname(version_file)):
    os.makedirs(os.path.dirname(version_file))

# Versiyon bilgilerini oluştur
build_date = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
build_number = 1

# Eğer versiyon dosyası varsa, build numarasını artır
if os.path.exists(version_file):
    with open(version_file, "r") as f:
        content = f.read()
        for line in content.split("\n"):
            if "#define BUILD_NUMBER" in line:
                try:
                    build_number = int(line.split(" ")[2]) + 1
                except:
                    build_number = 1

# Etiket dosyasını oluştur
with open(version_file, "w") as f:
    f.write(f"""/**
 * Kuluçka Makinesi Kontrol Sistemi - Otomatik Oluşturulan Build Bilgileri
 * 
 * Bu dosya her derleme öncesinde otomatik olarak oluşturulur.
 */

#ifndef BUILD_INFO_H
#define BUILD_INFO_H

#define BUILD_DATE "{build_date}"
#define BUILD_NUMBER {build_number}

#endif // BUILD_INFO_H
""")

# İşlem bittiğinde bilgi mesajı
print(f"Build bilgileri güncellendi: {build_date}, Build #{build_number}")

# LittleFS klasörü yapısını oluştur
def ensure_dir(dir_path):
    if not os.path.exists(dir_path):
        os.makedirs(dir_path)

# Data klasörünü oluştur ve temel dosyaları kopyala
data_dir = os.path.join(project_dir, "data")
ensure_dir(data_dir)

# Eğer profiles.json dosyası yoksa bir örnek oluştur
profiles_json = os.path.join(data_dir, "profiles.json")
if not os.path.exists(profiles_json):
    with open(profiles_json, "w") as f:
        f.write("""[
  {
    "type": 0,
    "name": "Tavuk",
    "total_days": 21,
    "stages": [
      {
        "temp": 37.8,
        "humidity": 55.0,
        "motor_active": true,
        "start_day": 1,
        "end_day": 9
      },
      {
        "temp": 37.5,
        "humidity": 60.0,
        "motor_active": true,
        "start_day": 10,
        "end_day": 17
      },
      {
        "temp": 37.2,
        "humidity": 70.0,
        "motor_active": false,
        "start_day": 18,
        "end_day": 21
      }
    ]
  }
]""")
    print(f"Örnek profil dosyası oluşturuldu: {profiles_json}")

# Eğer system.json dosyası yoksa bir örnek oluştur
system_json = os.path.join(data_dir, "system.json")
if not os.path.exists(system_json):
    with open(system_json, "w") as f:
        f.write("""{
  "settings": {
    "type": 0,
    "target_temp": 37.8,
    "target_humidity": 65.0,
    "total_days": 21,
    "start_time": 0,
    "motor_enabled": true,
    "pid_kp": 10.0,
    "pid_ki": 0.1,
    "pid_kd": 50.0,
    "hum_hysteresis": 2.0
  },
  "motor": {
    "duration": 14000,
    "interval": 7200000
  },
  "alarm": {
    "enabled": true,
    "high_temp": 38.5,
    "low_temp": 36.5,
    "high_hum": 80.0,
    "low_hum": 50.0,
    "temp_diff": 2.0,
    "hum_diff": 10.0
  }
}""")
    print(f"Örnek sistem ayarları dosyası oluşturuldu: {system_json}")