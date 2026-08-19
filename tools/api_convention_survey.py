#!/usr/bin/env python3
"""Classify every public alwan declaration against the v2.0 convention.

Rules checked:
  R1. ctx (alwan_ctx*) must be last when present.
  R2. each *_stride must immediately follow the buffer it strides.
  R3. no output buffer (`*_out` or non-const pointer) after an input buffer (`*_in` or const pointer).
  R4. count / width / height must come after the buffer-stride block, not before.

Heuristic: a "buffer" is a pointer parameter; a "stride" is a parameter named
`*_stride` or `*_row_stride`; an output is a non-const pointer; an input is a
const pointer or a value type.
"""

from __future__ import annotations
import re
import sys
from pathlib import Path

# This script lives at <git>/alwan_dev/tools/ and operates on the sibling
# alwan repo at <git>/alwan/.
ALWAN_ROOT = Path(__file__).resolve().parent.parent.parent / "alwan"
HEADER = ALWAN_ROOT / "src/alwan/alwan.h"

# Regex for a function decl spread across one or more lines.
DECL_RE = re.compile(
    r"^\s*(?:static\s+|extern\s+)?"
    r"(?P<ret>(?:int|void|alwan_\w+|alwan_\w+\s*\*|size_t)\s*\*?)\s+"
    r"(?P<name>alwan_\w+)\s*"
    r"\((?P<params>[^;]*?)\)\s*;",
    re.MULTILINE | re.DOTALL,
)


def split_params(p: str) -> list[str]:
    """Split a param string at top-level commas (ignoring commas inside parens)."""
    out: list[str] = []
    depth = 0
    cur = []
    for ch in p:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    tail = "".join(cur).strip()
    if tail:
        out.append(tail)
    return [x for x in out if x and x != "void"]


def classify_param(p: str) -> dict:
    """Best-effort classification: kind + name + const-ness + pointer-depth.

    Distinguishes BUFFER pointers (large arrays — scalar element type) from
    VALUE-INPUT pointers (small struct const-refs like alwan_xyz, alwan_rgb,
    alwan_mat3x3). Value-input pointers are NOT treated as input buffers.
    """
    pp = re.sub(r"/\*.*?\*/", "", p, flags=re.DOTALL).strip()
    name_match = re.search(r"(\w+)\s*$", pp)
    name = name_match.group(1) if name_match else ""
    type_part = pp[: name_match.start()].strip() if name_match else pp
    is_ptr = "*" in pp
    is_const_ptr = is_ptr and "const" in type_part
    is_ctx = "alwan_ctx" in type_part and is_ptr and not is_const_ptr
    is_stride = name.endswith("stride")
    is_count = name in ("count", "n", "num")
    is_size = name in ("width", "height", "depth", "size", "row_count")

    # Recognise value-input pointers: pointers to small alwan struct types
    # used as parameters (white_xyz, lift, gain, gamma, src_space, etc.).
    # These are NOT bulk buffers — they're tuning knobs / metadata.
    _struct_value_types = (
        "alwan_xyz", "alwan_rgb", "alwan_lab", "alwan_luv", "alwan_lch",
        "alwan_oklab", "alwan_oklch", "alwan_jzazbz", "alwan_jzczhz",
        "alwan_ictcp", "alwan_ipt", "alwan_igpgtg", "alwan_icacb",
        "alwan_ycbcr", "alwan_ycocg", "alwan_yccbccrc", "alwan_uvw",
        "alwan_din99", "alwan_hunter_lab", "alwan_iptch", "alwan_prolab",
        "alwan_osa_ucs", "alwan_ucs", "alwan_prismatic", "alwan_hcl",
        "alwan_ihls", "alwan_cam_jab", "alwan_hsluv", "alwan_hpluv",
        "alwan_okhsl", "alwan_okhsv", "alwan_cubehelix", "alwan_hlc",
        "alwan_hsv", "alwan_hsl", "alwan_hsp", "alwan_hsplog", "alwan_hsy",
        "alwan_hwb", "alwan_xyy", "alwan_lchuv", "alwan_cmy", "alwan_cmyk",
        "alwan_mat3x3", "alwan_mat4x4", "alwan_vec2", "alwan_vec3",
        "alwan_rgb_space_desc", "alwan_st2086_metadata",
        "alwan_content_light_level", "alwan_spd_shape",
    )
    # Strip macros like ALWAN_MAP_LIT(...) for type detection.
    type_for_match = re.sub(r"ALWAN_MAP_LIT\(([^)]*)\)", r"\1", type_part)
    is_struct_value = is_const_ptr and any(
        st in type_for_match for st in _struct_value_types
    )

    # Buffer types: pointers to scalar element types (alwan_f32/f64, float,
    # double, uint8/uint16, alwan_map_lane, alwan_simd_lane, void*, etc).
    _buffer_scalar_types = (
        "alwan_f32", "alwan_f64", "float", "double",
        "alwan_uint8", "alwan_uint16", "alwan_uint32", "alwan_uint64",
        "alwan_int8", "alwan_int16", "alwan_int32", "alwan_int64",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "alwan_map_lane", "alwan_simd_lane", "alwan_simd",
        "void", "char", "size_t",
    )
    type_for_buf_match = re.sub(r"ALWAN_MAP_LIT\(([^)]*)\)", r"\1", type_part)
    is_buffer_type = any(
        bt in type_for_buf_match for bt in _buffer_scalar_types
    )
    # `char const *path` etc.: treat string parameters as value inputs unless
    # name explicitly indicates a buffer (out, in, _ch{N}).
    _buffer_name_patterns = re.compile(r"^(out|in|src|dst|buf|o\d+|i\d+|in\d+|out\d+)(_|$)")
    looks_like_buffer = bool(_buffer_name_patterns.match(name)) or name.endswith(("_in", "_out", "_buf"))

    is_out_buf = (is_ptr and not is_const_ptr and not is_ctx and not is_stride
                  and is_buffer_type and not is_struct_value)
    is_in_buf = (is_const_ptr and is_buffer_type and looks_like_buffer
                 and not is_struct_value)
    return {
        "raw": p,
        "name": name,
        "type": type_part,
        "is_ptr": is_ptr,
        "is_const_ptr": is_const_ptr,
        "is_ctx": is_ctx,
        "is_stride": is_stride,
        "is_count": is_count,
        "is_size": is_size,
        "is_out_buf": is_out_buf,
        "is_in_buf": is_in_buf,
    }


def check(name: str, params: list[dict]) -> list[str]:
    issues = []
    n = len(params)

    # R1: ctx must be last.
    ctx_idxs = [i for i, p in enumerate(params) if p["is_ctx"]]
    if ctx_idxs and ctx_idxs[-1] != n - 1:
        issues.append(f"R1: ctx at position {ctx_idxs[-1]} (of {n}); should be last")

    # R2: every stride must immediately follow a buffer.
    for i, p in enumerate(params):
        if p["is_stride"]:
            if i == 0 or not (params[i - 1]["is_out_buf"] or params[i - 1]["is_in_buf"]):
                issues.append(f"R2: {p['name']} at pos {i} not adjacent to a buffer")

    # R3: no output buffer after an input buffer (only check the strided buffer block,
    # i.e. before the first count/size/value-input/ctx).
    block_end = n
    for i, p in enumerate(params):
        if p["is_count"] or p["is_size"] or p["is_ctx"]:
            block_end = i
            break
        if not (p["is_out_buf"] or p["is_in_buf"] or p["is_stride"]):
            block_end = i
            break
    seen_in = False
    for i in range(block_end):
        p = params[i]
        if p["is_in_buf"]:
            seen_in = True
        elif p["is_out_buf"] and seen_in:
            issues.append(f"R3: output buffer {p['name']} at pos {i} appears after an input buffer")

    # R4: count/size before any buffer.
    first_buf = next((i for i, p in enumerate(params) if p["is_out_buf"] or p["is_in_buf"]), None)
    if first_buf is not None:
        for i in range(first_buf):
            p = params[i]
            if p["is_count"] or p["is_size"]:
                issues.append(f"R4: sizing param {p['name']} at pos {i} appears before any buffer")
                break

    return issues


def main() -> int:
    src = HEADER.read_text(encoding="utf-8")
    decls = list(DECL_RE.finditer(src))
    if not decls:
        print(f"No declarations parsed from {HEADER}", file=sys.stderr)
        return 2

    compliant = 0
    non_compliant = 0
    issues_by_category: dict[str, list[str]] = {}

    for m in decls:
        name = m.group("name")
        params_str = m.group("params")
        params = [classify_param(p) for p in split_params(params_str)]
        if not params:
            compliant += 1
            continue
        issues = check(name, params)
        if not issues:
            compliant += 1
        else:
            non_compliant += 1
            for issue in issues:
                cat = issue.split(":", 1)[0]
                issues_by_category.setdefault(cat, []).append(f"{name}: {issue}")

    total = compliant + non_compliant
    print(f"=== Survey of {HEADER} ===")
    print(f"Total functions parsed:  {total}")
    print(f"Compliant:               {compliant}  ({100*compliant/total:.1f}%)")
    print(f"Non-compliant:           {non_compliant}  ({100*non_compliant/total:.1f}%)")
    print()
    print("=== Issues by rule ===")
    for cat in sorted(issues_by_category):
        print(f"{cat}: {len(issues_by_category[cat])} occurrences")
    print()
    print("=== Non-compliant samples (first 30 per rule) ===")
    for cat in sorted(issues_by_category):
        print(f"\n--- {cat} ---")
        for s in issues_by_category[cat][:30]:
            print(f"  {s}")
    # Also dump distinct violator function names (useful for batch sweeps).
    print()
    print("=== All distinct functions per rule ===")
    for cat in sorted(issues_by_category):
        names = sorted({s.split(":", 1)[0] for s in issues_by_category[cat]})
        print(f"{cat}: {len(names)} unique functions")
        for n in names:
            print(f"  {n}")
    return 0 if non_compliant == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
