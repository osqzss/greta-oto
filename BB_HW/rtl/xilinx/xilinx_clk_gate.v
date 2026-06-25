//----------------------------------------------------------------------
// xilinx_clk_gate.v:
//   Xilinx FPGA replacement for latch-based gated_clock_wrapper.
//   Uses BUFHCE (regional clock buffer + clock enable) for glitch-free
//   clock gating. Include this file INSTEAD OF backend_wrapper/clock_gating.v
//   when targeting Xilinx devices.
//----------------------------------------------------------------------
//
// Why BUFHCE instead of BUFGCE:
//   PS7 FCLK_CLK0 is buffered by a global BUFG, whose output feeds these
//   gated-clock buffers. With BUFGCE this forms a BUFG->BUFGCE cascade; the
//   7-series placer requires cascaded BUFGs to be adjacent and cannot place the
//   5 gated-clock BUFGCEs next to the single FCLK BUFG -> [Place 30-120]. The
//   earlier workaround (CLOCK_DEDICATED_ROUTE FALSE) forced the BUFG->BUFGCE
//   connection onto GENERAL ROUTING, adding ~2.8 ns of clock-net delay on the
//   gated branch only. That created ~-3 ns launch/capture clock skew on paths
//   from a gated register to an ungated load (e.g. the shared Legendre ROM),
//   which dominated the setup (WNS) violation.
//
//   BUFG->BUFHCE is a legal topology (global BUFG driving a regional/horizontal
//   buffer via the dedicated clock spine, NOT a BUFG->BUFG cascade). It needs no
//   CLOCK_DEDICATED_ROUTE override, keeps the gated clock on dedicated routing,
//   and collapses the skew to a fraction of a ns while preserving the exact
//   glitch-free clock-gating behaviour. The placer itself recommends inserting a
//   BUFH on this connection ([Place 30-120] resolution text).
//
//   Note: a BUFHCE drives a single clock region, so all loads of that gated
//   clock must fit in one region. This is true for the small per-correlator
//   gated clocks (where the setup-critical Weil/Legendre paths live), so those
//   use BUFHCE (USE_BUFH=1, default) to remove the skew.
//   The acquisition-engine core gated clock (ae_core) is much larger and does
//   NOT fit in a single clock region (-> [Place 30-487] region overflow), and
//   it is not on any setup-critical path, so that instance keeps BUFGCE
//   (USE_BUFH=0). The single remaining BUFGCE->FCLK-BUFG cascade still needs the
//   CLOCK_DEDICATED_ROUTE FALSE relaxation (constraints_zybo_z7_impl.xdc); the
//   extra clock-net delay on that branch is harmless because it is not critical.

module gated_clock_wrapper #(parameter USE_BUFH = 1)
(
output clk_out,
input  clk_in,
input  en,
input  te
);

generate
if (USE_BUFH) begin : g_bufh
    // CE_TYPE("SYNC") samples CE to provide glitch-free gating, matching BUFGCE.
    BUFHCE #(.CE_TYPE("SYNC"), .INIT_OUT(0)) u_bufhce
    (
        .O  (clk_out),
        .I  (clk_in),
        .CE (en | te)
    );
end else begin : g_bufg
    // SIM_DEVICE must match the target architecture (Zynq-7000 = 7SERIES) so the
    // unisim model is correct and Vivado does not emit [Netlist 29-345].
    BUFGCE #(.SIM_DEVICE("7SERIES")) u_bufgce
    (
        .O  (clk_out),
        .I  (clk_in),
        .CE (en | te)
    );
end
endgenerate

endmodule
