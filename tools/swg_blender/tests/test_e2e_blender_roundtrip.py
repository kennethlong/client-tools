"""Pytest wrapper for headless Blender import/export round-trip."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from glob import glob
from pathlib import Path

import pytest

from swg_scene.mesh_skeletal import load_skeletal_mesh
from swg_pipeline.swg_main_paths import appearance_skeleton

from tests.test_golden import _resolve_golden_msh
from tests.test_skeletal import _resolve_mgn

ROOT = Path(__file__).resolve().parents[1]
E2E_SCRIPT = ROOT / "swg_blender" / "e2e_blender_roundtrip.py"


def _resolve_skt_for_mgn(mgn_path: Path) -> Path:
    scene = load_skeletal_mesh(mgn_path)
    if not scene.meshes or not scene.meshes[0].skeleton_template_names:
        pytest.skip("golden mgn has no skeleton_template_names")
    skt_name = Path(scene.meshes[0].skeleton_template_names[0]).name
    golden = Path(__file__).parent / "golden" / skt_name
    if golden.is_file():
        return golden
    live = appearance_skeleton(skt_name)
    if live and live.is_file():
        return live
    pytest.skip(f"No skeleton {skt_name} for mgn round-trip")


def _find_blender_exe() -> str | None:
    env = os.environ.get("BLENDER_EXE")
    if env:
        p = Path(env)
        if p.is_file():
            return str(p)
    found = shutil.which("blender") or shutil.which("blender.exe")
    if found:
        return found
    for pattern in (
        r"C:\Program Files\Blender Foundation\Blender *\blender.exe",
        r"C:\Program Files\Blender Foundation\Blender\blender.exe",
    ):
        matches = sorted(glob(pattern), reverse=True)
        if matches:
            return matches[0]
    return None


def _run_blender_roundtrip(
    kind: str,
    source: Path,
    output: Path,
    *,
    skt: Path | None = None,
) -> dict:
    blender = _find_blender_exe()
    if not blender:
        pytest.skip("Blender not found: set BLENDER_EXE or install blender on PATH")

    cmd = [
        blender,
        "-b",
        "--python",
        str(E2E_SCRIPT),
        "--",
        kind,
        str(source),
        str(output),
    ]
    if skt is not None:
        cmd.extend(["--skt", str(skt)])

    proc = subprocess.run(
        cmd,
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        timeout=300,
    )
    if proc.returncode != 0 and not proc.stdout.strip():
        pytest.fail(
            f"Blender exited {proc.returncode}\nstderr:\n{proc.stderr[-4000:]}"
        )

    lines = [ln.strip() for ln in proc.stdout.splitlines() if ln.strip()]
    if not lines:
        pytest.fail(f"No JSON on stdout\nstderr:\n{proc.stderr[-4000:]}")
    json_line = None
    for line in reversed(lines):
        if line.startswith("{"):
            json_line = line
            break
    if json_line is None:
        pytest.fail(
            f"No JSON line on stdout\nstdout tail:\n{proc.stdout[-2000:]}\n"
            f"stderr:\n{proc.stderr[-2000:]}"
        )
    try:
        payload = json.loads(json_line)
    except json.JSONDecodeError as exc:
        pytest.fail(
            f"Invalid JSON from Blender: {exc}\nstdout tail:\n{proc.stdout[-2000:]}\n"
            f"stderr:\n{proc.stderr[-2000:]}"
        )
    if proc.returncode != 0 and payload.get("ok"):
        payload["ok"] = False
    if not payload.get("ok"):
        err = "; ".join(payload.get("errors") or [payload.get("error", "unknown")])
        pytest.fail(f"E2E {kind} failed: {err}\nstderr:\n{proc.stderr[-2000:]}")
    return payload


@pytest.mark.e2e
def test_e2e_blender_static_roundtrip(tmp_path: Path) -> None:
    source = _resolve_golden_msh()
    out = tmp_path / "roundtrip.msh"
    payload = _run_blender_roundtrip("static", source, out)
    assert out.is_file()
    assert payload.get("kind") == "static"
    stats = payload.get("stats") or {}
    assert stats.get("vertices") == 139
    assert stats.get("triangles") == 201


@pytest.mark.e2e
def test_e2e_blender_skeletal_roundtrip(tmp_path: Path) -> None:
    source = _resolve_mgn()
    skt = _resolve_skt_for_mgn(source)
    out = tmp_path / "roundtrip.mgn"
    payload = _run_blender_roundtrip("skeletal", source, out, skt=skt)
    assert out.is_file()
    assert payload.get("kind") == "skeletal"
