# -----------------------------------------------------------------------
# constraints_zybo_z7.xdc
#   Timing and I/O constraints for Zybo Z7-20 + MAX2771 PMOD board.
#
#   JC (I/Q data, chip A)
#     Pin3 T11 -> Q[1]_A   Pin4 T10 -> I[1]_A
#     Pin7 T12 -> Q[0]_A   Pin8 U12 -> I[0]_A
#
#   JB (SPI via PS EMIO + CLKOUT)
#     Pin1 V8  -> SPI SDATA (MOSI)
#     Pin2 W8  -> SPI SCLK
#     Pin4 V7  -> SPI /CS_A
#     Pin5 Y7  -> CLKOUT (adc_clk, MRCC capable)
#
#   JD (PPS outputs + event mark)
#     Pin1 T14 -> pps_pulse1   Pin2 T15 -> pps_pulse2
#     Pin3 P14 -> pps_pulse3   Pin4 R14 <- event_mark
#
#   Crystal: 24 MHz
#   CLKOUT (adc_clk): 12 MHz  (83.333 ns period)
#   LO: 1572.420 MHz  ->  IF = 3.0 MHz
# -----------------------------------------------------------------------

# -----------------------------------------------------------------------
# ADC sample clock: MAX2771 CLKOUT on JB Pin5 (Y7 = IO_L13P_T2_MRCC_13)
# 12 MHz, asynchronous to AXI system clock.
# -----------------------------------------------------------------------
set_property PACKAGE_PIN Y7        [get_ports adc_clk]
set_property IOSTANDARD  LVCMOS33  [get_ports adc_clk]
create_clock -period 83.333 -name adc_clk [get_ports adc_clk]

# Declare adc_clk asynchronous to the Zynq FCLK.
# clk_fpga_0 is the name Vivado assigns to FCLK_CLK0 in the PS7 block design.
set_clock_groups -asynchronous \
    -group [get_clocks clk_fpga_0] \
    -group [get_clocks adc_clk]

# -----------------------------------------------------------------------
# MAX2771 chip A I/Q data  (JC)
# 2-bit sign/magnitude: [1]=sign, [0]=magnitude
# -----------------------------------------------------------------------
set_property PACKAGE_PIN T10       [get_ports {max_i[1]}]
set_property PACKAGE_PIN U12       [get_ports {max_i[0]}]
set_property PACKAGE_PIN T11       [get_ports {max_q[1]}]
set_property PACKAGE_PIN T12       [get_ports {max_q[0]}]

set_property IOSTANDARD  LVCMOS33  [get_ports {max_i[1]}]
set_property IOSTANDARD  LVCMOS33  [get_ports {max_i[0]}]
set_property IOSTANDARD  LVCMOS33  [get_ports {max_q[1]}]
set_property IOSTANDARD  LVCMOS33  [get_ports {max_q[0]}]

# I/Q data is driven synchronous to CLKOUT by the MAX2771 (~3-5 ns prop delay)
set_input_delay -clock adc_clk -max 5.0 [get_ports {max_i[*] max_q[*]}]
set_input_delay -clock adc_clk -min 1.0 [get_ports {max_i[*] max_q[*]}]

# -----------------------------------------------------------------------
# MAX2771 SPI  (PS SPI0 via EMIO, JB)
# Port names match those made external in bd_gnss_zynq.tcl.
# -----------------------------------------------------------------------
set_property PACKAGE_PIN V8        [get_ports spi0_mosi_o]
set_property PACKAGE_PIN W8        [get_ports spi0_sclk_o]
set_property PACKAGE_PIN V7        [get_ports {spi0_ss_o[0]}]

set_property IOSTANDARD  LVCMOS33  [get_ports spi0_mosi_o]
set_property IOSTANDARD  LVCMOS33  [get_ports spi0_sclk_o]
set_property IOSTANDARD  LVCMOS33  [get_ports {spi0_ss_o[0]}]

# -----------------------------------------------------------------------
# PPS outputs  (JD)
# -----------------------------------------------------------------------
set_property PACKAGE_PIN T14       [get_ports pps_pulse1]
set_property PACKAGE_PIN T15       [get_ports pps_pulse2]
set_property PACKAGE_PIN P14       [get_ports pps_pulse3]

set_property IOSTANDARD  LVCMOS33  [get_ports pps_pulse1]
set_property IOSTANDARD  LVCMOS33  [get_ports pps_pulse2]
set_property IOSTANDARD  LVCMOS33  [get_ports pps_pulse3]

# -----------------------------------------------------------------------
# Event mark input  (JD Pin4 = R14)
# -----------------------------------------------------------------------
set_property PACKAGE_PIN R14       [get_ports event_mark]
set_property IOSTANDARD  LVCMOS33  [get_ports event_mark]

# -----------------------------------------------------------------------
# BRAM synthesis hint (usually auto-inferred; uncomment if Vivado uses LUTs)
# -----------------------------------------------------------------------
# set_property ram_style block [get_cells -hierarchical -filter {NAME =~ *mem*}]
