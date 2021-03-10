#ifndef K65TWR_TSI_H_
#define K65TWR_TSI_H_

#define BRD_PAD1_CH  12U
#define BRD_PAD2_CH  11U

void TSIInit(void);             /* Touch Sensor Initialization    */
INT16U TSIPend(INT16U tout, OS_ERR *os_err); /* Pend on TSI press*/
/* tout - semaphore timeout           */
/* *err - destination of err code     */
/* Error codes are identical to a semaphore */

#endif
