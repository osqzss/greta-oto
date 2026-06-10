//----------------------------------------------------------------------
// gnss_top_axi.v:
//   AXI4-Lite slave wrapper around gnss_top for Zynq PS integration.
//
//   The gnss_top host bus is a simple parallel interface:
//     cs/rd/wr qualify a 14-bit DWORD address (64 KB space).
//   This wrapper bridges AXI4-Lite byte addresses [15:2] to that bus.
//
//   Read latency: gnss_top registers host_d4rd on posedge clk after
//   cs+rd are asserted, so reads require one wait cycle (RD_WAIT state).
//
//   Suggested Vivado block design connection:
//     S_AXI_ACLK    <- Zynq FCLK_CLK0 (e.g. 100 MHz)
//     S_AXI_ARESETN <- proc_sys_reset peripheral_aresetn
//     irq           -> Zynq IRQ_F2P[0]
//     MAX2771 CLKOUT -> adc_clk (constrain as async clock in XDC)
//     MAX2771 I/Q   -> max_i / max_q ports
//----------------------------------------------------------------------

`include "address.v"

module gnss_top_axi
#(
    parameter C_S_AXI_ADDR_WIDTH = 16  // byte address, covers 64KB
)
(
    // AXI4-Lite slave
    input  wire                          S_AXI_ACLK,
    input  wire                          S_AXI_ARESETN,

    input  wire [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_AWADDR,
    input  wire [2:0]                    S_AXI_AWPROT,
    input  wire                          S_AXI_AWVALID,
    output reg                           S_AXI_AWREADY,

    input  wire [31:0]                   S_AXI_WDATA,
    input  wire [3:0]                    S_AXI_WSTRB,
    input  wire                          S_AXI_WVALID,
    output reg                           S_AXI_WREADY,

    output reg  [1:0]                    S_AXI_BRESP,
    output reg                           S_AXI_BVALID,
    input  wire                          S_AXI_BREADY,

    input  wire [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_ARADDR,
    input  wire [2:0]                    S_AXI_ARPROT,
    input  wire                          S_AXI_ARVALID,
    output reg                           S_AXI_ARREADY,

    output reg  [31:0]                   S_AXI_RDATA,
    output reg  [1:0]                    S_AXI_RRESP,
    output reg                           S_AXI_RVALID,
    input  wire                          S_AXI_RREADY,

    // MAX2771 sample interface (connect CLKOUT directly to adc_clk in top-level)
    input  wire        adc_clk,          // MAX2771 CLKOUT
    input  wire [1:0]  max_i,            // MAX2771 I[1:0]
    input  wire [1:0]  max_q,            // MAX2771 Q[1:0]

    // External signals
    input  wire        event_mark,
    output wire        pps_pulse1,
    output wire        pps_pulse2,
    output wire        pps_pulse3,
    output wire        pps_irq,
    output wire        irq
);

wire clk   = S_AXI_ACLK;
wire rst_b = S_AXI_ARESETN;

//----------------------------------------------------------
// MAX2771 → gnss_top ADC data conversion
//----------------------------------------------------------
wire [7:0] adc_data;
max2771_if u_max2771_if (
    .max_i    (max_i),
    .max_q    (max_q),
    .adc_data (adc_data)
);

//----------------------------------------------------------
// Write channel: buffer AW and W independently
//----------------------------------------------------------
reg         aw_buf_valid;
reg  [13:0] aw_buf_addr;   // DWORD address
reg         w_buf_valid;
reg  [31:0] w_buf_data;

wire aw_accept = S_AXI_AWVALID & S_AXI_AWREADY;
wire w_accept  = S_AXI_WVALID  & S_AXI_WREADY;

// Fire write when both address and data are available
wire wr_fire = (aw_buf_valid || aw_accept) && (w_buf_valid || w_accept);

always @(posedge clk or negedge rst_b) begin
    if (!rst_b) begin
        aw_buf_valid  <= 1'b0;
        aw_buf_addr   <= 14'h0;
        S_AXI_AWREADY <= 1'b1;
    end else begin
        if (wr_fire) begin
            aw_buf_valid  <= 1'b0;
            S_AXI_AWREADY <= 1'b1;
        end else if (aw_accept) begin
            aw_buf_valid  <= 1'b1;
            aw_buf_addr   <= S_AXI_AWADDR[15:2];
            S_AXI_AWREADY <= 1'b0;
        end
    end
end

always @(posedge clk or negedge rst_b) begin
    if (!rst_b) begin
        w_buf_valid  <= 1'b0;
        w_buf_data   <= 32'h0;
        S_AXI_WREADY <= 1'b1;
    end else begin
        if (wr_fire) begin
            w_buf_valid  <= 1'b0;
            S_AXI_WREADY <= 1'b1;
        end else if (w_accept) begin
            w_buf_valid  <= 1'b1;
            w_buf_data   <= S_AXI_WDATA;
            S_AXI_WREADY <= 1'b0;
        end
    end
end

// Mux: prefer buffered value, fall back to direct AXI input on same cycle
wire [13:0] wr_addr = aw_buf_valid ? aw_buf_addr : S_AXI_AWADDR[15:2];
wire [31:0] wr_data = w_buf_valid  ? w_buf_data  : S_AXI_WDATA;

// Write response
always @(posedge clk or negedge rst_b) begin
    if (!rst_b) begin
        S_AXI_BVALID <= 1'b0;
        S_AXI_BRESP  <= 2'b00;
    end else begin
        if (wr_fire)
            S_AXI_BVALID <= 1'b1;
        else if (S_AXI_BREADY)
            S_AXI_BVALID <= 1'b0;
    end
end

//----------------------------------------------------------
// Read channel state machine
//----------------------------------------------------------
localparam RD_IDLE = 2'd0, RD_EXEC = 2'd1, RD_WAIT = 2'd2, RD_RESP = 2'd3;
reg [1:0]  rd_state;
reg [13:0] rd_addr_reg;

always @(posedge clk or negedge rst_b) begin
    if (!rst_b) begin
        rd_state      <= RD_IDLE;
        rd_addr_reg   <= 14'h0;
        S_AXI_ARREADY <= 1'b1;
        S_AXI_RVALID  <= 1'b0;
        S_AXI_RDATA   <= 32'h0;
        S_AXI_RRESP   <= 2'b00;
    end else begin
        case (rd_state)
            RD_IDLE: begin
                if (S_AXI_ARVALID && S_AXI_ARREADY) begin
                    rd_addr_reg   <= S_AXI_ARADDR[15:2];
                    S_AXI_ARREADY <= 1'b0;
                    rd_state      <= RD_EXEC;
                end
            end
            RD_EXEC: begin
                // host_cs + host_rd asserted this cycle (combinational below)
                rd_state <= RD_WAIT;
            end
            RD_WAIT: begin
                // gnss_top registers its output on posedge clk; data valid now
                S_AXI_RDATA  <= host_d4rd;
                S_AXI_RRESP  <= 2'b00;
                S_AXI_RVALID <= 1'b1;
                rd_state     <= RD_RESP;
            end
            RD_RESP: begin
                if (S_AXI_RREADY) begin
                    S_AXI_RVALID  <= 1'b0;
                    S_AXI_ARREADY <= 1'b1;
                    rd_state      <= RD_IDLE;
                end
            end
        endcase
    end
end

//----------------------------------------------------------
// gnss_top host bus drive
//----------------------------------------------------------
wire host_cs  = wr_fire || (rd_state == RD_EXEC);
wire host_wr  = wr_fire;
wire host_rd  = (rd_state == RD_EXEC);
wire [13:0] host_addr = wr_fire ? wr_addr : rd_addr_reg;
wire [31:0] host_d4wt = wr_data;
wire [31:0] host_d4rd;

//----------------------------------------------------------
// gnss_top instance
//----------------------------------------------------------
gnss_top u_gnss_top
(
    .clk         (clk        ),
    .rst_b       (rst_b      ),

    .adc_clk     (adc_clk    ),
    .adc_data    (adc_data   ),

    .host_cs     (host_cs    ),
    .host_rd     (host_rd    ),
    .host_wr     (host_wr    ),
    .host_addr   (host_addr  ),
    .host_d4wt   (host_d4wt  ),
    .host_d4rd   (host_d4rd  ),

    .event_mark  (event_mark ),
    .pps_pulse1  (pps_pulse1 ),
    .pps_pulse2  (pps_pulse2 ),
    .pps_pulse3  (pps_pulse3 ),
    .pps_irq     (pps_irq    ),
    .irq         (irq        )
);

endmodule
