# Cape-spike + scene-difference evidence (2026-06-02)

Two screenshots of the SAME character ("Little Bigman", Mos Eisley cantina), release client, captured back-to-back by flipping `client.cfg` `rasterMajor` (11 = D3D11/gl11, 5 = D3D9/gl05). Kept because they show two distinct D3D11-vs-D3D9 deltas cleanly in one A/B pair.

## `d3d11-cape-spikes_screenShot0235.jpg` (rasterMajor=11, gl11)
- **Cape/robe tearing (the bug under investigation):** sharp dark triangular spikes erupt from the character's neck/shoulder/upper-back — a few skinned-mesh vertices flung to garbage positions. **D3D11-only.** "Capture-invisible (live-only)" per handoff — RenderDoc replay shows it clean.
- **Lighting/gamma delta:** whole scene washed-out / flat-grey, low contrast (the known GAMMA-01 / Phase-19 brightness delta).

## `d3d9-clean-reference_screenShot0236.jpg` (rasterMajor=5, gl05)
- **Robe renders perfectly — no spikes.** Confirms the cape tearing is a **D3D11 skinning regression**, not a pre-existing engine quirk.
- **Lighting is correct:** warm cantina tones, proper contrast, crisp textures — the reference look D3D11 should match.

## Use
- Cape-spike fix: the D3D9 shot is the parity target (clean robe). Diff the D3D11 skinned-vertex path.
- GAMMA-01 / brightness work: this pair is a ready-made before/after — D3D11 washed-out vs D3D9 warm, same scene/character.

Reference memory: [[project_d3d11_cape_spike_skinning_regression]].
