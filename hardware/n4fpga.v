`timescale 1ns / 1ps
// n4fpga.v - Top level module for the ECE 544 Getting Started project
//
// Copyright Roy Kravitz, Portland State University 2013-2015, 2016
//
// Created By:	Roy Kravitz
// Date:		31-December-2014
// Version:		1.0
//
// Description:
// ------------
// This module provides the top level for the Getting Started hardware.
// The module assume that a PmodCLP is plugged into the JA and JB
// expansion ports and that a PmodENC is plugged into the JD expansion 
// port (bottom row).  
//////////////////////////////////////////////////////////////////////
module n4fpga(
    input				clk,			// 100Mhz clock input
    input				btnC,			// center pushbutton
    input				btnU,			// UP (North) pusbhbutton
    input				btnL,			// LEFT (West) pushbutton
    input				btnD,			// DOWN (South) pushbutton  - used for system reset
    input				btnR,			// RIGHT (East) pushbutton
	input				btnCpuReset,	// CPU reset pushbutton
    input	[15:0]		sw,				// slide switches on Nexys 4
    output	[15:0] 		led,			// LEDs on Nexys 4   
    output              RGB1_Blue,      // RGB1 LED (LD16) 
    output              RGB1_Green,
    output              RGB1_Red,
    output              RGB2_Blue,      // RGB2 LED (LD17)
    output              RGB2_Green,
    output              RGB2_Red,
    output [7:0]        an,             // Seven Segment display
    output [6:0]        seg,
    output              dp,
    
    input				uart_rtl_rxd,	// USB UART Rx and Tx on Nexys 4
    output				uart_rtl_txd,	
    
    output	[7:0] 		JA,				// JA Pmod connector - PmodCLP data bus
										// both rows are used
    output	[7:0] 		JB,				// JB Pmod connector - PmodCLP control signals
										// only the bottom row is used
    output	[7:0] 		JC,				// JC Pmod connector - debug signals
										// only the bottom row is used
	input	[7:0]		JD				// JD Pmod connector - PmodENC signals
);

// internal variables
wire				sysclk;
wire				sysreset_n, sysreset;
wire				rotary_a, rotary_b, rotary_press, rotary_sw;
wire	[7:0]		lcd_d;
wire				lcd_rs, lcd_rw, lcd_e;

wire	[7:0]	gpio_in;				// embsys GPIO input port
wire	[7:0]	gpio_out;				// embsys GPIO output port
wire            pwm_out;                // PWM output from the axi_timer

// make the connections to the GPIO port.  Most of the bits are unused in the Getting
// Started project but GPIO's provide a convenient way to get the inputs and
// outputs from logic you create to and from the Microblaze.  For example,
// you may decide that using an axi_gpio peripheral is a good way to interface
// your hardware pulse-width detect logic with the Microblaze.  Our application
// is simple.

//wrap the pwm_out from the timer back to the application program for software pulse-width detect
assign gpio_in = {7'b0000000, pwm_out};

// The FIT interrupt routine synthesizes a 20KHz signal and makes it
// available on GPIO2[0].  Bring it to the top level as an
// indicator that the system is running...could be handy for debug
wire            clk_20khz;
assign clk_20khz = gpio_out[0];

// We want to show the PWM output on LD15 (the leftmost LED;  The higher the PWM
// duty cycle, the brighter the LED but, the LEDs are controlled by the 
// Microblaze through Nexys4.  So, we have to play some games.  Instead of
// driving the leds directly, we will write 0 to led[15] and then OR in the PWM
// output. 
wire    [15:0]  led_int;                // Nexys4IO drives these outputs
assign led = {(pwm_out | led_int[15]), led_int[14:0]};  // LEDs are driven by led

// make the connections

// system-wide signals
assign sysclk = clk;
assign sysreset_n = btnCpuReset;		// The CPU reset pushbutton is asserted low.  The other pushbuttons are asserted high
										// but the microblaze for Nexys 4 expects reset to be asserted low
assign sysreset = ~sysreset_n;			// Generate a reset signal that is asserted high for any logic blocks expecting it.

// PmodCLP signals
// JA - top and bottom rows
// JB - bottom row only
assign JA = lcd_d[7:0];
assign JB = {1'b0, lcd_e, lcd_rw, lcd_rs, 2'b00, clk_20khz, pwm_out};

// PmodENC signals
// JD - bottom row only
// Pins are assigned such that turning the knob to the right
// causes the rotary count to increment.
assign rotary_a = JD[5];
assign rotary_b = JD[4];
assign rotary_press = JD[6];
assign rotary_sw = JD[7];

// Debug signals are on both rows of JC
assign JC = {lcd_e, lcd_rs, lcd_rw, 1'b0, lcd_d[3:0]}; 
			
// instantiate the embedded system
system EMBSYS
       (.PmodCLP_DataBus(lcd_d),
        .PmodCLP_E(lcd_e),
        .PmodCLP_RS(lcd_rs),
        .PmodCLP_RW(lcd_rw),
        .PmodENC_A(rotary_a),
        .PmodENC_B(rotary_b),
        .PmodENC_BTN(rotary_press),
        .PmodENC_SWT(rotary_sw),
        .RGB1_Blue(RGB1_Blue),
        .RGB1_Green(RGB1_Green),
        .RGB1_Red(RGB1_Red),
        .RGB2_Blue(RGB2_Blue),
        .RGB2_Green(RGB2_Green),
        .RGB2_Red(RGB2_Red),
        .an(an),
        .btnC(btnC),
        .btnD(btnD),
        .btnL(btnL),
        .btnR(btnR),
        .btnU(btnU),
        .dp(dp),
        .led(led_int),
        .seg(seg),
        .sw(sw),
        .sysreset_n(sysreset_n),
        .sysclk(sysclk),
        .uart_rtl_rxd(uart_rtl_rxd),
        .uart_rtl_txd(uart_rtl_txd),
        .gpio_0_GPIO2_tri_o(gpio_out),
        .gpio_0_GPIO_tri_i(gpio_in),
        .pwm0(pwm_out));

endmodule

