"""Localhost-only uvicorn entrypoint for the tre-compare API (Phase-29 Plan 03).

``python -m tre_compare`` builds the app via :func:`tre_compare.api.create_app` (default
Cache + default installs.toml) and serves it on ``127.0.0.1`` ONLY. The bind address is the
single control on remote reachability (T-29-03-03): this tool opens request-controlled
filesystem paths, so it MUST NOT bind any all-interfaces address — a single-user localhost
diagnostic only.
"""

from __future__ import annotations

import uvicorn

from .api import create_app

# Localhost ONLY (the file-reading tool must not be network-reachable on all interfaces).
HOST = "127.0.0.1"
PORT = 8765


def main() -> None:
    app = create_app()
    uvicorn.run(app, host=HOST, port=PORT)


if __name__ == "__main__":
    main()
