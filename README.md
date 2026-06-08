<img width="1448" height="726" alt="dort_bacak2" src="https://github.com/user-attachments/assets/fc2ed993-d633-402a-8371-d9c8284180af" />

# SPİDER ASSİST (DÖRT BACAK)

Bu proje, her bacağında iki eklem (Coxa ve Tibia) bulunan, toplam 8 servo motor ile hareket eden dört bacaklı (quadruped) bir robotun donanım ve yazılım altyapısını içermektedir. Sistem, ESP32 mikrodenetleyicisi üzerinden Bluetooth Low Energy (BLE) protokolü ile haberleşmekte ve Python tabanlı bir masaüstü arayüzü ile kontrol edilmektedir.

## 🎯 Proje Amacı 

Bu projenin temel amacı, hareket kısıtlılığı yaşayan bireylerin çevresel bağımsızlıklarını artırmak üzere; Bluetooth Low Energy (BLE) üzerinden uzaktan kontrol edilebilen, otonom engel algılama refleksine sahip, 8 serbestlik dereceli (8-DOF) bir masaüstü mekatronik robot asistan geliştirmektir. Geliştirilen bu sistem, kullanıcı dostu bir arayüz ile mekatronik donanımı entegre ederek, engelli bireylerin asgari fiziksel eforla bulundukları kısıtlı alanlarda güvenli bir şekilde erişim ve kontrol sağlamalarına olanak tanımayı hedeflemektedir.
## 📌 Proje Özellikleri

* **Kablosuz Kontrol:** BLE (Bluetooth Low Energy) teknolojisi kullanılarak düşük gecikmeli, asenkron motor kontrolü.
* **Engelden Kaçma Otonomisi:** HC-SR04 ultrasonik sensörü ile 15 cm altındaki engellerin tespiti. Engel algılandığında motorlar durdurulur ve arayüze anlık uyarı gönderilir.
* **Yumuşak Hareket Algoritması (Smooth Kinematics):** Servo motorların ani akım çekmesini ve mekanik hasar görmesini engellemek amacıyla `yumusakHareket` fonksiyonu ile kademeli açı geçişleri.
* **Güç Tasarrufu:** Robot durur pozisyondayken PWM sinyalleri kesilerek motorlar serbest bırakılır (`motorlariSerbestBirak`), böylece aşırı ısınma ve güç tüketimi önlenir.
* **Kullanıcı Arayüzü (GUI):** Python `Tkinter` ve `Bleak` kütüphaneleri kullanılarak geliştirilmiş, klavye kısayollarını (W, A, S, X) destekleyen asenkron masaüstü kontrol yazılımı.

## 🛠️ Donanım Gereksinimleri

* 1x ESP32 (veya ESP32-S3) Mikrodenetleyici
* 1x PCA9685 16-Kanal PWM Servo Sürücü Modülü (I2C Adresi: 0x40)
* 8x Servo Motor (SG90, MG90S vb.)
* 1x HC-SR04 Ultrasonik Mesafe Sensörü
* 1x XL4015 Voltaj Düşürücü (Buck)
* 1x 3.3V-5V Lojik Dönüştürücü
* 2x 18650 Li-ion Pil (Sistem güç kaynağı)

## 🖨️ 3D Baskı ve Mekanik Tasarım

Robotun fiziksel gövdesini ve eklem parçalarını oluşturmak için gerekli olan tüm `.stl` uzantılı 3D model dosyaları `3d_models/` klasöründe sunulmuştur. Parçaların üretimi için standart PLA malzeme ve optimum dayanıklılık/ağırlık dengesi için %30 doluluk (infill) oranı önerilmektedir.

## 💻 Kullanılan Teknolojiler ve Kütüphaneler

**Gömülü Yazılım (C++ / Arduino IDE):**
* `Wire.h` - I2C iletişimi
* `Adafruit_PWMServoDriver.h` - PCA9685 kontrolü
* `BLEDevice.h`, `BLEServer.h`, `BLEUtils.h`, `BLE2902.h` - Bluetooth BLE protokolü

**Masaüstü Yazılımı (Python):**
* `tkinter` - Grafiksel Kullanıcı Arayüzü (GUI)
* `asyncio` & `threading` - Asenkron BLE haberleşmesi ve arayüz tepkiselliği
* `bleak` - Platform bağımsız BLE istemci bağlantısı

## 🚀 Kurulum ve Çalıştırma

### 1. Mikrodenetleyicinin Programlanması
1. Arduino IDE üzerinde ESP32 kart paketinin yüklü olduğundan emin olun.
2. `Adafruit PWM Servo Driver Library` kütüphanesini kütüphane yöneticisinden kurun.
3. Repoda bulunan `.ino` uzantılı dosyayı ESP32 kartınıza yükleyin. 

### 2. Python Arayüzünün Çalıştırılması
Python ortamınızda gerekli kütüphaneleri kurun:
```bash
pip install bleak asyncio.
```
### 3. `software/python_ble_gui` dizinine gidin ve arayüzü başlatın:
   ```bash
   cd software/python_ble_gui
   python controller.py
   ```
### 4. 
Arayüz açıldığında "Bluetooth ile Robota Bağlan" butonuna tıklayarak eşleşmeyi tamamlayın. Klavye üzerinden (W, A, S) kısayollarını kullanarak veya butonlarla robotu kontrol edebilirsiniz.

## 👥 Proje Yürütücüleri
* Furkan Solmaz
* Elif Nur Demirezen

## 📄 Lisans

Bu proje MIT Lisansı ile lisanslanmıştır. Daha fazla bilgi için `LICENSE` dosyasına göz atabilirsiniz.
