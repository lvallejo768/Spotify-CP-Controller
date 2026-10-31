"""Compatibility entry point for your existing macOS LaunchAgent."""
from spotify_controller import main

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('Stopped', flush=True)
