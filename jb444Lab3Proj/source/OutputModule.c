/*****************************************************************************************
* This module operates the DAC and the Flex timer in order to generate a signal between
* 10 to 10k Hz. It will be taking taking in the volume, which controls the volume of the
* Sine wave and the duty cycle of the square wave
*
* Jacob Bindernagel
* 3/11/2021
*****************************************************************************************/
#include "app_cfg.h"
#include "os.h"
#include "MCUType.h"
#include "K65TWR_ClkCfg.h"
#include "K65TWR_GPIO.h"
#include "OutputModule.h"
#include "K65TWR_GPIO.h"
#include "arm_math.h"
#include "input.h"
#include "UserInt.h"
/*****************************************************************************************
* Allocate task control blocks
*****************************************************************************************/
static OS_TCB SineOutputTaskTCB;
static OS_TCB SquareOutputTaskTCB;

/*****************************************************************************************
* Allocate task stack space.
*****************************************************************************************/
static CPU_STK SineOutputTaskStk[APP_CFG_SIN_GEN_TASK_STK_SIZE];
static CPU_STK SquareOutputTaskStk[APP_CFG_SQUARE_GEN_STK_SIZE];

/******************************************************************************************
 * Defines
 ******************************************************************************************/

//Defines for Square Wave
#define HIGH_FREQ_PRESCALER 0
#define MID_FREQ_PRESCALAR  6u
#define LOW_FREQ_PRESCALAR 7u
#define UPPER_THRESHOLD_FREQ 915 //Can't use the unscaled clock at this frequency (mod <= 0x7FFe)
#define LOWEST_THREHOLD_FREQ 14    //Must rescale again
#define UNSCALED_CLK_FREQ 60000000
#define SCALED_CLK_FREQ   937500
#define TWICE_SCALED_CLK_FREQ  468750
#define MAX_VOL 20

//Defines for Sine wave
#define SIZE_CODE_16BIT   0x1
#define NUM_BLOCKS        2
#define BYTES_PER_SAMPLE  2
#define SAMPLES_PER_BLOCK 1024
#define BYTES_PER_BLOCK             (SAMPLES_PER_BLOCK*BYTES_PER_SAMPLE)
#define BYTES_PER_BUFFER            (NUM_BLOCKS*BYTES_PER_BLOCK)
#define DMA_OUT_CH        0
#define SAMPLE_PERIOD_Q31 44739
#define ABS_VAL_MASK       0x7FFFFFFF
#define DC_OFFSET 2048

typedef struct{
    INT8U index;
    OS_SEM flag;
}DMA_BLOCK_RDY;

/******************************************************************************************
 * Variables
 ******************************************************************************************/
 DMA_BLOCK_RDY dmaInBlockRdy;
 static INT16S DMABuffer[NUM_BLOCKS][SAMPLES_PER_BLOCK];


/*****************************************************************************************
* Task Function Prototypes.
*   - Private if in the same module as startup task. Otherwise public.
*****************************************************************************************/
static void SquareOutputTask(void *p_arg);
static void SineOutputTask(void *p_arg);
static INT8U DMAPend(OS_TICK tout, OS_ERR *os_err_ptr);
void DMA0_DMA16_IRQHandler(void);

void OutputInit(void){


    OS_ERR os_err;

    SIM->SCGC3 |= SIM_SCGC3_FTM3(1); /* Enable clock gate for FTM3 */
    SIM->SCGC5 |= SIM_SCGC5_PORTE(1); /* Enable clock gate for PORTE */
    PORTE->PCR[8] = PORT_PCR_MUX(6); /* Set PCR for FTM output */


    //Intialization of the DMA
//    OSSemCreate(&dmaInBlockRdy.flag, "Block Ready", 0, &os_err);

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
    DMAMUX->CHCFG[DMA_OUT_CH] |= ~(DMAMUX_CHCFG_ENBL_MASK|DMAMUX_CHCFG_TRIG_MASK);

    //Set up input for DMA.
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
                     OS_OPT_TASK_NONE,
                     &os_err);




}



/******************************************************************************
 * Calculates a data table and shoves it through the DMA to the DAC. Uses the DSP
 * CMSIS module to calculate the sine wave, and uses a ping pong buffer to
 * communicate with the DAC.
 *
 *
 * Inputs: None
 * Outputs: None
 ******************************************************************************/
static void SineOutputTask(void *p_arg){

    (void)p_arg;
    const q31_t AC_FACTOR = 0x5D1745D; // AC_FACTOR = 1.5/(20*3.3) = 1/44 in q31
    OS_ERR os_err;
    INT16U sample_index = 0;
    INT16U buffer_index;
    q31_t xarg = 0;
    INT32U freq;
    INT32U vol;
    q31_t sine_value;
    STATE mode;

    (void) p_arg;
    while(1){


        freq = UIFreqGet();
        vol = UILevGet();
        mode = UIStateGet();


        DB1_TURN_OFF();

        if(mode == SINEWAVE_MODE){
        while (sample_index < SAMPLES_PER_BLOCK){
            sine_value = arm_sin_q31(xarg); //Computes sine wave value
            arm_mult_q31(&sine_value,&AC_FACTOR,&sine_value,1); //Multiplies by 1/20 of the volume (1.5/(3.3*20))
            sine_value = ((sine_value*vol) >> 20) + DC_OFFSET; //applies volume level, shifts to 12 bits, and applies DC offset

            DMABuffer[buffer_index][sample_index] = (INT16S)sine_value;

            xarg = xarg + (freq*SAMPLE_PERIOD_Q31); //Increments counter
            xarg = xarg & ABS_VAL_MASK; //Masks sign bit for roll over
            sample_index++;
        }
        sample_index = 0;
        DB1_TURN_OFF();
        }
    }

}



/******************************************************************************
 *Operates the FTM to produce a Square Wave. Sends the signal to PortE (Pin A59)
 *
 * Inputs: None
 * Outputs: None
 *
 ******************************************************************************/

    static void  SquareOutputTask(void *p_arg){

        OS_ERR os_err;
        INT16U freq;
        INT16U mod;
        INT32U duty;
        INT8U vol;
        STATE mode;
        (void) p_arg;

        while(1){

            freq = UIFreqGet();
            vol = UILevGet();
            mode = UIStateGet();


            DB0_TURN_ON();

            if(mode == PULSETRAIN_MODE){

                if(freq <= LOWEST_THREHOLD_FREQ){
                    //System Clock, Centered Pulse, Prescaler
                    FTM3->SC = FTM_SC_CLKS(1)|FTM_SC_CPWMS(1)|FTM_SC_PS(LOW_FREQ_PRESCALAR);

                    //PWM polarity
                    FTM3->CONTROLS[3].CnSC = FTM_CnSC_ELSA(0)|FTM_CnSC_ELSB(1);

                    //Calculates wanted mod (Tp = Tsys*2*mod)
                    mod = TWICE_SCALED_CLK_FREQ/(freq*2);

                    //Sticks value in FTM's mod register
                    FTM3->MOD = FTM_MOD_MOD(mod);
                }


                else if((freq > LOWEST_THREHOLD_FREQ) && (freq <= UPPER_THRESHOLD_FREQ)){
                    FTM3->SC = FTM_SC_CLKS(1)|FTM_SC_CPWMS(1)|FTM_SC_PS(MID_FREQ_PRESCALAR);
                    FTM3->CONTROLS[3].CnSC = FTM_CnSC_ELSA(0)|FTM_CnSC_ELSB(1);
                    mod = SCALED_CLK_FREQ/(freq*2);
                    FTM3->MOD = FTM_MOD_MOD(mod);
                }

                else{
                    FTM3->SC = FTM_SC_CLKS(1)|FTM_SC_CPWMS(1)|FTM_SC_PS(HIGH_FREQ_PRESCALER);
                    FTM3->CONTROLS[3].CnSC = FTM_CnSC_ELSA(0)|FTM_CnSC_ELSB(1);
                    mod = UNSCALED_CLK_FREQ/(freq*2);
                    FTM3->MOD = FTM_MOD_MOD(mod);

                }

                //Computes duty cycle based on volume and inputs
                duty = ((INT32U)mod * (INT32U)vol) / MAX_VOL;
                //Set signal pulse width (duty cycle)
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

void DMA0_DMA16_IRQHandler(void){
    OS_ERR os_err;
    OSIntEnter();
    DB1_TURN_ON();
    DMA0->CINT = DMA_CINT_CINT(DMA_OUT_CH);
    dmaInBlockRdy.index ^= 1;                            //toggle buffer index
    OSSemPost(&(dmaInBlockRdy.flag),OS_OPT_POST_1,&os_err);
    DB1_TURN_OFF();
    OSIntExit();
}

/****************************************************************************************
* DMA Interrupt Handler for the sample stream
* 08/30/2015 TDM
***************************************************************************************/
static INT8U DMAPend(OS_TICK tout, OS_ERR *os_err_ptr){
        OSSemPend(&(dmaInBlockRdy.flag), tout, OS_OPT_PEND_BLOCKING,(void *)0, os_err_ptr);
        return dmaInBlockRdy.index;
 }
