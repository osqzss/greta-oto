//----------------------------------------------------------------------
// rom_wrapper.v:
//   Generic behavioural single-port ROM wrappers (moved out of mem_wrapper.v).
//
//          Copyright (C) 2020-2029 by Jun Mo, All rights reserved.
//
//   These are the portable/simulation versions. For the Xilinx Vivado flow
//   they are REPLACED by the xpm_memory_sprom versions in
//   rtl/xilinx/xilinx_rom_wrapper.v, and THIS file is intentionally NOT added
//   to the Vivado project (see vivado/create_project.tcl) so that no duplicate
//   design units appear. Use this file for non-Xilinx / RTL-simulation targets.
//----------------------------------------------------------------------

// single port ROM for B1C Legendre code
module b1c_legendre_rom_640x16_wrapper
(
	input clk,
	input rd,
	input [9:0] addr,
	output [15:0] rdata
);

sprom #(.ROM_SIZE(640), .ADDR_WIDTH(10), .DATA_WIDTH(16)) u_rom
(
	.clk   (clk    ),
	.rd    (rd     ),
	.addr  (addr   ),
	.rdata (rdata  )
);

endmodule

// single port ROM for L1C Legendre code
module l1c_legendre_rom_640x16_wrapper
(
	input clk,
	input rd,
	input [9:0] addr,
	output [15:0] rdata
);

sprom #(.ROM_SIZE(640), .ADDR_WIDTH(10), .DATA_WIDTH(16)) u_rom
(
	.clk   (clk    ),
	.rd    (rd     ),
	.addr  (addr   ),
	.rdata (rdata  )
);

endmodule

// single port ROM for Galileo memory code
module memory_code_rom_12800x32_wrapper
(
	input clk,
	input rd,
	input [13:0] addr,
	output [31:0] rdata
);

sprom #(.ROM_SIZE(12800), .ADDR_WIDTH(14), .DATA_WIDTH(32)) u_rom
(
	.clk   (clk    ),
	.rd    (rd     ),
	.addr  (addr   ),
	.rdata (rdata  )
);

endmodule
