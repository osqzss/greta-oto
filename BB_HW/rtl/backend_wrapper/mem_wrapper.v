//----------------------------------------------------------------------
// mem_wrapper.v:
//   wrapper of SRAM and ROM model
//
//          Copyright (C) 2020-2029 by Jun Mo, All rights reserved.
//
//----------------------------------------------------------------------

// this file contains wrappers of RAM and ROM used in GNSS RTL
// the wrapper implements the behavior model of memory for simulation
// back-end may replace these wrappers with memory blocks
// it also should be noted that when the following parameters changed,
// the memory configuration in wrapper should also change
// MATCH_FILTER_DEPTH affects coherent and noncoherent RAM depth
// FIFO_SIZE and DATA_WIDTH in u_te_fifo affects TE FIFO RAM depth and width

// full dual port memory for AE config buffer
module ae_config_buffer_256x32_wrapper
(
input clk_a,
input [7:0] addr_a,
input en_a,
input wr_a,
input [31:0] wdata_a,
output [31:0] rdata_a,

input clk_b,
input [7:0] addr_b,
input en_b,
input wr_b,
input [31:0] wdata_b,
output [31:0] rdata_b
);

dpram_full #(.RAM_SIZE(256), .ADDR_WIDTH(8), .DATA_WIDTH(32))	u_ram
(
   	.clk_a      (clk_a               ),
   	.addr_a     (addr_a              ),
   	.rd_a       (en_a && ~wr_a       ),
   	.wr_a       (wr_a                ),
   	.wdata_a    (wdata_a             ),
   	.rdata_a    (rdata_a             ),
   	.clk_b      (clk_b               ),
   	.addr_b     (addr_b              ),
   	.rd_b       (en_b && ~wr_b       ),
   	.wr_b       (wr_b                ),
   	.wdata_b    (wdata_b             ),
   	.rdata_b    (rdata_b             )
);

endmodule

// single port SRAM for AE coherent buffer
module ae_coh_buffer_682x192_wrapper
(
	input clk,
	input en,
	input we,
	input [9:0] addr,
	input [191:0] wdata,
	output [191:0] rdata
);

spram #(.RAM_SIZE(682), .ADDR_WIDTH(10), .DATA_WIDTH(192)) u_ram
(
	.clk   (clk    ),
	.en    (en     ),
	.we    (we     ),
	.addr  (addr   ),
	.wdata (wdata  ),
	.rdata (rdata  )
);

endmodule

// single port SRAM for AE non-coherent buffer
module ae_noncoh_buffer_682x64_wrapper
(
	input clk,
	input en,
	input we,
	input [9:0] addr,
	input [63:0] wdata,
	output [63:0] rdata
);

spram #(.RAM_SIZE(682), .ADDR_WIDTH(10), .DATA_WIDTH(64)) u_ram
(
	.clk   (clk    ),
	.en    (en     ),
	.we    (we     ),
	.addr  (addr   ),
	.wdata (wdata  ),
	.rdata (rdata  )
);

endmodule

// single port SRAM for AE sample data buffer
module ae_sample_buffer_32768x32_wrapper
(
	input clk,
	input en,
	input we,
	input [14:0] addr,
	input [31:0] wdata,
	output [31:0] rdata
);

spram #(.RAM_SIZE(32768), .ADDR_WIDTH(15), .DATA_WIDTH(32)) u_ram
(
	.clk   (clk    ),
	.en    (en     ),
	.we    (we     ),
	.addr  (addr   ),
	.wdata (wdata  ),
	.rdata (rdata  )
);

endmodule

// single port SRAM for TE FIFO
module te_fifo_ram_2560x32_wrapper
(
	input clk,
	input en,
	input we,
	input [11:0] addr,
	input [31:0] wdata,
	output [31:0] rdata
);

spram #(.RAM_SIZE(2560), .ADDR_WIDTH(12), .DATA_WIDTH(32)) u_ram
(
	.clk   (clk    ),
	.en    (en     ),
	.we    (we     ),
	.addr  (addr   ),
	.wdata (wdata  ),
	.rdata (rdata  )
);

endmodule

// single port SRAM for TE state buffer
module te_state_buffer_1024x32_wrapper
(
	input clk,
	input en,
	input we,
	input [9:0] addr,
	input [31:0] wdata,
	output [31:0] rdata
);

spram #(.RAM_SIZE(1024), .ADDR_WIDTH(10), .DATA_WIDTH(32)) u_ram
(
	.clk   (clk    ),
	.en    (en     ),
	.we    (we     ),
	.addr  (addr   ),
	.wdata (wdata  ),
	.rdata (rdata  )
);

endmodule

// single port SRAM for TE FIFO for L5
module te_fifo_ram_6144x32_wrapper
(
	input clk,
	input en,
	input we,
	input [12:0] addr,
	input [31:0] wdata,
	output [31:0] rdata
);

spram #(.RAM_SIZE(6144), .ADDR_WIDTH(13), .DATA_WIDTH(32)) u_ram
(
	.clk   (clk    ),
	.en    (en     ),
	.we    (we     ),
	.addr  (addr   ),
	.wdata (wdata  ),
	.rdata (rdata  )
);

endmodule

// NOTE: the three single-port ROM wrappers (b1c_legendre / l1c_legendre /
// memory_code) used to live here. For the Vivado flow they are replaced by the
// xpm versions in rtl/xilinx/xilinx_rom_wrapper.v. To keep the generic versions
// available for non-Xilinx targets WITHOUT creating duplicate design units in
// the Vivado project, they were moved to rtl/backend_wrapper/rom_wrapper.v
// (that file is simply not added to the Vivado project; see create_project.tcl).
