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
# BUFG -> BUFGCE cascade relaxation (AE core gated clock only)
#   The 4 per-correlator gated clocks now use BUFHCE (xilinx_clk_gate.v,
#   USE_BUFH=1). BUFG->BUFHCE is a legal dedicated-routing topology and needs no
#   override; moving those off general routing removed the ~-3 ns skew that
#   dominated the setup (WNS) violation on the Weil/Legendre correlator paths.
#
#   The acquisition-engine core gated clock (ae_core, USE_BUFH=0) keeps BUFGCE
#   because its gated domain does not fit in a single clock region (BUFHCE ->
#   [Place 30-487]). That single BUFGCE still forms a BUFG->BUFGCE cascade with
#   the FCLK BUFG, so the dedicated-route rule is relaxed for it. The resulting
#   extra clock-net delay is harmless: the AE core is not on any setup-critical
#   path. (Net/hierarchy name as reported by the placer; exists only at impl.)
set_property CLOCK_DEDICATED_ROUTE FALSE \
    [get_nets gnss_zynq_i/processing_system7_0/inst/FCLK_CLK0]
