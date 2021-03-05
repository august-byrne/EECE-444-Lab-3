/*
 * input.h
 *
 *  Created on: Mar 4, 2021
 *      Author: August
 */

#ifndef INPUT_H_
#define INPUT_H_
#define KEY_LEN

typedef enum {SINEWAVE_MODE, PULSETRAIN_MODE, WAITING_MODE} STATE;

typedef struct {
	INT8C buffer[KEY_LEN-1];
	OS_SEM flag;
} KEY_BUFFER;

typedef struct{
    INT8U buffer;
    OS_SEM flag;
}TSI_BUFFER;

typedef struct{
    STATE buffer;
    OS_SEM flag;
}CTRL_STATE;

void inputInit(void);

#endif /* INPUT_H_ */
