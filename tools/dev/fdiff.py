#!/usr/bin/env python3
"""Normalized single-function diff for fast decompilation iteration.

Slices the original function's bytes out of the pristine overlay, wraps them into
a base object, and hands both to objdiff-cli so you get a register / stack /
branch-normalized diff instead of a raw hex comparison.

Address model
-------------
Our build lays functions out *sequentially*, so the moment our reconstruction is
longer or shorter than the original, every symbol after it shifts. The original
address is therefore taken from the function's own name (`func_800F9818` ->
0x800F9818), not from where it landed in our ELF. Our address comes from the ELF.
Original extent is the gap to the next original address.

Works for matched and unmatched functions alike: the pristine overlay under data/
is the source of truth, not the split asm (which disappears once a function
matches).

Usage:
    fdiff.py <MODULE.BIN> <function> [options]

    --raw         skip objdiff, just print our words vs the original's
    --no-build    skip the incremental relink of the overlay
    -n N          how many diff hunks to show (default 12)

Examples:
    fdiff.py PLG085.BIN func_800F9818
    fdiff.py PLG049.BIN func_800FA76C --no-build
"""
import argparse
import difflib
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

BUILD = "build"
ORIG = "data"
OBJDIFF = "tools/objdiff/objdiff-cli"
OBJCOPY = "mipsel-linux-gnu-objcopy"
OBJDUMP = "mipsel-linux-gnu-objdump"
NM = "mipsel-linux-gnu-nm"

# func_800F9818 / D_800F1234 style names carry the *original* address.
NAME_ADDR = re.compile(r"^(?:func|D|d|lbl|lbl|lohi|hi_?|lo_?)_?([0-9A-Fa-f]{8})$")

_vram_cache = {}


def overlay_vram(module):
    """The code segment's vram, straight out of the module's splat.yaml."""
    if module not in _vram_cache:
        cfg = f"config/EFFECT/{module}/splat.yaml"
        vram, in_code = None, False
        for ln in open(cfg, errors="ignore"):
            if re.match(r"\s*type:\s*code\b", ln):
                in_code = True
            m = re.match(r"\s*vram:\s*(0x[0-9A-Fa-f]+)", ln)
            if m and in_code:
                vram = int(m.group(1), 16)
                break
        if vram is None:
            sys.exit(f"could not find code segment vram in {cfg}")
        _vram_cache[module] = vram
    return _vram_cache[module]


def nm_table(elf):
    """[(addr, size, name)] for defined symbols in our linked ELF.

    Tolerant of the three-column form (`addr type name`) that nm emits for
    symbols it cannot size.
    """
    out = subprocess.run([NM, "-S", "-n", "--defined-only", elf],
                        capture_output=True, text=True).stdout
    rows = []
    for ln in out.splitlines():
        m = re.match(r"([0-9a-f]+) ([0-9a-f]+) ([A-Za-z]) (\S+)", ln)
        if m:
            rows.append((int(m.group(1), 16), int(m.group(2), 16), m.group(4)))
            continue
        m = re.match(r"([0-9a-f]+) ([A-Za-z]) (\S+)", ln)
        if m:
            rows.append((int(m.group(1), 16), 0, m.group(3)))
    return rows


def original_addr(name):
    """The original address encoded in a splat-style name, or None."""
    m = NAME_ADDR.match(name)
    return int(m.group(1), 16) if m else None


def locate(elf, func):
    """(our_addr, orig_addr, our_size) for `func`."""
    rows = nm_table(elf)
    our_addr = our_size = None
    for addr, size, name in rows:
        if name == func:
            our_addr, our_size = addr, size
            break
    if our_addr is None:
        sys.exit(f"{func} not found in {elf}")

    orig_addr = original_addr(func)
    if orig_addr is None:
        # Renamed function: no address in the name, fall back to where we put it.
        return our_addr, our_addr, our_size

    if not our_size:
        # nm could not size it; fall back to the gap to the next original address.
        others = sorted(a for a, _s, n in rows
                       if (oa := original_addr(n)) is not None and oa > orig_addr
                       for a in [oa])
        our_size = (others[0] - orig_addr) if others else 0
    return our_addr, orig_addr, our_size


def elf_sections(elf):
    """[(addr, offset, size)] for allocatable sections, parsed from the ELF by hand.

    objdump cannot be trusted to enumerate every word here: it does not know the
    PSX's GTE opcodes and silently drops the instructions it cannot decode, which
    makes any word count it reports short.
    """
    with open(elf, "rb") as f:
        blob = f.read()
    assert blob[:4] == b"\x7fELF", f"{elf} is not an ELF"
    assert blob[5] == 1, f"{elf} is not 32-bit"
    end = "<" if blob[5] == 1 else ">"
    sh_off, = struct.unpack(f"{end}I", blob[0x20:0x24])
    sh_ent, sh_num, sh_strndx = struct.unpack(
        f"{end}HHH", blob[0x2E:0x34])
    secs = []
    for i in range(sh_num):
        base = sh_off + i * sh_ent
        sh_addr, sh_offset, sh_size = struct.unpack(
            f"{end}III", blob[base + 12:base + 24])
        if sh_size:
            secs.append((sh_addr, sh_offset, sh_size))
    return blob, secs


def elf_read(blob, sections, addr, size):
    for s_addr, s_off, s_size in sections:
        if s_addr <= addr and addr + size <= s_addr + s_size:
            return blob[s_off + (addr - s_addr): s_off + (addr - s_addr) + size]
    raise SystemExit(f"no section covers {addr:#x}+{size}")


def our_words_from_elf(elf, addr, size):
    """Our function's instruction words, straight out of the linked ELF.

    Read from the ELF rather than the built .BIN: `make` relinks the .elf without
    necessarily regenerating the raw .BIN, which silently hands you stale bytes.
    """
    blob, sections = elf_sections(elf)
    return words(elf_read(blob, sections, addr, size))


def words(blob):
    return [int.from_bytes(blob[i:i + 4], "little")
            for i in range(0, len(blob) - 3, 4)]


def make_base_object(blob, func, out_path):
    """Wrap raw instruction bytes into an ELF object with `func` at .text:0."""
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tf:
        tf.write(blob)
        raw = tf.name
    try:
        subprocess.run([
            OBJCOPY, "-I", "binary", "-O", "elf32-tradlittlemips",
            "--rename-section", ".data=.text,alloc,load,contents,code",
            "--add-symbol", f"{func}=.text:0x0,function,global",
            raw, out_path,
        ], check=True, capture_output=True)
    finally:
        os.unlink(raw)


def run_objdiff(elf, base, func):
    r = subprocess.run([OBJDIFF, "diff", "-1", elf, "-2", base, func,
                       "-o", "-", "--format", "json"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None
    try:
        return json.loads(r.stdout)
    except json.JSONDecodeError:
        return None


def ins_text(ins):
    for key in ("formatted", "instruction"):
        v = ins.get(key)
        if isinstance(v, str) and v:
            return v
        if isinstance(v, dict):
            f = v.get("formatted")
            if f:
                return f
    return None


def side_instructions(side, func):
    """Normalized instruction text for `func` out of an objdiff file blob."""
    for sym in side.get("symbols") or []:
        if sym.get("name") == func:
            return [i.get("instruction", {}).get("formatted", "")
                    for i in sym.get("instructions") or []]
    return []


def norm_addr(text, lo, hi, base):
    """Rewrite absolute overlay addresses as plain offsets from `base`.

    objdiff renders our side's branch targets as absolute addresses (our ELF has
    full symbol info) but the base object's as bare immediates. Collapsing ours to
    the same plain-offset form means an unchanged branch compares equal and only the
    differences we actually introduced survive.
    """
    def sub(m):
        v = int(m.group(0), 16)
        if lo <= v < hi:
            return f"{v - base:#x}"
        return m.group(0)

    return re.sub(r"0x8[0-9A-Fa-f]{7}", sub, text)


def report_objdiff(d, func, limit, context, norm=None):
    ours = side_instructions(d.get("left") or {}, func)
    orig = side_instructions(d.get("right") or {}, func)
    if not ours or not orig:
        print("objdiff returned no instructions for this symbol")
        return

    if norm:
        lo, hi, base = norm
        ours = [norm_addr(t, lo, hi, base) for t in ours]
        orig = [norm_addr(t, lo, hi, base) for t in orig]

    sm = difflib.SequenceMatcher(None, ours, orig, autojunk=False)
    same = sum(b.size for b in sm.get_matching_blocks())
    print(f"aligned: {same}/{max(len(ours), len(orig))} instructions match "
          f"({100.0 * same / max(1, len(orig)):.1f}%)")

    shown = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        shown += 1
        if shown > limit:
            print("  ... (use -n to see more)")
            return
        print(f"  --- ours[{i1}:{i2}] vs orig[{j1}:{j2}] ({tag}) ---")
        for label, arr, lo_, hi_ in (("ours", ours, i1, i2),
                                    ("orig", orig, j1, j2)):
            if hi_ - lo_ > 2 * context:
                rng = list(range(lo_, lo_ + context)) + \
                    list(range(hi_ - context, hi_))
            else:
                rng = list(range(lo_, hi_))
            for k in rng:
                print(f"    {label} {k:4}  {arr[k]}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("module")
    ap.add_argument("function")
    ap.add_argument("--raw", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-norm", action="store_true",
                    help="show absolute addresses instead of relative offsets")
    ap.add_argument("-n", type=int, default=12, help="hunks to show")
    ap.add_argument("-c", dest="context", type=int, default=2,
                    help="context lines per hunk")
    a = ap.parse_args()

    mod, fn = a.module, a.function
    elf = f"{BUILD}/data/EFFECT/{mod}.elf"
    orig_bin = f"{ORIG}/EFFECT/{mod}"

    if not a.no_build:
        r = subprocess.run(["make", elf, "DISKIMAGE=/SLUS-01040.bin"],
                          capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout)
            print(r.stderr, file=sys.stderr)
            sys.exit("build failed")

    our_addr, orig_addr, size = locate(elf, fn)
    if not size:
        sys.exit(f"{fn} has no size in {elf}")
    off = orig_addr - overlay_vram(mod)
    with open(orig_bin, "rb") as f:
        f.seek(off)
        orig_blob = f.read(size)

    rw = words(orig_blob)
    ow = our_words_from_elf(elf, our_addr, size)

    if ow == rw:
        print(f"{fn}  ours {our_addr:#x} / orig {orig_addr:#x}  "
              f"{len(rw)}w  MATCH")
        return

    common = min(len(ow), len(rw))
    print(f"{fn}  ours {our_addr:#x} / orig {orig_addr:#x}  "
          f"ours={len(ow)}w orig={len(rw)}w  DIFFERS  "
          f"{sum(x != y for x, y in zip(ow, rw))}/{common} words differ")

    if a.raw:
        for i in range(max(len(ow), len(rw))):
            x = f"{ow[i]:08X}" if i < len(ow) else "--"
            y = f"{rw[i]:08X}" if i < len(rw) else "--"
            if x != y:
                print(f"  [{i:4}] ours {x}   orig {y}")
        return

    with tempfile.TemporaryDirectory() as td:
        base = os.path.join(td, "base.o")
        make_base_object(orig_blob, fn, base)
        d = run_objdiff(elf, base, fn)
        if d is None:
            print("objdiff unavailable -- raw word diff only")
            subprocess.run([sys.executable, __file__, mod, fn, "--raw",
                           "--no-build"])
            return
        vram = overlay_vram(mod)
        hi = vram + os.path.getsize(orig_bin)
        report_objdiff(d, fn, a.n, a.context,
                      norm=None if a.no_norm else (vram, hi, orig_addr))


if __name__ == "__main__":
    main()
