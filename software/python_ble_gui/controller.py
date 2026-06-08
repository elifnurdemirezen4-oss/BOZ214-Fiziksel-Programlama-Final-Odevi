import tkinter as tk
import asyncio
import threading
from bleak import BleakClient, BleakScanner

UART_TX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
UART_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

ble_client = None
loop = asyncio.new_event_loop()

def start_async_loop(loop):
    asyncio.set_event_loop(loop)
    loop.run_forever()

threading.Thread(target=start_async_loop, args=(loop,), daemon=True).start()

def notification_handler(sender, data):
    cevap = data.decode('utf-8').strip()
    if "Engel goruldu" in cevap:
        pencere.after(0, lambda: durum_etiketi.config(text="⚠️ DEVAM EDEMİYORUM: ENGEL VAR!", fg="red"))
    elif "Engel kalkti" in cevap:
        pencere.after(0, lambda: durum_etiketi.config(text="✅ Engel Kalktı! Yürüyüşe Devam...", fg="green"))

async def connect_and_listen():
    global ble_client
    pencere.after(0, lambda: durum_etiketi.config(text="Durum: Robot Aranıyor 🔍", fg="orange"))
    
    devices = await BleakScanner.discover()
    robot_device = None
    
    # İŞTE MAC'İ KANDIRDIĞIMIZ YER:
    # Mac inatla eski ismi (BLUAI) hatırlasa bile bağlanacağız!
    for d in devices:
        if d.name and ("DORTBACAK" in d.name or "BLUAI" in d.name):
            robot_device = d
            print(f"Hedef Yakalandı: Mac bu cihazı '{d.name}' olarak görüyor.")
            break
    
    if robot_device:
        try:
            ble_client = BleakClient(robot_device.address)
            await ble_client.connect()
            pencere.after(0, lambda: durum_etiketi.config(text="✅ Bluetooth Bağlantısı Başarılı! 🛑", fg="black"))
            await ble_client.start_notify(UART_TX_CHAR_UUID, notification_handler)
        except Exception as e:
            print(f"Bağlantı Hatası: {e}")
            pencere.after(0, lambda: durum_etiketi.config(text="Hata: Cihaz bulundu ama bağlanılamadı!", fg="red"))
    else:
        pencere.after(0, lambda: durum_etiketi.config(text="Hata: Robot Bulunamadı! Gücü Kontrol Et.", fg="red"))

def komut_gonder(komut_metni):
    if ble_client and ble_client.is_connected:
        tam_komut = f"{komut_metni}\n".encode('utf-8')
        asyncio.run_coroutine_threadsafe(ble_client.write_gatt_char(UART_RX_CHAR_UUID, tam_komut), loop)
        
        if komut_metni == "ILERI":
            durum_etiketi.config(text="Durum: İleri Gidiliyor ⬆️", fg="blue")
        elif komut_metni == "GERI":
            durum_etiketi.config(text="Durum: Geri Gidiliyor ⬇️", fg="blue")
        elif komut_metni == "DUR":
            durum_etiketi.config(text="Durum: Bekliyor 🛑", fg="black")

def baglan_butonu_fonk():
    asyncio.run_coroutine_threadsafe(connect_and_listen(), loop)

# ==========================================
# --- ARAYÜZ (GUI) TASARIMI ---
# ==========================================
pencere = tk.Tk()
pencere.title("DÖRTBACAK Kablosuz Kontrol")
pencere.geometry("400x350") 
pencere.configure(bg="#f0f0f0")

baslik = tk.Label(pencere, text="🤖 DÖRTBACAK Robot Kontrolü", font=("Arial", 16, "bold"), bg="#f0f0f0")
baslik.pack(pady=15)

btn_baglan = tk.Button(pencere, text="Bluetooth ile Robota Bağlan", font=("Arial", 12, "bold"), bg="lightblue", command=baglan_butonu_fonk)
btn_baglan.pack(pady=10)

durum_etiketi = tk.Label(pencere, text="Durum: Bağlantı Bekleniyor...", font=("Arial", 13, "bold"), fg="gray", bg="#f0f0f0")
durum_etiketi.pack(pady=10)

btn_ileri = tk.Button(pencere, text="İLERİ (W)", font=("Arial", 12), width=15, command=lambda: komut_gonder("ILERI"))
btn_ileri.pack(pady=5)

btn_dur = tk.Button(pencere, text="DUR (X)", font=("Arial", 12, "bold"), fg="red", width=15, command=lambda: komut_gonder("DUR"))
btn_dur.pack(pady=5)

btn_geri = tk.Button(pencere, text="GERİ (S)", font=("Arial", 12), width=15, command=lambda: komut_gonder("GERI"))
btn_geri.pack(pady=5)

pencere.bind('<w>', lambda event: komut_gonder("ILERI"))
pencere.bind('<s>', lambda event: komut_gonder("GERI"))
pencere.bind('<x>', lambda event: komut_gonder("DUR"))
pencere.bind('<W>', lambda event: komut_gonder("ILERI"))
pencere.bind('<S>', lambda event: komut_gonder("GERI"))
pencere.bind('<X>', lambda event: komut_gonder("DUR"))

pencere.mainloop()