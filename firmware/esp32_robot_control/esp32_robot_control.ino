#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// ================= BLE (BLUETOOTH) AYARLARI =================
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
String bleKomut = "";

// Python kodu ile eşleşecek özel kimlikler (UUID)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { 
      deviceConnected = true; 
      Serial.println("Bluetooth Baglandi!");
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Bluetooth Koptu! Tekrar yayin yapiliyor...");
      pServer->startAdvertising(); // Bağlantı koparsa hemen tekrar görünür ol
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      // Yeni kütüphaneye uygun olarak doğrudan Arduino String'i kullanıyoruz
      String rxValue = pCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        rxValue.trim();
        bleKomut = rxValue; 
      }
    }
};

// Hem seri porta hem de Bluetooth üzerinden Python'a mesaj gönderen fonksiyon
void bleMesajGonder(String mesaj) {
  Serial.println(mesaj);
  if (deviceConnected) {
    pTxCharacteristic->setValue(mesaj.c_str());
    pTxCharacteristic->notify();
  }
}

// ================= ULTRASONİK SENSÖR PİNLERİ =================
const int TRIG_PIN = 5;  
const int ECHO_PIN = 18; 
const int ENGEL_MESAFESI = 15; 

// ================= PİN HARİTASI =================
const int PIN_FR_COXA = 0;   const int PIN_FR_TIBIA = 2;
const int PIN_BL_COXA = 8;   const int PIN_BL_TIBIA = 10;
const int PIN_FL_COXA = 4;   const int PIN_FL_TIBIA = 6;
const int PIN_BR_COXA = 12;  const int PIN_BR_TIBIA = 14;

// ================= AÇI DEĞERLERİ =================
const int TIBIA_YERDE = 375;   
const int TIBIA_HAVADA = 340;  
const int FR_COXA_CENTER = 375; const int FR_COXA_ILERI = 395; const int FR_COXA_GERI = 355;
const int BR_COXA_CENTER = 375; const int BR_COXA_ILERI = 395; const int BR_COXA_GERI = 355;
const int FL_COXA_CENTER = 375; const int FL_COXA_ILERI = 355; const int FL_COXA_GERI = 395;
const int BL_COXA_CENTER = 375; const int BL_COXA_ILERI = 355; const int BL_COXA_GERI = 395;

// ================= HAFIZA DEĞİŞKENLERİ =================
int mevcutKonum[16]; 
String aktifDurum = "DUR"; 
bool engelVarMi = false; 

// ================= MESAFE ÖLÇÜM FONKSİYONU =================
long mesafeOlc() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10); digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  if (duration == 0) return 999; 
  return duration * 0.034 / 2; 
}
 
// ================= YUMUŞAK HAREKET FONKSİYONU =================
void yumusakHareket(int pin, int hedefAci) {
  int baslangic = mevcutKonum[pin];
  int adimGecikmesi = 5; 
  if (baslangic == hedefAci) return; 
  if (baslangic < hedefAci) {
    for (int p = baslangic; p <= hedefAci; p++) { pwm.setPWM(pin, 0, p); delay(adimGecikmesi); }
  } else {
    for (int p = baslangic; p >= hedefAci; p--) { pwm.setPWM(pin, 0, p); delay(adimGecikmesi); }
  }
  mevcutKonum[pin] = hedefAci; delay(15); 
}

// ================= GÜÇ KORUMA FONKSİYONU =================
void motorlariSerbestBirak() {
  int butunMotorlar[] = {0, 2, 4, 6, 8, 10, 12, 14};
  for(int i=0; i<8; i++) { pwm.setPWM(butunMotorlar[i], 0, 4096); }
}
 
void setup() {
  Serial.begin(115200); 
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);

  // --- BLE KURULUMU ---
  BLEDevice::init("DORTBACAK_V1"); // Mac'i kandırmak için V1 ekledik!
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  
  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  
  pService->start();
  pServer->getAdvertising()->start();
  // --------------------

  pwm.begin();
  pwm.setOscillatorFrequency(27000000); pwm.setPWMFreq(50); delay(100);
 
  Serial.println("Robot hazirlik pozisyonuna geciyor...");
  int butunMotorlar[] = {0, 2, 4, 6, 8, 10, 12, 14};
  for(int i=0; i<8; i++) {
    int pin = butunMotorlar[i];
    pwm.setPWM(pin, 0, 375); mevcutKonum[pin] = 375; delay(200); 
  }
  
  delay(1000); motorlariSerbestBirak(); 
  Serial.println("ESP32-S BLE Hazir! Eslenme bekleniyor...");
}
 
void loop() {
  // 1. BLUETOOTH VEYA SERİ PORTTAN YENİ KOMUT GELDİ Mİ?
  String command = "";
  
  if (bleKomut != "") {
    command = bleKomut;
    bleKomut = ""; // İşledikten sonra temizle
  } 
  else if (Serial.available() > 0) {
    command = Serial.readStringUntil('\n');
    command.trim(); 
  }

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

  // 2. AKTİF DURUMA GÖRE SÜREKLİ HAREKET ET
  if (aktifDurum == "ILERI") {
      // ==== GÜVENLİK REFLEKSİ KONTROLÜ ====
      long mesafe = mesafeOlc();
      
      if (mesafe <= ENGEL_MESAFESI) {
        if (!engelVarMi) { 
            // Python Arayüzüne Bluetooth ile uyarı gönderiyoruz
            bleMesajGonder("--> DIKKAT! Engel goruldu");
            motorlariSerbestBirak();
            engelVarMi = true;
        }
        delay(100); 
        return; 
      } 
      else {
        if (engelVarMi) { 
            bleMesajGonder("--> Engel kalkti");
            engelVarMi = false;
        }
      }
      // ====================================

      yumusakHareket(PIN_FR_TIBIA, TIBIA_HAVADA); yumusakHareket(PIN_BL_TIBIA, TIBIA_HAVADA); 
      yumusakHareket(PIN_FR_COXA, FR_COXA_ILERI); yumusakHareket(PIN_BL_COXA, BL_COXA_ILERI); 
      yumusakHareket(PIN_FL_COXA, FL_COXA_GERI);  yumusakHareket(PIN_BR_COXA, BR_COXA_GERI);  
      yumusakHareket(PIN_FR_TIBIA, TIBIA_YERDE);  yumusakHareket(PIN_BL_TIBIA, TIBIA_YERDE);  
     
      yumusakHareket(PIN_FL_TIBIA, TIBIA_HAVADA); yumusakHareket(PIN_BR_TIBIA, TIBIA_HAVADA); 
      yumusakHareket(PIN_FL_COXA, FL_COXA_ILERI); yumusakHareket(PIN_BR_COXA, BR_COXA_ILERI); 
      yumusakHareket(PIN_FR_COXA, FR_COXA_GERI);  yumusakHareket(PIN_BL_COXA, BL_COXA_GERI);  
      yumusakHareket(PIN_FL_TIBIA, TIBIA_YERDE);  yumusakHareket(PIN_BR_TIBIA, TIBIA_YERDE);  
  }
  else if (aktifDurum == "GERI") {
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