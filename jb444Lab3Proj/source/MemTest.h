/*
 * MemTest.h
 *	Header file for MemTest which has a prototype of CalcChkSum
 *  Created on: Oct 19, 2020
 *      Author: August
 */

#ifndef MEMTEST_H_
#define MEMTEST_H_

/********************************************************************
* BIOGetStrg() - Calculates the checksum between two addresses.
*
* Return value: The calculated checksum from start to end address
*
* Arguments: *startaddr is a pointer to the starting address
*            *endaddr is a pointer to the starting address
********************************************************************/
INT16U CalcChkSum(INT8U *startaddr, INT8U *endaddr);

#endif /* MEMTEST_H_ */
