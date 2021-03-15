/*****************************************************************************************
* A simple demo program for uCOS-III.
* It tests multitasking, the timer, and task semaphores.
* This version is written for the K65TWR board, LED8 and LED9.
* If uCOS is working the green LED should toggle every 100ms and the blue LED
* should toggle every 1 second.
* Version 2017.2
* 01/06/2017, Todd Morton
* Version 2018.1 First working version for MCUXpresso
* 12/06/2018 Todd Morton
* Version 2021.1 First working version for MCUX11.2
* 01/04/2020 Todd Morton
* Version 2021.1.1 Removed error traps
* 01/14/2021 Jacob Bindernagel
*****************************************************************************************/
#include "app_cfg.h"
#include "os.h"
#include "MCUType.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "LcdLayered.h"
#include "input.h"
#include "UserInt.h"
#include "OutputModule.h"

/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
static OS_TCB AppTaskStartTCB;
/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
static CPU_STK AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE];

/*****************************************************************************************
* Task Function Prototypes. 
*   - Private if in the same module as startup task. Otherwise public.
*****************************************************************************************/
static void  AppStartTask(void *p_arg);

/*****************************************************************************************
* main()
*****************************************************************************************/
void main(void) {
	OS_ERR  os_err;

	K65TWR_BootClock();
	CPU_IntDis();               /* Disable all interrupts, OS will enable them  */

	OSInit(&os_err);                    /* Initialize uC/OS-III                         */

	OSTaskCreate(&AppTaskStartTCB,                  /* Address of TCB assigned to task */
				 "Start Task",                      /* Name you want to give the task */
				 AppStartTask,                      /* Address of the task itself */
				 (void *) 0,                        /* p_arg is not used so null ptr */
				 APP_CFG_TASK_START_PRIO,           /* Priority you assign to the task */
				 &AppTaskStartStk[0],               /* Base address of task�s stack */
				 (APP_CFG_TASK_START_STK_SIZE/10u), /* Watermark limit for stack growth */
				 APP_CFG_TASK_START_STK_SIZE,       /* Stack size */
				 0,                                 /* Size of task message queue */
				 0,                                 /* Time quanta for round robin */
				 (void *) 0,                        /* Extension pointer is not used */
				 (OS_OPT_TASK_NONE), /* Options */
				 &os_err);                          /* Ptr to error code destination */

	OSStart(&os_err);               /*Start multitasking(i.e. give control to uC/OS)    */

}

/*****************************************************************************************
* STARTUP TASK
* This should run once and be suspended. Could restart everything by resuming.
* (Resuming not tested)
* Todd Morton, 01/06/2016
*****************************************************************************************/
static void AppStartTask(void *p_arg) {
	OS_ERR os_err;

	(void)p_arg;                        /* Avoid compiler warning for unused variable   */

	OS_CPU_SysTickInitFreq(SYSTEM_CLOCK);

	/* Initialize StatTask. This must be called when there is only one task running.
	 * Therefore, any function call that creates a new task must come after this line.
	 * Or, alternatively, you can comment out this line, or remove it. If you do, you
	 * will not have accurate CPU load information                                       */
//    OSStatTaskCPUUsageInit(&os_err);

	GpioDBugBitsInit();
	inputInit();
	OSTimeDly(3000, OS_OPT_TIME_PERIODIC, &os_err); // delay 3000 ms for inputInit() to deal with displaying the checksum
	UIInit();
	OutputInit();

	OSTaskDel((OS_TCB *)0, &os_err);
}
