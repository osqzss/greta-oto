# impl_retime_try1.tcl
# WNS improvement experiment #1 — tooling only (no RTL/constraint change).
# Re-run place/route from the linked, constrained opt_design checkpoint using
# timing-driven directives + post-route phys_opt_design.
#
# Run:  vivado -mode batch -source impl_retime_try1.tcl
# (run from D:/FPGA/greta-oto/vivado)

set runs   D:/FPGA/greta-oto/vivado/gnss_zynq/gnss_zynq.runs
set outdir D:/FPGA/greta-oto/vivado/try1
file mkdir $outdir

open_checkpoint $runs/impl_1/gnss_zynq_wrapper_opt.dcp

# --- Place: extra timing-driven effort ---
place_design -directive ExtraTimingOpt

# --- Post-place physical optimization (aggressive) ---
phys_opt_design -directive AggressiveExplore

# --- Route ---
route_design -directive Explore

# --- Post-route physical optimization: iterate while it keeps helping ---
phys_opt_design -directive AggressiveExplore
phys_opt_design -directive Explore

# --- Reports ---
report_timing_summary -file $outdir/timing_summary_try1.rpt
write_checkpoint -force $outdir/routed_try1.dcp

set wns [get_property SLACK [get_timing_paths -setup -max_paths 1]]
set whs [get_property SLACK [get_timing_paths -hold  -max_paths 1]]
puts "================ RESULT try1 ================"
puts "WNS = $wns ns"
puts "WHS = $whs ns"
puts "============================================"
