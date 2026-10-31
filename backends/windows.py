"""Spotify Windows media-session backend. Imported only on Windows."""
import asyncio


class SpotifyBackend:
    def __init__(self):
        # Dynamic import avoids platform-specific editor errors on macOS.
        import importlib
        media = importlib.import_module('winrt.windows.media.control')
        self.manager_type = media.GlobalSystemMediaTransportControlsSessionManager

    async def _session(self):
        manager = await self.manager_type.request_async()
        for session in manager.get_sessions():
            if 'spotify' in session.source_app_user_model_id.lower():
                return session
        return None

    async def _title(self):
        session = await self._session()
        if session is None:
            return 'NO SONG'
        metadata = await session.try_get_media_properties_async()
        return metadata.title or 'NO SONG'

    async def _command(self, cmd):
        session = await self._session()
        if session is None:
            return False
        if cmd == 'P':
            return await session.try_toggle_play_pause_async()
        if cmd == 'S':
            return await session.try_skip_next_async()
        return False

    def title(self):
        return asyncio.run(self._title())

    def command(self, cmd):
        return asyncio.run(self._command(cmd))
