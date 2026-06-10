//----------------------------------------------------------------------
// max2771_if.v:
//   MAX2771 GNSS RF frontend to gnss_top ADC interface adapter.
//
//   MAX2771 outputs 2-bit I and 2-bit Q samples (sign + magnitude):
//     I[1] / Q[1] = sign bit  (1 = negative)
//     I[0] / Q[0] = magnitude (0 = low, 1 = high)
//
//   gnss_top expects 8-bit samples in 4-bit sign/magnitude format:
//     adc_data[7:4] = I channel:  bit[7]=sign, bit[6]=mag, bits[5:4]=0
//     adc_data[3:0] = Q channel:  bit[3]=sign, bit[2]=mag, bits[1:0]=0
//
//   When MAX2771 is configured for 1-bit output (sign only),
//   tie the unused magnitude pin to 0 externally or via parameter.
//
//   MAX2771 CLKOUT connects directly to gnss_top adc_clk input.
//   The sync_data module inside gnss_top handles CDC from adc_clk
//   to the system clock domain.
//----------------------------------------------------------------------

module max2771_if
(
// MAX2771 pins
input [1:0] max_i,      // I channel: [1]=sign, [0]=magnitude
input [1:0] max_q,      // Q channel: [1]=sign, [0]=magnitude

// gnss_top ADC interface
output [7:0] adc_data   // packed 4-bit S/M: I at [7:4], Q at [3:0]
);

assign adc_data = {max_i[1], max_i[0], 2'b00, max_q[1], max_q[0], 2'b00};

endmodule
