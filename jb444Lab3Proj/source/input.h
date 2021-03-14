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

typedef struct{
    STATE buffer;
    OS_SEM flag;
    OS_SEM flag_square;
    OS_SEM flag_sine;
}CTRL_STATE;

extern KEY_BUFFER inKeyBuffer;
extern TSI_BUFFER inLevBuffer;
extern CTRL_STATE CtrlState;

void inputInit(void);
INT8U* getInKeyPend(INT8U pendMode, INT16U tout, OS_ERR *os_err);
INT8U getInLevPend(INT16U tout, OS_ERR *os_err);
STATE getInStatePend(INT16U tout, OS_ERR *os_err);

#endif /* INPUT_H_ */
