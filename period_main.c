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
void heartBeat_TASK(void);
void captureEvent_HWI(void);

/* ---- Globals -----*/
uint32_t period;    // measured period  (40000 at test signal of 2kHz)
uint32_t first;     // first edge value (always changing...)

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
    SysCtlPWMClockSet(SYSCTL_PWMDIV_1);

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
void periodInit(void)
{
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

    // Enable timer peripheral clock
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER0))
    {
    }

    //Disable timer to configure
    TimerDisable(TIMER0_BASE, TIMER_A);

    // Configure the timer
    TimerConfigure(TIMER0_BASE, TIMER_CFG_A_CAP_TIME | TIMER_CFG_SPLIT_PAIR);
    TimerLoadSet(TIMER0_BASE, TIMER_A, 0x0000ffff);

    // set prescale to get 24 bits
    TimerPrescaleSet(TIMER0_BASE, TIMER_A, 0xff);

    // rising edge event
    TimerControlEvent(TIMER0_BASE, TIMER_A, TIMER_EVENT_POS_EDGE);

    // interrup enable
    TimerIntEnable(TIMER0_BASE, TIMER_CAPA_EVENT);

    // clear interrupt
    TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT);

    // enable timer to configure
    TimerEnable(TIMER0_BASE, TIMER_A);

    // interrupt enable
    IntEnable(INT_TIMER0A);
}

/* ----- Capture timer evet interrupt handler ---- */
// called every first edge of T0CCP0 (PB6)
void captureEvent_HWI(void)
{
    // clear interrupt flag
    TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT);

    // get the actual period
    // 0x00ffffff = 24 bit mask
    period = (first - TimerValueGet(TIMER0_BASE, TIMER_A))&0x00ffffff;

    // get the timer value at the first edge
    first = TimerValueGet(TIMER0_BASE, TIMER_A);
}

/* ----- There always a led blinking.... -----*/
void heartBeat_TASK(void)
{

    periodInit();   // initialize period measurement timer
    pwmInit();      // initialize pwm test signal

    while (1)
    {
        Task_sleep(500);
        GPIO_toggle(Board_LED0);
    }
}
