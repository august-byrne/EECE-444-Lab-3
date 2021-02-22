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
/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
static OS_TCB AppTaskStartTCB;
static OS_TCB AppTask1TCB;
static OS_TCB AppTask2TCB;

/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
static CPU_STK AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE];
static CPU_STK AppTask1Stk[APP_CFG_TASK1_STK_SIZE];
static CPU_STK AppTask2Stk[APP_CFG_TASK2_STK_SIZE];

/*****************************************************************************************
* Task Function Prototypes. 
*   - Private if in the same module as startup task. Otherwise public.
*****************************************************************************************/
static void  AppStartTask(void *p_arg);
static void  AppTask1(void *p_arg);
static void  AppTask2(void *p_arg);

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
    GpioLED8Init();
    GpioLED9Init();
    GpioDBugBitsInit();

    OSTaskCreate(&AppTask1TCB,                  /* Create Task 1                    */
                "App Task1 ",
                AppTask1,
                (void *) 0,
                APP_CFG_TASK1_PRIO,
                &AppTask1Stk[0],
                (APP_CFG_TASK1_STK_SIZE / 10u),
                APP_CFG_TASK1_STK_SIZE,
                0,
                0,
                (void *) 0,
                (OS_OPT_TASK_NONE),
                &os_err);

    OSTaskCreate(&AppTask2TCB,    /* Create Task 2                    */
                "App Task2 ",
                AppTask2,
                (void *) 0,
                APP_CFG_TASK2_PRIO,
                &AppTask2Stk[0],
                (APP_CFG_TASK2_STK_SIZE / 10u),
                APP_CFG_TASK2_STK_SIZE,
                0,
                0,
                (void *) 0,
                (OS_OPT_TASK_NONE),
                &os_err);


    OSTaskDel((OS_TCB *)0, &os_err);

}

/*****************************************************************************************
* TASK #1
* Uses OSTimeDelay to signal the Task2 semaphore every second.
* It also toggles the green LED every 100ms.
*****************************************************************************************/
static void AppTask1(void *p_arg){

    INT8U timcntr = 0;                              /* Counter for one second flag      */
    OS_ERR os_err;
    (void)p_arg;
    
    while(1){
    
        DB1_TURN_OFF();                             /* Turn off debug bit while waiting */
    	OSTimeDly(100,OS_OPT_TIME_PERIODIC,&os_err);     /* Task period = 100ms   */

        DB1_TURN_ON();                          /* Turn on debug bit while ready/running*/
        LED8_TOGGLE();                          /* Toggle green LED                     */
        timcntr++;
        if(timcntr == 10){                     /* Signal Task2 every second             */
            (void)OSTaskSemPost(&AppTask2TCB,OS_OPT_POST_NONE,&os_err);

            timcntr = 0;
        }else{
        }
    }
}

/*****************************************************************************************
* TASK #2
* Pends on its semaphore and toggles the blue LED every second
*****************************************************************************************/
static void AppTask2(void *p_arg){

    OS_ERR os_err;

    (void)p_arg;

    while(1) {                                  /* wait for Task 1 to signal semaphore  */

        DB2_TURN_OFF();                         /* Turn off debug bit while waiting     */
        OSTaskSemPend(0,                        /* No timeout                           */
                      OS_OPT_PEND_BLOCKING,     /* Block until posted                   */
                      (void *)0,                /* No timestamp                         */
                      &os_err);

        DB2_TURN_ON();                          /* Turn on debug bit while ready/running*/
        LED9_TOGGLE();;                         /* Toggle blue LED                    */
    }
}

/********************************************************************************/
