# -----------------------------------------------------------------------
# constraints_template.xdc
#   Timing and pin constraints template for Zynq + MAX2771.
#   Replace pin names (IOxx_xx) with your actual board pin locations.
# -----------------------------------------------------------------------

# -----------------------------------------------------------------------
# System clock (from Zynq FCLK or external oscillator)
# Vivado block design generates this automatically if using FCLK_CLK0.
# Uncomment only if the system clock enters the PL on a dedicated pin.
# -----------------------------------------------------------------------
# create_clock -period 10.000 -name sys_clk [get_ports sys_clk_p]

# -----------------------------------------------------------------------
# MAX2771 sample clock (CLKOUT pin of MAX2771)
# This is an asynchronous clock relative to the system clock.
# sync_data.v inside gnss_top handles the CDC.
# Typical CLKOUT rate: 4.096 MHz, 8.192 MHz, or 16.368 MHz.
# -----------------------------------------------------------------------
create_clock -period 243.584 -name adc_clk [get_ports adc_clk]
# ^^^ 243.584 ns = 4.096 MHz.  Adjust to match MAX2771 CLKOUT configuration.

# Declare adc_clk asynchronous to the AXI system clock
set_clock_groups -asynchronous \
    -group [get_clocks clk_fpga_0] \
    -group [get_clocks adc_clk]
# ^^^ Replace clk_fpga_0 with actual system clock name from Vivado timing report.

# -----------------------------------------------------------------------
# MAX2771 I/Q data pins
# Replace IOxx_xx with actual bank/pin from your schematic.
# -----------------------------------------------------------------------
set_property PACKAGE_PIN IOxx_xx [get_ports {max_i[0]}]
set_property PACKAGE_PIN IOxx_xx [get_ports {max_i[1]}]
set_property PACKAGE_PIN IOxx_xx [get_ports {max_q[0]}]
set_property PACKAGE_PIN IOxx_xx [get_ports {max_q[1]}]
set_property PACKAGE_PIN IOxx_xx [get_ports adc_clk]

set_property IOSTANDARD LVCMOS33 [get_ports {max_i[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {max_i[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {max_q[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {max_q[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports adc_clk]

# -----------------------------------------------------------------------
# MAX2771 I/Q data input timing
# The MAX2771 drives I/Q data synchronous to CLKOUT.
# Set input delay relative to adc_clk rising edge.
# Adjust min/max based on MAX2771 datasheet tpd specs (~3-5 ns).
# -----------------------------------------------------------------------
set_input_delay -clock adc_clk -max 5.0 [get_ports {max_i[*] max_q[*]}]
set_input_delay -clock adc_clk -min 1.0 [get_ports {max_i[*] max_q[*]}]

# -----------------------------------------------------------------------
# PPS outputs
# -----------------------------------------------------------------------
set_property PACKAGE_PIN IOxx_xx [get_ports pps_pulse1]
set_property PACKAGE_PIN IOxx_xx [get_ports pps_pulse2]
set_property PACKAGE_PIN IOxx_xx [get_ports pps_pulse3]
set_property IOSTANDARD LVCMOS33 [get_ports pps_pulse1]
set_property IOSTANDARD LVCMOS33 [get_ports pps_pulse2]
set_property IOSTANDARD LVCMOS33 [get_ports pps_pulse3]

# -----------------------------------------------------------------------
# Event mark input (optional external timing reference)
# -----------------------------------------------------------------------
set_property PACKAGE_PIN IOxx_xx [get_ports event_mark]
set_property IOSTANDARD LVCMOS33 [get_ports event_mark]

# -----------------------------------------------------------------------
# MAX2771 SPI configuration interface
# Use PS SPI0 or SPI1 (routed through MIO or EMIO) or add a PL SPI master.
# If using PS MIO SPI, no PL constraints are needed here.
# -----------------------------------------------------------------------
# set_property PACKAGE_PIN IOxx_xx [get_ports max2771_sclk]
# set_property PACKAGE_PIN IOxx_xx [get_ports max2771_sdata]
# set_property PACKAGE_PIN IOxx_xx [get_ports max2771_cs_n]
# set_property IOSTANDARD LVCMOS33 [get_ports max2771_sclk]
# set_property IOSTANDARD LVCMOS33 [get_ports max2771_sdata]
# set_property IOSTANDARD LVCMOS33 [get_ports max2771_cs_n]

# -----------------------------------------------------------------------
# BRAM synthesis attribute (add to project as global XDC or in RTL)
# These attributes guide Vivado to use BRAM for spram/sprom instances.
# Usually inferred automatically; add only if Vivado uses LUTs instead.
# -----------------------------------------------------------------------
# set_property ram_style block [get_cells -hierarchical -filter {NAME =~ *mem*}]
