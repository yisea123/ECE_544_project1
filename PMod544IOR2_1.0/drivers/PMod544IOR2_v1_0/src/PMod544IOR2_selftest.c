
/***************************** Include Files *******************************/
#include "PMod544IOR2.h"
#include "xparameters.h"
#include "stdio.h"
#include "xil_io.h"

/************************** Constant Definitions ***************************/
#define READ_WRITE_MUL_FACTOR 0x10

/************************** Function Definitions ***************************/
/**
 *
 * Run a self-test on the driver/device. Note this may be a destructive test if
 * resets of the device are performed.
 *
 * If the hardware system is not built correctly, this function may never
 * return to the caller.
 *
 * @param   baseaddr_p is the base address of the PMOD544IOR2instance to be worked on.
 *
 * @return
 *
 *    - XST_SUCCESS   if all self-test code passed
 *    - XST_FAILURE   if any self-test code failed
 *
 * @note    Caching must be turned off for this function to work.
 * @note    Self test may fail if data memory and device are not on the same bus.
 *
 */
XStatus PMOD544IO_Reg_SelfTest(u32 baseaddr)
{
	int write_loop_index;
	int read_loop_index;
	int Index;

	xil_printf("******************************\n\r");
	xil_printf("* PMod544IO Peripheral Self Test\n\r");
	xil_printf("******************************\n\n\r");

	/*
	 * Write to user logic slave module register(s) that aren't used and read back
	 */
	xil_printf("User logic slave module test...\n\r");

	for (write_loop_index = 5 ; write_loop_index < 8; write_loop_index++)
	  PMOD544IO_mWriteReg (baseaddr, write_loop_index*4, (write_loop_index+1)*READ_WRITE_MUL_FACTOR);
	for (read_loop_index = 5 ; read_loop_index < 8; read_loop_index++)
	{
		// slave register 0 (ROTLCD_STS) and slave register 2 (ROT_CNT) are read-only register and should be skipped
		if ((read_loop_index == 0) || (read_loop_index == 2)) 
		{
			// register 0 (BTNSW_IN) is a read-only register skip it
			continue;
		}
		else
		{
			if ( PMOD544IO_mReadReg (baseaddr, read_loop_index*4) != (read_loop_index+1)*READ_WRITE_MUL_FACTOR) {
				xil_printf ("Error reading register value at address %x\n", (int)baseaddr + read_loop_index*4);
				return XST_FAILURE;
			}
		}
	 
	  }

	xil_printf("   - slave register write/read passed\n\n\r");

	return XST_SUCCESS;
}
