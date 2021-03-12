/*
 * input.h
 *
 *  Created on: Mar 5, 2021
 *      Author: August
 */

#ifndef INPUT_H_
#define INPUT_H_
#define KEY_LEN 5

typedef enum {SINEWAVE_MODE, PULSETRAIN_MODE, WAITING_MODE} STATE;

typedef struct {
	INT8U buffer[KEY_LEN];
	OS_SEM flag;
	OS_SEM enter;
} KEY_BUFFER;

typedef struct{
    INT8U buffer;
    OS_SEM flag;
}TSI_BUFFER;

extern KEY_BUFFER inKeyBuffer;
extern TSI_BUFFER inLevBuffer;
extern STATE CtrlState;
extern OS_MUTEX CtrlStateKey;

void inputInit(void);

#endif /* INPUT_H_ */
