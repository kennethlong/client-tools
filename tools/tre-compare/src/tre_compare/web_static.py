"""SPA static-mount helper for tre-compare (Phase-30 Plan 01 / D-02 — the ONLY backend
addition for the whole frontend phase).

``create_app`` mounts a :class:`SpaStaticFiles` at ``/`` (registered LAST, after the four
API routes — the ``/`` mount is greedy, so order matters; T-30-01-03) to serve the built
Vite bundle from ``web/build/`` on the SAME single 127.0.0.1 process/port (D-02). No second
listener, no bind widening — the serve is additive inside the existing app.

``SpaStaticFiles`` subclasses Starlette's ``StaticFiles`` and adds a 404->index.html
fallback so any non-asset path renders the SPA instead of a raw 404. The fallback ONLY ever
re-serves ``index.html`` from the mounted directory — request paths are never joined to the
filesystem manually, so Starlette's path confinement (T-30-01-01) is preserved.

``WEB_DIR`` mirrors ``config.DEFAULT_INSTALLS_PATH``'s resolution shape: a 3-level ``.parent``
walk from this file (``src/tre_compare/web_static.py`` -> tre_compare -> src -> tre-compare)
to reach ``tools/tre-compare/web/build``.
"""

from __future__ import annotations

from pathlib import Path

from starlette.exceptions import HTTPException
from starlette.responses import Response
from starlette.staticfiles import StaticFiles

# tools/tre-compare/web/build  (web_static.py is at tools/tre-compare/src/tre_compare/) —
# same 3-level .parent walk config.py uses to reach the tool root.
WEB_DIR = Path(__file__).resolve().parent.parent.parent / "web" / "build"


class SpaStaticFiles(StaticFiles):
    """``StaticFiles`` that falls back to ``index.html`` on a 404.

    A bare ``StaticFiles(html=True)`` serves ``index.html`` for DIRECTORY requests but
    returns a plain 404 for any unknown path — it does NOT fall back to ``index.html`` for
    arbitrary routes. This subclass adds that fallback so a deep-link / accidental path
    still renders the single-page app. Any non-404 error is re-raised unchanged.
    """

    async def get_response(self, path: str, scope) -> Response:
        try:
            return await super().get_response(path, scope)
        except HTTPException as exc:
            if exc.status_code == 404:
                # Re-serve the SPA shell from the SAME mounted dir (no manual path join).
                return await super().get_response("index.html", scope)
            raise
