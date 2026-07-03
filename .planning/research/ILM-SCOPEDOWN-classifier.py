"""ILM scope-down classifier (Plan A, 2026-07-03).

Classifies every stage/ilm_extract file that OVERLAPS retail into:
  KEEP  - space-scoped: required/desired for JTL space (the reason the layer exists)
  PARK  - ground-scoped Legends preference changes: retail must win; moved (reversibly)
          to stage/ilm_extract_parked/ preserving relpath
  JUDGE - ambiguous; listed for human review (defaults to PARK=retail-faithful,
          flagged prominently for the space verify)
ILM-exclusive files (no retail counterpart) are untouched -- additive content.

Outputs:
  .planning/research/ILM-SCOPEDOWN-manifest.csv   (every overlap: bucket, rule, rationale)
  .planning/research/ILM-SCOPEDOWN-SUMMARY.md     (counts per rule + judgment list)
Run with --apply to perform the parking moves (dry-run by default).
"""
import sys, csv, shutil, re
from pathlib import Path
sys.path.insert(0, r'D:/Code/swg-blender-plugin/swg_pipeline')
import tre_reader as tr

BASE = Path(r'D:/Code/SWGSource Client v3.0')
ILM_ROOT = Path(r'D:/Code/swg-client-v2/stage/ilm_extract')
PARK_ROOT = Path(r'D:/Code/swg-client-v2/stage/ilm_extract_parked')
OUT_DIR = Path(r'D:/Code/swg-client-v2/.planning/research')

# Ship / space-entity name tokens (JTL chassis + station + celestial-adjacent)
SHIP = ('xwing','ywing','awing','bwing','z95','arc170','tie_','jf1','jedi_fighter',
        'jedifighter','hutt_fighter','hutt_heavy','blacksun','slave1','vt49','yt1300',
        'yt2400','y8_int','ykl_int','decimator','dunelizard','firespray','striker',
        'grievous','corellian_corvette','star_destroyer','ss3000','pirate_asteroid',
        'planetoid','asteroid','transport_beacon','gunboat','scyk','kimogila','krayt',
        'belbullab','vulture','droid_fighter','naboofighter','tieoppressor',
        'tie_advanced','tie_bomber','tie_interceptor','tie_aggressor','cockpit')

# Ordered rules: (bucket, rule-name, predicate)
def mk_rules():
    def any_tok(s, toks): return any(t in s for t in toks)
    return [
        # --- KEEP: space-scoped ---
        ('KEEP','space-datatable',      lambda p: p.startswith('datatables/space/') or p == 'datatables/missiles.iff'),
        ('KEEP','ship-clientdata',      lambda p: p.startswith('clientdata/ship/')),
        ('KEEP','ship-objtemplate',     lambda p: p.startswith('object/tangible/ship/')),
        ('KEEP','ship-asset',           lambda p: any_tok(p, SHIP)),
        ('KEEP','space-combat-fx',      lambda p: any_tok(p, ('pt_bolt_','pt_hit_','pt_explosion_starfighter','pt_explosion_bomber','thruster','contrail','pt_swoosh_contrails','pt_plasma_fireburst','pt_explosion_finger'))),
        ('KEEP','space-combat-cef',     lambda p: p.startswith('clienteffect/cbt_') or p == 'clienteffect/ui_missile_aquiring.cef'),
        ('KEEP','space-blast-shader',   lambda p: p in ('shader/pt_blast_lines.sht','shader/pt_blast_ring_fill.sht','shader/pt_lightening.sht','shader/pt_lightening_2.sht')),
        # --- PARK: ground-scoped Legends preference changes ---
        ('PARK','interior-datatable',   lambda p: p == 'datatables/interior/interior.iff'),   # merged override carries ILM-only rows
        ('PARK','cutscene-datatable',   lambda p: p.startswith('datatables/cutscenes/')),
        ('PARK','terrain-def',          lambda p: p.startswith('terrain/')),
        ('PARK','pixel-program',        lambda p: p.startswith('pixel_program/')),            # HIGH RISK: shader-compile inputs
        ('PARK','default-shader',       lambda p: p in ('shader/defaultappearance.sht','shader/defaultshader.sht')),
        ('PARK','interior-shader',      lambda p: p.startswith('shader/intr_') or p in ('shader/rbl_windows_destroyed_as8.sht','shader/neutral_hangarwall_destroyed_ascd21.sht')),
        ('PARK','palette',              lambda p: p.endswith('.pal')),
        ('PARK','character-texture',    lambda p: any_tok(p, ('hum_f_','hum_m_','twk_','wke_','bth_b_','am_head','r2head','robe_s05'))),
        ('PARK','force-jedi-fx',        lambda p: any_tok(p, ('drain_force','force_knockdown','light_saber'))),
        ('PARK','ground-vehicle',       lambda p: 'flashspeeder' in p),
        ('PARK','intro-texture',        lambda p: p.startswith('texture/intro_')),
        ('PARK','starport-mesh',        lambda p: 'starport' in p),                            # JUDGE-adjacent; retail has them
        ('PARK','sky-planet-visual',    lambda p: any_tok(p, ('planet_','pln_','grad_sky_'))), # prior art: ILM planet_tatooine.pln was WRONG (override precedent)
        ('PARK','terrain-texture',      lambda p: p.startswith('texture/') and any_tok(p, ('dirt_','grss_','rock_','sand_','snow_','frst_','ptch_','stco_','cave_rock','poi_','tatt_','tat_sandlight','dant_','dath_','corellia_','corl_fountain','exarkun','neutral_detail'))),
        ('PARK','interior-texture',     lambda p: p.startswith('texture/intr_')),
        # --- late-stage disambiguation (order matters: spaceport BEFORE the space token) ---
        ('PARK','spaceport-interior',   lambda p: p.startswith('texture/spaceport_')),        # ground starport building re-lights
        ('PARK','shared-base-effect',   lambda p: p == 'effect/e_particle_emisadd.eft'),      # RESTORE-FIRST if space particles regress
        ('KEEP','space-named',          lambda p: any_tok(p, ('space','nebula','hyperspace','capship','ship'))),
        # --- JUDGE: shared/ambiguous ---
        ('JUDGE','shared-particle-fx',  lambda p: p.startswith('appearance/pt_') or p.startswith('shader/pt_') or p.startswith('texture/pt_')),
        ('JUDGE','shared-effect',       lambda p: p.startswith('effect/')),
    ]

def main(apply=False):
    # retail-effective map (last-wins over data_* then patch_* ascending)
    effective = {}
    for tre in sorted(BASE.glob('data_*.tre')) + sorted(BASE.glob('patch_*.tre')):
        try:
            for name in tr.list_tre(tre):
                effective[name] = tre.name
        except Exception:
            pass

    rules = mk_rules()
    rows, counts = [], {}
    parked = 0
    for p in sorted(ILM_ROOT.rglob('*')):
        if not p.is_file():
            continue
        rel = p.relative_to(ILM_ROOT).as_posix()
        if rel not in effective:
            continue  # ILM-exclusive: untouched
        bucket, rule = 'JUDGE', 'unmatched'
        for b, r, pred in rules:
            if pred(rel):
                bucket, rule = b, r
                break
        rows.append((rel, bucket, rule, effective[rel]))
        counts[(bucket, rule)] = counts.get((bucket, rule), 0) + 1
        if apply and bucket in ('PARK', 'JUDGE'):   # JUDGE defaults retail-faithful
            dest = PARK_ROOT / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(p), str(dest))
            parked += 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    with open(OUT_DIR/'ILM-SCOPEDOWN-manifest.csv', 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['file','bucket','rule','retail_source_tre'])
        w.writerows(rows)

    lines = [f"{'(applied)' if apply else '(dry-run)'} overlaps classified: {len(rows)}, parked: {parked}", '']
    for (b, r), n in sorted(counts.items()):
        lines.append(f"  {b:5s} {r:22s} {n}")
    lines.append('')
    lines.append('JUDGE files (parked by default, restore-first candidates if space regresses):')
    for rel, b, r, _ in rows:
        if b == 'JUDGE':
            lines.append(f"  {rel}  [{r}]")
    report = '\n'.join(lines)
    print(report)
    (OUT_DIR/'ILM-SCOPEDOWN-run-report.txt').write_text(report, encoding='utf-8')

if __name__ == '__main__':
    main(apply='--apply' in sys.argv)
