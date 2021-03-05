/*******************************************************************************
 * UserInt.h
 *
 *
 * Rachel Givens 03/05/2021
 ******************************************************************************/

#ifndef USERINT_H_
#define USERINT_H_

<<<<<<< Upstream, based on origin/master
void UIInit(void);
=======

#define R_ON 1
#define R_OFF 0
#define EN_ON 1
#define EN_OFF 0

void UIInit(void);

INT32U SWCountPend(INT16U tout, OS_ERR *os_err);

void SWCounterCntrlSet(INT8U enable, INT8U reset);
>>>>>>> d909694 Tasks and Files Created

#endif /* USERINT_H_ */
