#!/usr/bin/env python3
"""Show which symbols a firmware module needs in a unit test, and where each comes from.

Run this before writing a test for a module:

    python Tests/tools/needs_fakes.py <module> [--build-dir Tests/build]
                                     [--with <module> ...] [-D MACRO[=value] ...]

<module> is the firmware file's basename, e.g. bsm, adc_management,
adBms_Application. Configure the tests first (cmake -S Tests -B Tests/build) so
the build dir has fw_test_cc.txt, fw_test_flags.txt and fw_src_dirs.txt.

--with <module>  You will link this real module too (add_fw_test SOURCES). Its
                 definitions satisfy symbols, and its own needs are added.
-D MACRO         Extra define, as in add_fw_test(... DEFINES ...).

The tool compiles the module like the test build does, lists the symbols the
object file leaves undefined, and sorts each one into:

    HAL fake         in HAL_FAKE_LIST (fakes/hal_fakes.h), always linked
    fake_<x>         in a FAKE_*_LIST in fakes/fake_<x>.h: add `FAKES <x>`
    real firmware    defined in another firmware .c: add it to SOURCES (and
                     rerun with --with to see what it needs) or write a fake
    MISSING          nothing provides it: the tool prints the line to paste

and finally prints a suggested add_fw_test() line.

Exit codes: 0 nothing missing, 1 symbols missing, 2 tool or compile error.
Python 3 standard library only.
"""

from __future__ import annotations

import argparse
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent.parent
FAKES_DIR = TESTS_DIR / "fakes"
FW_DIR = TESTS_DIR.parent
DRIVERS_DIR = FW_DIR / "Drivers"

# C library, libm and compiler runtime: provided by the host toolchain.
RUNTIME_RE = re.compile(
    r"""^(
        __.*                                   # compiler/libc internals, sanitizers
      | _.*                                    # reserved names (e.g. _fltused, _assert)
      | .*\..*                                 # .refptr.* and other linker artefacts
      | mem(cpy|set|move|cmp|chr)
      | str[a-z]*
      | (s|sn|f|v|vs|vsn|vf)?printf | puts | putchar | fputs | fputc | fwrite | fflush
      | (s|f)?scanf
      | (c|m|re)alloc | free | abort | exit | atexit
      | ato[ifl] | strto[a-z]+ | abs | labs | qsort | bsearch | s?rand
      | (sqrt|exp|exp2|log|log2|log10|pow|fabs|floor|ceil|round|trunc|fmod
         |sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|fmin|fmax|hypot
         |lround|lrint|nan|isnan|isinf|copysign|ldexp|frexp|modf)[fl]?
      | pthread_.* | sched_yield | usleep | nanosleep
    )$""",
    re.X,
)

LIST_DEF_RE = re.compile(r"^#define\s+(\w+_LIST)\s*\(\s*VALUE\s*,\s*VOID\s*\)", re.M)


class ToolError(Exception):
    pass


# --------------------------------------------------------------------------- build info


def read_lines(path: Path) -> list[str]:
    if not path.exists():
        raise ToolError(
            f"{path} not found. Configure the tests first:\n"
            "    cmake -S Tests -B Tests/build\n"
            "(from High_Voltage/Tractive_Battery), or pass --build-dir."
        )
    return [ln.strip() for ln in path.read_text().splitlines() if ln.strip()]


def split_cc(line: str) -> list[str]:
    """fw_test_cc.txt holds '<compiler path> [args]'; the path may contain spaces."""
    if Path(line).exists():
        return [line]
    parts = line.split(" ")
    for i in range(len(parts), 0, -1):
        exe = " ".join(parts[:i])
        if Path(exe).exists() or shutil.which(exe):
            return [exe] + [p for p in parts[i:] if p]
    return line.split()


def find_source(module: str, src_dirs: list[Path]) -> Path:
    found = [d / f"{module}.c" for d in src_dirs if (d / f"{module}.c").exists()]
    if len(found) != 1:
        what = "no" if not found else "more than one"
        dirs = "\n    ".join(str(d) for d in (found or src_dirs))
        raise ToolError(f"{what} firmware source named {module}.c:\n    {dirs}")
    return found[0]


def compile_object(cc: list[str], flags: list[str], src: Path, out: Path) -> None:
    cmd = cc + flags + ["-w", "-c", str(src), "-o", str(out)]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode == 0:
        return
    output = proc.stdout + proc.stderr
    sys.stderr.write(output)
    msg = (
        f"compiling {src.name} failed (output above).\n"
        "This usually means a HAL type, register bit or macro is missing from\n"
        "Tests/fakes/stm32g4xx_hal.h (or a CubeMX pin name from Tests/fakes/main.h).\n"
        "Copy the definition from Drivers/ or Core/Inc/main.h and rerun."
    )
    # HAL functions are only declared through HAL_FAKE_LIST, so a missing HAL
    # function shows up here as an undeclared function rather than at link time.
    undeclared = re.findall(
        r"(?:undeclared function|implicit declaration of function) '(\w+)'", output
    )
    if undeclared:
        msg += "\n\nUndeclared functions: add a fake for each, then rerun.\n"
        include_dirs = [Path(f[2:]) for f in flags if f.startswith("-I")]
        for name in dict.fromkeys(undeclared):
            msg += "\n" + missing_function_advice(name, include_dirs)
    raise ToolError(msg)


# --------------------------------------------------------------------------- symbols


def object_format(data: bytes) -> str:
    if data[:4] == b"\x7fELF":
        return "elf"
    if data[:4] in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe"):
        return "macho"
    machine = struct.unpack_from("<H", data, 0)[0]
    if machine == 0x14C:
        return "coff-i386"
    if machine in (0x8664, 0xAA64, 0x1C4):
        return "coff"
    raise ToolError("unknown object file format")


def elf_symbols(data: bytes) -> tuple[set[str], set[str]]:
    is64 = data[4] == 2
    end = "<" if data[5] == 1 else ">"
    if is64:
        shoff = struct.unpack_from(end + "Q", data, 0x28)[0]
        shentsize, shnum = struct.unpack_from(end + "HH", data, 0x3A)
        sh_fmt, sym_fmt, sym_size = end + "IIQQQQIIQQ", end + "IBBHQQ", 24
    else:
        shoff = struct.unpack_from(end + "I", data, 0x20)[0]
        shentsize, shnum = struct.unpack_from(end + "HH", data, 0x2E)
        sh_fmt, sym_fmt, sym_size = end + "IIIIIIIIII", end + "IIIBBH", 16
    sections = [struct.unpack_from(sh_fmt, data, shoff + i * shentsize) for i in range(shnum)]
    undefined, defined = set(), set()
    for sec in sections:
        if sec[1] != 2:  # SHT_SYMTAB
            continue
        str_off = sections[sec[6]][4]
        for off in range(sec[4], sec[4] + sec[5], sym_size):
            fields = struct.unpack_from(sym_fmt, data, off)
            if is64:
                name_off, info, _, shndx = fields[0], fields[1], fields[2], fields[3]
            else:
                name_off, info, shndx = fields[0], fields[3], fields[5]
            if info >> 4 not in (1, 2):  # GLOBAL or WEAK
                continue
            name = data[str_off + name_off : data.index(b"\0", str_off + name_off)].decode()
            if name:
                (undefined if shndx == 0 else defined).add(name)
    return undefined, defined


def coff_symbols(data: bytes) -> tuple[set[str], set[str]]:
    if struct.unpack_from("<HH", data, 0) == (0, 0xFFFF):
        raise ToolError("bigobj COFF files are not supported; install nm or llvm-nm")
    symtab, nsyms = struct.unpack_from("<II", data, 8)
    strtab = symtab + nsyms * 18
    undefined, defined = set(), set()
    i = 0
    while i < nsyms:
        off = symtab + i * 18
        raw = data[off : off + 8]
        value, section, _, storage, naux = struct.unpack_from("<IhHBB", data, off + 8)
        if raw[:4] == b"\0\0\0\0":
            name_off = strtab + struct.unpack_from("<I", raw, 4)[0]
            name = data[name_off : data.index(b"\0", name_off)].decode()
        else:
            name = raw.rstrip(b"\0").decode()
        if storage == 2:  # IMAGE_SYM_CLASS_EXTERNAL
            # section 0 with a value is a common (tentative) definition
            (undefined if section == 0 and value == 0 else defined).add(name)
        i += 1 + naux
    return undefined, defined


def nm_symbols(nm: str, obj: Path) -> tuple[set[str], set[str]]:
    out = subprocess.run([nm, "-g", str(obj)], capture_output=True, text=True, check=True)
    undefined, defined = set(), set()
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[-2] in ("U", "w", "v"):
            if parts[-2] == "U":
                undefined.add(parts[-1])
        elif len(parts) >= 3:
            defined.add(parts[-1])
    return undefined, defined


def object_symbols(obj: Path) -> tuple[set[str], set[str]]:
    """(undefined, defined) external symbols, with any C-level '_' prefix removed."""
    data = obj.read_bytes()
    fmt = object_format(data)
    nm = shutil.which("nm") or shutil.which("llvm-nm")
    if nm:
        undefined, defined = nm_symbols(nm, obj)
    elif fmt == "elf":
        undefined, defined = elf_symbols(data)
    elif fmt.startswith("coff"):
        undefined, defined = coff_symbols(data)
    else:
        raise ToolError("no nm or llvm-nm on PATH to read this object file")
    if fmt in ("coff-i386", "macho"):  # these formats prefix C names with '_'

        def strip(names: set[str]) -> set[str]:
            return {n[1:] if n.startswith("_") else n for n in names}

        undefined, defined = strip(undefined), strip(defined)
    return undefined, defined


# --------------------------------------------------------------------------- source scans


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def split_top_level(args: str) -> list[str]:
    parts, depth, cur = [], 0, ""
    for ch in args:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return parts


def fake_lists() -> dict[str, tuple[str, str]]:
    """name -> (provider, kind). provider is 'HAL' or the fake module name."""
    provided: dict[str, tuple[str, str]] = {}
    headers = [FAKES_DIR / "hal_fakes.h"] + sorted(FAKES_DIR.glob("fake_*.h"))
    for header in headers:
        provider = "HAL" if header.name == "hal_fakes.h" else header.stem[len("fake_") :]
        text = header.read_text()
        for m in LIST_DEF_RE.finditer(text):
            body = []
            for line in text[m.end() :].splitlines():
                body.append(line.rstrip().rstrip("\\"))
                if not line.rstrip().endswith("\\"):
                    break
            body_text = strip_comments(" ".join(body))
            for entry in re.finditer(r"\b(VALUE|VOID)\s*\(", body_text):
                depth, j = 1, entry.end()
                while depth and j < len(body_text):
                    depth += {"(": 1, ")": -1}.get(body_text[j], 0)
                    j += 1
                args = split_top_level(body_text[entry.end() : j - 1])
                name = args[1] if entry.group(1) == "VALUE" else args[0]
                provided.setdefault(name, (provider, "function"))
    # Global variables that fake .c files define (FFF can't fake data)
    for src in [FAKES_DIR / "hal_fakes.c"] + sorted(FAKES_DIR.glob("fake_*.c")):
        provider = "HAL" if src.name == "hal_fakes.c" else src.stem[len("fake_") :]
        for name in defined_variables(src.read_text()):
            provided.setdefault(name, (provider, "variable"))
    return provided


FUNC_DEF_RE = re.compile(
    r"^(?!static\b|extern\b|typedef\b|return\b|else\b)[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)"
    r"\s*\([^;{}]*\)\s*\{",
    re.M,
)
VAR_DEF_RE = re.compile(
    r"^(?!static\b|extern\b|typedef\b|return\b|#)(?:(?:const|volatile)\s+)*[A-Za-z_]\w*"
    r"(?:\s+(?:const|volatile))*[\s*]+([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*(?:=[^;]*)?;",
    re.M,
)


def defined_variables(text: str) -> set[str]:
    return set(VAR_DEF_RE.findall(strip_comments(text)))


def firmware_definitions(src_dirs: list[Path]) -> dict[str, Path]:
    """symbol -> the firmware .c that defines it (regex scan, best effort)."""
    defs: dict[str, Path] = {}
    for d in src_dirs:
        for src in sorted(d.glob("*.c")):
            text = strip_comments(src.read_text(errors="replace"))
            names = set(FUNC_DEF_RE.findall(text)) | defined_variables(text)
            for name in names:
                defs.setdefault(name, src)
    return defs


def find_prototype(name: str, header_dirs: list[Path]) -> tuple[str, Path] | None:
    """Fake-list line for `name`, from its declaration in a header."""
    proto = re.compile(
        r"(?:^|[;}\n])\s*([A-Za-z_][\w \t*]*?)\b" + re.escape(name) + r"\s*\(([^;{}]*)\)\s*[;{]",
        re.S,
    )
    var = re.compile(r"\bextern\s+[^;()]*\b" + re.escape(name) + r"\b[^;()]*;")
    for d in header_dirs:
        for header in sorted(d.rglob("*.h")):
            if FAKES_DIR in header.parents:
                continue
            text = strip_comments(header.read_text(errors="replace"))
            if name not in text:
                continue
            m = proto.search(text)
            if m:
                return to_fake_line(name, m.group(1), m.group(2)), header
            if var.search(text):
                return "", header  # a variable: no list line
    return None


def to_fake_line(name: str, ret: str, params: str) -> str:
    drop = r"\b(extern|static|inline|__weak|__STATIC_INLINE|__STATIC_FORCEINLINE|__IO)\b"
    ret = " ".join(re.sub(drop, " ", ret).split())
    types = []
    for p in split_top_level(" ".join(params.split())):
        if p in ("void", ""):
            continue
        if p == "...":
            types.append("...")
            continue
        arrays = len(re.findall(r"\[[^\]]*\]", p))
        p = re.sub(r"\[[^\]]*\]", "", p).strip()
        if "(" not in p:  # drop the parameter name, keep the type
            m = re.match(r"^(.*?[\s*])([A-Za-z_]\w*)$", p)
            if m and m.group(1).strip() not in ("", "const", "volatile", "struct", "enum"):
                p = m.group(1)
        p = re.sub(r"\s*\*\s*", "*", p.strip()) + "*" * arrays
        types.append(p)
    args = "".join(", " + t for t in types)
    if ret == "void":
        return f"VOID({name}{args})"
    return f"VALUE({ret}, {name}{args})"


# --------------------------------------------------------------------------- main


def missing_function_advice(name: str, include_dirs: list[Path]) -> str:
    """The fake-list line to paste for `name`, and the file it goes in."""
    found = find_prototype(name, include_dirs + [DRIVERS_DIR])
    if not found:
        return f"  {name}: no declaration found in the firmware or Drivers/ headers"
    line, header = found
    if not line:
        return (
            f"  {name}: global variable declared in {rel(header)}; define it in\n"
            f"      Tests/fakes/fake_{header.stem}.c and zero it in the reset"
        )
    if DRIVERS_DIR in header.parents or header.name.startswith("cmsis"):
        where = "HAL_FAKE_LIST in Tests/fakes/hal_fakes.h"
    else:
        where = f"FAKE_{header.stem.upper()}_LIST in Tests/fakes/fake_{header.stem}.h"
    return f"  {line}\n      -> {where}  (declared in {rel(header)})"


def rel(path: Path) -> str:
    try:
        return path.resolve().relative_to(FW_DIR).as_posix()
    except ValueError:
        return str(path)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(
        description="Show which symbols a firmware module needs in a unit test.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("\n\n", 2)[2] if __doc__ else None,
    )
    ap.add_argument("module", help="firmware file basename, e.g. bsm or adBms_Application")
    ap.add_argument("--build-dir", type=Path, default=TESTS_DIR / "build")
    ap.add_argument("--with", dest="with_", action="append", default=[], metavar="MODULE")
    ap.add_argument("-D", "--define", action="append", default=[], metavar="MACRO")
    args = ap.parse_args(argv)

    build = args.build_dir
    cc = split_cc(read_lines(build / "fw_test_cc.txt")[0])
    flags = read_lines(build / "fw_test_flags.txt") + [f"-D{d}" for d in args.define]
    src_dirs = [Path(d) for d in read_lines(build / "fw_src_dirs.txt")]

    modules = [args.module] + [m for m in args.with_ if m != args.module]
    sources = {m: find_source(m, src_dirs) for m in modules}
    undefined: dict[str, set[str]] = {}  # symbol -> modules that need it
    defined: set[str] = set()
    with tempfile.TemporaryDirectory() as tmp:
        for m, src in sources.items():
            obj = Path(tmp) / f"{m}.o"
            compile_object(cc, flags, src, obj)
            und, dfn = object_symbols(obj)
            defined |= dfn
            for name in und:
                undefined.setdefault(name, set()).add(m)

    provided = fake_lists()
    fw_defs = firmware_definitions(src_dirs)
    include_dirs = [Path(f[2:]) for f in flags if f.startswith("-I")]

    rows, fakes, real, missing = [], [], [], []
    for name in sorted(undefined, key=str.lower):
        if name in defined:
            continue
        provider = provided.get(name)
        if provider and provider[0] == "HAL":
            rows.append((name, "HAL fake", "fakes/hal_fakes.h"))
        elif provider:
            fake = provider[0]
            alt = f" (real: {rel(fw_defs[name])})" if name in fw_defs else ""
            rows.append((name, f"fake_{fake}", f"FAKES {fake}{alt}"))
            if fake not in fakes:
                fakes.append(fake)
        elif RUNTIME_RE.match(name):
            continue
        elif name in fw_defs:
            path = fw_defs[name]
            rows.append((name, "real firmware", f"SOURCES {path.stem}  ({rel(path)})"))
            if path.stem not in real:
                real.append(path.stem)
        else:
            rows.append((name, "MISSING", ""))
            missing.append(name)

    print(f"What {' + '.join(modules)} needs from other code:\n")
    if rows:
        w0 = max([len("symbol")] + [len(r[0]) for r in rows])
        w1 = max([len("provided by")] + [len(r[1]) for r in rows])
        print(f"  {'symbol':<{w0}}  {'provided by':<{w1}}  how to link it")
        print(f"  {'-' * w0}  {'-' * w1}  {'-' * 14}")
        for name, cls, how in rows:
            print(f"  {name:<{w0}}  {cls:<{w1}}  {how}".rstrip())
    else:
        print("  (none)")

    if real:
        print(
            "\nReal firmware with no fake yet. For each, either link the real file "
            "(add_fw_test SOURCES, and\nrerun with --with to see what it needs) or "
            "write fakes/fake_<module>.{h,c} and use FAKES:\n"
        )
        for stem in real:
            print(f"  {stem}:  SOURCES {stem}  (rerun: --with {stem})  |  FAKES {stem}")

    if missing:
        print("\nMISSING: paste each line into the fake list shown.\n")
        for name in missing:
            print(missing_function_advice(name, include_dirs))

    parts = [args.module]
    sources = modules[1:] + real  # real firmware with no fake has to be linked
    if sources:
        parts.append("SOURCES " + " ".join(sources))
    if fakes:
        parts.append("FAKES " + " ".join(fakes))
    if args.define:
        parts.append("DEFINES " + " ".join(args.define))
    print(f"\nSuggested CMake line (Tests/CMakeLists.txt):\n\n    add_fw_test({' '.join(parts)})")
    if real:
        again = " ".join(f"--with {m}" for m in modules[1:] + real)
        print(f"\nThen check what the added SOURCES need:  needs_fakes.py {args.module} {again}")
    if missing:
        print(f"\n{len(missing)} symbol(s) missing: add them, then rerun.")
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ToolError as e:
        print(f"needs_fakes: {e}", file=sys.stderr)
        sys.exit(2)
    except (OSError, subprocess.SubprocessError) as e:
        print(f"needs_fakes: {e}", file=sys.stderr)
        sys.exit(2)
