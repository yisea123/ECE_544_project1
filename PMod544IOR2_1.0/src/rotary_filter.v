// rotary_filter.v - Interface to the Rotary Encoder on the PmodENC
//
// Copyright Roy Kravitz, Portland State University 2014, 2015
// Converted from Ken Chapman's VHDL code
//
// Created By:	Roy Kravitz
// Date:		15-March-2014
// Version:		1.0
//
// Revision History:
// -----------------
//	15-Mar-2014		RK		Created this module from the one I've used for years in ECE 540 and 544
//	
// Description:
// ------------
// This module encodes the A and B quadrature input from the rotary encoder to two signals
// The "rotary_event" output  is pulsed when the rotary encoder knob is turned in either
// direction.  The "rotary_left" output indicates which direction the knob was turned.  A 1 on
// "rotary_left" indicates the knob was turned to the left, a 0 indicates the knob was turned
// to the right
//
// NOTE:  This module assumes that the input clock is running much faster than the knob can
// be turned.
//
//////////////////////////////////////////////////////////////////////////////////////

module rotary_filter (
	input			rotary_a,			// A input from S3E Rotary Encoder
					rotary_b,			// B input from S3E Rotary Encoder
	
	output reg		rotary_event,		// Asserted high when rotary encoder changes position
					rotary_left,		// Asserted high when rotary direction is to the left
				
	input			clk					// input clock
);

	// declare internal variables
	reg				rotary_a_int,		// synchronization flip flops
					rotary_b_int;
				
	reg				rotary_q1,			// state flip-flops 
					rotary_q2,
					delay_rotary_q1;
					
					
	// The rotary switch contacts are filtered using their offset (one-hot) style to  
	// clean them. Circuit concept by Peter Alfke.
	always @(posedge clk) begin
		// Synchronize inputs to clock domain using flip-flops in input/output blocks.
		rotary_a_int <= rotary_a;
		rotary_b_int <= rotary_b;
		
		case ({rotary_b_int, rotary_a_int})
			2'b00: 	begin
						rotary_q1 <= 0;         
						rotary_q2 <= rotary_q2;
				   	end
			2'b01: 	begin
						rotary_q1 <= rotary_q1;         
						rotary_q2 <= 0;
				   	end
 			2'b10: 	begin
						rotary_q1 <= rotary_q1;         
						rotary_q2 <= 1;
				   	end
 			2'b11: 	begin
						rotary_q1 <= 1;         
						rotary_q2 <= rotary_q2;
				   	end
		endcase
	end //always
	
	// The rising edges of 'rotary_q1' indicate that a rotation has occurred and the 
	// state of 'rotary_q2' at that time will indicate the direction. 
	always @(posedge clk) begin
		// catch the first edge
	    delay_rotary_q1 <= rotary_q1;
      	if (rotary_q1 && ~delay_rotary_q1) begin
			// rotary position has changed
        	rotary_event <= 1;
       		rotary_left <= rotary_q2;
       	end
       	else begin
		// rotary position has not changed
        	rotary_event <= 0;
        	rotary_left <= rotary_left;
      	end
     end //always

endmodule