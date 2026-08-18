"""TRE-builder decisive experiment (CONSULT-76 synthesis section 4).

Hand-writes two minimal TREE/0005 archives + a conflicting loose file:
  treexp_a.tre  (WITH trailing MD5 block, priority 11 when mounted):
      - STORED override:  texture/ui_starwars_logo.dds  <- contents of ui_rebel_logo_red.dds
      - TOMBSTONE:        sound/music_main_title.snd    (length=0 -> engine 'deleted')
  treexp_b.tre  (WITHOUT MD5 block, priority 10):
      - COMPRESSED override: texture/ui_logo_soe.dds    <- contents of ui_imperial_logo_bluegray.dds
  loose/texture/ui_starwars_logo.dds = ORIGINAL stock bytes (mounted searchPath priority 9)

One boot then reads out:
  logo = rebel insignia  -> stored path OK AND numeric priority governs across node types
  logo = normal          -> (if music silent) loose ALWAYS beats trees regardless of priority
  soe logo = imperial    -> compressed path OK AND the MD5 block is optional
  login music silent     -> zero-length entry is a real tombstone at runtime
  boot FATAL 'TreeFile corruption' -> layout wrong somewhere (per-field bisect from there)

Format per engine source (authoritative): TreeFile_SearchNode.cpp:440-577 (mount+lookup),
Crc.cpp:66-76 (CRC), TreeFileBuilder.cpp (write order); cross-checked against the
CONSULT-76 Codex spec + Cursor byte maps.
"""
import io, os, sys, struct, zlib, hashlib

sys.path.insert(0, r'D:\Code\swg-blender-plugin\swg_pipeline')
import tre_reader

STOCK = r'D:\Code\SWGSource Client v3.0'
OUT = r'D:\Code\Galaxies-Reborn\stage-B-x64'
LOOSE = os.path.join(OUT, 'treexp_loose')

# ---- engine CRC (Crc.cpp: MSB-first table CRC, init 0xFFFFFFFF, final xor init) ----
def make_table():
    poly = 0x04C11DB7
    t = []
    for i in range(256):
        c = i << 24
        for _ in range(8):
            c = ((c << 1) ^ poly) & 0xFFFFFFFF if (c & 0x80000000) else ((c << 1) & 0xFFFFFFFF)
        t.append(c)
    return t
CRCTABLE = make_table()

def engine_crc(name):
    crc = 0xFFFFFFFF
    for ch in name.encode('latin1'):
        crc = (CRCTABLE[((crc >> 24) ^ ch) & 0xFF] ^ ((crc << 8) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return crc ^ 0xFFFFFFFF

# sanity: verify against a real stock archive's own TOC
def verify_crc():
    p = os.path.join(STOCK, 'patch_08.tre')
    for e in tre_reader.read_tre_entries(p)[:25]:
        want = e.crc
        got = engine_crc(e.path.replace(chr(92), '/').lower())
        if want != got:
            # try backslash form
            got_bs = engine_crc(e.path.replace('/', chr(92)).lower())
            raise SystemExit(f'CRC mismatch for {e.path}: toc={want:08x} fwd={got:08x} bs={got_bs:08x}')
    print('CRC algorithm verified against stock patch_08.tre TOC (25 entries)')

# ---- stock payload fetch (winning copy via the same priority walk as the coverage scan) ----
def fetch(logical):
    # search the high-priority patch tres first, then TOCs
    tres = ['swgsource_3.0.tre','patch_17_00.tre','patch_16_00.tre','patch_15_02.tre',
            'patch_14_00.tre','patch_13_00.tre','patch_12_00.tre','patch_11_03.tre',
            'patch_11_02.tre','patch_11_01.tre','patch_11_00.tre','patch_10.tre',
            'patch_09.tre','patch_08.tre','patch_07.tre','patch_06.tre','patch_05.tre',
            'patch_04.tre','patch_03.tre','patch_02.tre','patch_01.tre','patch_00.tre',
            'gu8.tre','default_patch.tre','data_texture_00.tre','data_texture_01.tre',
            'data_texture_02.tre','data_texture_03.tre','data_texture_04.tre',
            'data_texture_05.tre','data_texture_06.tre','data_texture_07.tre',
            'data_other_00.tre','data_sample_00.tre','data_music_00.tre','bottom.tre']
    needle = logical.lower()
    for t in tres:
        fp = os.path.join(STOCK, t)
        if not os.path.isfile(fp):
            continue
        for e in tre_reader.read_tre_entries(fp):
            if e.path.replace(chr(92), '/').lower() == needle:
                return tre_reader.read_tre_payload(fp, e.path), t
    # fall back to the sku TOCs
    for toc in ('sku0_client.toc','sku1_client.toc','sku2_client.toc','sku3_client.toc'):
        tp = os.path.join(STOCK, toc)
        for e in tre_reader.read_search_toc_entries(tp):
            if e.path.replace(chr(92), '/').lower() == needle:
                with open(os.path.join(STOCK, e.tre_file), 'rb') as f:
                    f.seek(e.offset)
                    raw = f.read(e.compressed_length if e.compressor else e.length)
                return (zlib.decompress(raw) if e.compressor else raw), toc + ':' + e.tre_file
    raise SystemExit('not found in stock: ' + logical)

# ---- archive writer (TREE/0005, retail 24-byte TOC, strict form) ----
def write_tre(path, entries, with_md5):
    """entries: list of (name, payload_bytes_or_None_for_tombstone, compress_bool)"""
    HEADER = 36
    # payload region (starts right after the header, builder-style)
    blobs = []
    offset = HEADER
    rows = []   # (crc, name, length, offset, compressor, compressed_length)
    for name, data, compress in entries:
        name = name.replace(chr(92), '/').lower()
        crc = engine_crc(name)
        if data is None:                      # tombstone
            rows.append((crc, name, 0, offset, 0, 0))
            continue
        if compress:
            cdata = zlib.compress(data, 9)
            blobs.append(cdata)
            rows.append((crc, name, len(data), offset, 2, len(cdata)))
            offset += len(cdata)
        else:                                 # stored
            blobs.append(data)
            rows.append((crc, name, len(data), offset, 0, 0))
            offset += len(data)

    # strict sort: CRC, then case-insensitive name (ASCII names -> plain lower compare)
    rows.sort(key=lambda r: (r[0], r[1]))

    # name block (uncompressed): NUL-terminated names in TOC order
    name_block = b''
    fnoffs = {}
    for crc, name, *_ in rows:
        fnoffs[name] = len(name_block)
        name_block += name.encode('latin1') + b'\x00'

    toc = b''
    md5s = b''
    payload_by_name = {n.replace(chr(92), '/').lower(): (d if d is not None else b'') for n, d, _ in entries}
    for crc, name, length, off, comp, clen in rows:
        toc += struct.pack('<Iiiiii', crc, length, off, comp, clen, fnoffs[name])
        md5s += hashlib.md5(payload_by_name[name]).digest()

    toc_offset = offset
    header = struct.pack('<4s4s7I', b'EERT', b'5000',   # 'TREE'/'0005' as big-endian tags written LE (disk bytes verified vs patch_08.tre)
                         len(rows), toc_offset,
                         0, len(toc),          # toc uncompressed
                         0, len(name_block),   # name block uncompressed
                         len(name_block))      # uncomp size == raw size (engine reads this on the raw path)
    with open(path, 'wb') as f:
        f.write(header)
        for b in blobs:
            f.write(b)
        f.write(toc)
        f.write(name_block)
        if with_md5:
            f.write(md5s)
    print(f'wrote {path}: {len(rows)} entries, md5_block={with_md5}, {os.path.getsize(path)} bytes')

def main():
    verify_crc()

    CORPUS = r'D:/Code/Galaxies-Reborn/_client_dx11'
    ui_hlsl = open(CORPUS + '/pixel_program/ui.psh', 'rb').read()                 # corpus HLSL PSHP IFF
    amod_hlsl = open(CORPUS + '/pixel_program/a_modulate2x.psh', 'rb').read()    # corpus HLSL PSHP IFF
    asm_inc = open(CORPUS + '/pixel_program/include/asm_constants.inc', 'rb').read()
    ui_asm, src = fetch('pixel_program/ui.psh')                                  # stock ASM copy for the loose conflict
    print(f'payloads: ui_hlsl {len(ui_hlsl)}B, amod_hlsl {len(amod_hlsl)}B, inc {len(asm_inc)}B, stock asm ui {len(ui_asm)}B<-{src}')

    write_tre(os.path.join(OUT, 'treexp_a.tre'), [
        ('pixel_program/ui.psh', ui_hlsl, False),                 # STORED override (log: substitution line vanishes)
        ('pixel_program/include/asm_constants.inc', asm_inc, False),  # include served VIA OUR TRE (read-path proof #3)
        ('sound/music_main_title.snd', None, False),              # TOMBSTONE (re-confirm)
    ], with_md5=True)

    write_tre(os.path.join(OUT, 'treexp_b.tre'), [
        ('pixel_program/a_modulate2x.psh', amod_hlsl, True),      # COMPRESSED override
    ], with_md5=False)

    # loose conflict at priority 9: the STOCK ASM ui.psh -- if loose wins, the
    # substitution warning for ui.psh comes BACK; if the priority-11 tre wins, it vanishes.
    ldir = os.path.join(LOOSE, 'pixel_program')
    os.makedirs(ldir, exist_ok=True)
    with open(os.path.join(ldir, 'ui.psh'), 'wb') as f:
        f.write(ui_asm)
    print('loose conflict written:', os.path.join(ldir, 'ui.psh'))

    for name in ('treexp_a.tre', 'treexp_b.tre'):
        p2 = os.path.join(OUT, name)
        for e in tre_reader.read_tre_entries(p2):
            data = tre_reader.read_tre_payload(p2, e.path) if e.length else b''
            print(f'  round-trip {name}: {e.path} crc={e.crc:08x} len={e.length} comp={e.compressor} ok={len(data)==e.length}')

main()
