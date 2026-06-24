//----------------------------------------------------------------------
// clock_gating.v:
//   clock gating model
//
//          Copyright (C) 2020-2029 by Jun Mo, All rights reserved.
//
//----------------------------------------------------------------------

// this file contains wrappers of clock gating model
// the wrapper implements the behavior model of clock gating
// back-end may replace this wrappers with gated clock unit
//
// REPLACED by rtl/xilinx/xilinx_clk_gate.v (BUFGCE) for the Vivado flow; this
// generic file is intentionally NOT added to the Vivado project (see
// vivado/create_project.tcl) to avoid a duplicate gated_clock_wrapper.
module gated_clock_wrapper
(
output clk_out,	// output of gated clock
input clk_in,	// clock input
input en,	// clock gating input control, 1 for enable and 0 for disable
input te	// test input, if not used, set to low
);

reg en_latch;

always @(*)
	if (!clk_in)
		en_latch = en | te;

assign clk_out = clk_in & en_latch;

endmodule
