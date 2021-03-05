/*
 * input.c
 *  This all of the input of the software
 *  Created on: Mar 3, 2021
 *  Lasted Edited On: Mar 3 2021
 *      Author: August Byrne
 */
#include "app_cfg.h"
#include "os.h"
#include "MCUType.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "LcdLayered.h"
#include "uCOSKey.h"

/*****************************************************************************************
* Variable Defines Here
*****************************************************************************************/
#define KEY_LEN 5
typedef enum {SINEWAVE_MODE, PULSETRAIN_MODE, WAITING_MODE} STATE;

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
static void inputInit(void);
static void inKeyTask(void *p_arg);
static void inLevelTask(void *p_arg);

/*****************************************************************************************
 * Mutex & Semaphores
*****************************************************************************************/
typedef struct {
	INT8C buffer[KEY_LEN-1];
	OS_SEM flag;
} KEY_BUFFER;
static KEY_BUFFER inKeyBuffer;

typedef struct{
    INT8U buffer;
    OS_SEM flag;
}TSI_BUFFER;
static TSI_BUFFER inLevBuffer;

typedef struct{
    STATE buffer;
    OS_SEM flag;
}CTRL_STATE;
static CTRL_STATE CtrlState;

/*****************************************************************************************
* input()
*****************************************************************************************/

//inputInit – Executes all required initialization for the resources in input.c
void inputInit(void){
	OS_ERR os_err;

	OSSemCreate(&(CtrlState.flag),"Key Press Buffer",0,&os_err);
	CtrlState.buffer = WAITING_MODE;
	OSSemCreate(&(inKeyBuffer.flag),"Key Press Buffer",0,&os_err);
	inKeyBuffer.buffer = 0;
	OSSemCreate(&(inLevBuffer.flag),"Touch Sensor Buffer",0,&os_err);
	inLevBuffer.buffer = 0;

	OSTaskCreate(&InKeyTaskTCB,                  /* Create Task 1                    */
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

	OSTaskCreate(&InLevelTaskTCB,                  /* Create Task 1                    */
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

static void inKeyTask(void *p_arg){
	OS_ERR os_err;
	INT8U kchar = 0;
	INT8C tempBuffer = 0;
	(void)p_arg;

	while(1){
		DB3_TURN_OFF();
		kchar = KeyPend(0, &os_err);
		switch (kchar){
		case 'a':		//CtrlState semaphore to sine wave mode
			inKeyBuffer.buffer = 0;
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
			CtrlState.buffer = SINEWAVE_MODE;
			OSSemPost(&(CtrlState.flag),OS_OPT_POST_NONE,&os_err);
		break;
		case 'b':		//CtrlState semaphore to square wave mode
			inKeyBuffer.buffer = 0;
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
			CtrlState.buffer = PULSETRAIN_MODE;
			OSSemPost(&(CtrlState.flag),OS_OPT_POST_NONE,&os_err);
		break;
		case 'c':		//CtrlState semaphore to square wave mode
			inKeyBuffer.buffer = 0;
			OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
			CtrlState.buffer = WAITING_MODE;
			OSSemPost(&(CtrlState.flag),OS_OPT_POST_NONE,&os_err);
		break;
		case 'd':		//remove the last number from the freq semaphore
			if (inKeyBuffer.buffer != 0 && inKeyBuffer.buffer[KEY_LEN-1] != '#'){	//is not empty
				for (i = 0; i < KEY_LEN-1; i++){
					tempBuffer[i] = inKeyBuffer.buffer[i+1];
				}
				tempBuffer[KEY_LEN-1] = 0;
				inKeyBuffer.buffer = tempBuffer;
				OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
				tempBuffer = 0;
			}else{}
		break;
		case '*':		//add blank cases for keys you don't want to have any effect
		break;
		default:		//it is a number to add to the freq semaphore, or it is '#'
			if(inKeyBuffer.buffer[STR_LEN-1] == 0 &&  inKeyBuffer.buffer[KEY_LEN-1] != '#'){
				for (i = KEY_LEN-1; i > 0; i--){
					tempBuffer[i] = inKeyBuffer.buffer[i-1];
				}
				tempBuffer[0] = kchar;
				inKeyBuffer.buffer = tempBuffer;
				OSSemPost(&(inKeyBuffer.flag),OS_OPT_POST_NONE,&os_err);
				tempBuffer = 0;
			}else{}
		}
		DB3_TURN_ON();
	}
}

static void inLevelTask(void *p_arg){
	OS_ERR os_err;
	INT8U tsense = 0;
	(void)p_arg;

	while(1){
		//handles all TSI scanning
		tsense = TSIPend(0, &os_err);
		if ((tsense & (1<<BRD_PAD1_CH)) != 0){
			if (inLevBuffer.buffer > 0){
				inLevBuffer.buffer -=1;
				OSSemPost(&(inLevBuffer.flag),OS_OPT_POST_NONE,&os_err);
			}
		}else if ((tsense & (1<<BRD_PAD2_CH)) != 0){
			if (inLevBuffer.buffer < 20){
				inLevBuffer.buffer +=1;
				OSSemPost(&(inLevBuffer.flag),OS_OPT_POST_NONE,&os_err);
			}
		}else{}
	}
}









