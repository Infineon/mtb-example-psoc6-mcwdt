/******************************************************************************
* File Name:   main.c

* Description: This is the source code for the PSoC 6 MCU Multi-Counter Watchdog
*              Timer (MCWDT) Example.
*
* Related Document: See README.md
*
*
*******************************************************************************
* (c) 2019-2020, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
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
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

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

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void handle_error(void);
static uint32_t read_switch_status(void);

/*******************************************************************************
* Global Variables
********************************************************************************/
cyhal_lptimer_t lptimerObj;

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. lptimer HAL APIs are used to work with
* the MCWDT block. The main loop waits till the button switch is pressed. Once pressed,
* it reads the timer value and gets the difference in time between the last two
* switch press events. It then prints the time over UART.
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

    /* Switch press event count value */
    uint32_t event1_cnt, event2_cnt;

    /* The time between two presses of switch */
    uint32_t timegap;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;

    /* BSP initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the User LED */
    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* GPIO initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the User button */
    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, 1);

    /* GPIO initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        handle_error();
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* retarget-io initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        handle_error();
    }

    /* Initialize MCWDT using lptimer HAL APIs */
    /* Default configuration of MCWDT is used */
    /* - Counter0 and Counter1 are cascaded */
    /* - Counter0 and Counter1 are set in interrupt mode - this feature not used in this CE */
    /* - WCO is the input clock source (configured in the earlier function call - cybsp_init()) */
    result = cyhal_lptimer_init(&lptimerObj);

    /* LPTIMER initialization failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        handle_error();
    }

    /* Initialize event count value */
    event1_cnt = 0;
    event2_cnt = 0;

    /* Print a message on UART */

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("PSoC 6 MCU Multi-Counter Watchdog Timer Example\n\r");
    printf("-----------------------------------------------\n\r");
    printf("\r\n\r\nMCWDT initialization is complete. Press the user button to display time stamps. \r\n");

    for(;;)
    {
        /* Check if the switch is pressed */
        /* Note that if the switch is pressed, the CPU will not return from
         * read_switch_status() function until the switch is released */
        if (0UL != read_switch_status())
        {
            /* Consider previous key press as 1st key press event */
            event1_cnt = event2_cnt;

            /* Consider the current switch press as 2nd switch press event */
            /* Get counter value from MCWDT */
            /* Note that Counter0 is cascaded to Counter1 of the MCWDT block */
            event2_cnt = cyhal_lptimer_read(&lptimerObj);

            /* Calculate the time between two presses of switch and print on the terminal */
            /* MCWDT Counter0 and Counter1 are clocked by LFClk sourced from WCO of frequency 32768 Hz */
            if(event2_cnt > event1_cnt)
            {
                timegap = (event2_cnt - event1_cnt)/CY_SYSCLK_WCO_FREQ;

                /* Print the timegap value */
                printf("\r\nThe time between two presses of user button = %ds\r\n", (unsigned int)timegap);
            }
            else /* counter overflow */
            {
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
*  Reads and returns the current status of the switch. If the switch is pressed,
*  CPU will be blocking until the switch is released.
*
* Parameters:
*  void
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
    while(0UL == cyhal_gpio_read(CYBSP_USER_BTN))
    {
        /* Switch is pressed. Proceed for debouncing. */
        cyhal_system_delay_ms(SWITCH_DEBOUNCE_CHECK_UNIT);
        ++delayCounter;

        /* Keep checking the switch status till the switch is pressed for a
         * minimum period of SWITCH_DEBOUNCE_CHECK_UNIT x SWITCH_DEBOUNCE_MAX_PERIOD_UNITS */
        if (delayCounter > SWITCH_DEBOUNCE_MAX_PERIOD_UNITS)
        {
            /* Wait till the switch is released */
            while(0UL == cyhal_gpio_read(CYBSP_USER_BTN))
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

            }while(0UL == cyhal_gpio_read(CYBSP_USER_BTN));

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
* This function processes unrecoverable errors such as UART initialization error.
* In case of such error the system will turn on LED and halt the CPU.
*
* Parameters:
*  void
*
* Return:
*  None
*
*******************************************************************************/
void handle_error(void)
{
     /* Disable all interrupts */
    __disable_irq();

    /* Turn on LED to indicate error */
    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);

    /* Halt the CPU */
    CY_ASSERT(0);
}

/* [] END OF FILE */
