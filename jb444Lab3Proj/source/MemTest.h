/*******************************************************************************
 * MemTest.h
 *
 * This header contains the CalcChkSum functionn and makes it public. That
 * function calculates the check sum found in a range of addresses.
 *
 *
 * Jacob Bindernagel, 10/19/2020
 *
 *****************************************************************************/

#ifndef MEMTEST_H_
#define MEMTEST_H_

INT16U CalcChkSum(INT8U *start_addr, INT8U *end_addr);



#endif /* MEMTEST_H_ */
