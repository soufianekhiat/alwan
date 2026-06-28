#!/usr/bin/env python3
"""
Core .h / .inc parity check.

Each ``src/alwan/core/alwan_*_core.h`` has two backends:
  * C backend  -- ``#include``s the matching ``alwan_*_core.inc`` twice to emit
    the f32 and f64 dual-precision functions (templated with ALWAN_CORE_* macros).
  * GPU / single backend -- the ``#else`` branch writes the same functions once
    in single precision (alwan_scalar, ALWAN_LITERAL, ALWAN_EXP, <name>_v, ...).

The two must stay in lock-step: a fix applied to the .inc but not mirrored into
the .h GPU branch (or vice versa) is a silent divergence bug (see the project's
"Core .h and .inc parity" feedback note). This script normalises the .inc's
ALWAN_CORE_* macro layer down to the GPU spelling and compares each shared
function body. It exits non-zero on any drift.

The ALWAN_CORE_* -> GPU-spelling map is DERIVED from
``alwan_core_f32_setup.h`` (GPU form == f32 expansion with the _f32/_F32 suffix
removed; ALWAN_CORE_T -> alwan_scalar), so it tracks the macro layer
automatically.

Usage:
  python check_core_parity.py [--quiet]   # exit 1 on drift
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.join(HERE, "..", "src", "alwan", "core")
SETUP = os.path.join(CORE, "alwan_core_f32_setup.h")


def build_macro_map():
    """Map every object/rename ALWAN_CORE_X to its GPU spelling (the f32
    expansion with the _f32/_F32 suffix stripped). The FN/FNV/FNLIT/FNVLIT
    family and ALWAN_CORE_T are handled separately by normalise()."""
    m = {}
    skip = {"ALWAN_CORE_T", "ALWAN_CORE_SUFFIX", "ALWAN_CORE_FN", "ALWAN_CORE_FNV",
            "ALWAN_CORE_FN_", "ALWAN_CORE_FNV_", "ALWAN_CORE_FN2_", "ALWAN_CORE_FNV2_",
            "ALWAN_CORE_FNLIT", "ALWAN_CORE_FNVLIT"}
    txt = open(SETUP, encoding="utf-8", errors="replace").read()
    for mo in re.finditer(r"^#define\s+(ALWAN_CORE_[A-Z0-9_]+)(\([^)]*\))?\s+(.*)$",
                          txt, re.M):
        name, args, body = mo.group(1), mo.group(2), mo.group(3).strip()
        if name in skip:
            continue
        # GPU spelling: if the f32 expansion is a single suffixed token
        # (ALWAN_EXP_F32, alwan_vec3_f32, ...), strip the _f32/_F32 suffix.
        # Otherwise the macro is inlined in the C setup but the GPU backend has
        # a parallel ALWAN_<X> macro (e.g. ALWAN_CORE_SELECT -> ALWAN_SELECT), so
        # fall back to ALWAN_ + the suffix of the ALWAN_CORE_ name.
        lead = body.split("(")[0].strip()
        gpu = lead.replace("_F32", "").replace("_f32", "")
        if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", gpu):
            gpu = "ALWAN_" + name[len("ALWAN_CORE_"):]
        m[name] = gpu
    return m


def normalise_inc(text, macro_map):
    """Rewrite .inc text into the GPU single-precision spelling."""
    # Function-name macros: consume the (base) argument.
    text = re.sub(r"ALWAN_CORE_FNVLIT\s*\(\s*([A-Za-z0-9_]+)\s*\)", r"\1_v", text)
    text = re.sub(r"ALWAN_CORE_FNV\s*\(\s*([A-Za-z0-9_]+)\s*\)", r"\1_v", text)
    text = re.sub(r"ALWAN_CORE_FNLIT\s*\(\s*([A-Za-z0-9_]+)\s*\)", r"\1", text)
    text = re.sub(r"ALWAN_CORE_FN\s*\(\s*([A-Za-z0-9_]+)\s*\)", r"\1", text)
    text = re.sub(r"\bALWAN_CORE_T\b", "alwan_scalar", text)
    # Longest names first so e.g. ALWAN_CORE_LOG10 isn't shadowed by a prefix.
    for name in sorted(macro_map, key=len, reverse=True):
        text = re.sub(r"\b" + re.escape(name) + r"\b", macro_map[name], text)
    return text


# ---- function extraction -------------------------------------------------

FUNC_RE = re.compile(
    r"ALWAN_INLINE\s+[^\n;{}]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*?\)\s*\{")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def extract_functions(text):
    """Return {name: canonical_body} for every ALWAN_INLINE function."""
    text = strip_comments(text)
    out = {}
    for mo in FUNC_RE.finditer(text):
        name = mo.group(1)
        # brace-match the body
        i = mo.end() - 1
        depth = 0
        j = i
        while j < len(text):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        body = text[i:j + 1]
        body = re.sub(r"\s+", " ", body)
        body = re.sub(r"\s*([(){}\[\],;])\s*", r"\1", body)  # ignore spacing around punctuation
        out[name] = body.strip()
    return out


CONST_RE = re.compile(r"^[ \t]*#define[ \t]+([A-Z][A-Z0-9_]*)[ \t]+(\S.*?)[ \t]*$", re.M)


def collect_consts(text):
    """Object-like #define NAME value constants (skip function-like macros)."""
    out = {}
    for mo in CONST_RE.finditer(text):
        name, val = mo.group(1), mo.group(2).strip()
        # only object-like: the char right after the name is whitespace, not '('
        if "(" in text[mo.start():mo.start(1) + len(name) + 1].split(name, 1)[-1][:1]:
            continue
        out[name] = re.sub(r"\s+", " ", val)
    return out


def resolve(body, consts):
    """Substitute constant names with their values (bounded passes), so two
    spellings of the same value (a literal vs a named coefficient, or _-suffixed
    collision-avoidance names) compare equal."""
    for _ in range(4):
        new = re.sub(r"\b([A-Z][A-Z0-9_]*)\b",
                     lambda m: consts.get(m.group(1), m.group(0)), body)
        if new == body:
            break
        body = new
    return body


def gpu_section(h_text):
    """The single-backend mirror is the #else branch of the backend switch."""
    m = re.search(r"#else\b.*?/\*\s*HLSL.*?\*/(.*?)#endif\s*/\*\s*ALWAN_BACKEND",
                  h_text, re.S)
    if m:
        return m.group(1)
    # Fallback: whole #else..#endif of the first ALWAN_BACKEND switch.
    m = re.search(r"#if\s+ALWAN_BACKEND\b.*?#else\b(.*?)#endif\s*/\*\s*ALWAN_BACKEND",
                  h_text, re.S)
    return m.group(1) if m else ""


def main():
    quiet = "--quiet" in sys.argv
    macro_map = build_macro_map()
    plat = os.path.join(CORE, "..", "alwan_platform.h")
    base_consts = collect_consts(open(plat, encoding="utf-8", errors="replace").read()) \
        if os.path.exists(plat) else {}
    drift = []
    for inc in sorted(f for f in os.listdir(CORE) if f.endswith("_core.inc")):
        base = inc[:-4]
        h = base + ".h"
        hp = os.path.join(CORE, h)
        if not os.path.exists(hp):
            continue
        h_text = open(hp, encoding="utf-8", errors="replace").read()
        gpu = extract_functions(gpu_section(h_text))
        if not gpu:
            continue  # no GPU mirror in this header -> nothing to compare
        inc_norm = normalise_inc(open(os.path.join(CORE, inc), encoding="utf-8",
                                      errors="replace").read(), macro_map)
        inc_fns = extract_functions(inc_norm)
        # Resolve constants (literal vs named, _-suffixed collision-avoidance
        # names) to their values so equivalent spellings compare equal.
        consts = dict(base_consts)
        consts.update(collect_consts(h_text))
        consts.update(collect_consts(inc_norm))
        for name, body in gpu.items():
            if name in inc_fns and resolve(inc_fns[name], consts) != resolve(body, consts):
                drift.append((h, name))
    if drift:
        print("core .h/.inc parity FAILED -- %d function(s) diverged:" % len(drift))
        for h, name in drift:
            print("  %s :: %s" % (h, name))
        return 1
    if not quiet:
        print("core .h/.inc parity OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
