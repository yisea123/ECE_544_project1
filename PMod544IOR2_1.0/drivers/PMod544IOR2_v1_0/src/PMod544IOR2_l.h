/**
*
* @file PMod544IOR2_l.h
*
* @author Roy Kravitz (roy.kravitz@pdx.edu)
* @copyright Portland State University, 2014, 2015
*
* This header file contains identifiers and low level driver functions for the PMod544IO
* custom peripheral.  The peripheral provides access to the PMOD's required for ECE544.  These
*    PMODCLP - A 2 line by 16 character LCD with an 8-bit parallel interface
*    PMODENC - A rotary encoder with a pushbutton and slide switch. 
*
* The PMODCLP and PMODENC are both controlled by Picoblaze-based firmware in the peripheral. 
* The rotary encoder pushbutton and slide switch are debounced in hardware with an instance 
* of debounce.v (also used in the Nexys4IO custom peripheral). 
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a	rhk	12/20/14	First release of driver
* </pre>
*
******************************************************************************/
#ifndef PMOD544IOR2_L_H
#define PMOD544IOR2_L_H


/************************** Constant Definitions *****************************/

/** @name Registers
 *
 * Register offsets for this device.
 * @{
 */

#define PMOD544IO_ROTLCD_STS_OFFSET 0
#define PMOD544IO_ROT_CNTRL_OFFSET 4
#define PMOD544IO_ROT_COUNT_OFFSET 8
#define PMOD544IO_LCD_CMD_OFFSET 12
#define PMOD544IO_LCD_DATA_OFFSET 16
#define PMOD544IO_RSVD00_OFFSET 20
#define PMOD544IO_RSVD01_OFFSET 24
#define PMOD544IO_RSVD02_OFFSET 28

/* @} */

/**************************** Type Definitions *****************************/
/**
 *
 * Write a value to a PMOD544IO register. A 32 bit write is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is written.
 *
 * @param   BaseAddress is the base address of the PMOD544IOdevice.
 * @param   RegOffset is the register offset from the base to write to.
 * @param   Data is the data written to the register.
 *
 * @return  None.
 *
 * @note
 * C-style signature:
 * 	void PMOD544IO_mWriteReg(u32 BaseAddress, unsigned RegOffset, u32 Data)
 *
 */
#define PMOD544IO_mWriteReg(BaseAddress, RegOffset, Data) \
  	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

/**
 *
 * Read a value from a PMOD544IO register. A 32 bit read is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is read from the register. The most significant data
 * will be read as 0.
 *
 * @param   BaseAddress is the base address of the PMOD544IO device.
 * @param   RegOffset is the register offset from the base to write to.
 *
 * @return  Data is the data from the register.
 *
 * @note
 * C-style signature:
 * 	u32 PMOD544IO_mReadReg(u32 BaseAddress, unsigned RegOffset)
 *
 */
#define PMOD544IO_mReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))

/************************** Function Prototypes ****************************/
/**
 *
 * Run a self-test on the driver/device. Note this may be a destructive test if
 * resets of the device are performed.
 *
 * If the hardware system is not built correctly, this function may never
 * return to the caller.
 *
 * @param   baseaddr is the base address of the PMOD544IO instance to be worked on.
 *
 * @return
 *
 *    - XST_SUCCESS   if all self-test code passed
 *    - XST_FAILURE   if any self-test code failed
 *
 * @note    Caching must be turned off for this function to work.
 * @note    Self test may fail if data memory and device are not on the same bus.
 * @note	This test assume the existence of a Serial port in the system (used for xil_printf)
 *
 */
XStatus PMOD544IO_Reg_SelfTest(u32 baseaddr);

#endif // PMOD544IOR2_L_H
