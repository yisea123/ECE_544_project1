// hw_detect.v --> hardwarwe logic used to detect PWM pulse-width
//
//
// Author:	Rehan Iqbal
// Organization: Portland State University
//
// Description:
// ------------
// Give a longer, paragraph description here.
//
////////////////////////////////////////////////////////////////////////////////////////////////

module hw_detect #(

	/******************************************************************/
	/* Parameter declarations						                  */
	/******************************************************************/

	// Define some timing parameters

	parameter integer 	CLK_FREQUENCY_HZ = 100000000)

	/******************************************************************/
	/* Port declarations							                  */
	/******************************************************************/

	(				
	input 					clock,			// 100MHz system clock
	input 			 		reset,			// active-high reset signal from Nexys4
	input 					pwm,			// PWM signal from AXI Timer in EMBSYS

	output reg	[31:0]		high_count,		// how long PWM was 'high' --> GPIO input on Microblaze
	output reg	[31:0]		low_count);		// how long PWM was 'low' --> GPIO input on Microblaze

	/******************************************************************/
	/* Local parameters and values		                  	  		  */
	/******************************************************************/

	reg			[31:0]		count;			// 32-bit counter used for high/low count intervals
	reg 					prev_pwm; 		// previous state of PWM; used to detect transitions

	/******************************************************************/
	/* Obtain the counts for high & low intervals	                  */
	/******************************************************************/

	always@(posedge clock) begin

		if (reset) begin					// check for synchronous reset

			count <= 32'b0;					// clear the counter
			high_count <= 32'b0;			// clear the 'high' register
			low_count <= 32'b0;				// clear the 'low' register
			prev_pwm <= 1'b0;				// clear the previous state

		end

		else if (pwm == 1'b1) begin 		// check if PWM is currently high

			if (prev_pwm != pwm) begin 		// if so, check whether there was a low-to-high transition
				count <= 32'b0; 			// clear the counter
				low_count <= count;			// store the 'low' count
				prev_pwm <= 1'b1;			// update the previous state to 'high'
			end

			else begin
				count <= count + 1'b1;		// otherwise, just increment count		
			end

		end

		else if (pwm == 1'b0) begin 		// check if PWM is currently low

			if (prev_pwm != pwm) begin 		// if so, check whether there was a high-to-low transition
				count <= 32'b0; 			// clear the counter
				high_count <= count; 		// store the 'high' count
				prev_pwm <= pwm; 			// update the previous state to 'low'
			end

			else begin
				count <= count + 1'b1; 		// otherwise, just increment count
			end

		end

	end

endmodule