#!/usr/bin/env python3
"""
gen_rom_init.py
  Generate Xilinx BRAM initialization files (.coe and .mem) from
  the HWModel C source arrays.

  Outputs (in BB_HW/rom_init/):
    b1c_legendre.coe / .mem   — 640 x 16-bit Legendre sequence for BDS B1C
    l1c_legendre.coe / .mem   — 640 x 16-bit Legendre sequence for GPS L1C
    memory_code.coe  / .mem   — 12800 x 32-bit Galileo E1 / BDS memory codes

  The .coe files target Xilinx Block Memory Generator IP.
  The .mem files target xpm_memory_sprom MEMORY_INIT_FILE parameter
  (one hex word per line, no prefix, width-padded).

Usage:
    python3 tools/gen_rom_init.py
  Run from the repository root, or any directory — paths are computed
  relative to this script's location.
"""

import re
import os
import sys

# ---------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------

def find_array_content(text, start_marker):
    """Return the substring from the first '{' after start_marker to
    its matching '}', inclusive.  Handles nested braces."""
    idx = text.find(start_marker)
    if idx == -1:
        raise ValueError(f"Marker not found: {start_marker!r}")
    brace_open = text.index('{', idx)
    depth = 0
    for i in range(brace_open, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[brace_open : i + 1]
    raise ValueError(f"Unmatched brace after marker: {start_marker!r}")


def parse_hex_values(array_text):
    """Extract all 0x... hex literals from array_text and return as int list."""
    return [int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]+)', array_text)]


def write_coe(path, values, data_width):
    """Write a Xilinx Block Memory Generator .coe file."""
    hex_digits = (data_width + 3) // 4
    with open(path, 'w') as f:
        f.write('memory_initialization_radix=16;\n')
        f.write('memory_initialization_vector=\n')
        for i, v in enumerate(values):
            sep = ',' if i < len(values) - 1 else ';'
            f.write(f'{v:0{hex_digits}X}{sep}\n')
    print(f'  wrote {path}  ({len(values)} x {data_width}-bit)')


def write_mem(path, values, data_width):
    """Write a $readmemh / xpm_memory_sprom MEMORY_INIT_FILE .mem file."""
    hex_digits = (data_width + 3) // 4
    with open(path, 'w') as f:
        for v in values:
            f.write(f'{v:0{hex_digits}x}\n')
    print(f'  wrote {path}  ({len(values)} x {data_width}-bit)')


# ---------------------------------------------------------------
# Paths
# ---------------------------------------------------------------

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT  = os.path.join(SCRIPT_DIR, '..')

WEIL_CPP = os.path.join(REPO_ROOT, 'HWModel', 'src', 'WeilPrn.cpp')
E1_H     = os.path.join(REPO_ROOT, 'HWModel', 'inc', 'E1_code.h')
OUT_DIR  = os.path.join(REPO_ROOT, 'BB_HW', 'rom_init')

os.makedirs(OUT_DIR, exist_ok=True)

# ---------------------------------------------------------------
# Legendre tables from WeilPrn.cpp
# ---------------------------------------------------------------

print('Parsing WeilPrn.cpp ...')
with open(WEIL_CPP, 'r') as f:
    weil_text = f.read()

b1c_content = find_array_content(weil_text, 'CWeilPrn::LegendreB1C')
l1c_content = find_array_content(weil_text, 'CWeilPrn::LegendreL1C')

b1c_vals = parse_hex_values(b1c_content)
l1c_vals = parse_hex_values(l1c_content)

if len(b1c_vals) != 640:
    sys.exit(f'ERROR: expected 640 B1C Legendre values, got {len(b1c_vals)}')
if len(l1c_vals) != 640:
    sys.exit(f'ERROR: expected 640 L1C Legendre values, got {len(l1c_vals)}')

print(f'  B1C Legendre: {len(b1c_vals)} entries OK')
print(f'  L1C Legendre: {len(l1c_vals)} entries OK')

write_coe(os.path.join(OUT_DIR, 'b1c_legendre.coe'), b1c_vals, 16)
write_mem(os.path.join(OUT_DIR, 'b1c_legendre.mem'), b1c_vals, 16)
write_coe(os.path.join(OUT_DIR, 'l1c_legendre.coe'), l1c_vals, 16)
write_mem(os.path.join(OUT_DIR, 'l1c_legendre.mem'), l1c_vals, 16)

# ---------------------------------------------------------------
# Galileo/BDS memory codes from E1_code.h
# ---------------------------------------------------------------

print('Parsing E1_code.h ...')
with open(E1_H, 'r') as f:
    e1_text = f.read()

e1_content = find_array_content(e1_text, 'GalE1Code')
e1_vals    = parse_hex_values(e1_content)

EXPECTED = 100 * 128  # 12800 DWORDs
if len(e1_vals) != EXPECTED:
    sys.exit(f'ERROR: expected {EXPECTED} memory code values, got {len(e1_vals)}')

print(f'  Memory codes: {len(e1_vals)} entries (100 SVs x 128 DWORDs) OK')

write_coe(os.path.join(OUT_DIR, 'memory_code.coe'), e1_vals, 32)
write_mem(os.path.join(OUT_DIR, 'memory_code.mem'), e1_vals, 32)

# ---------------------------------------------------------------
# Summary
# ---------------------------------------------------------------

print()
print('Done.  Files in BB_HW/rom_init/:')
for f in sorted(os.listdir(OUT_DIR)):
    size = os.path.getsize(os.path.join(OUT_DIR, f))
    print(f'  {f:30s}  {size:7d} bytes')

print()
print('Next steps:')
print('  Option A (XPM, recommended):')
print('    Copy *.mem files to your Vivado project directory (or set')
print('    MEMORY_INIT_FILE with a full path in xilinx_rom_wrapper.v).')
print()
print('  Option B (Block Memory Generator IP):')
print('    In Vivado: IP Catalog -> Block Memory Generator')
print('    Set "Load Init File" to the corresponding .coe file.')
print('    Name the IP to match the wrapper module name.')
