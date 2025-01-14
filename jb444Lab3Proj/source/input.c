/*
 * input.c
 *  This is all of the input for the function generator of EECE444 Lab 3
 *  Created on: Mar 3, 2021
 *  Last Edited On: 3/16/2021
 *      Author: August Byrne
 *
 *  Edited by Jacob Bindernagel 3/14/2021
 *  - Added semaphore flags for the output tasks to pend on
 */
#include "app_cfg.h"
#include "os.h"
#include "MCUType.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "K65TWR_TSI.h"
#include "uCOSKey.h"
#include "input.h"

/*****************************************************************************************
* Variable Defines Here
*****************************************************************************************/
#define ASCII_0 48

typedef struct{
    INT8U buffers;
    OS_SEM flag;
}TSI_BUFFER;

typedef struct {
    INT8U buffer[KEY_LEN];
    OS_SEM flag;
    OS_SEM enter;
} KEY_BUFFER;

typedef struct{
    STATE buffer;
    OS_SEM flag;
    OS_SEM flag_square;
    OS_SEM flag_sine;
}CTRL_STATE;

/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
static OS_TCB InKeyTaskTCB;
static OS_TCB InLevelTaskTCB;

/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
static CPU_STK InKeyTaskStartStk[APP_CFG_INKEY_STK_SIZE];
static CPU_STK InLevelTaskStartStk[APP_CFG_INLEVEL_STK_SIZE];

/*****************************************************************************************
* Task Function Prototypes.
*   - Private if in the same module as startup task. Otherwise public.
*****************************************************************************************/
static void inKeyTask(void *p_arg);
static void inLevelTask(void *p_arg);

/*****************************************************************************************
 * Mutex & Semaphores
*****************************************************************************************/
static KEY_BUFFER inKeyBuffer;
static TSI_BUFFER inLevBuffer;
static CTRL_STATE CtrlState;

/*****************************************************************************************
* input()
*****************************************************************************************/
//inputInit – Executes all required initialization for the resources in input.c
void inputInit(void){
	OS_ERR os_err;

	TSIInit();
	KeyInit();

	OSSemCreate(&(CtrlState.flag),"Key Press Buffer",0,&os_err);
	OSSemCreate(&(CtrlState.flag_square),"Key Press Buffer",0,&os_err);
	OSSemCreate(&(CtrlState.flag_sine),"Key Press Buffer",0,&os_err);
	CtrlState.buffer = WAITING_MODE;
	OSSemCreate(&(inKeyBuffer.flag),"Key Press Buffer",0,&os_err);
	OSSemCreate(&(inKeyBuffer.enter),"Key Press Enter",0,&os_err);
	for (int i = 0; i < KEY_LEN; ++i){
		inKeyBuffer.buffer[i] = 0;
	}
	OSSemCreate(&(inLevBuffer.flag),"Touch Sensor Buffer",0,&os_err);
	inLevBuffer.buffers = 0;

	OSTaskCreate(&InKeyTaskTCB,                  /* Create Key Task                    */
				"InKeyTask ",
				inKeyTask,
				(void *) 0,
				APP_CFG_INKEY_TASK_PRIO,
				&InKeyTaskStartStk[0],
				(APP_CFG_INKEY_STK_SIZE / 10u),
				APP_CFG_INKEY_STK_SIZE,
				0,
				0,
				(void *) 0,
				(OS_OPT_TASK_NONE),
				&os_err);

	OSTaskCreate(&InLevelTaskTCB,                  /* Create Level Task                    */
				"InLevelTask ",
				inLevelTask,
				(void *) 0,
				APP_CFG_INLEVEL_TASK_PRIO,
				&InLevelTaskStartStk[0],
				(APP_CFG_INLEVEL_STK_SIZE / 10u),
				APP_CFG_INLEVEL_STK_SIZE,
				0,
				0,
				(void *) 0,
				(OS_OPT_TASK_NONE),
				&os_err);

}

/******************************************************************************
 * inKeyTask
 *
 * Works with multiple semaphores and writes keyPresses to a buffer
 * Pends on keypress with KeyPend
 * August Byrne, 03/15/2021
 *****************************************************************************/
static void inKeyTask(void *p_arg){
	OS_ERR os_err;
	INT8U kchar = 0;
	INT16U values = 0;
	(void)p_arg;

	while(1){
		DB3_TURN_OFF();
		kchar = KeyPend(0, &os_err);
		DB3_TURN_ON();
		switch (kchar){
		case DC1:		//'A' changes CtrlState semaphore to sine wave mode
			for (int i = 0; i < KEY_LEN; i++){
				inKeyBuffer.buffer[i] = 0;
			}
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
			CtrlState.buffer = SINEWAVE_MODE;
			OSSemPost(&(CtrlState.flag),OS_OPT_POST_NONE,&os_err);
			OSSemPost(&(CtrlState.flag_sine),OS_OPT_POST_NONE,&os_err);
		break;
		case DC2:		//'B' changes CtrlState semaphore to square wave mode
			for (int i = 0; i < KEY_LEN; i++){
				inKeyBuffer.buffer[i] = 0;
			}
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
			CtrlState.buffer = PULSETRAIN_MODE;
			OSSemPost(&(CtrlState.flag),OS_OPT_POST_NONE,&os_err);
			OSSemPost(&(CtrlState.flag_square),OS_OPT_POST_NONE,&os_err);
		break;
		case DC3:		//'C' changes CtrlState semaphore to idle mode
			for (int i = 0; i < KEY_LEN; i++){
				inKeyBuffer.buffer[i] = 0;
			}
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
			CtrlState.buffer = WAITING_MODE;
			OSSemPost(&(CtrlState.flag),OS_OPT_POST_NONE,&os_err);
		break;
		case DC4:		//'D' removes the last number from the freq semaphore
			for (int i = 0; i < KEY_LEN-1; i++){
				inKeyBuffer.buffer[i] = inKeyBuffer.buffer[i+1];
			}
			inKeyBuffer.buffer[KEY_LEN-1] = 0;
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
		break;
		case '*':		//add blank cases for keys you don't want to have any effect
		break;
		case '#':		//enter has been pressed
			OSSemPost(&(inKeyBuffer.enter),OS_OPT_POST_NONE,&os_err);
			OSTimeDly(8, OS_OPT_TIME_PERIODIC, &os_err); // delay 8 ms so that the value can be recorded before it is wiped
			for (int i = 0; i < KEY_LEN; i++){
				inKeyBuffer.buffer[i] = 0;
			}
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
		break;
		default:		//it is a number to add to the freq semaphore
			if(inKeyBuffer.buffer[KEY_LEN-1] == 0){
				values = inKeyBuffer.buffer[4]*10000+inKeyBuffer.buffer[3]*1000+inKeyBuffer.buffer[2]*100+inKeyBuffer.buffer[1]*10+inKeyBuffer.buffer[0];
				if (values == 0 && kchar == ASCII_0){
					//do nothing for leading zeros case
				}else if (values <= 10000){
					for (int i = KEY_LEN-1; i > 0; i--){
						inKeyBuffer.buffer[i] = inKeyBuffer.buffer[i-1];
					}
					inKeyBuffer.buffer[0] = kchar;
				}else{
					inKeyBuffer.buffer[4] = ASCII_0+1;
					inKeyBuffer.buffer[3] = ASCII_0;
					inKeyBuffer.buffer[2] = ASCII_0;
					inKeyBuffer.buffer[1] = ASCII_0;
					inKeyBuffer.buffer[0] = ASCII_0;
				}
				OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
			}else{}
		}
	}
}

/******************************************************************************
 * inKeyTask
 *
 * Works with a semaphore and writes touch pad presses to a buffer
 * Pends on the touch sensor with TSIPend
 * August Byrne, 03/15/2021
 *****************************************************************************/
static void inLevelTask(void *p_arg){
	OS_ERR os_err;
	INT16U tsense;
	(void)p_arg;

	while(1){
		//handles all TSI scanning
		tsense = TSIPend(0, &os_err);
		if ((tsense & (1<<BRD_PAD2_CH)) != 0){
			if (inLevBuffer.buffers > 0){
				inLevBuffer.buffers--;
				OSSemPost(&(inLevBuffer.flag),OS_OPT_POST_NONE,&os_err);
			}else{}
		}else if ((tsense & (1<<BRD_PAD1_CH)) != 0){
			if (inLevBuffer.buffers < 20){
				inLevBuffer.buffers++;
				OSSemPost(&(inLevBuffer.flag),OS_OPT_POST_NONE,&os_err);
			}else{}
		}else{}
	}
}

INT8U* getInKeyPend(INT8U pendMode, INT16U tout, OS_ERR *os_err){
	if(pendMode == 0){
		OSSemPend(&(inKeyBuffer.enter),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
	}else if(pendMode == 1){
		OSSemPend(&(inKeyBuffer.flag),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
	}else{}
	return inKeyBuffer.buffer;
}

INT8U getInLevPend(INT16U tout, OS_ERR *os_err){
	OSSemPend(&(inLevBuffer.flag),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
	return inLevBuffer.buffers;
}

STATE getInStatePend(INT16U tout, OS_ERR *os_err){
	OSSemPend(&(CtrlState.flag),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
	return CtrlState.buffer;
}

STATE SinePend(INT16U tout, OS_ERR *os_err){
    OSSemPend(&(CtrlState.flag_sine),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
    return CtrlState.buffer;
}

STATE SquarePend(INT16U tout, OS_ERR *os_err){
    OSSemPend(&(CtrlState.flag_square),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
    return CtrlState.buffer;
}
