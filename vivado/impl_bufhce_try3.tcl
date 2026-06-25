# impl_bufhce_try3.tcl
# Hybrid clock fix: BUFHCE for the 4 per-correlator gated clocks (small, hold
# the setup-critical Weil/Legendre paths) + BUFGCE for the large AE-core gated
# clock (USE_BUFH=0, keeps CLOCK_DEDICATED_ROUTE FALSE). Fixes try2's
# [Place 30-487] region overflow on the AE core.
#
# Run:  vivado -mode batch -source impl_bufhce_try3.tcl   (from .../vivado)

set proj   D:/FPGA/greta-oto/vivado/gnss_zynq/gnss_zynq.xpr
set outdir D:/FPGA/greta-oto/vivado/try3
file mkdir $outdir

open_project $proj

catch { reset_run gnss_zynq_gnss_top_axi_0_0_synth_1 }
reset_run synth_1
launch_runs synth_1 -jobs 4
wait_on_run synth_1

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
report_timing_summary -file $outdir/timing_summary_try3.rpt
report_clock_utilization -file $outdir/clock_util_try3.rpt

set wns [get_property SLACK [get_timing_paths -setup -max_paths 1]]
set whs [get_property SLACK [get_timing_paths -hold  -max_paths 1]]
puts "================ RESULT try3 (hybrid BUFHCE+BUFGCE) ================"
puts "WNS = $wns ns"
puts "WHS = $whs ns"
puts "synth status: [get_property STATUS [get_runs synth_1]]"
puts "impl  status: [get_property STATUS [get_runs impl_1]]"
puts "==================================================================="
