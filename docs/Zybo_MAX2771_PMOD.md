A1: The reference crystal frequency of the MAX2771 board is 24MHz.
A2: I/Q output bit with from MAX2771 is 2-bit sign/magnitude. 01 = +3, 00 = +1, 10 = -1, 11 = -3.
A3: There are two MAX2771 chips _A and _B on the board. Use JB for SPI and JC for I/Q signals.
Please use chip _A for this project.

PMOD  JB  MAX2771
---------------------
Pin1  V8  SPI SDATA
Pin2  W8  SPI SCLK
Pin3  U7  SPI /CS_B
Pin4  V7  SPI /CS_A
Pin5  Y7  CLKOUT (I/Q sample clock from MAX2771)
Pin6  Y6  Not connected
Pin7  V6  LOCK_B
Pin8  W6  LOCK_A

PMOD  JC  MAX2771
---------------------
Pin1  V15  Q[1]_B
Pin2  W15  I[1]_B
Pin3  T11  Q[1]_A
Pin4  T10  I[1]_A
Pin5  W14  Q[0]_B
Pin6  Y14  I[0]_B
Pin7  T12  Q[0]_A
Pin8  U12  I[0]_A

A4: Use FreeRTOS for the PS.
A5: Please find the Zybo-Z7-Master.xdc file in this folder.

pocketgnss.c is a sample PS code that I test the MAX2771 register read/write with bit-banging SPI.