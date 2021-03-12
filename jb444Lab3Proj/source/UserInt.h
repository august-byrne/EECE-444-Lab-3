/*******************************************************************************
 * UserInt.h
 *
 *
 * Rachel Givens 03/05/2021
 ******************************************************************************/

#ifndef USERINT_H_
#define USERINT_H_

#include "os.h"

extern OS_MUTEX VolumeKey;
extern OS_MUTEX FrequencyKey;

void UIInit(void);
#endif /* USERINT_H_ */
