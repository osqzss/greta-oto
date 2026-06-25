# impl_bufhce_try2.tcl
# Structural clock fix verification: BUFGCE -> BUFHCE (xilinx_clk_gate.v) +
# removal of CLOCK_DEDICATED_ROUTE FALSE (constraints_zybo_z7_impl.xdc).
# Re-synthesize (RTL changed) and re-implement using the directive recipe that
# worked in try1 (place ExtraTimingOpt / route Explore / post-route phys_opt).
#
# Run:  vivado -mode batch -source impl_bufhce_try2.tcl   (from .../vivado)

set proj   D:/FPGA/greta-oto/vivado/gnss_zynq/gnss_zynq.xpr
set outdir D:/FPGA/greta-oto/vivado/try2
file mkdir $outdir

open_project $proj

# RTL of gnss_top_axi is synthesized out-of-context in the BD; reset that OOC
# run too so the BUFHCE change is picked up, then the top-level synthesis.
catch { reset_run gnss_zynq_gnss_top_axi_0_0_synth_1 }
reset_run synth_1
launch_runs synth_1 -jobs 4
wait_on_run synth_1

# Configure impl_1 to the recipe that closed most of the gap in try1.
set_property STEPS.PLACE_DESIGN.ARGS.DIRECTIVE ExtraTimingOpt          [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true                     [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore    [get_runs impl_1]
set_property STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE Explore                 [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true          [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore [get_runs impl_1]

reset_run impl_1
launch_runs impl_1 -jobs 4
wait_on_run impl_1

open_run impl_1
report_timing_summary -file $outdir/timing_summary_try2.rpt
report_clock_utilization -file $outdir/clock_util_try2.rpt

set wns [get_property SLACK [get_timing_paths -setup -max_paths 1]]
set whs [get_property SLACK [get_timing_paths -hold  -max_paths 1]]
puts "================ RESULT try2 (BUFHCE) ================"
puts "WNS = $wns ns"
puts "WHS = $whs ns"
puts "synth status: [get_property STATUS [get_runs synth_1]]"
puts "impl  status: [get_property STATUS [get_runs impl_1]]"
puts "====================================================="
