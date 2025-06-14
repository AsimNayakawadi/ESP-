# nfc_cmd.py
import sys
import serial

def main():
    if len(sys.argv) < 2:
        print("Usage: python nfc_cmd.py <COM_PORT> [baud]")
        sys.exit(1)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) >= 3 else 115200

    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as e:
        print(f"ERROR opening {port}@{baud}: {e}")
        sys.exit(1)

    print(f"Listening on {port} @ {baud} baud. Ctrl–C to exit.\n")
    try:
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(line)
    except KeyboardInterrupt:
        print("\nExiting.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
