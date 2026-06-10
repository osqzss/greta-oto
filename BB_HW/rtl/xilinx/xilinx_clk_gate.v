//----------------------------------------------------------------------
// xilinx_clk_gate.v:
//   Xilinx FPGA replacement for latch-based gated_clock_wrapper.
//   Uses BUFGCE primitive for glitch-free clock gating.
//   Include this file INSTEAD OF backend_wrapper/clock_gating.v when
//   targeting Xilinx devices.
//----------------------------------------------------------------------

module gated_clock_wrapper
(
output clk_out,
input  clk_in,
input  en,
input  te
);

BUFGCE u_bufgce
(
    .O  (clk_out),
    .I  (clk_in),
    .CE (en | te)
);

endmodule
