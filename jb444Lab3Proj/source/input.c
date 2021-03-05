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
//#define ENABLE_ON 1
//#define ENABLE_OFF 0
//#define RESET_ON 1
//#define RESET_OFF 0
//typedef enum {CTRL_COUNT,CTRL_WAIT,CTRL_CLEAR} CNTR_CTRL_STATE;
//#define MAX_COUNT  600000 /* max count of the counter (99:59:99) */
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
static void inputInit(void *p_arg);
static void inKeyTask(void *p_arg);
static void inLevelTask(void *p_arg);

/*****************************************************************************************
 * Mutex & Semaphores
*****************************************************************************************/
typedef struct {
	INT32U buffer;
	OS_SEM flag;
} KEY_BUFFER;
static KEY_BUFFER keyBuffer;

typedef struct{
    INT8U buffer;
    OS_SEM flag;
}TSI_BUFFER;
static TSI_BUFFER TSIBuffer;

static CTRL_STATE CtrlState;
OS_MUTEX CtrlStateKey;

/*****************************************************************************************
* input()
*****************************************************************************************/

//inputInit – Executes all required initialization for the resources in SWCounter.c
void inputInit(void){
	OS_ERR os_err;

	OSMutexCreate(&CtrlStateKey, "Control State Key", &os_err);

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
	CTRL_STATE current_state;
	(void)p_arg;

	while(1){
		DB3_TURN_OFF();
		kchar = KeyPend(0, &os_err);
		if (kchar == '*'){
			current_state = SWCounterGet();
			if (current_state == CTRL_CLEAR){
				SWCounterCntrlSet(1,0);
			}else if (current_state == CTRL_WAIT){
				SWCounterCntrlSet(0,1);
			}else if (current_state == CTRL_COUNT){
				SWCounterCntrlSet(0,0);
			}else{}
		}else if(kchar == '#'){
			OSMutexPend(&appTimerCntrKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
			LcdDispTime(LCD_ROW_2, LCD_COL_1, LCD_LAYER_LAP,appTimerCount[2],appTimerCount[1],appTimerCount[0]);
			OSMutexPost(&appTimerCntrKey, OS_OPT_POST_NONE, &os_err);
		}else{}
		DB3_TURN_ON();
	}
}

static void inLevelTask(void *p_arg){

	INT8U tsense = 0;
	(void)p_arg;

	while(1){
		//handles all TSI scanning
		tsense=TSIPend(0, &os_err);
		if (tsense == ){

		}else if (tsense == ){

		}else{}
		}
	}









