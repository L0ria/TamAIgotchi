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

## LocalAI Server Requirements

TamAIgotchi talks to LocalAI through the OpenAI-compatible API. Two models are used, and the exact names are sent by the firmware:

| Purpose | Endpoint | Model name sent | Set by |
|---------|----------|-----------------|--------|
| Speech-to-text | `POST /v1/audio/transcriptions` | `whisper-1` | hardcoded in the LocalAI-ESP32 library |
| Chat completion | `POST /v1/chat/completions` | `gpt-4` | `TamAIgotchi.ino` (`chat.setModel("gpt-4")`) |

Both requests carry an `Authorization: Bearer <api_key>` header, so the key configured on the ESP32 (default `sk1234567890` in `config.h`, changeable on the setup page) must match the one LocalAI is started with. If LocalAI runs without `LOCALAI_API_KEY`, any value works.

### 1. `whisper-1` — speech-to-text

The transcription model name is hardcoded to `whisper-1` in the library, so you need a model registered under exactly that name. LocalAI serves whisper through the `whisper` (whisper.cpp) backend:

1. Install the backend:

   ```bash
   local-ai backends install whisper
   ```

2. Download a whisper.cpp weight file into the `models` folder (example: `whisper-base.en`):

   ```bash
   mkdir -p models
   wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin -O models/whisper-base.en
   ```

3. Register it as `whisper-1` (e.g. in `models/whisper-1.yaml`):

   ```yaml
   name: whisper-1
   backend: whisper
   parameters:
     model: whisper-base.en
   ```

### 2. `gpt-4` — chat completion

Any small LLM works — it just has to be **named** `gpt-4` (or aliased to that name). Example with Llama 3.2 1B:

1. Install the backend:

   ```bash
   local-ai backends install llama-cpp
   ```

2. Download a GGUF weight file into the `models` folder (example: `llama-3.2-1b`):

   ```bash
   wget https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_K_M.gguf -O models/llama-3.2-1b
   ```

3. Alias it to `gpt-4` (e.g. in `models/gpt-4.yaml`):

   ```yaml
   name: gpt-4
   backend: llama-cpp
   parameters:
     model: llama-3.2-1b
   ```

### Quick start

```bash
# backends
local-ai backends install whisper
local-ai backends install llama-cpp

# weights
mkdir -p models
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin -O models/whisper-base.en
wget https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_K_M.gguf -O models/llama-3.2-1b

# model configs (see the YAML files above)
#   models/whisper-1.yaml -> name: whisper-1, backend: whisper,   parameters.model: whisper-base.en
#   models/gpt-4.yaml     -> name: gpt-4,     backend: llama-cpp, parameters.model: llama-3.2-1b

# start LocalAI (optionally with an API key)
LOCALAI_API_KEY=sk1234567890 local-ai run
```

Then point the ESP32 at `http://<server-ip>:8080/v1/` on the setup page.

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
