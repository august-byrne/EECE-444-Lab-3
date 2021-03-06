/*******************************************************************************
* INT16U CalcChkSum(INT8U *start_addr, INT8U *end_addr)
* Jacob Bindernagel, 10/19/2020
* EECE 344 Lab 2
* This function takes in two addresses and adds each byte found in within them
* and the addresses between them.
*
*10/28/2020 - Removed unnecessary header file "BasicIO.h"
*******************************************************************************/
#include "MCUType.h"
#include "MemTest.h"

INT16U CalcChkSum(INT8U *start_addr, INT8U *end_addr){

    INT8U *current_byte_ptr;
    INT16U chk_sum = 0;

    current_byte_ptr = start_addr;

    while(current_byte_ptr < end_addr){
        chk_sum = chk_sum + (INT16U) *current_byte_ptr;
        current_byte_ptr++;
    }

        //Adds the final address's byte while avoiding the terminal count bug.
        chk_sum = chk_sum + (INT16U) *current_byte_ptr;

    return chk_sum;
}
