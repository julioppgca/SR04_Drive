/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>

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

#include "sr04.h"

/* ---- Globals -----*/
uint32_t pulseWidth;    // measured pulse width  (?????? at test signal of 2kHz)

/* -----  Pulse width measurement Timer init -----*/
void sr04_Init(void)
{

    //Set CPU Clock to 80MHz. 400MHz PLL/2 = 200MHz DIV 2.5 = 80MHz
    SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);


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

    // interrupt enable, capture event timer 0A
    TimerIntEnable(TIMER0_BASE, TIMER_CAPA_EVENT);

    // clear interrupt
    TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT);

    // enable timers
    TimerEnable(TIMER0_BASE, TIMER_A | TIMER_B);

    // Timer 0A interrupt enable (vector 35)
    IntEnable(INT_TIMER0A);
}

/* ----- Capture timer evet interrupt handler ---- */
// called every falling edge of T0CCP0 (PB6)
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

    // post new distance semaphore
    Semaphore_post(newDistance_SEM);

}

void triggerPin_init(void)
{
    // Trigger pin
    //
    // Enable Peripheral Clocks
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    //
    // Configure the GPIO Pin Mux for PB0
    // for GPIO_PB0
    //
    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_0);

    // Enable timer peripheral clock
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    SysCtlPeripheralReset(SYSCTL_PERIPH_TIMER1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1));

    // Configure the timer
    TimerConfigure(TIMER1_BASE, TIMER_CFG_A_PERIODIC_UP | TIMER_CFG_SPLIT_PAIR);
    TimerLoadSet(TIMER1_BASE, TIMER_A, (80e6 / 100e3)); //10us

    // interrupt enable, timeout event timer 0A
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

    // clear interrupt
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

}


void triggerPin_HWI(void)
{
    // clear interrupt
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

    // clear trigger pin
    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, 0);

    // disable timer
    TimerDisable(TIMER1_BASE, TIMER_A);

    // reload timer value
    TimerLoadSet(TIMER1_BASE, TIMER_A, (80e6 / 100e3)); //10us

}

void triggerSr04(void)
{
    // trigger sr04
    // 10us pulse in sr04 trigger pin, timer based
    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_0, GPIO_PIN_0);
    TimerEnable(TIMER1_BASE, TIMER_A);
}

// TODO: insert timeout verification
void getDistance_TASK(void)
{
    triggerPin_init();
    sr04_Init();

    while (1)
    {
        triggerSr04();
        Task_sleep(DISTANCE_READ_INTERVAL);
        //Semaphore_pend(newDistance_SEM, BIOS_WAIT_FOREVER);
    }
}
