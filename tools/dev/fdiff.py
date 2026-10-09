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
import glob
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
COMPILERS = ("2.7.2-psx", "2.7.2-cdk", "2.8.1-psx")

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


BASE_TARGET = re.compile(r"^(j|jal)\s+(func_)?(0x)?([0-9A-Fa-f]{1,8})$")


def norm_base_targets(text, lo, hi, base):
    """Rebuild the base object's truncated branch targets as relative offsets.

    With no symbol table, objdiff renders `j`/`jal` operands from the raw 26-bit
    jump field, so an in-overlay target shows as e.g. `0xfa334` and a call into
    the main executable as `func_00043E3C`. Both are really 0x80000000-anchored
    addresses; restoring the anchor lets them compare against our side's resolved
    form instead of reading as a false difference on every branch.
    """
    m = BASE_TARGET.match(text)
    if not m:
        return text
    mnem, is_func, _, literal = m.groups()
    v = int(literal, 16)
    if v >= 0x80000000:
        return text
    v |= 0x80000000
    if lo <= v < hi:
        return f"{mnem} {v - base:#x}"
    return f"{mnem} func_{v:08X}" if is_func else f"{mnem} {v:#x}"


def report_objdiff(d, func, limit, context, norm=None):
    ours = side_instructions(d.get("left") or {}, func)
    orig = side_instructions(d.get("right") or {}, func)
    if not ours or not orig:
        print("objdiff returned no instructions for this symbol")
        return

    if norm:
        lo, hi, base = norm
        ours = [norm_addr(t, lo, hi, base) for t in ours]
        orig = [norm_addr(norm_base_targets(t, lo, hi, base), lo, hi, base)
                for t in orig]

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


def true_orig_size(mod, fn, orig_addr):
    """The original function's real length, taken from splat's own disassembly.

    Our function and the original need not be the same length. Slicing the
    pristine overlay by *our* size therefore reads silently past the end of the
    original function and reports the following function's instructions as
    unexplained differences -- and shrinks the alignment denominator. splat's
    nonmatching .s carries the authoritative extent, so use that when present.
    """
    hits = glob.glob(f"{BUILD}/src/EFFECT/{mod}/nonmatchings/**/{fn}.s",
                    recursive=True)
    if not hits:
        return None
    pat = re.compile(r"/\*\s*[0-9A-F]+\s+([0-9A-F]{8})\s+[0-9A-F]{8}\s*\*/")
    try:
        text = open(hits[0], errors="ignore").read()
    except OSError:
        return None
    # The .s also holds the function's .rodata jump table at a lower address,
    # so keep only the words that actually belong to the body.
    body = [int(m.group(1), 16) for m in pat.finditer(text)
            if int(m.group(1), 16) >= orig_addr]
    if not body:
        return None
    return max(body) + 4 - orig_addr


def frame_size(obj):
    """The function's stack frame, read from its prologue."""
    d = subprocess.run([OBJDUMP, "-d", obj], capture_output=True, text=True).stdout
    m = re.search(r"addiu\s+(?:\$)?sp,(?:\$)?sp,(-?\d+)", d)
    return abs(int(m.group(1))) if m else None


def compiler_sweep(mod, fn):
    """Build the same source under every available compiler and report which one
    reproduces the original most closely.

    EFFECT is a mixed toolchain: the category-wide 2.8.1-psx pin is correct for
    the files already decompiled, but some original overlays were built with the
    PSYQ CDK compiler instead. Rather than guessing per candidate, compile it
    under all three and let the frame size and alignment pick.
    """
    elf = f"{BUILD}/data/EFFECT/{mod}.elf"
    orig_addr = original_addr(fn)
    vram = overlay_vram(mod)
    hi = hi_of(mod)
    off = orig_addr - vram
    with open(f"{ORIG}/EFFECT/{mod}", "rb") as f:
        f.seek(off)
        orig_blob = f.read()

    # The object we care about is whichever .c in this module defines `fn`.
    mod_dir = f"src/EFFECT/{mod}"
    srcs = []
    for dp, _dirs, files in os.walk(mod_dir):
        for name in files:
            if not name.endswith(".c"):
                continue
            path = os.path.join(dp, name)
            if fn in open(path, errors="ignore").read():
                srcs.append(path)
    if not srcs:
        sys.exit(f"no source under {mod_dir} mentions {fn}")
    objs = [f"{BUILD}/{s}.o" for s in srcs]

    of = orig_frame(mod, fn)
    of = abs(of) if of else None
    print(f"{fn}  original frame {f'0x{of:x}' if of else '?'}")
    print(f"  {'compiler':<14} {'frame':>8}  {'vs orig':>8}  "
          f"{'aligned':>8}  verdict")

    rows = []
    for ver in COMPILERS:
        for stale in objs + [elf]:
            if os.path.exists(stale):
                os.unlink(stale)
        r = subprocess.run(["make", elf, f"CC1VER={ver}",
                           "DISKIMAGE=/SLUS-01040.bin"],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print(f"  {ver:<14} {'-':>8}  {'-':>8}  {'-':>8}  build failed")
            continue

        our_addr, _, size = locate(elf, fn)
        if not size:
            print(f"  {ver:<14} {'-':>8}  {'-':>8}  {'-':>8}  "
                  f"no size for {fn}")
            continue
        tsize = true_orig_size(mod, fn, orig_addr) or size
        ow = our_words_from_elf(elf, our_addr, size)
        rw = words(orig_blob[:tsize])
        frame = next((frame_size(o) for o in objs
                     if os.path.exists(o) and frame_size(o)), None)
        fs = f"0x{frame:x}" if frame else "?"
        delta = f"{frame - of:+d}" if (frame and of) else "?"

        if ow == rw:
            print(f"  {ver:<14} {fs:>8}  {delta:>8}  {'--':>8}  MATCH")
            rows.append((ver, frame, 100.0))
            continue

        with tempfile.TemporaryDirectory() as td:
            base = os.path.join(td, "base.o")
            make_base_object(orig_blob[:tsize], fn, base)
            d = run_objdiff(elf, base, fn)
        pct = None
        if d:
            ours = [norm_addr(t, vram, hi, orig_addr)
                   for t in side_instructions(d.get("left") or {}, fn)]
            origl = [norm_addr(norm_base_targets(t, vram, hi, orig_addr),
                              vram, hi, orig_addr)
                     for t in side_instructions(d.get("right") or {}, fn)]
            sm = difflib.SequenceMatcher(None, ours, origl, autojunk=False)
            pct = 100.0 * sum(b.size for b in sm.get_matching_blocks()) \
                / max(1, len(origl))
        print(f"  {ver:<14} {fs:>8}  {delta:>8}  "
              f"{f'{pct:.1f}%' if pct is not None else '?':>8}  "
              f"differs ({len(ow) - len(rw):+d}w)")
        rows.append((ver, frame, pct))

    match = [r for r in rows if r[2] == 100.0]
    if match:
        print(f"\n  -> {match[0][0]} reproduces the original bytes")
    else:
        ranked = sorted((r for r in rows if r[2] is not None),
                       key=lambda r: -r[2])
        if ranked:
            print(f"\n  -> closest: {ranked[0][0]} at {ranked[0][2]:.1f}% aligned")


def hi_of(mod):
    return overlay_vram(mod) + os.path.getsize(f"{ORIG}/EFFECT/{mod}")


def orig_frame(mod, fn):
    """The original function's frame, decoded from its own prologue word.

    addiu is opcode 9; both source and target must be $sp for this to be the
    frame allocation rather than some other arithmetic on the stack pointer.
    """
    addr = original_addr(fn)
    if addr is None:
        return None
    off = addr - overlay_vram(mod)
    with open(f"{ORIG}/EFFECT/{mod}", "rb") as f:
        f.seek(off)
        w = int.from_bytes(f.read(4), "little")
    if (w >> 26) != 0x09:
        return None
    if ((w >> 21) & 0x1F) != 29 or ((w >> 16) & 0x1F) != 29:
        return None
    imm = w & 0xFFFF
    return imm - 0x10000 if imm >= 0x8000 else imm


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("module")
    ap.add_argument("function")
    ap.add_argument("--raw", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--compilers", action="store_true",
                    help="build under every available compiler and report which "
                         "one reproduces the original")
    ap.add_argument("--no-norm", action="store_true",
                    help="show absolute addresses instead of relative offsets")
    ap.add_argument("-n", type=int, default=12, help="hunks to show")
    ap.add_argument("-c", dest="context", type=int, default=2,
                    help="context lines per hunk")
    a = ap.parse_args()

    if a.compilers:
        compiler_sweep(a.module, a.function)
        return

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
    tsize = true_orig_size(mod, fn, orig_addr) or size
    with open(orig_bin, "rb") as f:
        f.seek(off)
        orig_blob = f.read(tsize)

    rw = words(orig_blob)
    ow = our_words_from_elf(elf, our_addr, size)

    if ow == rw:
        print(f"{fn}  ours {our_addr:#x} / orig {orig_addr:#x}  "
              f"{len(rw)}w  MATCH")
        return

    common = min(len(ow), len(rw))
    delta = len(ow) - len(rw)
    print(f"{fn}  ours {our_addr:#x} / orig {orig_addr:#x}  "
          f"ours={len(ow)}w orig={len(rw)}w ({delta:+d}w)  DIFFERS  "
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
