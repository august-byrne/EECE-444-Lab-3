/*******************************************************************************
* UserInt.c -
* Contains UIInit()
*
* Rachel Givens 03/04/2020
*******************************************************************************/
#include "app_cfg.h"
#include "UserInt.h"
#include "os.h"
#include "MCUType.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "MemTest.h"
#include "LcdLayered.h"
#include "uCOSKey.h"
#include "input.h"

#define ASCII_SHIFT 48
#define MAX_DIGITS 5

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

INT16U UIFreqGet(void);
INT8U UILevGet(void);
STATE UIStateGet(void);

static STATE StateCntrl = WAITING_MODE;
static INT8U Lev = 0;
static INT16U Frequency = 0;

OS_MUTEX FrequencyKey;
OS_MUTEX VolumeKey;
OS_MUTEX StateKey;

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
    INT8U inKeyBufferFreq[KEY_LEN];

    (void)p_arg;

    while(1){
        DB3_TURN_OFF();
        inKeyBufferFreq = getInKeyPend(1, 0, &os_err);
        LcdDispClear(APP_LAYER_TYPE);
        LcdDispClear(APP_LAYER_CHKSUM);
        for (int i = 0; i < KEY_LEN; i++){
            if (inKeyBufferFreq[i] != 0){
                LcdDispChar(LCD_ROW_2,5-i,APP_LAYER_FREQ,inKeyBufferFreq[i]);
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
 *
 * Edited by Jacob Bindernagel on 3/13/2021
 * Converts ASCII to decimal value.
 *
 *****************************************************************************/

void uiDispTask(void *p_arg){
    OS_ERR os_err;
    INT8U key_index = 0;
    INT8U freq_comps[MAX_DIGITS];
    INT8U inKeyBuffer[KEY_LEN];

    (void)p_arg;

    while(1){
        DB4_TURN_OFF();
        inKeyBuffer =  getInKeyPend(0, 0, &os_err);
        for (int i = 0; i < KEY_LEN; i++){
            if (inKeyBuffer[i] != 0){
                LcdDispChar(LCD_ROW_1,5-i,APP_LAYER_FREQ,inKeyBuffer[i]);
            }else{
                LcdDispChar(LCD_ROW_1,5-i,APP_LAYER_FREQ,' ');
            }
        }
        LcdDispString(LCD_ROW_1,LCD_COL_7,APP_LAYER_FREQ,"Hz");
        OSMutexPend(&FrequencyKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
        //Converts each char into it's true value
        while(key_index <= MAX_DIGITS){
            if(inKeyBuffer[key_index] != 0){
                freq_comps[key_index] = inKeyBuffer[key_index] - ASCII_SHIFT;
            }
            else{
                freq_comps[key_index] = 0;
            }
            key_index++;
        }
        key_index = 0;
        //Sums the entire thing.
        Frequency = freq_comps[4]*10000 + freq_comps[3]*1000 + freq_comps[2]*100 + freq_comps[1]*10 + freq_comps[0];
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
    INT8U inLevel;
    (void)p_arg;

    while(1){
        DB5_TURN_OFF();
        inLevel = getInLevPend(0, &os_err);
        LcdDispClear(APP_LAYER_VOL);
        switch(uiStateCntrl){
        case SINEWAVE_MODE:
            LcdDispDecWord(LCD_ROW_1, LCD_COL_15,APP_LAYER_VOL,inLevel,2,LCD_DEC_MODE_AR);
        break;
        case PULSETRAIN_MODE:
            if((inLevel <= 19) && (inLevel >= 2)){
                LcdDispDecWord(LCD_ROW_1,LCD_COL_14,APP_LAYER_VOL,DutyCycle[inLevel],2,LCD_DEC_MODE_AR);
            }else if(inLevel <= 1){
                LcdDispDecWord(LCD_ROW_1,LCD_COL_15,APP_LAYER_VOL,DutyCycle[inLevel],1,LCD_DEC_MODE_AR);
            }else{
                LcdDispDecWord(LCD_ROW_1,LCD_COL_13,APP_LAYER_VOL,DutyCycle[inLevel],3,LCD_DEC_MODE_AR);
            }
            break;
        default:
        break;
        }
        OSMutexPend(&VolumeKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
        Lev = inLevel;
        OSMutexPost(&VolumeKey, OS_OPT_POST_NONE, &os_err);
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
    STATE uiStateCntrl;

    (void)p_arg;

    while(1){
        uiStateCntrl = getInStatePend(0, &os_err);
        LcdDispClear(APP_LAYER_VOL);
        LcdDispClear(APP_LAYER_UNIT);
        if(uiStateCntrl == PULSETRAIN_MODE){
            if((Lev <= 19) && (Lev >= 2)){
                LcdDispDecWord(LCD_ROW_1,LCD_COL_14,APP_LAYER_VOL,DutyCycle[Lev],2,LCD_DEC_MODE_AR);
            }else if(Lev <= 1){
                LcdDispDecWord(LCD_ROW_1,LCD_COL_15,APP_LAYER_VOL,DutyCycle[Lev],1,LCD_DEC_MODE_AR);
            }else{
                LcdDispDecWord(LCD_ROW_1,LCD_COL_13,APP_LAYER_VOL,DutyCycle[Lev],3,LCD_DEC_MODE_AR);
            }
            LcdDispString(LCD_ROW_1,LCD_COL_16,APP_LAYER_UNIT,"%");
        }else if(uiStateCntrl == SINEWAVE_MODE){
            LcdDispDecWord(LCD_ROW_1, LCD_COL_15,APP_LAYER_VOL,Lev,2,LCD_DEC_MODE_AR);
            LcdDispString(LCD_ROW_1,LCD_COL_16,APP_LAYER_UNIT," ");
        }else{
            // do nothing
        }
        OSMutexPend(&StateKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
        StateCntrl = uiStateCntrl;
        OSMutexPost(&StateKey, OS_OPT_POST_NONE, &os_err);
    }

}

INT16U UIFreqGet(void){
    INT16U Freq;
    OS_ERR os_err;
    OSMutexPend(&FrequencyKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
    Freq = Frequency;
    OSMutexPost(&FrequencyKey, OS_OPT_POST_NONE, &os_err);
    return Freq;
}

INT8U UILevGet(void){
    INT8U Level;
    OS_ERR os_err;
    OSMutexPend(&VolumeKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
    Level = Lev;
    OSMutexPost(&VolumeKey, OS_OPT_POST_NONE, &os_err);
    return Level;
}

STATE UIStateGet(void){
    STATE State;
    OS_ERR os_err;
    OSMutexPend(&StateKey, 0, OS_OPT_PEND_BLOCKING, (void *)0, &os_err);
    State = StateCntrl;
    OSMutexPost(&StateKey, OS_OPT_POST_NONE, &os_err);
    return State;
}

