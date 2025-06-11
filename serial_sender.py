import serial
import time

PORT = "COM3"
BAUD_RATE = 9600
CSV_FILE = "weather_data.csv"

def send_weather_data():
    try:
        with serial.Serial(PORT, BAUD_RATE, timeout=1) as ser:
            time.sleep(2)

            with open(CSV_FILE, "r") as file:
                lines = file.readlines()

            print("Start")
            for line in lines:
                line = line.strip()
                if line:
                    ser.write((line + "\n").encode())
                    print("Sending:", line)
                    time.sleep(0.5)

            print("Complete")

    except serial.SerialException as e:
        print(f"× Serial port error: {e}")
    except FileNotFoundError:
        print(f"× File not found: {CSV_FILE}")

if __name__ == "__main__":
    send_weather_data()
