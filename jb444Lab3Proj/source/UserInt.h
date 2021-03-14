/*******************************************************************************
 * UserInt.h
 *
 *
 * Rachel Givens 03/05/2021
 ******************************************************************************/

#ifndef USERINT_H_
#define USERINT_H_

#include "MCUType.h"
#include "os.h"
#include "input.h"

void UIInit(void);
INT16U UIFreqGet(void);
INT8U UILevGet(void);
STATE UIStateGet(void);

#endif /* USERINT_H_ */
