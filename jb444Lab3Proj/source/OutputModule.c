/*****************************************************************************************
* This module operates the DAC and the Flex timer in order to generate a signal between
* 10 to 10k Hz. It will be taking taking in the volume, which controls the volume of the
* Sine wave and the duty cycle of the square wave
*****************************************************************************************/
#include "app_cfg.h"
#include "os.h"
#include "MCUType.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "OutputModule.h"
#include "K65TWR_GPIO.h"
/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
// static OS_TCB SignalStateTaskTCB;
//static OS_TCB SineOutputTaskTCB;
static OS_TCB SquareOutputTaskTCB;
static OS_TCB AppTaskStartTCB;

/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
// static CPU_STK SignalStateTaskStk[APP_CFG_TASK_START_STK_SIZE];
//static CPU_STK SineOutputTaskStk[APP_CFG_SIN_GEN_TASK_STK_SIZE];
static CPU_STK SquareOutputTaskStk[APP_CFG_SQUARE_GEN_STK_SIZE];
static CPU_STK AppTaskStartStk[APP_CFG_SQUARE_GEN_STK_SIZE];


/******************************************************************************************
 * Defines
 ******************************************************************************************/

#define DMA_IN_CH       2
#define NO_PRESCALER 0
#define LOW_FREQ_PRESCALAR  6u // Nu = 2^(-N)

#define LOWER_THREHOLD_FREQ 458
#define UNSCALED_CLK_FREQ 30000000
#define SCALED_CLK_FREQ   468750

#define MAX_DUTY_CYCLE 20
#define SAMPLES_PER_BLOCK 64
#define BYTES_PER_SAMPLE 8
#define BYTES_PER_BUFFER 50
#define NUM_BLOCKS 2

typedef struct{
    INT8U index;
    OS_SEM flag;
}DMA_BLOCK_RDY;

typedef enum {SINEWAVE_MODE, PULSETRAIN_MODE, WAITING_MODE} STATE;

/******************************************************************************************
 * Variables
 ******************************************************************************************/
// DMA_BLOCK_RDY dmaInBlockRdy;

//static STATE CurrentState = PULSETRAIN_MODE;
//static INT16U Freq = 100;

/*****************************************************************************************
* Task Function Prototypes.
*   - Private if in the same module as startup task. Otherwise public.
*****************************************************************************************/


//static void SignalStateTask(void *p_arg);
static void SquareOutputTask(void *p_arg);
//static void SineOutputTask(void *p_arg);

static void AppStartTask(void *p_arg);
//void DMA2_DMA18_IRQHandler(void);
//static INT8U PwrDMAInPend(OS_TICK tout, OS_ERR *os_err_ptr);




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
    OutputInit();


    OSTaskCreate(&SquareOutputTaskTCB,
                    "Square Task ",
                    SquareOutputTask,
                    (void *) 0,
                    APP_CFG_SQUARE_GEN_TASK_PRIO,
                    &SquareOutputTaskStk[0],
                    (APP_CFG_SQUARE_GEN_STK_SIZE / 10u),
                    APP_CFG_SQUARE_GEN_STK_SIZE,
                    0,
                    0,
                    (void *) 0,
                    (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                    &os_err);



    OSTaskDel((OS_TCB *)0, &os_err);

}




void OutputInit(void){



    SIM->SCGC3 |= SIM_SCGC3_FTM3(1); /* Enable clock gate for FTM3 */
    SIM->SCGC5 |= SIM_SCGC5_PORTE(1); /* Enable clock gate for PORTE */
    PORTE->PCR[8] = PORT_PCR_MUX(6); /* Set PCR for FTM output */


    //Intialization of the DMA
/*    OSSemCreate(&dmaInBlockRdy.flag, "Block Ready", 0, &os_err);

    // dmaInBlockRdy.index indicates the buffer currently not being used by the DMA in the Ping-Pong scheme.
    // This is a bit more open loop than I like but there doesn't seem to be a status bit that
    // distinguishes between a half-full interrupt,INTHALF, and a full interrupt, INTMAJOR.
    // Bottom line, this has to start at one. The DMA fills the [0] block first so, by the time
    // the HALFINT happens, it is working on the [1] block. The ISR toggles the initial value to
    // zero so the [0] block is processed first.
    dmaInBlockRdy.index = 1;

    //enable DMA clocks
    SIM->SCGC6 |= SIM_SCGC6_DMAMUX(1);
    SIM->SCGC7 |= SIM_SCGC7_DMA(1);
    SIM->SCGC6 |= SIM_SCGC6_PIT(1);

    //Make sure DMAMUX is disabled
    //DMAMUX->CHCFG[DMA_OUT_CH] |= DMAMUX_CHCFG_ENBL(0)|DMAMUX_CHCFG_TRIG(0);
DMAMUX->CHCFG[DMA_OUT_CH] |= ~(DMAMUX_CHCFG_ENBL_MASK|DMAMUX_CHCFG_TRIG_MASK);

    //Minor Loop Mapping Enabled, Round Robin Arbitration, Debug enabled
//    DMA0->CR = DMA_CR_EMLM(1) | DMA_CR_ERCA(1) | DMA_CR_ERGA(1) | DMA_CR_EDBG(1);

    //source address is ADC0_RA register
    DMA0->TCD[DMA_OUT_CH].SADDR = DMA_SADDR_SADDR(&DMABuffer[0][0]);


    DMA0->TCD[DMA_OUT_CH].ATTR = DMA_ATTR_SMOD(0) | DMA_ATTR_SSIZE(SIZE_CODE_16BIT)
                                    | DMA_ATTR_DMOD(0) | DMA_ATTR_DSIZE(SIZE_CODE_16BIT);

    //No offset for source data address.  Always read ADC0
    DMA0->TCD[DMA_OUT_CH].SOFF = DMA_SOFF_SOFF(BYTES_PER_SAMPLE);

    //No adjustment to source address at end of major loop.  Always take from ADC_RA buffer
    DMA0->TCD[DMA_OUT_CH].SLAST = DMA_SLAST_SLAST(-(BYTES_PER_BUFFER));

    //destination buffer address
    DMA0->TCD[DMA_OUT_CH].DADDR = DMA_DADDR_DADDR(&DAC0->DAT[0].DATL);

    DMA0->TCD[DMA_OUT_CH].DOFF = DMA_DOFF_DOFF(0);

    //After Major loop, jump back to the beginning of each channel buffer
    DMA0->TCD[DMA_OUT_CH].DLAST_SGA = DMA_DLAST_SGA_DLASTSGA(0);

    //Minor loop size in bytes.
    DMA0->TCD[DMA_OUT_CH].NBYTES_MLNO = DMA_NBYTES_MLNO_NBYTES(BYTES_PER_SAMPLE);

    //Set minor loop iteration counters to number of minor loops in the major loop
    DMA0->TCD[DMA_OUT_CH].CITER_ELINKNO = DMA_CITER_ELINKNO_ELINK(0)|DMA_CITER_ELINKNO_CITER(NUM_BLOCKS*SAMPLES_PER_BLOCK);
    DMA0->TCD[DMA_OUT_CH].BITER_ELINKNO = DMA_BITER_ELINKNO_ELINK(0)|DMA_BITER_ELINKNO_BITER(NUM_BLOCKS*SAMPLES_PER_BLOCK);

    //Enable interrupt at half filled Rx buffer and end of major loop.
    //This allows "ping-pong" buffer processing.
    DMA0->TCD[DMA_OUT_CH].CSR = DMA_CSR_ESG(0) | DMA_CSR_MAJORELINK(0) | DMA_CSR_BWC(3) |
                                DMA_CSR_INTHALF(1) | DMA_CSR_INTMAJOR(1) | DMA_CSR_DREQ(0) | DMA_CSR_START(0);


    //Configure PIT channels
    PIT->MCR = 0x00;
    PIT->CHANNEL[0].LDVAL = 1249; //48 kHz (60 MHz source clock)
    PIT->CHANNEL[0].TCTRL = PIT_TCTRL_TEN(1);

    //trigger source is PIT (60)
    DMAMUX->CHCFG[DMA_OUT_CH] = DMAMUX_CHCFG_ENBL(1) | DMAMUX_CHCFG_SOURCE(60) | DMAMUX_CHCFG_TRIG(1);

    SIM->SCGC2 |= SIM_SCGC2_DAC0(1);
    DAC0->C0 = (DAC_C0_DACEN(1) | DAC_C0_DACRFS(1) | DAC_C0_DACTRGSEL(1));

    //enable DMA Rx interrupt
    NVIC_EnableIRQ(DMA_OUT_CH);

    //All set to go, enable DMA channel(s)!
    DMA0->SERQ = DMA_SERQ_SERQ(DMA_OUT_CH);

*/

/*    OSTaskCreate(&SignalStateTaskTCB,
                "State Change Task ",
                SignalStateTask,
                (void *) 0,
                APP_CFG_STATE_GEN_TASK_PRIO,
                &SquareOutputTaskStk[0],
                (APP_CFG_SQUARE_GEN_STK_SIZE / 10u),
                APP_CFG_SQUARE_GEN_STK_SIZE,
                0,
                0,
                (void *) 0,
                (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                &os_err); */


    /*OSTaskCreate(&SineOutputTaskTCB,
                    "Sine Task",
                    SineOutputTask,
                    (void *) 0,
                    APP_CFG_SIN_GEN_TASK_PRIO,
                    &SineOutputTaskStk[0],
                    (APP_CFG_SQUARE_GEN_STK_SIZE / 10u),
                    APP_CFG_SQUARE_GEN_STK_SIZE,
                    0,
                    0,
                    (void *) 0,
                    (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                    &os_err); */






}

/******************************************************************************
 *Operates the FTM to produce a Square Wave. Sends the signal to PortE (Pin A59)
 *
 * Inputs: None
 * Outputs: None
 *
 *  Bugs: Lower frequencies (501Hz>) Do not work when the volume is higher than 14
 *
 ******************************************************************************/

static void  SquareOutputTask(void *p_arg){


    STATE Mode = PULSETRAIN_MODE;
    INT16U freq = 500;
    INT16U period;
    INT32U duty;
    INT8U vol = 19;
    (void) p_arg;

    while(1){


    DB0_TURN_ON();
    if(Mode == PULSETRAIN_MODE){


            if(freq <= LOWER_THREHOLD_FREQ){
                /* Bus clock, center-aligned, divide by 1 prescaler */
                FTM3->SC = FTM_SC_CLKS(1)|FTM_SC_CPWMS(1)|FTM_SC_PS(LOW_FREQ_PRESCALAR);
                /*PWM polarity */
                FTM3->CONTROLS[3].CnSC = FTM_CnSC_ELSA(0)|FTM_CnSC_ELSB(1);

                period = SCALED_CLK_FREQ/freq;
                /* Set signal period */
                FTM3->MOD = FTM_MOD_MOD(period);
            }

            else{
                FTM3->SC = FTM_SC_CLKS(1)|FTM_SC_CPWMS(1)|FTM_SC_PS(NO_PRESCALER);
                FTM3->CONTROLS[3].CnSC = FTM_CnSC_ELSA(0)|FTM_CnSC_ELSB(1);
                period = UNSCALED_CLK_FREQ/freq;
                FTM3->MOD = FTM_MOD_MOD(period);

            }

        //Computes duty cycle based on volume and inputs
        duty = ((INT32U)period * (INT32U)vol) / MAX_DUTY_CYCLE;
        /* Set signal pulse width (duty cycle) */
        FTM3->CONTROLS[3].CnV = FTM_CnV_VAL((INT16U)duty);

        DB0_TURN_OFF();
        }
    else{
        DB0_TURN_OFF();
    }
    }

}


/****************************************************************************************
 * DMA Interrupt Handler for the sample stream
 * 08/30/2015 TDM
 ***************************************************************************************/

/*static void DMA2_DMA18_IRQHandler(void){
    OS_ERR os_err;
    OSIntEnter();
    DB1_TURN_ON();
    DMA0->CINT = DMA_CINT_CINT(DMA_IN_CH);
    dmaInBlockRdy.index ^= 1;                            //toggle buffer index
    OSSemPost(&(dmaInBlockRdy.flag),OS_OPT_POST_1,&os_err);
    DB1_TURN_OFF();
    OSIntExit();
} */
