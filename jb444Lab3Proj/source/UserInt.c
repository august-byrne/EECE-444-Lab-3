/*******************************************************************************
* UserInt.c -
* Contains UIInit()
*
* Rachel Givens 03/04/2020
*******************************************************************************/
#include "UserInt.h"
#include "os.h"
#include "MCUType.h"
#include "app_cfg.h"
#include "K65TWR_GPIO.h"
#include "K65TWR_ClkCfg.h"
#include "MemTest.h"
#include "LcdLayered.h"
#include "uCOSKey.h"
#include "input.h"

static OS_TCB uiFreqTaskTCB;
static OS_TCB uiDispTaskTCB;
static OS_TCB uiVolTaskTCB;
static OS_TCB uiStateTaskTCB;

static CPU_STK uiFreqTaskStk[APP_CFG_UIF_TASK_STK_SIZE];
static CPU_STK uiDispTaskStk[APP_CFG_UID_TASK_STK_SIZE];
static CPU_STK uiVolTaskStk[APP_CFG_UIV_TASK_STK_SIZE];
static CPU_STK uiStateTaskStk[APP_CFG_UIS_TASK_STK_SIZE];

static void uiFreqTask(void *p_arg);
static void uiDispTask(void *p_arg);
static void uiVolTask(void *p_arg);
static void uiStateTask(void *p_arg);

OS_MUTEX VolumeKey;
OS_MUTEX FrequencyKey;


static STATE uiStateCntrl = WAITING_MODE;

static INT8U lev = 0;


static const INT8U DutyCycle[21] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100};

/*******************************************************************************
* UIInit Code
*
*
* Rachel Givens 03/05/2021
*******************************************************************************/
void UIInit(void){

    OS_ERR os_err;

    LcdInit();

    OSTaskCreate(&uiFreqTaskTCB,
                 "UIF Task",
                 uiFreqTask,
                 (void *)0,
                 APP_CFG_UIF_TASK_PRIO,
                 &uiFreqTaskStk[0],
                 APP_CFG_UIF_TASK_STK_SIZE/10,
                 APP_CFG_UIF_TASK_STK_SIZE,
                 0,
                 0,
                 (void*)0,
                 OS_OPT_TASK_NONE,
                 &os_err);

    OSTaskCreate(&uiDispTaskTCB,
                 "UID Task",
                 uiDispTask,
                 (void *)0,
                 APP_CFG_UID_TASK_PRIO,
                 &uiDispTaskStk[0],
                 APP_CFG_UID_TASK_STK_SIZE/10,
                 APP_CFG_UID_TASK_STK_SIZE,
                 0,
                 0,
                 (void*)0,
                 OS_OPT_TASK_NONE,
                 &os_err);

    OSTaskCreate(&uiVolTaskTCB,
                 "UIV Task",
                 uiVolTask,
                 (void *)0,
                 APP_CFG_UIV_TASK_PRIO,
                 &uiVolTaskStk[0],
                 APP_CFG_UIV_TASK_STK_SIZE/10,
                 APP_CFG_UIV_TASK_STK_SIZE,
                 0,
                 0,
                 (void*)0,
                 OS_OPT_TASK_NONE,
                 &os_err);

    OSTaskCreate(&uiStateTaskTCB,
                 "UIS Task",
                 uiStateTask,
                 (void *)0,
                 APP_CFG_UIS_TASK_PRIO,
                 &uiStateTaskStk[0],
                 APP_CFG_UIS_TASK_STK_SIZE/10,
                 APP_CFG_UIS_TASK_STK_SIZE,
                 0,
                 0,
                 (void*)0,
                 OS_OPT_TASK_NONE,
                 &os_err);

    OSMutexCreate(&FrequencyKey, "Frequency", &os_err);
    OSMutexCreate(&VolumeKey, "Volume", &os_err);

}

/******************************************************************************
 * uiFreqTask Code
 *
 * Writes the Current Entry into the LCD
 * Pends on KeyTask
 *
 * Rachel Givens, 03/05/2021
 *****************************************************************************/

void uiFreqTask(void *p_arg){

    OS_ERR os_err;

    (void)p_arg;

    while(1){
        DB3_TURN_OFF();

        OSSemPend(&(inKeyBuffer.flag), 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
        LcdDispClear(APP_LAYER_TYPE);
        LcdDispClear(APP_LAYER_CHKSUM);
		for (int i = 0; i < KEY_LEN; i++){
			if (inKeyBuffer.buffer[i] != 0){
				LcdDispChar(LCD_ROW_2,5-i,APP_LAYER_FREQ,inKeyBuffer.buffer[i]);
			}else{
				LcdDispChar(LCD_ROW_2,5-i,APP_LAYER_FREQ,' ');
			}
		}
        DB3_TURN_ON();
    }
}

/******************************************************************************
 * uiDispTask Code
 *
 * Writes Current Frequency to the left side top row of in Hz
 * Pends on KeyTask's #
 * Rachel Givens, 03/05/2021
 *****************************************************************************/

void uiDispTask(void *p_arg){
    OS_ERR os_err;
    INT16U frequency;

    (void)p_arg;

    while(1){
        DB4_TURN_OFF();

        OSSemPend(&(inKeyBuffer.enter), 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
		for (int i = 0; i < KEY_LEN; i++){
			if (inKeyBuffer.buffer[i] != 0){
				LcdDispChar(LCD_ROW_1,5-i,APP_LAYER_FREQ,inKeyBuffer.buffer[i]);
			}else{
				LcdDispChar(LCD_ROW_1,5-i,APP_LAYER_FREQ,' ');
			}
		}
        LcdDispString(LCD_ROW_1,LCD_COL_7,APP_LAYER_FREQ,"Hz");

        OSMutexPend(&FrequencyKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
        frequency = inKeyBuffer.buffer[4]*10000 + inKeyBuffer.buffer[3]*1000 + inKeyBuffer.buffer[2]*100 + inKeyBuffer.buffer[1]*10 + inKeyBuffer.buffer[0];
        OSMutexPost(&FrequencyKey, OS_OPT_POST_NONE, &os_err);

        DB4_TURN_ON();

    }

}

/******************************************************************************
 * uiVolTask Code
 *
 * Writes Volume to the LCD, top row right side
 * Pends on inLev Task
 * Rachel Givens, 03/05/2021
 *****************************************************************************/
void uiVolTask(void *p_arg){
    OS_ERR os_err;

    (void)p_arg;

    while(1){
        DB5_TURN_OFF();

        OSMutexPend(&VolumeKey,0, OS_OPT_POST_NONE,(CPU_TS*) 0, &os_err);
        lev = inLevBuffer.buffer;
        OSMutexPost(&VolumeKey, OS_OPT_POST_NONE, &os_err);

        LcdDispClear(APP_LAYER_VOL);
        switch(uiStateCntrl){
        case SINEWAVE_MODE:
        	LcdDispDecWord(LCD_ROW_1, LCD_COL_15,APP_LAYER_VOL,lev,2,LCD_DEC_MODE_AR);
        break;
        case PULSETRAIN_MODE:
            if((lev <= 19) && (lev >= 2)){
            	LcdDispDecWord(LCD_ROW_1,LCD_COL_14,APP_LAYER_VOL,DutyCycle[lev],2,LCD_DEC_MODE_AR);
            }else if(lev <= 1){
            	LcdDispDecWord(LCD_ROW_1,LCD_COL_15,APP_LAYER_VOL,DutyCycle[lev],1,LCD_DEC_MODE_AR);
            }else{
            	LcdDispDecWord(LCD_ROW_1,LCD_COL_13,APP_LAYER_VOL,DutyCycle[lev],3,LCD_DEC_MODE_AR);
            }
            break;
        default:
        break;
        }

        }

        DB5_TURN_ON();


    }



/*******************************************************************************
* StatePend Code
*
*
* Rachel Givens 03/05/2021
*******************************************************************************/
void uiStateTask(void *p_arg){
    OS_ERR os_err;

    (void)p_arg;

    while(1){
		OSTimeDly(20,OS_OPT_TIME_PERIODIC,&os_err);     /* Task period = 20ms   */

		OSMutexPend(&CtrlStateKey,0,OS_OPT_PEND_BLOCKING,(CPU_TS *)0,&os_err);
		uiStateCntrl = CtrlState;
		OSMutexPost(&CtrlStateKey,OS_OPT_POST_NONE,&os_err);

        LcdDispClear(APP_LAYER_VOL);
        LcdDispClear(APP_LAYER_UNIT);
        if(uiStateCntrl == PULSETRAIN_MODE){
            if((lev <= 19) && (lev >= 2)){
            	LcdDispDecWord(LCD_ROW_1,LCD_COL_14,APP_LAYER_VOL,DutyCycle[lev],2,LCD_DEC_MODE_AR);
            }else if(lev <= 1){
            	LcdDispDecWord(LCD_ROW_1,LCD_COL_15,APP_LAYER_VOL,DutyCycle[lev],1,LCD_DEC_MODE_AR);
            }else{
            	LcdDispDecWord(LCD_ROW_1,LCD_COL_13,APP_LAYER_VOL,DutyCycle[lev],3,LCD_DEC_MODE_AR);
            }
            LcdDispString(LCD_ROW_1,LCD_COL_16,APP_LAYER_UNIT,"%");
        }else if(uiStateCntrl == SINEWAVE_MODE){
        	LcdDispDecWord(LCD_ROW_1, LCD_COL_15,APP_LAYER_VOL,lev,2,LCD_DEC_MODE_AR);
            LcdDispString(LCD_ROW_1,LCD_COL_16,APP_LAYER_UNIT," ");
        }else{
            // do nothing
        }
    }

}
