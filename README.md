# ESP NFC Project

A modular NFC tag reader/writer system using ESP32 with support for buzzer, display, and optional Wi-Fi communication.

## 📁 Folder Structure

- `arduino/` – All Arduino `.ino` source code
- `arduino/CompleteRunningCode/` – Alternate working versions of the code
- `python/` – Python tool for interacting with NFC
- `docs/` – Wiring guide and documentation

## 🔧 Setup

### Arduino
1. Open any `.ino` file with Arduino IDE
2. Select your ESP32 board and port
3. Upload and monitor via serial

### Python
```bash
cd python
python nfc_cmd.py
