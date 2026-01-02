# ESP32 WebSocket Audio Client

This firmware turns your ESP32 device into a WebSocket audio client for Elato, enabling real-time conversations with AI characters.

## Hardware Setup

<img src="../assets/pcb-design.png" alt="Hardware Setup" width="100%">

### Components Needed
- XIAO ESP32S3 Sense (onboard PDM microphone)
- I2S speaker with amplifier (MAX98357A recommended)
- Microspeaker
- Button/Touch sensor and RGB LED (optional but recommended)

### Pin Connections

| **Component** | **Standard ESP32** |
|---------------|-------------------|
| **Microphone (onboard PDM)** |                   |
| DATA                 | GPIO 41          |
| CLK                  | GPIO 42          |
| **Speaker**   |                  |                   |
| WS                    | GPIO 5            |
| BCK             | GPIO 6            |
| DATA             | GPIO 7            |
| SD (shutdown)         | GPIO 10           |
| **Control**             |                   |
| Button               | GPIO 2            |
| LED (Blue)              | GPIO 13           |
| LED (Red)           | GPIO 9            |
| LED (Green)           | GPIO 8            |

Note: If you are using an external I2S microphone instead of the XIAO ESP32S3 Sense onboard mic, update the mic pins and `MIC_INPUT_IS_PDM` in `src/Config.cpp`.

## Software Setup

### Using PlatformIO

1. Install Visual Studio Code and the PlatformIO extension
2. Clone this repository
3. Open the project folder in PlatformIO. `Open > Open Project > firmware-arduino`
4. Edit `src/Config.cpp` with your server details:
   - If using locally: Set your computer's IP address in `ws_server` and `backend_server`
   - If using production: Ensure proper certificates are set
5. Build and upload to your ESP32


### To use locally:
1. **Find your local IP address**:
   - View your Wifi IP when you click on Wifi Settings > Your Wifi Network > Details, OR 
   - On macOS/Linux: Open Terminal and run `ifconfig`
   - On Windows: Open Command Prompt and run `ipconfig`
   - Look for your active network interface (WiFi: `en0` on Mac, `wlan0` on Linux, `Wireless LAN adapter Wi-Fi` on Windows)
   - Note the IP address (e.g., `192.168.1.100`)

2. **Update firmware configuration**:
   - In the firmware project, set `DEV_MODE` in Config.cpp
   - Update the WebSocket server IP to your local IP address

## NVS Storage

We store the following data in Non-Volatile Storage (NVS) on the ESP32:
1. **Auth token**: The supabase auth token that is used to authenticate the device with the backend server.
2. **Factory reset**: Whether the device has been factory reset.
3. **Wifi credentials**: The wifi credentials of the device.

## First-Time Setup

1. Power on your ESP32
2. Connect to the "Elato device" WiFi network from your phone/computer
3. A configuration portal will open (or navigate to 192.168.4.1)
4. Enter your home WiFi credentials
5. The device will restart and connect to your WiFi

## Usage

1. Power on the device
2. The LED indicates status:
   - Green 🟢: Setup mode and websocket/wifi is not connected
   - Blue 🔵: Device is speaking
   - Yellow 🟡: Device is listening to user
   - Red 🔴: Processing user request 
   - Cyan 🩵: OTA in progress
   - Magenta 🩷: Soft AP mode

## Troubleshooting

- If connection fails, check your WiFi signal and server details
- Monitor serial output at 115200 baud for detailed logs

## Deploying and Advanced Config

1. Add your Deno and Vercel server Root CA to `config.cpp`. You can find the Root CA of vercel by running the following command and picking the Root certificate in the chain:

```bash
# vercel server root ca
openssl s_client -showcerts -connect <your-vercel-deomain>.vercel.app:443 </dev/null

# deno server root ca
openssl s_client -showcerts -connect <your-deno-domain>.deno.dev:443 </dev/null
```

2. Edit `Config.cpp` to customize:
- Server addresses and ports
- Audio sample rate (default 24kHz)
- Pin assignments for different boards

3. For development, uncomment `#define DEV_MODE` in Config.h to use local servers without SSL. To deploy to production, comment out `#define DEV_MODE` and set the proper server addresses and ports.

4. To use the button functions, uncomment `#define TOUCH_MODE` in Config.h
