"""Cross-platform MSP430FR6989 host controller. Python 3.9+ (macOS)."""
import os
import platform
import queue
import time
import unicodedata
from concurrent.futures import ThreadPoolExecutor

import psutil
import serial
from serial.tools import list_ports

VID = 0x2047
PID = 0x0013
BAUD = 9600
SONG_INTERVAL = 0.8
SPOTIFY_INTERVAL = 0.5
RECONNECT_INTERVAL = 2

SYSTEM = platform.system()
if SYSTEM == 'Darwin':
    from backends.macos import SpotifyBackend
elif SYSTEM == 'Windows':
    from backends.windows import SpotifyBackend
else:
    raise RuntimeError('Supported platforms: macOS and Windows')


def spotify_running():
    target = 'spotify.exe' if SYSTEM == 'Windows' else 'spotify'
    for process in psutil.process_iter(['name']):
        try:
            if (process.info['name'] or '').lower() == target:
                return True
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass
    return False


def find_uart_port():
    override = os.getenv('MSP430_PORT')
    if override:
        return override
    matches = [p for p in list_ports.comports() if p.vid == VID and p.pid == PID]
    explicit = [p for p in matches if ('application uart' in
                ((p.interface or '') + ' ' + (p.description or '')).lower()
                or 'backchannel uart' in
                ((p.interface or '') + ' ' + (p.description or '')).lower())]
    if len(explicit) == 1:
        return explicit[0].device
    if SYSTEM == 'Darwin':
        observed = [p for p in matches if p.device.startswith('/dev/cu.')
                    and p.device.endswith('03')]
        if len(observed) == 1:
            return observed[0].device
    if matches:
        print('Ambiguous UART; set MSP430_PORT. Candidates:',
              ', '.join(p.device for p in matches), flush=True)
    return None


def normalize(title):
    title = unicodedata.normalize('NFKD', title)
    title = title.encode('ascii', 'ignore').decode('ascii').upper()
    title = ' '.join(''.join(c if ('A' <= c <= 'Z' or '0' <= c <= '9')
                             else ' ' for c in title).split())
    return title[:63] or 'NO SONG'


def send(ser, value):
    ser.write(value.encode('ascii'))


def run_session(ser, backend, metadata_worker, media_worker):
    active = None
    manual_sleep = False
    last_title = None
    title_future = None
    next_spotify_check = 0
    next_title_check = 0
    commands = queue.Queue(maxsize=16)
    ser.reset_input_buffer()
    print('Connected:', ser.port, flush=True)

    def dispatch_command():
        # Executed on a separate worker: serial reads never wait on AppleScript.
        while True:
            command = commands.get()
            if command is None:
                return
            if not spotify_running():
                continue
            try:
                backend.command(command)
                print('Playback:', command, flush=True)
            except Exception as exc:
                print('Playback error:', exc, flush=True)

    worker = media_worker.submit(dispatch_command)
    try:
        while True:
            now = time.monotonic()
            if now >= next_spotify_check:
                next_spotify_check = now + SPOTIFY_INTERVAL
                new_active = spotify_running()
                if new_active != active:
                    active = new_active
                    last_title = None
                    title_future = None
                    if not manual_sleep:
                        send(ser, 'A' if active else 'X')
                    if active:
                        next_title_check = 0
                    print('Spotify', 'opened' if active else 'closed', flush=True)

            data = ser.read(1)  # 20 ms timeout
            if data:
                cmd = data.decode('ascii', errors='ignore')
                if cmd == 'Z':
                    manual_sleep = True
                    print('Manual LPM3', flush=True)
                elif cmd == 'W':
                    manual_sleep = False
                    active = spotify_running()
                    last_title = None
                    title_future = None
                    next_title_check = 0
                    send(ser, 'A' if active else 'X')
                    print('Woke from LPM3', flush=True)
                elif cmd in ('P', 'S') and active and not manual_sleep:
                    try:
                        commands.put_nowait(cmd)
                    except queue.Full:
                        print('Playback queue full; command dropped', flush=True)
                    next_title_check = 0

            now = time.monotonic()
            if active and not manual_sleep:
                if title_future is None and now >= next_title_check:
                    title_future = metadata_worker.submit(backend.title)
                    next_title_check = now + SONG_INTERVAL
                if title_future is not None and title_future.done():
                    try:
                        title = title_future.result()
                        if title is not None and spotify_running():
                            title = normalize(title)
                            if title != last_title:
                                send(ser, 'T:' + title + '\n')
                                last_title = title
                                print('LCD <-', title, flush=True)
                    except Exception as exc:
                        print('Metadata error:', exc, flush=True)
                    title_future = None
            elif title_future is not None and title_future.done():
                title_future = None
    finally:
        # An old serial session is ending; let the media thread exit.
        try:
            commands.put_nowait(None)
        except queue.Full:
            pass
        # Don't wait on the worker because UART recovery must be prompt.



def main():
    backend = SpotifyBackend()

    print("=" * 45, flush=True)
    print("Spotify-CP-Controller", flush=True)
    print("=" * 45, flush=True)

    print(f"Operating System: {SYSTEM}", flush=True)
    print(f"Spotify Backend: {backend.__class__.__name__}",
          flush=True)

    print("Starting MSP430 connection manager...",
          flush=True)

    waiting_logged = False

    with ThreadPoolExecutor(max_workers=1) as metadata_worker, \
         ThreadPoolExecutor(max_workers=2) as media_worker:

        while True:

            port = find_uart_port()

            if port is None:

                if not waiting_logged:
                    print(
                        "MSP430FR6989 not detected.",
                        flush=True
                    )
                    print(
                        f"Waiting for connection "
                        f"(retry every {RECONNECT_INTERVAL}s)...",
                        flush=True
                    )

                    waiting_logged = True

                time.sleep(RECONNECT_INTERVAL)
                continue

            waiting_logged = False

            try:

                print(
                    f"LaunchPad detected: {port}",
                    flush=True
                )

                with serial.Serial(
                    port,
                    BAUD,
                    timeout=0.02,
                    write_timeout=1
                ) as ser:

                    run_session(
                        ser,
                        backend,
                        metadata_worker,
                        media_worker
                    )

            except (
                serial.SerialException,
                OSError
            ) as exc:

                print(
                    f"UART connection lost: {exc}",
                    flush=True
                )

                print(
                    f"Reconnecting in {RECONNECT_INTERVAL}s...",
                    flush=True
                )

                time.sleep(RECONNECT_INTERVAL)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('Stopped', flush=True)
