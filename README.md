# MSP430FR6989 Spotify controller — cross-platform Python

Preserves existing CCS firmware; no firmware changes for this update.

- MSP430 -> PC: `P` play/pause, `S` next, `Z` manual LPM3, `W` manual wake.
- PC -> MSP430: `A` Spotify opened, `X` Spotify closed, `T:TITLE\n` display title.
- Automatic LPM0 and manual LPM3 remain firmware-controlled.
- Mac and Windows backends are separate. No Windows-only Python imports are loaded on a Mac.
- Playback and title queries have separate workers to reduce button-to-action delay; timing is not guaranteed.

## macOS

Stop the existing LaunchAgent/controller before testing to avoid two processes opening the same UART port.

```bash
cd ~/msp430_crossplatform/python
python3 -m pip install -r requirements-macos.txt
python3 spotify_controller.py
```

If you want to keep the existing macOS `spotify_launcher.py`, place `spotify_controller.py`, `spotify_mac.py`, and `backends/` together in `~/spotify_mac/`. The existing launcher can run `spotify_mac.py` unchanged. Keep your old files backed up.

## Windows

Use Windows 10/11, Python 3.11 or 3.12, and the Spotify desktop app.

```powershell
py -3.11 -m pip install -r requirements-windows.txt
py -3.11 spotify_controller.py
```

If the application UART COM port is ambiguous, find it in Device Manager and override it:

```powershell
$env:MSP430_PORT="COM5"
py -3.11 spotify_controller.py
```

Windows uses Global System Media Transport Controls. This backend **has not yet been verified on a Windows system**, and Spotify must expose a supported media session.

## Limitations

- The Mac backend still uses Spotify-targeted AppleScript, which carries a small app-quit/relaunch race.
- During manual LPM3, only an MCU button press can wake the device.
- USB UART selection fallback ending in `03` is based on one tested Mac setup, not a universal guarantee.
- A quick press is handled when the firmware emits its command (presently on button release). The Python changes reduce metadata-related delays but do not eliminate firmware debounce or OS playback latency.
