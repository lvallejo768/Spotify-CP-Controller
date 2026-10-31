# Spotify-CP-Controller

**Cross-Platform Spotify Media Controller using the Texas Instruments MSP430FR6989 LaunchPad, Embedded C, Python, and UART Communication.**

An embedded hardware–software project that allows users to control Spotify through physical buttons, view scrolling song titles on a six-character LCD, and take advantage of automatic and manual low-power modes.

The controller uses bidirectional UART communication between an MSP430FR6989 LaunchPad and a Python application running on macOS or Windows.

**Project Status**
- macOS: Fully tested with the MSP430FR6989 LaunchPad
- Windows: Beta implementation; hardware testing pending

---

## Features

### Physical Media Controls
- S1: Play/Pause Spotify playback
- S2: Skip to the next track
- Button debouncing to prevent unintended commands
- Responsive playback using independent command and metadata processing

### Live Spotify Song Display
- Automatically retrieves the currently playing song
- Displays song titles on the LaunchPad's built-in LCD
- Scrolls titles longer than six characters
- Automatically updates when the song changes
- Converts unsupported characters into LCD-compatible text

### Automatic Low-Power Mode (LPM0)
- Detects when Spotify closes
- Displays `OFF` for approximately two seconds
- Clears and disables the LCD
- Enters LPM0 while maintaining UART wakeup capability
- Automatically wakes when Spotify opens again
- Restores the current song title

### Manual Low-Power Mode (LPM3)
- Hold S1 + S2 simultaneously for approximately two seconds
- The LCD turns off and the MCU enters LPM3
- Press either S1 or S2 to wake the controller
- Restores UART communication and synchronizes with Spotify
- Prevents the wakeup button press from triggering playback

### Automatic USB Detection
- Detects compatible MSP430 USB serial interfaces
- Supports automatic reconnection
- Handles changes in macOS serial-port names
- Supports manually specifying a COM port on Windows

---

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| MSP430FR6989 LaunchPad | Main embedded controller |
| Built-in LCD_C | Six-character segmented display |
| S1 and S2 | Physical playback and sleep controls |
| USB cable | Power and UART communication |
| macOS or Windows PC | Executes the Python host application |

### MSP430 Pin Configuration

| Component | MSP430 Pin | Function |
|-----------|------------|----------|
| S1 | P1.1 | Play/Pause |
| S2 | P1.2 | Next Track |
| LED1 | P1.0 | Red LED feedback |
| LED2 | P9.7 | Green LED feedback |
| UART TX | P3.4 | eUSCI_A1 TXD |
| UART RX | P3.5 | eUSCI_A1 RXD |

UART operates at **9600 baud, 8N1**.

---

## Repository Structure

```text
Spotify-CP-Controller/
├── backends/
│   ├── __init__.py
│   ├── macos.py
│   └── windows.py
├── firmware/
│   └── main.c
├── spotify_controller.py
├── spotify_mac.py
├── requirements-macos.txt
├── requirements-windows.txt
└── README.md
```

### File Descriptions

| File | Purpose |
|------|---------|
| `spotify_controller.py` | Main cross-platform controller |
| `spotify_mac.py` | macOS compatibility entry point |
| `backends/macos.py` | AppleScript Spotify integration |
| `backends/windows.py` | Windows media-session integration |
| `requirements-macos.txt` | macOS Python dependencies |
| `requirements-windows.txt` | Windows Python dependencies |

### MSP430FR6989 Firmware

The embedded C firmware is available in [`firmware/main.c`](firmware/main.c).

Developed using Texas Instruments Code Composer Studio (CCS), the firmware implements:

- Physical playback controls using S1 and S2
- UART communication with the Python host application
- Six-character LCD song-title scrolling
- Button debouncing and GPIO interrupts
- Automatic LPM0 and manual LPM3 power management
- Sleep and wakeup event handling

The firmware is designed for the MSP-EXP430FR6989 LaunchPad. A complete importable CCS project configuration has not yet been published.

---

## Installation — macOS

### Requirements

- macOS
- Python 3.9 or newer
- Spotify Desktop
- MSP430FR6989 LaunchPad
- USB data cable
- Compatible firmware programmed onto the LaunchPad

### 1. Clone the Repository

```bash
git clone https://github.com/lvallejo768/Spotify-CP-Controller.git
cd Spotify-CP-Controller
```

### 2. Install Dependencies

```bash
python3 -m pip install -r requirements-macos.txt
```

### 3. Connect the LaunchPad

Connect the MSP430FR6989 LaunchPad to your Mac using USB.

The controller attempts to detect the compatible UART port automatically.

### 4. Run the Controller

```bash
python3 -u spotify_controller.py
```

Expected output:

```text
=============================================
Spotify-CP-Controller
=============================================
Operating System: Darwin
Spotify Backend: SpotifyBackend
Starting MSP430 connection manager...
LaunchPad detected: /dev/cu.usbmodem...
Connected: /dev/cu.usbmodem...
Spotify opened
LCD <- SONG TITLE
```

Your exact USB port name may differ.

### 5. Configure macOS Permissions

macOS may request permission to allow Python or Terminal to control Spotify.

Grant the requested Automation permission.

You can review permissions under:

**System Settings → Privacy & Security → Automation**

### 6. Test the Controls

- Press S1 to play or pause Spotify.
- Press S2 to skip a song.
- Verify the LCD displays the current song title.
- Close Spotify and check that automatic LPM0 activates.
- Reopen Spotify and verify automatic wakeup.

**Important:** Do not run multiple Python controller instances or serial terminals using the same LaunchPad UART.

---

## Installation — Windows (Beta)

### Requirements

- Windows 10 or Windows 11
- Python 3.11 or 3.12 recommended
- Spotify Desktop
- MSP430FR6989 LaunchPad
- Compatible firmware
- TI USB drivers, if required

### 1. Clone the Repository

Open PowerShell:

```powershell
git clone https://github.com/lvallejo768/Spotify-CP-Controller.git
cd Spotify-CP-Controller
```

### 2. Install Dependencies

```powershell
py -3.11 -m pip install -r requirements-windows.txt
```

### 3. Connect the LaunchPad

Connect the MSP430FR6989 using USB.

Windows should detect the LaunchPad's serial interface.

If necessary, identify the application UART COM port in Device Manager.

### 4. Run the Controller

```powershell
py -3.11 spotify_controller.py
```

If automatic serial-port detection fails, specify the COM port manually:

```powershell
$env:MSP430_PORT = "COM5"
py -3.11 spotify_controller.py
```

Replace `COM5` with the actual COM port assigned to the LaunchPad.

**Windows support is experimental and has not yet been validated on a physical Windows computer.** Media-session compatibility, USB detection, and sleep/wake behavior still require testing.

---

## UART Communication Protocol

Communication between Python and the MSP430FR6989 uses bidirectional UART.

**Configuration:** 9600 baud, 8 data bits, no parity, 1 stop bit.

### MSP430 → Computer

| Command | Description |
|---------|-------------|
| `P` | Play/Pause |
| `S` | Next Track |
| `Z` | Entering manual LPM3 |
| `W` | Woke from manual LPM3 |

### Computer → MSP430

| Command | Description |
|---------|-------------|
| `A` | Spotify opened / automatic wake |
| `X` | Spotify closed / display OFF |
| `T:SONG TITLE\n` | Update LCD song title |

Example:

```text
MSP430 -> Python: P
Python -> Spotify: Play/Pause

Spotify -> Python: BLINDING LIGHTS
Python -> MSP430: T:BLINDING LIGHTS\n
MSP430 LCD: Scrolling song title
```

Song titles are normalized to uppercase ASCII letters, digits, and spaces to accommodate the segmented LCD.

---

## Power Management

The controller implements two distinct low-power modes.

### Automatic LPM0

When Spotify closes:

1. Python detects that Spotify is no longer running.
2. Python sends the `X` command through UART.
3. The MSP430 displays `OFF`.
4. After approximately two seconds, the LCD clears and switches off.
5. The MSP430 enters LPM0.
6. The UART remains available for wakeup.

When Spotify reopens:

1. Python detects Spotify.
2. Python sends the `A` command.
3. The UART receive interrupt wakes the MSP430.
4. The LCD is reactivated.
5. Python retrieves and sends the current song title.

### Manual LPM3

When S1 and S2 are held simultaneously for approximately two seconds:

1. The MSP430 sends `Z` to Python.
2. UART reception is suspended.
3. The LCD and LEDs turn off.
4. The MCU enters LPM3.
5. A physical S1 or S2 button interrupt wakes the MCU.
6. UART communication resumes.
7. The MSP430 sends `W` to Python.
8. The controller restores the appropriate display state.

Unlike automatic LPM0, manual LPM3 requires a physical button press to wake.

**Note:** The onboard debugger and other LaunchPad circuitry still consume power. Actual current reduction has not yet been quantitatively measured.

---

## Performance and Responsiveness

Earlier versions of the project processed serial commands and Spotify metadata requests in the same execution loop.

This occasionally caused noticeable delays between pressing S1/S2 and Spotify executing the command.

The cross-platform implementation separates playback command processing from song-title retrieval using worker threads.

This reduces delays caused by blocking metadata requests and improves overall responsiveness.

In macOS hardware testing, S1 and S2 no longer exhibited noticeable playback delay.

No formal latency benchmarks have been recorded yet.

---

## Verification Status

| Feature | macOS | Windows |
|---------|-------|---------|
| Spotify detection | Tested | Pending |
| USB UART communication | Tested | Pending |
| S1 Play/Pause | Tested | Pending |
| S2 Next Track | Tested | Pending |
| Scrolling LCD titles | Tested | Pending |
| Automatic UART detection | Tested | Pending |
| Automatic LPM0 | Tested | Pending |
| Manual LPM3 | Tested | Pending |
| Improved playback responsiveness | Tested | Pending |

All listed macOS functions have been tested using the MSP430FR6989 LaunchPad.

Windows support remains in beta until hardware validation is completed.

---

## Project Motivation

Spotify-CP-Controller was developed as a personal embedded systems project to explore communication between a microcontroller and desktop software. The project combines embedded C programming, Python, serial communication, interrupt handling, LCD control, operating-system integration, and low-power design. Its primary engineering objectives are reliable hardware–software communication, responsive physical controls, cross-platform compatibility, and efficient power management.
