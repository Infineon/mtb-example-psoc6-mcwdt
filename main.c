/******************************************************************************
* File Name:   main.c

* Description: This is the source code for the PSoC 6 MCU Multi-Counter Watchdog
*              Timer (MCWDT) Example.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2019-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
********************************************************************************/

/* Switch press/release check interval in milliseconds for debouncing */
#define SWITCH_DEBOUNCE_CHECK_UNIT          (1u)

/* Number of debounce check units to count before considering that switch is pressed
 * or released */
#define SWITCH_DEBOUNCE_MAX_PERIOD_UNITS    (80u)

/* The function Cy_MCWDT_Enable() waits for some delay in microseconds before 
 * returning */
#define MCWDT_0_ENABLE_DELAY                (93u)

#define LED_ON                              (0u)      /* Value to switch LED ON  */
#define LED_OFF                             (!LED_ON) /* Value to switch LED OFF */


/*******************************************************************************
* Function Prototypes
********************************************************************************/
void handle_error(void);
static uint32_t read_switch_status(void);


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. The application uses cascade of Counter 
* 0 and Counter 1 of MCWDT block. The main loop waits till the button switch is 
* pressed. Once pressed, it reads the timer value and gets the difference in time
* between the last two switch press events. It then prints the time over UART.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    cy_en_mcwdt_status_t mcwdt_init_status = CY_MCWDT_SUCCESS;

#if defined(CY_DEVICE_SECURE)
    cyhal_wdt_t wdt_obj;
    /* Clear watchdog timer so that it doesn't trigger a reset */
    result = cyhal_wdt_init(&wdt_obj, cyhal_wdt_get_max_timeout_ms());
    CY_ASSERT(CY_RSLT_SUCCESS == result);
    cyhal_wdt_free(&wdt_obj);
#endif

    /* Switch press event count value */
    uint32_t event1_cnt, event2_cnt;
    uint32_t counter1_value, counter0_value;

    /* The time between two presses of switch */
    uint32_t timegap;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    
    /* BSP initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, 
                                 CY_RETARGET_IO_BAUDRATE);
    
    /* retarget-io initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        handle_error();
    }

    /* Initialize the MCWDT_0 */
    mcwdt_init_status = Cy_MCWDT_Init(MCWDT_0_HW, &MCWDT_0_config);
    
    if(mcwdt_init_status!=CY_MCWDT_SUCCESS)
    {
        handle_error();
    }

    /* Enable the MCWDT_0 counters */
    Cy_MCWDT_Enable(MCWDT_0_HW, CY_MCWDT_CTR0|CY_MCWDT_CTR1,
                    MCWDT_0_ENABLE_DELAY);

    /* Initialize event count value */
    event1_cnt = 0;
    event2_cnt = 0;

    /* Print a message on UART */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("*************** "
            "PSoC 6 MCU: Multi-Counter Watchdog Timer Example "
            "*************** \r\n\n");

    printf("\r\nMCWDT initialization is complete. Press the user button to "
           "display the time between two presses of the user button. \r\n");
    

    for(;;)
    {
        /* Check if the switch is pressed.
         * Note that if the switch is pressed, the CPU will not return from
         * read_switch_status() function until the switch is released.
         */
        if (0UL != read_switch_status())
        {
            /* Consider previous key press as 1st key press event */
            event1_cnt = event2_cnt;

            /* Consider current key press as 2nd key press event and get live
             * counter value from MCWDT_0.
             * Note that MCWDT_0 Counter1 is cascaded from MCWDT_0 Counter0 
             */
            counter0_value = Cy_MCWDT_GetCount(MCWDT_0_HW, CY_MCWDT_COUNTER0);
            counter1_value = Cy_MCWDT_GetCount(MCWDT_0_HW, CY_MCWDT_COUNTER1);
            event2_cnt = ((counter1_value<<16) | (counter0_value<<0));

            /* Calculate the time between two presses of switch and print on the 
             * terminal. MCWDT Counter0 and Counter1 are clocked by LFClk sourced 
             * from WCO of frequency 32768 Hz
             */
            if(event2_cnt > event1_cnt)
            {
                timegap = (event2_cnt - event1_cnt)/CY_SYSCLK_WCO_FREQ;
                /* Print the timegap value */
                printf("\r\nThe time between two presses of user button = %ds\r\n", 
                       (unsigned int)timegap);
            }
            else /* counter overflow */
            {
                timegap = 0;
                /* Print a message on overflow of counter */
                printf("\r\n\r\nCounter overflow detected\r\n");
            }

        }
    }
}


/*******************************************************************************
* Function Name: read_switch_status
********************************************************************************
* Summary:
*  Reads and returns the current status of the switch.
*
* Parameters:
*  None
*
* Return:
*  Returns non-zero value if switch is pressed and zero otherwise.
*
*******************************************************************************/
uint32_t read_switch_status(void)
{
    uint32_t delayCounter = 0;
    uint32_t sw_status = 0;

    /* Check if the switch is pressed */
    while(0UL == Cy_GPIO_Read(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM))
    {
        /* Switch is pressed. Proceed for debouncing. */
        Cy_SysLib_Delay(SWITCH_DEBOUNCE_CHECK_UNIT);
        ++delayCounter;

        /* Keep checking the switch status till the switch is pressed for a minimum
         * period of SWITCH_DEBOUNCE_CHECK_UNIT x SWITCH_DEBOUNCE_MAX_PERIOD_UNITS
         */
        if (delayCounter > SWITCH_DEBOUNCE_MAX_PERIOD_UNITS)
        {
            /* Wait till the switch is released */
            while(0UL == Cy_GPIO_Read(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM))
            {
            }

            /* Debounce when the switch is being released */
            do
            {
                delayCounter = 0;

                while(delayCounter < SWITCH_DEBOUNCE_MAX_PERIOD_UNITS)
                {
                    cyhal_system_delay_ms(SWITCH_DEBOUNCE_CHECK_UNIT);
                    ++delayCounter;
                }

            }while(0UL == Cy_GPIO_Read(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM));

            /* Switch is pressed and released*/
            sw_status = 1u;
        }
    }

    return (sw_status);
}


/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* This function processes unrecoverable errors such as UART component 
* initialization error. In case of such error the system will Turn on ERROR_LED 
* and stay in an infinite loop of this function.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void handle_error(void)
{
     /* Disable all interrupts */
    __disable_irq();
    
    /* Turn on error LED */
    Cy_GPIO_Write(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN, LED_ON);
    
    /* Halt the CPU */
    CY_ASSERT(0);

}


/* [] END OF FILE */
