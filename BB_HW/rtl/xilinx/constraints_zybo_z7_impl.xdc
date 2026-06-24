# -----------------------------------------------------------------------
# constraints_zybo_z7_impl.xdc
#   Implementation-ONLY timing constraints for the Zybo Z7-20 + gnss_top_axi.
#
#   This file is added with USED_IN_SYNTHESIS = false (see create_project.tcl),
#   so it is processed only at implementation, where the full design is linked
#   and the PS7-generated clock clk_fpga_0 (FCLK_CLK0) actually exists.
#
#   Putting it here avoids:
#     - [Vivado 12-4739] "No valid object(s) found ... clk_fpga_0" at synthesis
#       (PS7 is a black box during synthesis of this top)
#     - [Designutils 20-1307] "Command 'if' is not supported in the xdc file"
#       (a Tcl existence-guard cannot be used inside an XDC)
#
#   adc_clk itself is created in the main constraints_zybo_z7.xdc (create_clock
#   on the adc_clk port), which is used in both synthesis and implementation.
# -----------------------------------------------------------------------

# adc_clk (MAX2771 CLKOUT, 12 MHz) is asynchronous to the Zynq FCLK_CLK0.
# clk_fpga_0 is the name Vivado assigns to FCLK_CLK0 in the PS7 block design.
set_clock_groups -asynchronous \
    -group [get_clocks clk_fpga_0] \
    -group [get_clocks adc_clk]

# -----------------------------------------------------------------------
# BUFG -> BUFGCE cascade relaxation
#   PS7 FCLK_CLK0 is buffered by a BUFG, whose output feeds the gated-clock
#   BUFGCE cells (xilinx_clk_gate.v) in ae_top and tracking_engine. That is a
#   BUFG->BUFGCE cascade; the 7-series placer requires cascaded BUFGs to be
#   adjacent and cannot place the 5 gated-clock BUFGCEs next to the single FCLK
#   BUFG -> [Place 30-120] rule_cascaded_bufg FAILED -> [Place 30-99] IO Clock
#   Placer failed.
#   All gated clocks are enable-style versions of the same 100 MHz FCLK, so we
#   relax the dedicated-route rule and let the BUFGCE clock inputs use general
#   routing. Verify timing closure in the implementation timing report after.
#   (Net/hierarchy name as reported by the placer message; exists only at impl.)
set_property CLOCK_DEDICATED_ROUTE FALSE \
    [get_nets gnss_zynq_i/processing_system7_0/inst/FCLK_CLK0]
