# TamAIgotchi
ESP32 based client for [LocalAI](https://localai.io/)

This is a basic demonstration on how an ESP32 with a digital microphone and oled display can act as frontend to LocalAI.
It can be compiled and installed using the Arduino IDE

## Required Parts

- ESP32 with PSRAM. For this project a LOLIN S2 Mini was used
- INMP441 I2S Microphone
- 0,96 Zoll OLED Display I2C 128 x 64

## Required Software

- [Arduino IDE](https://docs.arduino.cc/software/ide/)
- [Arduino-ESP32](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)

## Connection Diagram

![Fritzing Connection Diagram](diagram/TamAIgotchi.png)

## Installation

```
git clone https://github.com/a-i-a-d/TamAIgotchi.git
cd TamAIgotchi
arduino-ide TamAIgotchi/TamAIgotchi.ino
```

## Configuration

### LocalAI endpoint & key

The LocalAI API URL and key are not hardcoded. They are stored by the ESP-Wifi-Config library and configured through the same web setup page as the WiFi:

- The values in `config.h` (`api_url` / `api_key`) only act as initial defaults (first boot / after a full reset).
- Change them at runtime: open the setup page (`http://<device-ip>:8080`, or `http://192.168.1.1:8080` in AP mode), go to the **Custom** tab, enter the API URL and key, and press **Save**. The device reboots and picks up the new values.
- Please make sure to escape dots (.) in the URL string, e.g. `http://192\.168\.1\.5:8080/v1/`.

> **Note:** the setup page requires a login. The default credentials are **username `admin`, password `pass_ESP`** — you can change them on the **Security** tab of the setup page.

### WiFi

The WiFi credentials are not hardcoded. They are stored by the ESP-Wifi-Config library and configured through a web setup page:

- If the ESP32 can connect to a known network, the display shows the IP address it was assigned.
- If it can't reach a known network, it starts an access point (named `TamAIgotchi_<mac>`). The display shows the access point name and its IP address (`192.168.1.1`). Connect to that access point and open the setup page (`http://192.168.1.1:8080`) to enter your WiFi SSID and password.

> **Note:** the setup page requires a login. The default credentials are **username `admin`, password `pass_ESP`** — you can change them on the **Security** tab of the setup page.

## Required Libraries

To compile the program you'll need to install the following libraries in the Arduino IDE:
- Adafruit SSD1306

You'll require two modified libraries:

**1. ESP-Wifi-Config (fork with the 63-character WiFi password fix + user-extensible settings)**

The stock ESP-Wifi-Config truncates WiFi passwords to 30 characters, so this project uses a fork with the fix:
- Download [ESP-Wifi-Config v2.3.0](https://github.com/L0ria/ESP-Wifi-Config/archive/refs/tags/v2.3.0.zip) (or the release asset `ESP-Wifi-Config-2.3.0.zip`)
- In the Arduino IDE click Sketch->Include Library->Add .ZIP Library...
- Select the downloaded ESP-Wifi-Config-2.3.0.zip file

**2. LocalAI-ESP32 (modified OpenAI-ESP32 for LocalAI)**
- Download [LocalAI-ESP32 library](https://github.com/a-i-a-d/LocalAI-ESP32/archive/refs/tags/v0.0.1.zip)
- In the Arduino IDE got click Sketch->Include Library->Add .ZIP Library...
- Select the downloaded LocalAI-ESP32-0.0.1.zip file

## Compilation and Upload

- Connect your ESP32 board via USB
- Select the correct ESP32 board in the Arduino IDE
- Click the Compile and Upload button

## Usage

When you push the button, the LED lights up and the microphone will record 5 seconds of audio.
The audio recording is sent to your LocalAI whisper model and gets transcoded into a text.
The text then is sent as prompt to the LocalAI gpt4 model and the response is shown on the oled display.

## Debug output

The sketch can print a detailed serial trace of every step (boot, pin setup, LocalAI settings, OLED init, WiFi mode, I2S init, recording size, transcription/prompt/response lengths, button events) prefixed with `[DEBUG]`.

- Debug output is **off by default**.
- To enable it, uncomment `#define DEBUG` in `TamAIgotchi/config.h` (or pass `-DDEBUG` as an extra build flag) and recompile.
- When `DEBUG` is not defined, all `D_TD()` / `D_TDDEC()` / `D_TDLN()` calls compile away to nothing — no runtime cost.
- The regular user-facing status and error lines (shown on the OLED and mirrored to serial) are always printed, independent of the debug switch.
