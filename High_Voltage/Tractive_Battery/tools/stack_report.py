#!/usr/bin/env python3
"""Worst-case stack usage per FreeRTOS task.

Reads the GCC -fstack-usage / -fcallgraph-info=su output (.ci files) from the
build directory, walks the call graph from each task entry function, and
compares the deepest call chain against the task's configured stack size in
main.c.

Limitations: calls through function pointers and library functions without
call-graph info (newlib: printf, memcpy, ...) are counted as 0 bytes and listed
as "unknown" so they can be checked by hand.

Usage: stack_report.py --build-dir build/Debug --main Core/Src/main.c [-v]
"""

import argparse
import glob
import os
import re
import sys

# Registers stacked on a context switch with the FPU in use:
# 26 words hardware frame (incl. S0-S15, FPSCR) + 9 words R4-R11/LR + 16 words S16-S31.
# Upper bound: assumes preemption lands at the deepest point of the call chain.
CONTEXT_SWITCH_BYTES = 204
LOW_MARGIN = 0.25  # Flag tasks with less than 25% of their stack free
# Leaf libc/libgcc helpers with tiny frames; not worth warning about
NEGLIGIBLE = re.compile(r"^(mem(cpy|set|move|cmp)|str(len|cmp|ncmp|cpy)|f(min|max)f?|__aeabi_\w+)$")

NODE_RE = re.compile(r'node: \{ title: "([^"]+)" label: "([^"]*)"')
EDGE_RE = re.compile(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"')
SIZE_RE = re.compile(r"\\n(\d+) bytes \((\w+)")
ATTR_RE = re.compile(
    r"osThreadAttr_t\s+(\w+)_attributes\s*=\s*\{[^}]*?\.stack_size\s*=\s*(\d+)\s*\*\s*4", re.S
)
THREAD_RE = re.compile(r"osThreadNew\s*\(\s*(\w+)\s*,[^,]*,\s*&(\w+)_attributes\s*\)")


def func_name(title):
    # Static functions are titled "file.c:func"; externs are just "func"
    return title.rsplit(":", 1)[-1]


def load_callgraph(build_dir):
    frame = {}  # func -> bytes
    dynamic = set()  # funcs with alloca/VLA frames
    calls = {}  # func -> set of callees
    ci_files = glob.glob(os.path.join(build_dir, "**", "*.ci"), recursive=True)
    for path in ci_files:
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                m = NODE_RE.match(line)
                if m:
                    name = func_name(m.group(1))
                    s = SIZE_RE.search(m.group(2))
                    if s:
                        frame[name] = max(frame.get(name, 0), int(s.group(1)))
                        if s.group(2) != "static":
                            dynamic.add(name)
                    continue
                m = EDGE_RE.match(line)
                if m:
                    calls.setdefault(func_name(m.group(1)), set()).add(func_name(m.group(2)))
    return ci_files, frame, dynamic, calls


def load_tasks(main_c):
    with open(main_c, encoding="utf-8", errors="replace") as f:
        src = f.read()
    sizes = {name: int(words) * 4 for name, words in ATTR_RE.findall(src)}
    return [(attr, entry, sizes.get(attr)) for entry, attr in THREAD_RE.findall(src)]


class Analyzer:
    def __init__(self, frame, dynamic, calls):
        self.frame, self.dynamic, self.calls = frame, dynamic, calls
        self.memo = {}

    def deepest(self, func, active=()):
        """Return (bytes, chain, unknown callees, recursive funcs) for the deepest path."""
        if func in active:
            return 0, [func + " (recursion)"], set(), {func}
        if func in self.memo:
            return self.memo[func]
        best_bytes, best_chain = 0, []
        unknown, recursive = set(), set()
        for callee in sorted(self.calls.get(func, ())):
            if callee not in self.frame:
                if not NEGLIGIBLE.match(callee):
                    unknown.add(callee)
                continue
            b, chain, u, r = self.deepest(callee, active + (func,))
            unknown |= u
            recursive |= r
            if b > best_bytes:
                best_bytes, best_chain = b, chain
        own = self.frame.get(func, 0)
        result = (own + best_bytes, [f"{func} ({own})"] + best_chain, unknown, recursive)
        if not recursive:
            self.memo[func] = result
        return result


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--build-dir", required=True)
    p.add_argument("--main", required=True, help="main.c containing osThreadNew calls")
    p.add_argument("-v", "--verbose", action="store_true", help="print deepest call chains")
    p.add_argument("--strict", action="store_true", help="exit 1 if any task may overflow")
    args = p.parse_args()

    ci_files, frame, dynamic, calls = load_callgraph(args.build_dir)
    if not ci_files:
        sys.exit(f"No .ci files in {args.build_dir}; build with -fstack-usage -fcallgraph-info=su")
    tasks = load_tasks(args.main)
    if not tasks:
        sys.exit(f"No osThreadNew calls found in {args.main}")

    analyzer = Analyzer(frame, dynamic, calls)
    rows, notes, over = [], [], False
    for name, entry, stack in tasks:
        used, chain, unknown, recursive = analyzer.deepest(entry)
        need = used + CONTEXT_SWITCH_BYTES
        if stack is None:
            status, free = "NO SIZE", None
        else:
            free = stack - need
            if free < 0:
                status, over = "AT RISK", True
            elif free < stack * LOW_MARGIN:
                status = "LOW"
            else:
                status = "ok"
        dyn = [f for f in dynamic if any(c.startswith(f + " ") for c in chain)]
        rows.append((name, used, need, stack, free, status))
        notes.append((name, chain, unknown, recursive, dyn))

    print(f"Worst-case stack per task (+{CONTEXT_SWITCH_BYTES} B context switch)\n")
    print(f"{'Task':<16}{'Calls':>8}{'+ctx':>8}{'Stack':>8}{'Free':>8}  Status")
    print("-" * 56)
    for name, used, need, stack, free, status in rows:
        stack_s = "?" if stack is None else str(stack)
        free_s = "?" if free is None else str(free)
        print(f"{name:<16}{used:>8}{need:>8}{stack_s:>8}{free_s:>8}  {status}")

    printed_header = False
    for name, chain, unknown, recursive, dyn in notes:
        warn = []
        if unknown:
            warn.append("unknown size (counted as 0): " + ", ".join(sorted(unknown)))
        if recursive:
            warn.append("recursion: " + ", ".join(sorted(recursive)))
        if dyn:
            warn.append("dynamic frame (alloca/VLA): " + ", ".join(sorted(dyn)))
        if args.verbose or warn:
            if not printed_header:
                print("\nDetails" + ("" if args.verbose else " (use -v for call chains)"))
                printed_header = True
            print(f"\n{name}:")
            if args.verbose:
                print("  " + " -> ".join(chain))
            for w in warn:
                print("  ! " + w)

    sys.exit(1 if args.strict and over else 0)


if __name__ == "__main__":
    main()
