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
#include "OutputModule.h"
/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
static OS_TCB SignalStateTaskTCB;
static OS_TCB SineOutputTaskTCB;
static OS_TCB SquareOutputTaskTCB;

/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
static CPU_STK SignalStateTaskStk[APP_CFG_TASK_START_STK_SIZE];
static CPU_STK SineOutputTaskStk[APP_CFG_SIN_GEN_TASK_STK_SIZE];
static CPU_STK SquareOutputTaskStk[APP_CFG_KEY_TASK_STK_SIZE];


/******************************************************************************************
 * Defines
 ******************************************************************************************/

#define DMA_IN_CH       2
#define SYS_CLK 30000000
#define HIGH_FREQ_PRESCALAR 0
#define LOW_FREQ_PRESCALAR  6u
#define FULL_DUTY_CYCLE 20


typedef struct{
    INT8U index;
    OS_SEM flag;
}DMA_BLOCK_RDY;


/******************************************************************************************
 * Variables
 ******************************************************************************************/



/*****************************************************************************************
* Task Function Prototypes.
*   - Private if in the same module as startup task. Otherwise public.
*****************************************************************************************/


//static void SignalStateTask(void *p_arg);
static void SquareOutputTask(void *p_arg);
//static void SineOutputTask(void *p_arg);
void OutputInit(void *p_arg);
static void FTMInit(void);
static void DMAInit(void);
void DMA2_DMA18_IRQHandler(void);


void OutputInit(void *p_arg){


    OS_ERR os_err;

    (void) p_arg;
    //Initialization
    FTMInit();
    DMAInit();


    OSTaskCreate(&SignalStateTaskTCB,
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
                &os_err);

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

    OSTaskCreate(&SineOutputTaskTCB,
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
                    &os_err);






}

static void  SquareOutputTask(void *p_arg){


    OS_ERR os_err;

    (void)p_arg;


    INT32U frequency = 10000;
    INT32U duty = 10;

    INT32U period;
    duty_per


    while(1){


        //Frequencies above 2 kHz are prescaled by 1
        if(frequency <= 2000){
        FTM3->SC = FTM_SC_CLKS(1)|FTM_SC_CPWMS(1)|FTM_SC_PS(HIGH_FREQ_PRESCALAR);
        period = SYS_CLK / frequency;
        duty_per = (period * duty)/ FULL_DUTY_CYCLE;
        }

        //Frequncies below 2 kHz are prescaled by 64
        else{
            FTM3->SC = FTM_SC_CLKS(1)|FTM_SC_CPWMS(1)|FTM_SC_PS(LOW_FREQ_PRESCALAR);
            period = SOURCE_CLK / frequency;
            duty_per = (period * duty) / FULL_DUTY_CYCLE;
        }

        FTM3->MOD = FTM_MOD_MOD((INT16U)period);
        FTM3->CONTROLS[3].CnV = FTM_CnV_Val((INT16U)duty_per);
    }


}







void DMAInit(void){
    OS_ERR os_err;

    OSSemCreate(&dmaBlockRdy.flag, "Block Ready", 0, &os_err);

    // dmaBlockRdy.index indicates the buffer currently not being used by the DMA in the Ping-Pong scheme.
    // This is a bit more open loop than I like but there doesn't seem to be a status bit that
    // distinguishes between a half-full interrupt,INTHALF, and a full interrupt, INTMAJOR.
    // Bottom line, this has to start at one. The DMA fills the [0] block first so, by the time
    // the HALFINT happens, it is working on the [1] block. The ISR toggles the initial value to
    // zero so the [0] block is processed first.
    dmaBlockRdy.index = 1;

    //enable DMA clocks
    SIM->SCGC6 |= SIM_SCGC6_DMAMUX(1);
    SIM->SCGC7 |= SIM_SCGC7_DMA(1);
    SIM->SCGC6 |= SIM_SCGC6_PIT(1);

    //Make sure DMAMUX is disabled
    //DMAMUX->CHCFG[DMA_OUT_CH] |= DMAMUX_CHCFG_ENBL(0)|DMAMUX_CHCFG_TRIG(0);
    DMAMUX->CHCFG[DMA_OUT_CH] &= ~(DMAMUX_CHCFG_ENBL_MASK|DMAMUX_CHCFG_TRIG_MASK);

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
    PIT->CHANNEL[0].LDVAL = 1249; //48 kHz (60 source clock
    PIT->CHANNEL[0].TCTRL = PIT_TCTRL_TEN(1);

    //trigger source is PIT (60)
    DMAMUX->CHCFG[DMA_OUT_CH] = DMAMUX_CHCFG_ENBL(1) | DMAMUX_CHCFG_SOURCE(60) | DMAMUX_CHCFG_TRIG(1);

    SIM->SCGC2 |= SIM_SCGC2_DAC0(1);
    DAC0->C0 = (DAC_C0_DACEN(1) | DAC_C0_DACRFS(1) | DAC_C0_DACTRGSEL(1));

    //enable DMA Rx interrupt
    NVIC_EnableIRQ(DMA_OUT_CH);

    //All set to go, enable DMA channel(s)!
    DMA0->SERQ = DMA_SERQ_SERQ(DMA_OUT_CH);
}


void FTMInit(void){

    SIM->SCGC3 |= SIM_SCGC3_FTM3(1); /* Enable clock gate for FTM3 */
    SIM->SCGC5 |= SIM_SCGC5_PORTE(1); /* Enable clock gate for PORTE */
    PORTE->PCR[8] = PORT_PCR_MUX(6); /* Set PCR for FTM output */
}



/****************************************************************************************
 * DMA Interrupt Handler for the sample stream
 * 08/30/2015 TDM
 ***************************************************************************************/
static void DMA2_DMA18_IRQHandler(void){
    OS_ERR os_err;
    OSIntEnter();
    DB1_TURN_ON();
    DMA0->CINT = DMA_CINT_CINT(DMA_IN_CH);
    dmaInBlockRdy.index ^= 1;                            //toggle buffer index
    OSSemPost(&(dmaInBlockRdy.flag),OS_OPT_POST_1,&os_err);
    DB1_TURN_OFF();
    OSIntExit();
}
/****************************************************************************************
 * DMA Interrupt Handler for the sample stream
 * 08/30/2015 TDM
 ***************************************************************************************/
static INT8U PwrDMAInPend(OS_TICK tout, OS_ERR *os_err_ptr){

    OSSemPend(&(dmaInBlockRdy.flag), tout, OS_OPT_PEND_BLOCKING,(void *)0, os_err_ptr);
    return dmaInBlockRdy.index;
}

