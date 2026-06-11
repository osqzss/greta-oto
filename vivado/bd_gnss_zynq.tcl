# -----------------------------------------------------------------------
# bd_gnss_zynq.tcl
#   IP Integrator block design for Zybo Z7-20 + gnss_top_axi.
#
#   Block design contents:
#     processing_system7_0  -- Zynq PS (100 MHz FCLK, GP0 AXI, EMIO SPI0,
#                               IRQ_F2P)
#     proc_sys_reset_0      -- reset synchronizer
#     axi_interconnect_0    -- 1-master / 1-slave AXI interconnect
#     gnss_top_axi_0        -- module reference to gnss_top_axi.v
#
#   External ports created:
#     adc_clk               -- MAX2771 CLKOUT  (JB Pin5, Y7)
#     max_i[1:0]            -- I-channel data  (JC)
#     max_q[1:0]            -- Q-channel data  (JC)
#     spi0_sclk_o           -- PS SPI0 SCLK    (JB Pin2, W8)
#     spi0_mosi_o           -- PS SPI0 MOSI    (JB Pin1, V8)
#     spi0_ss_o[0]          -- PS SPI0 CS_A    (JB Pin4, V7)
#     pps_pulse1/2/3        -- PPS outputs     (JD)
#     event_mark            -- timing input    (JD Pin4)
#
#   Source this file from create_project.tcl AFTER adding RTL sources.
# -----------------------------------------------------------------------

create_bd_design "gnss_zynq"

# -----------------------------------------------------------------------
# Zynq PS7
# -----------------------------------------------------------------------
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 \
    processing_system7_0

# Attach DDR and FIXED_IO ports automatically
apply_bd_automation \
    -rule xilinx.com:bd_rule:processing_system7 \
    -config {
        make_external "FIXED_IO, DDR"
        Master "Disable"
        Slave  "Disable"
    } [get_bd_cells processing_system7_0]

# PS configuration:
#   FCLK_CLK0 = 100 MHz, GP0 AXI master, EMIO SPI0, F2P interrupt
set_property -dict [list \
    CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ  {100}  \
    CONFIG.PCW_USE_M_AXI_GP0             {1}    \
    CONFIG.PCW_USE_FABRIC_INTERRUPT      {1}    \
    CONFIG.PCW_IRQ_F2P_INTR              {1}    \
    CONFIG.PCW_SPI0_PERIPHERAL_ENABLE    {1}    \
    CONFIG.PCW_SPI0_SPI0_IO              {EMIO} \
] [get_bd_cells processing_system7_0]

# -----------------------------------------------------------------------
# proc_sys_reset
# -----------------------------------------------------------------------
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 \
    proc_sys_reset_0

connect_bd_net \
    [get_bd_pins processing_system7_0/FCLK_CLK0] \
    [get_bd_pins proc_sys_reset_0/slowest_sync_clk]
connect_bd_net \
    [get_bd_pins processing_system7_0/FCLK_RESET0_N] \
    [get_bd_pins proc_sys_reset_0/ext_reset_in]

# -----------------------------------------------------------------------
# AXI Interconnect  (1 master -> 1 slave)
# -----------------------------------------------------------------------
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 \
    axi_interconnect_0
set_property CONFIG.NUM_MI {1} [get_bd_cells axi_interconnect_0]

foreach pin {ACLK S00_ACLK M00_ACLK} {
    connect_bd_net \
        [get_bd_pins processing_system7_0/FCLK_CLK0] \
        [get_bd_pins axi_interconnect_0/$pin]
}
connect_bd_net \
    [get_bd_pins proc_sys_reset_0/interconnect_aresetn] \
    [get_bd_pins axi_interconnect_0/ARESETN]
connect_bd_net \
    [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
    [get_bd_pins axi_interconnect_0/S00_ARESETN]
connect_bd_net \
    [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
    [get_bd_pins axi_interconnect_0/M00_ARESETN]

connect_bd_intf_net \
    [get_bd_intf_pins processing_system7_0/M_AXI_GP0] \
    [get_bd_intf_pins axi_interconnect_0/S00_AXI]

connect_bd_net \
    [get_bd_pins processing_system7_0/FCLK_CLK0] \
    [get_bd_pins processing_system7_0/M_AXI_GP0_ACLK]

# -----------------------------------------------------------------------
# gnss_top_axi  (module reference)
# The RTL source must already be in the fileset before sourcing this file.
# -----------------------------------------------------------------------
create_bd_cell -type module -reference gnss_top_axi gnss_top_axi_0

connect_bd_net \
    [get_bd_pins processing_system7_0/FCLK_CLK0] \
    [get_bd_pins gnss_top_axi_0/S_AXI_ACLK]
connect_bd_net \
    [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
    [get_bd_pins gnss_top_axi_0/S_AXI_ARESETN]

connect_bd_intf_net \
    [get_bd_intf_pins axi_interconnect_0/M00_AXI] \
    [get_bd_intf_pins gnss_top_axi_0/S_AXI]

# IRQ: baseband done -> IRQ_F2P[0]
connect_bd_net \
    [get_bd_pins gnss_top_axi_0/irq] \
    [get_bd_pins processing_system7_0/IRQ_F2P]

# -----------------------------------------------------------------------
# AXI address assignment  (default: 0x43C0_0000, 64 KB)
# -----------------------------------------------------------------------
assign_bd_address
set_property offset 0x43C00000 \
    [get_bd_addr_segs {gnss_top_axi_0/S_AXI/reg0}]
set_property range 64K \
    [get_bd_addr_segs {gnss_top_axi_0/S_AXI/reg0}]

# -----------------------------------------------------------------------
# External ports: MAX2771 I/Q (JC) + ADC clock (JB)
# -----------------------------------------------------------------------
create_bd_port -dir I          adc_clk
create_bd_port -dir I -from 1 -to 0 max_i
create_bd_port -dir I -from 1 -to 0 max_q

connect_bd_net [get_bd_ports adc_clk] [get_bd_pins gnss_top_axi_0/adc_clk]
connect_bd_net [get_bd_ports max_i]   [get_bd_pins gnss_top_axi_0/max_i]
connect_bd_net [get_bd_ports max_q]   [get_bd_pins gnss_top_axi_0/max_q]

# -----------------------------------------------------------------------
# External ports: PS SPI0 EMIO (JB)
# PS7 EMIO SPI0 pins: SCLK_O, MOSI_O, SS_O[2:0], MISO_I
# -----------------------------------------------------------------------
create_bd_port -dir O          spi0_sclk_o
create_bd_port -dir O          spi0_mosi_o
create_bd_port -dir O -from 2 -to 0 spi0_ss_o

connect_bd_net \
    [get_bd_ports spi0_sclk_o] \
    [get_bd_pins processing_system7_0/SPI0_SCLK_O]
connect_bd_net \
    [get_bd_ports spi0_mosi_o] \
    [get_bd_pins processing_system7_0/SPI0_MOSI_O]
connect_bd_net \
    [get_bd_ports spi0_ss_o] \
    [get_bd_pins processing_system7_0/SPI0_SS_O]

# Tie MISO to 0 (write-only MAX2771 configuration; wire up if reads needed)
set_property -dict [list CONFIG.CONST_VAL {0} CONFIG.CONST_WIDTH {1}] \
    [create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_miso]
connect_bd_net \
    [get_bd_pins xlconstant_miso/dout] \
    [get_bd_pins processing_system7_0/SPI0_MISO_I]

# -----------------------------------------------------------------------
# External ports: PPS outputs + event mark (JD)
# -----------------------------------------------------------------------
create_bd_port -dir O  pps_pulse1
create_bd_port -dir O  pps_pulse2
create_bd_port -dir O  pps_pulse3
create_bd_port -dir I  event_mark

connect_bd_net [get_bd_ports pps_pulse1]  [get_bd_pins gnss_top_axi_0/pps_pulse1]
connect_bd_net [get_bd_ports pps_pulse2]  [get_bd_pins gnss_top_axi_0/pps_pulse2]
connect_bd_net [get_bd_ports pps_pulse3]  [get_bd_pins gnss_top_axi_0/pps_pulse3]
connect_bd_net [get_bd_ports event_mark]  [get_bd_pins gnss_top_axi_0/event_mark]

# -----------------------------------------------------------------------
# Validate and save
# -----------------------------------------------------------------------
validate_bd_design
save_bd_design
