// ================= KÜTÜPHANELER =================
#include <Wire.h>                  // I2C haberleşme protokolü (PCA9685 sürücüsü için gerekli)
#include <Adafruit_PWMServoDriver.h>// 16 kanallı PWM servo sürücü kartının kütüphanesi
#include <BLEDevice.h>             // ESP32 Bluetooth Low Energy (BLE) temel kütüphanesi
#include <BLEServer.h>             // ESP32'yi bir Bluetooth sunucusu yapmak için
#include <BLEUtils.h>              // BLE yardımcı fonksiyonları
#include <BLE2902.h>               // Veri bildirimleri (Notify) için gerekli BLE standardı

// PCA9685 sürücüsünü 0x40 I2C adresiyle başlatıyoruz
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// ================= BLE (BLUETOOTH) AYARLARI =================
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic; // ESP32'den veriyi (Tx) göndereceğimiz kanal
bool deviceConnected = false;          // Cihazın bağlı olup olmadığını tutan bayrak (flag)
String bleKomut = "";                  // Gelen komutu geçici olarak saklayacağımız değişken

// BLE cihazlarının birbirini tanıması için gereken evrensel benzersiz kimlikler (UUID)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // Ana servis kimliği
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Veri alma (Receive) kimliği
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Veri gönderme (Transmit) kimliği

// Bluetooth sunucu olaylarını (bağlanma/kopma) yakalayan sınıf
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { 
      deviceConnected = true; 
      Serial.println("Bluetooth Baglandi!");
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Bluetooth Koptu! Tekrar yayin yapiliyor...");
      pServer->startAdvertising(); // Bağlantı koparsa cihazın tekrar bulunabilir olmasını sağlar
    }
};

// Bluetooth üzerinden gelen verileri dinleyen sınıf
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      // Gelen veriyi okuyup String formatına çevirir
      String rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        rxValue.trim();     // Başındaki ve sonundaki boşlukları temizler
        bleKomut = rxValue; // Ana döngüde işlenmek üzere değişkene aktarır
      }
    }
};

// Çift yönlü iletişim fonksiyonu: Hem seri porta yazdırır hem de bağlıysa BLE üzerinden arayüze/telefona iletir
void bleMesajGonder(String mesaj) {
  Serial.println(mesaj);
  if (deviceConnected) {
    pTxCharacteristic->setValue(mesaj.c_str());
    pTxCharacteristic->notify(); // İstemciye (örn. Python arayüzü) yeni veri olduğunu bildirir
  }
}

// ================= ULTRASONİK SENSÖR PİNLERİ =================
const int TRIG_PIN = 5;  // Sinyal gönderen pin (Tetik)
const int ECHO_PIN = 18; // Yansıyan sinyali okuyan pin
const int ENGEL_MESAFESI = 15; // Robotun durması gereken kritik mesafe (cm)

// ================= PİN HARİTASI (KINEMATİK YAPI) =================
// Robotun 4 bacağının omuz (COXA) ve diz (TIBIA) motorlarının PCA9685 üzerindeki port numaraları
const int PIN_FR_COXA = 0;   const int PIN_FR_TIBIA = 2;  // Ön Sağ (Front Right)
const int PIN_BL_COXA = 8;   const int PIN_BL_TIBIA = 10; // Arka Sol (Back Left)
const int PIN_FL_COXA = 4;   const int PIN_FL_TIBIA = 6;  // Ön Sol (Front Left)
const int PIN_BR_COXA = 12;  const int PIN_BR_TIBIA = 14; // Arka Sağ (Back Right)

// ================= AÇI DEĞERLERİ =================
// PWM sinyali için pulse uzunlukları (Mekanik kalibrasyon değerleri)
const int TIBIA_YERDE = 375;   // Bacağın yere basma açısı
const int TIBIA_HAVADA = 340;  // Bacağın adım atmak için kalkma açısı

// Omuz motorlarının merkez, ileri adım ve geri adım kalibrasyon açıları
const int FR_COXA_CENTER = 375; const int FR_COXA_ILERI = 395; const int FR_COXA_GERI = 355;
const int BR_COXA_CENTER = 375; const int BR_COXA_ILERI = 395; const int BR_COXA_GERI = 355;
const int FL_COXA_CENTER = 375; const int FL_COXA_ILERI = 355; const int FL_COXA_GERI = 395;
const int BL_COXA_CENTER = 375; const int BL_COXA_ILERI = 355; const int BL_COXA_GERI = 395;

// ================= HAFIZA DEĞİŞKENLERİ =================
int mevcutKonum[16];           // Her bir servonun anlık pozisyonunu tutan dizi (Yumuşak hareket için gerekli)
String aktifDurum = "DUR";     // Durum makinesi (State Machine) değişkeni
bool engelVarMi = false;       // Sensör engel tespit etti mi durumunu tutar

// ================= MESAFE ÖLÇÜM FONKSİYONU =================
long mesafeOlc() {
  // Sensörü temizle ve 10 mikrosaniyelik bir ses dalgası gönder
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10); digitalWrite(TRIG_PIN, LOW);
  
  // Ses dalgasının gidip gelme süresini ölç (Zaman aşımı: 30000 mikrosaniye)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (duration == 0) return 999; // Okuma yapılamadıysa güvenli bir yüksek değer döndür
  
  // Yol = Hız * Zaman formülünden mesafeyi cm cinsinden hesapla (Ses hızı ~0.034 cm/us)
  return duration * 0.034 / 2; 
}
 
// ================= YUMUŞAK HAREKET (İNTERPOLASYON) FONKSİYONU =================
// Motorların ani hareket edip sarsılmasını önler, başlangıç ve hedef açı arasında yavaşça adım atar
void yumusakHareket(int pin, int hedefAci) {
  int baslangic = mevcutKonum[pin];
  int adimGecikmesi = 5; // Hızı belirleyen gecikme süresi
  
  if (baslangic == hedefAci) return; // Zaten hedefteyse işlem yapma
  
  if (baslangic < hedefAci) {
    for (int p = baslangic; p <= hedefAci; p++) { pwm.setPWM(pin, 0, p); delay(adimGecikmesi); }
  } else {
    for (int p = baslangic; p >= hedefAci; p--) { pwm.setPWM(pin, 0, p); delay(adimGecikmesi); }
  }
  mevcutKonum[pin] = hedefAci; // Yeni konumu hafızaya kaydet
  delay(15); 
}

// ================= GÜÇ KORUMA FONKSİYONU =================
// Motorlara giden PWM sinyalini keserek aşırı ısınmayı ve güç tüketimini önler
void motorlariSerbestBirak() {
  int butunMotorlar[] = {0, 2, 4, 6, 8, 10, 12, 14};
  for(int i=0; i<8; i++) { pwm.setPWM(butunMotorlar[i], 0, 4096); } // 4096 değeri PWM sinyalini tamamen kapatır
}
 
void setup() {
  Serial.begin(115200); 
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);

  // --- BLE KURULUMU ---
  BLEDevice::init("DORTBACAK_V1"); // Cihazın dışarıdan görünecek adı
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  
  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  
  pService->start();
  pServer->getAdvertising()->start(); // Cihazı keşfedilebilir hale getir
  // --------------------

  // --- MOTOR SÜRÜCÜ KURULUMU ---
  pwm.begin();
  pwm.setOscillatorFrequency(27000000); // PCA9685 iç osilatör frekansı (hassasiyet için)
  pwm.setPWMFreq(50); delay(100);       // Analog servolar için standart 50Hz (20ms periyot) frekans ayarı
 
  Serial.println("Robot hazirlik pozisyonuna geciyor...");
  int butunMotorlar[] = {0, 2, 4, 6, 8, 10, 12, 14};
  
  // Tüm motorları güvenli başlangıç açısına (merkeze) çek
  for(int i=0; i<8; i++) {
    int pin = butunMotorlar[i];
    pwm.setPWM(pin, 0, 375); mevcutKonum[pin] = 375; delay(200); 
  }
  
  delay(1000); 
  motorlariSerbestBirak(); // Bekleme modunda enerji tasarrufu için motorları sal
  Serial.println("ESP32-S BLE Hazir! Eslenme bekleniyor...");
}
 
void loop() {
  // 1. HABERLEŞME KONTROLÜ: BLE veya Seri Port üzerinden yeni veri var mı?
  String command = "";
  
  if (bleKomut != "") {
    command = bleKomut;
    bleKomut = ""; // İşledikten sonra komut çakışmasını önlemek için temizle
  } 
  else if (Serial.available() > 0) {
    command = Serial.readStringUntil('\n');
    command.trim(); 
  }

  // Komut alındıysa durum makinesini (aktifDurum) güncelle
  if (command != "") {
    if (command == "ILERI" || command == "GERI") {
      aktifDurum = command; 
      Serial.println("Surekli hareket basliyor: " + aktifDurum);
      engelVarMi = false; 
    }
    else if (command == "DUR") {
      aktifDurum = "DUR"; 
      Serial.println("DUR komutu alindi. Robot durduruluyor...");
      motorlariSerbestBirak(); 
    }
  }

  // 2. HAREKET VE GÜVENLİK DÖNGÜSÜ
  if (aktifDurum == "ILERI") {
      // ==== GÜVENLİK REFLEKSİ (ENGELLDEN KAÇINMA) ====
      long mesafe = mesafeOlc();
      
      if (mesafe <= ENGEL_MESAFESI) {
        if (!engelVarMi) { 
            // Engel ilk defa görüldüyse arayüze bilgi gönder ve motorları durdur
            bleMesajGonder("--> DIKKAT! Engel goruldu");
            motorlariSerbestBirak();
            engelVarMi = true;
        }
        delay(100); 
        return; // Engel varken alt satırlardaki yürüyüş koduna geçmesini engeller (döngüyü başa sarar)
      } 
      else {
        if (engelVarMi) { 
            // Engel ortadan kalktıysa arayüze bilgi ver
            bleMesajGonder("--> Engel kalkti");
            engelVarMi = false;
        }
      }
      // ====================================

      // YÜRÜYÜŞ DÖNGÜSÜ (Çapraz Bacak Yürüyüşü - Trot Gait)
      // 1. Aşama: Sağ Ön ve Sol Arka bacakları havaya kaldır
      yumusakHareket(PIN_FR_TIBIA, TIBIA_HAVADA); yumusakHareket(PIN_BL_TIBIA, TIBIA_HAVADA); 
      // Omuzlardan bu iki bacağı ileri atarken, yerdeki bacakları geriye çekerek gövdeyi iter
      yumusakHareket(PIN_FR_COXA, FR_COXA_ILERI); yumusakHareket(PIN_BL_COXA, BL_COXA_ILERI); 
      yumusakHareket(PIN_FL_COXA, FL_COXA_GERI);  yumusakHareket(PIN_BR_COXA, BR_COXA_GERI);  
      // Kaldırılan bacakları yere bas
      yumusakHareket(PIN_FR_TIBIA, TIBIA_YERDE);  yumusakHareket(PIN_BL_TIBIA, TIBIA_YERDE);  
     
      // 2. Aşama: Sol Ön ve Sağ Arka bacakları havaya kaldır
      yumusakHareket(PIN_FL_TIBIA, TIBIA_HAVADA); yumusakHareket(PIN_BR_TIBIA, TIBIA_HAVADA); 
      // Havada olanları ileri at, yerde olanları geri çek
      yumusakHareket(PIN_FL_COXA, FL_COXA_ILERI); yumusakHareket(PIN_BR_COXA, BR_COXA_ILERI); 
      yumusakHareket(PIN_FR_COXA, FR_COXA_GERI);  yumusakHareket(PIN_BL_COXA, BL_COXA_GERI);  
      // Kaldırılan bacakları yere bas
      yumusakHareket(PIN_FL_TIBIA, TIBIA_YERDE);  yumusakHareket(PIN_BR_TIBIA, TIBIA_YERDE);  
  }
  else if (aktifDurum == "GERI") {
      // Geri yürüyüş döngüsü: İleri yürüyüşün omuz motorlarındaki yönlerinin tersine çevrilmiş hali
      yumusakHareket(PIN_FR_TIBIA, TIBIA_HAVADA); yumusakHareket(PIN_BL_TIBIA, TIBIA_HAVADA); 
      yumusakHareket(PIN_FR_COXA, FR_COXA_GERI);  yumusakHareket(PIN_BL_COXA, BL_COXA_GERI); 
      yumusakHareket(PIN_FL_COXA, FL_COXA_ILERI); yumusakHareket(PIN_BR_COXA, BR_COXA_ILERI);  
      yumusakHareket(PIN_FR_TIBIA, TIBIA_YERDE);  yumusakHareket(PIN_BL_TIBIA, TIBIA_YERDE);  
     
      yumusakHareket(PIN_FL_TIBIA, TIBIA_HAVADA); yumusakHareket(PIN_BR_TIBIA, TIBIA_HAVADA); 
      yumusakHareket(PIN_FL_COXA, FL_COXA_GERI);  yumusakHareket(PIN_BR_COXA, BR_COXA_GERI); 
      yumusakHareket(PIN_FR_COXA, FR_COXA_ILERI); yumusakHareket(PIN_BL_COXA, BL_COXA_ILERI);  
      yumusakHareket(PIN_FL_TIBIA, TIBIA_YERDE);  yumusakHareket(PIN_BR_TIBIA, TIBIA_YERDE);  
  }
}