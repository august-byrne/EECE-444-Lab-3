/*
 * MemTest.c
 *	This module takes in a start and end address, and calculates
 *	the checksum of all of the values between the two supplied addresses.
 *	It also accounts for the terminal count bug.
 *  Created on: Oct 18, 2020
 *  Last Edited on: 11/28/20
 *      Author: August Byrne
 */

#include "MCUType.h"               /* Include header files                    */
#include "MemTest.h"

INT16U CalcChkSum(INT8U *startaddr, INT8U *endaddr){
	INT16U check_sum = 0;
	while (startaddr < endaddr){
		check_sum += (INT16U) *startaddr;	//add all of the data in the addresses listed
		startaddr ++;	//make the pointer point to the next address
	}
	check_sum += (INT16U) *startaddr;	//add the last index to the checksum, navigating around the terminal count bug
	return check_sum;
}
