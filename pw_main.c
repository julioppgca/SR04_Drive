/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
// #include <ti/drivers/I2C.h>
// #include <ti/drivers/SDSPI.h>
// #include <ti/drivers/SPI.h>
// #include <ti/drivers/UART.h>
// #include <ti/drivers/Watchdog.h>
// #include <ti/drivers/WiFi.h>

/*  Tivaware Library */
#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/pwm.h"
#include "driverlib/interrupt.h"

/* Board Header file */
#include "project_includes/Board.h"

void pwmInit(void);
void pulseWidth_Init(void);
void heartBeat_TASK(void);
void captureEvent_HWI(void);


/* ---- Globals -----*/
uint32_t pulseWidth;    // measured pulse width  (?????? at test signal of 2kHz)

/*
 *  ======== main ========
 */
int main(void)
{
    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    System_printf("Starting the example\nSystem provider is set to SysMin. "
                  "Halt the target to view any SysMin contents in ROV.\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}


/* -----    PWM Init function   ----- */
/* Just to test the period measurement.... */
// PA6 - M1PWM2 GEN1
// test signal 2kHz duty 50%
void pwmInit(void)
{
    //Configure PWM Clock to match system - 80MHz.
    SysCtlPWMClockSet(SYSCTL_PWMDIV_64);

    // Enable clock to Port A
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA))
        ;

    // Enable clock to PWM0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_PWM1))
        ;

    // Configure the GPIO Pin Mux for PA6
    // for M0PWM0
    MAP_GPIOPinConfigure(GPIO_PA6_M1PWM2);
    MAP_GPIOPinTypePWM(GPIO_PORTA_BASE, GPIO_PIN_6);

    //Configure PWM Options
    PWMGenConfigure(PWM1_BASE, PWM_GEN_1,
    PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);

    //Set the Period (expressed in clock ticks) - 80MHz/2kHz
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_1, (80e6/2e3));

    //Set PWM duty-50% (Period /2)
    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_2, (80e6/2e3) / 2);

    // Enable the PWM generator
    PWMGenEnable(PWM1_BASE, PWM_GEN_1);

    // Turn on output 0
    PWMOutputState(PWM1_BASE, PWM_OUT_2_BIT, true);
}

/* ----- Timer period measurement init -----*/
void pulseWidth_Init(void)
{
    // Pin init
    //
    // Enable Peripheral Clocks
    //
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    //
    // Configure the GPIO Pin Mux for PB6
    // for T0CCP0
    //
    MAP_GPIOPinConfigure(GPIO_PB6_T0CCP0);
    MAP_GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_6);

    //
    // Configure the GPIO Pin Mux for PB7
    // for T0CCP1
    //
    MAP_GPIOPinConfigure(GPIO_PB7_T0CCP1);
    MAP_GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_7);

    // Timer init
    // Enable timer peripheral clock
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0))
    {
    }


    // Timer 0A init
    //Disable timers to configure
    TimerDisable(TIMER0_BASE, TIMER_A | TIMER_B);

    // Configure the timer
    // Timer 0A and 0B as 16bit capture time mode
    TimerConfigure(TIMER0_BASE, TIMER_CFG_A_CAP_TIME | TIMER_CFG_B_CAP_TIME | TIMER_CFG_SPLIT_PAIR);
    // star at the top 2^16 = 65535 = 0xffff;
    TimerLoadSet(TIMER0_BASE, TIMER_A, 0x0000ffff);
    TimerLoadSet(TIMER0_BASE, TIMER_B, 0x0000ffff);

    // set prescale to get 24 bits
    // 0xffff * 0xff = 0xffffff
    TimerPrescaleSet(TIMER0_BASE, TIMER_A, 0xff);
    TimerPrescaleSet(TIMER0_BASE, TIMER_B, 0xff);

    // set to falling edge event of T0CCP0
    TimerControlEvent(TIMER0_BASE, TIMER_A, TIMER_EVENT_NEG_EDGE);
    // set to falling edge event of T0CCP1
    TimerControlEvent(TIMER0_BASE, TIMER_B, TIMER_EVENT_POS_EDGE);

    // interrup enable
    TimerIntEnable(TIMER0_BASE, TIMER_CAPA_EVENT);

    // clear interrupt
    TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT);

    // enable timers
    TimerEnable(TIMER0_BASE, TIMER_A | TIMER_B);

    // Timer interrupt enable
    IntEnable(INT_TIMER0A);
}

/* ----- Capture timer evet interrupt handler ---- */
// called every first edge of T0CCP0 (PB6)
// works good from 20Hz to ~200kHz (depends on software overhead)
// PB6 and PB7 must be connected together
// rising edge of PB7 copy timer value
// falling edge of PB6 generate interrupt
void captureEvent_HWI(void)
{
    // clear interrupt flag
    TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT);

    // get the actual period
    // 0x00ffffff = 24 bit mask
    pulseWidth = (TimerValueGet(TIMER0_BASE, TIMER_B) - TimerValueGet(TIMER0_BASE, TIMER_A))&0x00ffffff;

}

/* ----- There always a led blinking.... -----*/
void heartBeat_TASK(void)
{

    pulseWidth_Init();   // initialize pulse width measurement timer
    pwmInit();      // initialize pwm test signal

    while (1)
    {
        Task_sleep(500);
        GPIO_toggle(Board_LED0);
    }
}
