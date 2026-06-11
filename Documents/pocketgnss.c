/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xgpio.h"
#include "xgpiops.h"
#include "sleep.h"

#define SPI_CHANNEL 1
#define SDATA 0x8
#define SCLK 0x4
#define SSB 0x2
#define SSA 0x1

// type definitions -------------------------------------------------------------
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed char int8_t;
typedef short int16_t;
typedef long int32_t;

// default MAX2771 register settings -------------------------------------------
#define MAX_ADDR 11

//  [CH1] F_LO = 1569.420 MHz, F_ADC = 24.000 MHz (I ), F_FILT =  6.0 MHz, BW_FILT =  4.2 MHz
//  [CH2] F_LO = 1176.450 MHz, F_ADC =  0.000 MHz (IQ), F_FILT =  0.0 MHz, BW_FILT = 23.4 MHz
/*
static uint32_t reg_default[][MAX_ADDR] = {
	{0xA2241797, 0x20550288, 0x0E9F21DC, 0x69888000, 0x00082008, 0x0647AE70,
	 0x08000000, 0x00000000, 0x01E0F281, 0x00000002, 0x00000004},
	{0xA224A019, 0x28550288, 0x0E9F31DC, 0x78888000, 0x00062008, 0x004CCD70,
	 0x08000000, 0x10000000, 0x01E0F281, 0x00000002, 0x00000004}
};
*/
//  [CH1] F_LO = 1572.420 MHz, F_ADC = 12.000 MHz (I ), F_FILT =  3.0 MHz, BW_FILT =  4.2 MHz
//  [CH2] F_LO = 1278.750 MHz, F_ADC =  0.000 MHz (IQ), F_FILT =  0.0 MHz, BW_FILT =  8.7 MHz

static uint32_t reg_default[][MAX_ADDR] = {
		{0xA2241BD7, 0x20550288, 0x0E8F21D0, 0x49888008, 0x0CCBEC80, 0x00000070,
		 0x08000000, 0x00000000, 0x01E0F281, 0x00000002, 0x00000004},
		{0xA224A00D, 0x28550288, 0x0E8F31D0, 0x19888008, 0x00D52100, 0x00000070,
		 0x08000000, 0x10000000, 0x01E0F281, 0x00000002, 0x00000004}
};

// XGPIO instance ---------------------------------------------------------------
XGpio gpio_0;

// read port D bit -------------------------------------------------------------
static uint8_t digitalRead(uint32_t port)
{
	uint32_t mask = XGpio_GetDataDirection(&gpio_0, SPI_CHANNEL);

	XGpio_SetDataDirection(&gpio_0, SPI_CHANNEL, mask | port); // 1:input

	return ((XGpio_DiscreteRead(&gpio_0, SPI_CHANNEL) & port)? 1 : 0);
}

// write port D bit ------------------------------------------------------------
static void digitalWrite (uint32_t port, uint8_t dat)
{
	uint32_t mask = XGpio_GetDataDirection(&gpio_0, SPI_CHANNEL);

	XGpio_SetDataDirection(&gpio_0, SPI_CHANNEL, mask & ~port); // 0:output

	if (dat)
		XGpio_DiscreteSet(&gpio_0, SPI_CHANNEL, port);
	else
		XGpio_DiscreteClear(&gpio_0, SPI_CHANNEL, port);
}

// write SPI SCLK --------------------------------------------------------------
static void write_sclk (void)
{
	digitalWrite(SCLK, 1);
	usleep(1);
	digitalWrite(SCLK, 0);
	usleep(1);
}

// write SPI SDATA -------------------------------------------------------------
static void write_sdata (uint8_t dat)
{
	digitalWrite(SDATA, dat);
	write_sclk();
}

// read SPI SDATA --------------------------------------------------------------
static uint8_t read_sdata (void)
{
	uint8_t dat = digitalRead(SDATA);
	write_sclk();

	return (dat);
}

// write MAX2771 SPI frame header ----------------------------------------------
static void write_head(uint16_t addr, uint8_t mode)
{
	int8_t i;

	for (i = 11; i >= 0; i--)
		write_sdata((uint8_t)(addr >> i) & 1);
	write_sdata(mode); // 0:write,1:read

	for (i = 0; i < 3; i++)
	    write_sdata(0);
}

// write MAX2771 register ------------------------------------------------------
static void write_reg(uint8_t cs, uint8_t addr, uint32_t val)
{
	int8_t i;

	digitalWrite(cs, 0);
	write_head(addr, 0);

	for (i = 31; i >= 0; i--)
		write_sdata((uint8_t)(val >> i) & 1);

	digitalWrite(cs, 1);
}

// read MAX2771 register -------------------------------------------------------
static uint32_t read_reg(uint8_t cs, uint8_t addr)
{
	uint32_t val = 0;
	int8_t i;

	digitalWrite(cs, 0);
	write_head(addr, 1);

	for (i = 31; i >= 0; i--)
	{
		val <<= 1;
		val |= read_sdata();
	}

	digitalWrite(cs, 1);

	return (val);
}

// load default MAX2771 register settings --------------------------------------
static void load_default(uint8_t cs)
{
	uint8_t addr;
	uint8_t ch;

	ch = 0; // default: SSA
	if (cs==SSB)
		ch = 1;

	for (addr = 0; addr < MAX_ADDR; addr++)
		write_reg(cs, addr, reg_default[ch][addr]);
}

// MAIN ------------------------------------------------------------------------
int main()
{
	uint8_t addr;
	uint32_t reg;

	XGpioPs gpio_ps;
	XGpioPs_Config *cfg_gpio_ps;

    init_platform();

    print("Hello PocketGNSS!\n\r");

    // Initialize PS GPIO for LED4 (MIO7)
    cfg_gpio_ps = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
    XGpioPs_CfgInitialize(&gpio_ps, cfg_gpio_ps, cfg_gpio_ps->BaseAddr);
    print("Successfully initialized XPAR_PS7_GPIO_0_DEVICE_ID.\n\r");

    XGpioPs_SetDirectionPin(&gpio_ps, 7, 1); // set MIO7 as output
    XGpioPs_SetOutputEnablePin(&gpio_ps, 7, 1);

    // Initialize AXI GPIO for MAX2771 SPI
    int status = XGpio_Initialize(&gpio_0, XPAR_GPIO_0_DEVICE_ID);
    if(status != XST_SUCCESS){
        xil_printf("XPAR_GPIO_0_DEVICE_ID initialization failed.\n");
        return XST_FAILURE;
    }
    print("Successfully initialized XPAR_GPIO_0_DEVICE_ID.\n\r");

    XGpio_SetDataDirection(&gpio_0, SPI_CHANNEL, 0); // all output
    XGpio_DiscreteWrite(&gpio_0, SPI_CHANNEL, SSA | SSB);

    //XGpio_DiscreteSet(&gpio_0, SPI_CHANNEL, SSA);
    //XGpio_DiscreteSet(&gpio_0, SPI_CHANNEL, SSB);
    usleep(1);
/*
    print("Read default CH1 registers.\n\r");
    for (addr = 0; addr < MAX_ADDR; addr++)
    {
    	reg = read_reg(SSA, addr);
    	xil_printf("%02d : %08x\n\r", addr, reg);
    }
*/
    print("Write CH1 registers.\n\r");
    load_default(SSA);

    print("Read loaded CH1 registers.\n\r");
    for (addr = 0; addr < MAX_ADDR; addr++)
    {
    	reg = read_reg(SSA, addr);
    	xil_printf("%02d : %08x\n\r", addr, reg);
    }
/*
    print("Read default CH2 registers.\n\r");
    for (addr = 0; addr < MAX_ADDR; addr++)
    {
    	reg = read_reg(SSB, addr);
        xil_printf("%02d : %08x\n\r", addr, reg);
    }
*/
    print("Write CH2 registers.\n\r");
    	load_default(SSB);

    print("Read loaded CH2 registers.\n\r");
    for (addr = 0; addr < MAX_ADDR; addr++)
    {
    	reg = read_reg(SSB, addr);
    	xil_printf("%02d : %08x\n\r", addr, reg);
    }

    for (;;)
    {
    	// heart beat LED
    	XGpioPs_WritePin(&gpio_ps, 7, 1);
       	sleep(1);
       	XGpioPs_WritePin(&gpio_ps, 7, 0);
       	sleep(1);
    }

    cleanup_platform();
    return 0;
}

