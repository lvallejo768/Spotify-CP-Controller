"""Spotify desktop AppleScript backend for macOS."""
import subprocess
import psutil


def running():
    for proc in psutil.process_iter(['name']):
        try:
            if (proc.info['name'] or '').lower() == 'spotify':
                return True
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue
    return False


class SpotifyBackend:
    def _script(self, command):
        if not running():
            return None
        # Application targeting can reopen Spotify during a quit race.
        # The process guard reduces but does not eliminate that possibility.
        script = ('tell application "System Events"\n'
                  'if not (exists process "Spotify") then return "SPOTIFY_CLOSED"\n'
                  'end tell\n'
                  'tell application "Spotify"\n' + command + '\nend tell')
        result = subprocess.run(['osascript', '-e', script], text=True,
                                capture_output=True, timeout=4)
        if result.returncode:
            raise RuntimeError(result.stderr.strip() or 'osascript failed')
        output = result.stdout.strip()
        return None if output == 'SPOTIFY_CLOSED' else output

    def title(self):
        return self._script('if player state is stopped then\n'
                            'return "NO SONG"\nelse\n'
                            'return name of current track\nend if')

    def command(self, cmd):
        if cmd == 'P':
            return self._script('playpause')
        if cmd == 'S':
            return self._script('next track')
