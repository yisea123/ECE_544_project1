`timescale  1 ns / 1 ns

module hw_detect_tb();

	/******************************************************************/
	/* Declaring the internal variables	  				              */
	/******************************************************************/

	localparam CLK_PERIOD = 10;

	reg 				clock;				// system clock
	reg 				reset;				// active-high reset signal
	reg 				pwm;				// PWM signal

	wire 	[31:0] 		high_count; 		// how long PWM was 'high'
	wire 	[31:0] 		low_count;			// how long PWM was 'low'

	/******************************************************************/
	/* Instantiating the DUT 						                  */
	/******************************************************************/

	hw_detect DUT(

		.clock				(clock),			// I [ 0 ] 100MHz system clock
		.reset 				(reset),			// I [ 0 ] active-high reset signal from Nexys4
		.pwm 				(pwm),				// I [ 0 ] PWM signal from AXI Timer in EMBSYS

		.high_count 		(high_count),		// O [31:0] how long PWM was 'high' --> GPIO input on Microblaze
		.low_count 			(low_count));		// O [31:0] how long PWM was 'low' --> GPIO input on Microblaze
	
	/******************************************************************/
	/* Running the testbench simluation				                  */
	/******************************************************************/

	// set conditions for initial inputs

	initial begin
		#0 clock <= 1'b0;
		#0 reset <= 1'b0;
		#0 pwm <= 1'b0;
	end

	// toggle clock repeatedly

	always begin
		#(CLK_PERIOD/2) clock <= ~clock;
	end

	// reset the device

	initial begin
		#(5 * CLK_PERIOD) reset <= 1'b1;		// apply reset after 5 cycles
		#(5 * CLK_PERIOD) reset <= 1'b0;		// keep reset applied for 5 cycles
	end

	// adjust PWM high & low intervals

	always begin
		#(10 * CLK_PERIOD) pwm = ~pwm; 			// low interval is 10 cycles
		#(20 * CLK_PERIOD) pwm = ~pwm;			// high interval is 20 cycles
	end

	// continuously monitor the high & low counts

	initial begin
		$monitor($time, " --> 'high_count' = %d, 'low_count' = %d", high_count, low_count);		
	end


endmodule