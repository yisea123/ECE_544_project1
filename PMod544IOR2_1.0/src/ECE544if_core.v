// ECE544if_core.v - Interface to external PMods used in ECE 544
//
// Copyright Roy Kravitz, Portland State University 2014, 2015
//
// Created By:	Roy Kravitz
// Date:		26-December-2014
// Version:		1.0
//
// Revision History:
// -----------------
//	26-Dec-2014		RK		Created this module from the N4EIF peripheral
//
// NOTES:
// ------
//	o	THIS INTERFACE REQUIRES THAT A DIGILENT PmodCLP (2 x 16 PARALLEL INTERFACE)
//		AND A PmodENC (ROTARY ENCODER WITH PUSHBUTTON AND SLIDE SWITCH) BE INSERTED
//		IN THREE OF THE PMOD EXPANSION CONNECTORS ON THE NEXYS 3.  THERE IS NO SIMPLE
//		WAY TO CHECK IF THE PERIPHERALS ARE INSERTED BO CAVEAT EMPTOR.
//
//	o	THIS VERSION OF THE INTERFACE CREATES AN 8-BIT INTERFACE TO THE PmodCLP. 
//		INITIALIZATION TIMING IS BASED ON A SAMSUNG KS0062U LCD CONTROLLER
//
//	o	THIS VERSION OF THE CORE HAS NO SUPPORT FOR THE PUSHBUTTONS, SWITCHES LED's,
//		OR SEVEN SEGMENT DISPLAY ON THE NEXYS4.  THOSE BOARD-SPECIFIC DEVICES ARE
//		SUPPORTED IN THE NEXYS4IO custom peripheral
//
// Description:
// ------------
// This module provides an interface to a PmodCLP and a PmodENC connected to 3 (PmodCLP uses two)
//  Pmod expansion ports on a Digilent Nexys 4 board.
//	This design uses a Xilinx PicoBlaze to control two distinct functions:
//	o	rotary encoder -  The rotary encoder delivers a 16-bit rotary_value[] register 
// 		that increments (decrements) every time the rotary encoder is moved to the
// 		right (left).  The rotary encoder is controlled by an 8-bit rotary_ctl[] input vector
//		with the following bits:
//			rotary_ctl[7]   = set the rotary_count to 0 when the signal is pulsed (0->1->0)
//			rotary_ctl[6]   = set the encoder increment value to rotary_ctl[3:0] when pulsed
//			rotary_ctl[5]   = reserved
//			rotary_ctl[4]   = set to limit the encoder values to >= 0  (no negative values)
//							  written when rotary_ctl[6] is pulsed.    
//			rotary_ctl[3:0] = increment/decrement interval for rotary_value[].  (default increment is 1)  
//
//			rotary_count_lo = least significant byte of rotary encoder count
//			rotary_count_hi = most significant byte of rotary encoder count
//			rotary_status   = returns the status of the rotary encoder control.  rotary_status[7] is a 'busy' flag
//							  which will be asserted high when the rotary encoder is busy.  rotary_status[6] is the
//							  'self test' flag.  It is asserted high while the controller is doing its
//							  self test and deasserted when the self test is complete.  Tbe pushbutton and slide
//							  switch on the ROTENC module are debounced in hardware and passed directly to one of the 
//							  to the Microblaze via a register in the AXI interface logic for the the custom peripheral
//
//	o	A 2 line x 16 character LCD - The LCD display by the PicoBlaze software. An external
//		CPU (such as a Microblaze or Picoblaze) puts characters on the display and executes
//		display commands through three registers:
//			lcd_cmd - 	contains the LCD command.  when lcd_cmd[7] is toggled the cmd in lcd_cmd[4:0]
//						is translated and passed to the LCD display controller.
//			lcd_data -	contains the LCD data (character or display address)
//			lcd_status -returns the status of the LCD control.  At this point only
//						lcd_status[7] is used.  This bit is a 'busy' bit which will
//						be asserted high when the LCD display is busy.	
//	
//////////////////////////////////////////////////////////////////////////////////////

module ECE544if_core
#(
	//parameter declarations
	parameter 			RESET_POLARITY_LOW = 1,				// Reset is active-low?  (default is yes)
	parameter 			SIMULATE = 0						// Simulate mode is disabled
)
(
	// System inputs
	input 				clk,            			// peripheral clock
	input				reset,						// peripheral reset - asserted high to reset
	
	// PmodENC interface
	input				PmodENC_A,	PmodENC_B,		// rotary encoder quadrature inputs
	
	// PmodCLP interface
	output				PmodCLP_RS,					// (J2.1) LCD register select (1 = data)
						PmodCLP_RW,					// (J2.2) LCD read/write (1 = read)
						PmodCLP_E,					// (J2.3) LCD enable (1 = enable command)
	output	[7:0]		PmodCLP_DataBus,			// (J1.1 - J1.10) LCD bi-directional 8-bit data bus 
		
	// Register-based interface to Application CPU.  Used by the drivers to control and
	// receive inputs from the rotary encoder and LCD.
	input	[7:0]		rotary_ctl,					// Rotary encoder control 
	output	[7:0]		rotary_count_lo,			// Rotary encoder count bits[7:0]
						rotary_count_hi,			// Rotary encoder count bits[15:8]
						rotary_status,				// Rotary encoder status
						
	input	[7:0]		lcd_cmd,					// LCD command register
						lcd_data,					// LCD data register						
	output	[7:0]		lcd_status					// LCD controller status	
);

	// I/O address ranges
	localparam  PBIF_START_ADDR = 8'd00,
	 	       PBIF_END_ADDR   = 8'd07;

	
	//  declare internal variables
	
	// use the RESET_POLARITY_LOW parameter to set the internal reset logic
	// All of the modules assume reset is asserted high.
	wire reset_int = RESET_POLARITY_LOW ? ~reset : reset;
	
	// internal interface to/from the control Picoblaze
	wire	[7:0] 		port_id;
	wire   				write_strobe;
	wire   				read_strobe;
	wire	[7:0] 		out_port;
	wire	[7:0] 		in_port;
	wire	[11:0] 		address;
	wire	[17:0] 		instruction;
	wire				bram_enable;
	wire				rdl;
	wire				kcpsm6_sleep = 1'b0;
	wire				kcpsm6_reset = reset_int; 
	
	wire				rotary_event;				// rotary encoder event - used as interrupt to Picoblaze
	wire				rotary_left;				// rotary encoder direction - 1 says motion was to the left		

	wire                int_request;                // Picoblaze interrupt handshaking
	reg                 interrupt;
	wire                interrupt_ack;
	assign int_request = rotary_event;
		
	wire	[7:0]		lcd_ctl;					// LCD control signals driven by the Picoblaze
	assign PmodCLP_RS = lcd_ctl[2];
	assign PmodCLP_RW = lcd_ctl[1];
	assign PmodCLP_E = lcd_ctl[0];
	
	wire	[7:0]		lcd_dbus;					// LCD data bus driven by the Picoblaze
	assign PmodCLP_DataBus = lcd_dbus;
	
	wire	[7:0]		rotenc_inputs;				// Rotary encoder inputs to Picoblaze
	assign rotenc_inputs = {6'b0, rotary_event, rotary_left};
	
	// I/O interface enables
	wire				en_pbif;			// enable for PicoBlaze interface
	assign en_pbif = (port_id >= PBIF_START_ADDR) && (port_id <= PBIF_END_ADDR);

	
	// instantiate the controller Picoblaze and its Program ROM
	kcpsm6 #(
		.interrupt_vector	(12'h3FF),
		.scratch_pad_memory_size(64),
		.hwbuild		(8'h00))
	CTLCPU (
		.address 		(address),
		.instruction 	(instruction),
		.bram_enable 	(bram_enable),
		.port_id 		(port_id),
		.write_strobe 	(write_strobe),
		.k_write_strobe (),				// Constant Optimized writes are not used in this implementation
		.out_port 		(out_port),
		.read_strobe 	(read_strobe),
		.in_port 		(in_port),
		.interrupt 		(interrupt),
		.interrupt_ack 	(interrupt_ack),	
		.reset 			(kcpsm6_reset),
		.sleep			(kcpsm6_sleep),
		.clk 			(clk)
	); 

	// instantiate the ECE 544 Control program ROM
	// JTAG update is disabled - This is the "production" peripheral
	// so save the functionality for the Application program
	ECE544ifpgm CTLPGM ( 
		.enable 		(bram_enable),
		.address 		(address),
		.instruction 	(instruction),
		.clk 			(clk));
	
	// instantiate the rotary encoder filter
	// module is included in this file
	rotary_filter	RF (
		.rotary_a(PmodENC_A),
		.rotary_b(PmodENC_B),	
		.rotary_event(rotary_event),
		.rotary_left(rotary_left),				
		.clk(clk)
	);
	
	// instantiate the interface to the control Picoblaze
	pblaze_if  PBIF (
		.Wr_Strobe(write_strobe),
		.Rd_Strobe(read_strobe),
		.AddrIn(port_id),
		.DataIn(out_port),
		.DataOut(in_port),
   	
		.rotary_ctl(rotary_ctl),
		.lcd_cmd(lcd_cmd),
		.lcd_data(lcd_data),
	
		.rotary_status(rotary_status),
		.rotary_count_lo(rotary_count_lo),
		.rotary_count_hi(rotary_count_hi),
		.lcd_status(lcd_status),
		.lcd_ctl(lcd_ctl),
		.lcd_dbus(lcd_dbus),
						
		.rotenc_inputs(rotenc_inputs),
		.enable(en_pbif),
		.reset(reset_int),
		.clk(clk)
	);

    // interrupt flip-flop	
	 always @ (posedge clk) begin
         if (interrupt_ack == 1'b1) begin
            interrupt <= 1'b0;
         end
         else if (int_request == 1'b1) begin
             interrupt <= 1'b1;
         end
         else begin
             interrupt <= interrupt;
         end
     end

endmodule
