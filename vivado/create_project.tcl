# -----------------------------------------------------------------------
# create_project.tcl
#   Recreate the Vivado project for Zybo Z7-20 + gnss_top_axi.
#
#   Usage (from Vivado Tcl console or tclsh with Vivado in PATH):
#     cd <repo-root>/vivado
#     vivado -mode batch -source create_project.tcl
#
#   The script is self-contained relative to the repo root.
#   Output project:  vivado/gnss_zynq/gnss_zynq.xpr
# -----------------------------------------------------------------------

set script_dir [file normalize [file dirname [info script]]]
set repo_root  [file normalize "$script_dir/.."]

# -----------------------------------------------------------------------
# Create project
# -----------------------------------------------------------------------
# -force: 既存の gnss_zynq/ プロジェクトがあれば削除して作り直す（再構成を繰り返せるように）
create_project -force gnss_zynq "$script_dir/gnss_zynq" -part xc7z020clg400-1

set_property board_part digilentinc.com:zybo-z7-20:part0:1.2 \
    [current_project]

set_property target_language Verilog [current_project]

# -----------------------------------------------------------------------
# Add all Verilog RTL sources
# RTL source directories (recursive glob picks up sub-directories)
#
# NOTE: backend_wrapper is intentionally NOT glob-added. Its generic wrappers
# clock_gating.v (gated_clock_wrapper) and rom_wrapper.v (the three ROM wrappers)
# are REPLACED by the Xilinx versions in rtl/xilinx/ (xilinx_clk_gate.v /
# xilinx_rom_wrapper.v). Adding the generic ones would create duplicate design
# units (HDL 9-3756 / filemgmt 20-1318). Only mem_wrapper.v (the RAM wrappers,
# which have no Xilinx-specific replacement) is added explicitly below.
# -----------------------------------------------------------------------
set rtl_dirs [list \
    "$repo_root/BB_HW/rtl/acquire_engine"   \
    "$repo_root/BB_HW/rtl/correlation"      \
    "$repo_root/BB_HW/rtl/gnss_top"         \
    "$repo_root/BB_HW/rtl/mem_arbiter"      \
    "$repo_root/BB_HW/rtl/mem_model"        \
    "$repo_root/BB_HW/rtl/tracking_engine"  \
    "$repo_root/BB_HW/rtl/xilinx"           \
]

foreach d $rtl_dirs {
    add_files -norecurse [glob -directory $d *.v]
}

# RAM wrappers only from backend_wrapper (ROM/clock-gating wrappers excluded --
# see note above; clock_gating.v and rom_wrapper.v are left out of the project).
add_files -norecurse "$repo_root/BB_HW/rtl/backend_wrapper/mem_wrapper.v"

# Top-level address defines (`define macros used across all RTL modules).
# The core RTL files do NOT `include "address.v" individually; they rely on
# the macros being globally visible. Mark address.v as a global include so
# Vivado prepends it to every file during synthesis AND block-design module
# reference elaboration (otherwise: CRITICAL WARNING [HDL 9-3952] undefined macro).
add_files -norecurse "$repo_root/BB_HW/rtl/address.v"
set_property file_type        "Verilog Header" [get_files "$repo_root/BB_HW/rtl/address.v"]
set_property is_global_include true             [get_files "$repo_root/BB_HW/rtl/address.v"]

# Vivado include path so `include "address.v" resolves correctly
set_property include_dirs "$repo_root/BB_HW/rtl" [current_fileset]

# Set design top for out-of-context module reference
# (The block design wrapper becomes the actual top; this is set later.)
set_property top gnss_top_axi [current_fileset]

# -----------------------------------------------------------------------
# ROM initialisation files
#   .mem  -> used by xpm_memory_sprom MEMORY_INIT_FILE (synthesis $readmem).
#           Adding them to the project puts rom_init/ on the search path so
#           synthesis can resolve the relative "*.mem" names (otherwise:
#           CRITICAL WARNING [Synth 8-4445] could not open $readmem data file).
#   .coe  -> kept for the Block Memory Generator fallback (not used by xpm).
# -----------------------------------------------------------------------
add_files -norecurse [list \
    "$repo_root/BB_HW/rom_init/b1c_legendre.mem"  \
    "$repo_root/BB_HW/rom_init/l1c_legendre.mem"  \
    "$repo_root/BB_HW/rom_init/memory_code.mem"   \
    "$repo_root/BB_HW/rom_init/b1c_legendre.coe"  \
    "$repo_root/BB_HW/rom_init/l1c_legendre.coe"  \
    "$repo_root/BB_HW/rom_init/memory_code.coe"   \
]
set_property used_in_simulation false \
    [get_files "$repo_root/BB_HW/rom_init/b1c_legendre.coe"]
set_property used_in_simulation false \
    [get_files "$repo_root/BB_HW/rom_init/l1c_legendre.coe"]
set_property used_in_simulation false \
    [get_files "$repo_root/BB_HW/rom_init/memory_code.coe"]

# -----------------------------------------------------------------------
# XDC constraints
#   - constraints_zybo_z7.xdc       : pins + create_clock (synthesis + impl)
#   - constraints_zybo_z7_impl.xdc  : adc_clk<->clk_fpga_0 async clock group.
#       Implementation-ONLY (USED_IN_SYNTHESIS false): clk_fpga_0 does not exist
#       during synthesis (PS7 is a black box), so applying it at synthesis emits
#       [Vivado 12-4739], and a Tcl guard is illegal in an XDC ([Designutils
#       20-1307]). Restricting it to implementation avoids both.
# -----------------------------------------------------------------------
add_files -fileset constrs_1 -norecurse \
    "$repo_root/BB_HW/rtl/xilinx/constraints_zybo_z7.xdc"
add_files -fileset constrs_1 -norecurse \
    "$repo_root/BB_HW/rtl/xilinx/constraints_zybo_z7_impl.xdc"
set_property used_in_synthesis false \
    [get_files "$repo_root/BB_HW/rtl/xilinx/constraints_zybo_z7_impl.xdc"]

# -----------------------------------------------------------------------
# Create block design (IP Integrator)
# gnss_top_axi module reference requires sources to be present first.
# -----------------------------------------------------------------------
source "$script_dir/bd_gnss_zynq.tcl"

# -----------------------------------------------------------------------
# Generate block design HDL wrapper and set as top
# -----------------------------------------------------------------------
make_wrapper -files [get_files gnss_zynq.bd] -top
add_files -norecurse \
    "$script_dir/gnss_zynq/gnss_zynq.gen/sources_1/bd/gnss_zynq/hdl/gnss_zynq_wrapper.v"
set_property top gnss_zynq_wrapper [current_fileset]
update_compile_order -fileset sources_1

puts ""
puts "Project created: $script_dir/gnss_zynq/gnss_zynq.xpr"
puts "Next steps:"
puts "  1. Open the project in Vivado"
puts "  2. Run synthesis and implementation"
puts "  3. Generate bitstream"
puts "  4. Export hardware (.xsa) for Vitis / FreeRTOS BSP generation"
