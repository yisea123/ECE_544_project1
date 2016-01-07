// pblaze_if.v - Interface to the control Picoblaze for the ECE 544 Pmods
//
// Copyright Roy Kravitz, Portland State University 2014, 2015
//
// Created By:	Roy Kravitz
// Date:		26-December-2014
// Version:		1.0
//
// Revision History:
// -----------------
//	26-Dec-2014		RK		Created this module from N3EIF.  This version removes all of the
//							inputs and outputs from the buttons, switches, and LEDs.
//	
// Description:
// ------------
// This module provides a register-based interface between the control Picoblaze and the application
// CPU for the ECE 544 custom peripheral tied to an external PmodCLP and a PmodENC.  The registers
// are all 8-bits wide so that the peripheral can be used in a Picoblaze or Microblaze-based system
// The padding and conversion to 32-bits for the Microblaze are handled by the custom peripheral
//
// Note: The address mapping (and assembly language code) has changed from N3EIF and N4EIF so this
// module is NOT compatible with the n3eif and n4eif firmware.
//////////////////////////////////////////////////////////////////////////////////////

module pblaze_if (
	// interface to the Picoblaze
	input 				Wr_Strobe,			// Write strobe - asserted to write I/O data
		 				Rd_Strobe,			// Read strobe - asserted to read I/O data
	input 		[7:0] 	AddrIn,				// I/O port address
	input 		[7:0] 	DataIn,				// Data to be written to I/O register
	output reg	[7:0] 	DataOut,			// I/O register data to picoblaze
   	
	// interface to the External CPU (Micoblaze for ECE 544)
	// inputs assume registers are instantiated in the AXI interface for the custom peripheral	
	input 		[7:0]	rotary_ctl,			// rotary control register
						lcd_cmd,			// LCD command register
						lcd_data,			// LCD data register
		
	output reg [7:0]	rotary_status,		// rotary encoder status
						rotary_count_lo,	// rotary count bits[7:0]
						rotary_count_hi,	// rotary count bits[15:8]
						lcd_status,			// LCD status register
						lcd_ctl,			// LCD control signals 
						lcd_dbus,			// LCD data bus
						
	// local interface
	input 		[7:0]	rotenc_inputs,		// rotary encoder and lcd data control inputs
	input				enable,				// enables I/O range when set to 1

	// clock and reset		
	input				reset,				// System reset.  Asserted high for reset
						clk					// System clock
 );
 

	// declare internal signals
	wire		write_en;				// write enable for this module				

					
	// respond to IO writes only if address is within my address range
	assign write_en = Wr_Strobe & enable;

	// read registers - drives registered values to the Picoblaze
	always @(posedge clk) begin
		if (enable) begin
			case (AddrIn[2:0])
				// Input registers
				3'b000 :	DataOut = rotary_ctl;		// rotary control register	
				3'b001 :	DataOut = rotenc_inputs;	// rotary filter inputs
														// PMODENC button and switch bypass the Picoblaze
				3'b010 :	DataOut = rotary_count_lo;	// least significant byte of rotary count
				3'b011 :	DataOut = rotary_count_hi;	// most significant byte of rotary count
														// These registers may be used for debug and selftest
				3'b100 : 	DataOut = lcd_cmd;			// lcd command		
				3'b101 : 	DataOut = lcd_data;			// lcd data	
				3'b110 : 	DataOut = 8'h00;			// reserved
				3'b111 :	DataOut = 8'h00;			// reserved		
			endcase
		end
	end // always - read registers


	// write registers - drives external CPU registers from Picoblaze
	always @(posedge clk or posedge reset) begin
		if (reset) begin
			rotary_status <= 8'h00;		// start w/ rotary encoder not busy
			rotary_count_lo <= 8'h00;	// start w/ rotary count = 0
			rotary_count_hi <= 8'h00;	// in both the low and high bytes	
			lcd_status <= 8'h80;		// start w/ LCD display busy
			lcd_ctl <= 8'h00;			// start w/ all LCD control signals at 0
			lcd_dbus <= 8'h00;			// start w/ LCD data bus at 0
		end
		else begin
			if(write_en) begin
				 case (AddrIn[2:0])
					// output registers (registers w/ no assigns are reserved)
					3'b000 :	rotary_status <= DataIn;
					3'b001 :	;
					3'b010 :	rotary_count_lo <= DataIn;
					3'b011 :	rotary_count_hi <= DataIn;
					3'b100 :	lcd_status <= DataIn;
					3'b101 :	lcd_ctl <= DataIn;	
					3'b110 :	lcd_dbus <= DataIn;
					3'b111 :	;
				endcase
			end
		end
	end // always - write registers
		
endmodule	