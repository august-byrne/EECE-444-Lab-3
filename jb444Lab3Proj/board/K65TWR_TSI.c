/* TSI low level module based on K65 Tower board low-power demo.
 * Todd Morton, 11/18/2014
 * Todd Morton, 11/19/2018 MCUXpresso version
 * Todd Morton, 11/17/2020 MCUX11.2 version
 * August Byrne, 3/4/2021 uCOS version with TSIPend
 */

#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "K65TWR_TSI.h"
#include "k65TWR_GPIO.h"

typedef enum {PROC1START2, PROC2START1} TSI_TASK_STATE_T;
typedef struct{
	INT16U baseline;
	INT16U offset;
	INT16U threshold;
}TOUCH_LEVEL_T;

typedef struct{
	INT16U buffer[2];
	OS_SEM flag;
}TSI_BUFFER;

#define MAX_NUM_ELECTRODES 16U

#define E1_TOUCH_OFFSET  0x0400U    // Touch offset from baseline
#define E2_TOUCH_OFFSET  0x0400U    // Determined experimentally
#define TSI0_ENABLE()    TSI0->GENCS |= TSI_GENCS_TSIEN_MASK
#define TSI0_DISABLE()   TSI0->GENCS &= ~TSI_GENCS_TSIEN_MASK

#define PAD_ONE 0
#define PAD_TWO 1

INT16U TSIPend(INT16U tout, OS_ERR *os_err);
static void TSITask(void *p_arg);
static TOUCH_LEVEL_T tsiSensorLevels[MAX_NUM_ELECTRODES];
static void tsiStartScan(INT8U channel);
static void tsiProcScan(INT8U channel);
static void TSIChCalibration(INT8U channel);
static TSI_BUFFER tsiBuffer;
static INT16U tsiSensorFlags = 0;
static INT8U padVal = 0;

/**********************************************************************************
* Allocate task control blocks
**********************************************************************************/
static OS_TCB tsiTaskTCB;
/*************************************************************************
* Allocate task stack space.
*************************************************************************/
static CPU_STK tsiTaskStk[APP_CFG_TSI_TASK_STK_SIZE];

/********************************************************************************
 * K65TWR_TSI0Init: Initializes TSI0 module
 * Notes:
 *
 ********************************************************************************/
void TSIInit(void){

	OS_ERR os_err;

	SIM->SCGC5 |= SIM_SCGC5_TSI(1);         //Turn on clock to TSI module
	SIM->SCGC5 |= SIM_SCGC5_PORTB(1);

	PORTB->PCR[18]=PORT_PCR_MUX(0);         //Set electrode pins to ALT0
	PORTB->PCR[19]=PORT_PCR_MUX(0);
	tsiSensorLevels[BRD_PAD1_CH].offset = E1_TOUCH_OFFSET;
	tsiSensorLevels[BRD_PAD2_CH].offset = E2_TOUCH_OFFSET;

	//16 consecutive scans, Prescale divide by 32, software trigger
	//16uA ext. charge current, 16uA Ref. charge current, .592V dV
	TSI0->GENCS = ((TSI_GENCS_EXTCHRG(5))|
				   (TSI_GENCS_REFCHRG(5))|
				   (TSI_GENCS_DVOLT(1))|
				   (TSI_GENCS_PS(5))|
				   (TSI_GENCS_NSCN(15)));

	TSI0_ENABLE();
	TSIChCalibration(BRD_PAD1_CH);
	TSIChCalibration(BRD_PAD2_CH);
	tsiBuffer.buffer[PAD_ONE] = 0x0000;           /* Init tsiBuffer      */
	tsiBuffer.buffer[PAD_TWO] = 0x0000;           /* Init tsiBuffer      */
	OSSemCreate(&(tsiBuffer.flag),"Tsi Semaphore",0,&os_err);
	//Create the key task
	OSTaskCreate((OS_TCB     *)&tsiTaskTCB,
				(CPU_CHAR   *)"uCOS tsi Task ",
				(OS_TASK_PTR ) TSITask,
				(void       *) 0,
				(OS_PRIO     ) APP_CFG_TSI_TASK_PRIO,
				(CPU_STK    *)&tsiTaskStk[0],
				(CPU_STK     )(APP_CFG_TSI_TASK_STK_SIZE / 10u),
				(CPU_STK_SIZE) APP_CFG_TSI_TASK_STK_SIZE,
				(OS_MSG_QTY  ) 0,
				(OS_TICK     ) 0,
				(void       *) 0,
				(OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
				(OS_ERR     *)&os_err);
	while(os_err != OS_ERR_NONE){           /* Error Trap                        */
	}
}

/********************************************************************************
 *   TSICalibration: Calibration to find non-touch baseline for a channel
 *                   channel - the channel to calibrate, range 0-15
 *                   Note - the sensor must not be pressed when this is executed.
 ********************************************************************************/
static void TSIChCalibration(INT8U channel){
		tsiStartScan(channel);
		while((TSI0->GENCS & TSI_GENCS_EOSF_MASK) == 0){} //wait for scan to finish
		TSI0->GENCS |= TSI_GENCS_EOSF(1);    //Clear flag
		tsiSensorLevels[channel].baseline = (INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK);
		tsiSensorLevels[channel].threshold = tsiSensorLevels[channel].baseline +
											 tsiSensorLevels[channel].offset;
}

/********************************************************************************
 *   TSITask: Cooperative task for timeslice scheduler
 *            Processes and starts alternate sensors each time through to avoid
 *            blocking.
 *            In order to not block the task period should be > 5ms.
 *            To not miss a press, the task period should be < ~25ms.
  ********************************************************************************/
void TSITask(void *p_arg){

	OS_ERR os_err;

	static TSI_TASK_STATE_T tsiTaskState = PROC1START2;
	(void)p_arg;
	while(1){
		DB2_TURN_OFF();                             /* Turn off debug bit while waiting */
		OSTimeDly(8,OS_OPT_TIME_PERIODIC,&os_err);     /* Task period = 8ms   */
		DB2_TURN_ON();                          /* Turn on debug bit while ready/running*/
		tsiStartScan(BRD_PAD1_CH);
		switch(tsiTaskState){
		case PROC1START2:
			tsiProcScan(BRD_PAD1_CH);
			if(tsiBuffer.buffer[PAD_ONE] != tsiSensorFlags){
				tsiBuffer.buffer[PAD_ONE] = tsiSensorFlags;
				padVal = PAD_ONE;
				(void)OSSemPost(&(tsiBuffer.flag), OS_OPT_POST_1, &os_err);   /* Signal new data in buffer */
			}
			DB1_TURN_ON();
			tsiStartScan(BRD_PAD2_CH);
			tsiTaskState = PROC2START1;
			break;
		case PROC2START1:
			tsiProcScan(BRD_PAD2_CH);
			if(tsiBuffer.buffer[PAD_TWO] != tsiSensorFlags){
				tsiBuffer.buffer[PAD_TWO] = tsiSensorFlags;
				padVal = PAD_TWO;
				(void)OSSemPost(&(tsiBuffer.flag), OS_OPT_POST_1, &os_err);   /* Signal new data in buffer */
			}
			DB1_TURN_OFF();
			tsiStartScan(BRD_PAD1_CH);
			tsiTaskState = PROC1START2;
			break;
		default:
			tsiTaskState = PROC1START2;
			break;
		}
		if(tsiBuffer.buffer[2]){

		}
	}

}

/********************************************************************************
 *   TSIGetStartScan: Starts a scan of a TSI sensor.
 *                    channel - the TSI channel to be started. Range 0-15
 ********************************************************************************/
static void tsiStartScan(INT8U channel){
	TSI0->DATA = TSI_DATA_TSICH(channel);       //set channel
	TSI0->DATA |= TSI_DATA_SWTS(1);             //start a scan sequence
}

/********************************************************************************
 *   TSIProcScan: Waits for the scan to complete, then sets the appropriate
 *                flags is a touch was detected.
 *                Note the scan must be started before this is called.
 *                channel - the channel to be processed
 ********************************************************************************/
static void tsiProcScan(INT8U channel){

	while((TSI0->GENCS & TSI_GENCS_EOSF_MASK) == 0){}
	TSI0->GENCS |= TSI_GENCS_EOSF(1);    //Clear flag

	/* Process electrode 1 */
	if((INT16U)(TSI0->DATA & TSI_DATA_TSICNT_MASK) > tsiSensorLevels[channel].threshold){
		tsiSensorFlags |= (INT16U)(1<<channel);
	}else{
		tsiSensorFlags &= (INT16U)(1<<channel);
	}

}

/********************************************************************************
 *   TSIPend: Pends on sensor flag, and returns value of semaphore buffer
 ********************************************************************************/
INT16U TSIPend(INT16U tout, OS_ERR *os_err){
	OSSemPend(&(tsiBuffer.flag),tout, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, os_err);
	return(tsiBuffer.buffer[padVal]);
}
