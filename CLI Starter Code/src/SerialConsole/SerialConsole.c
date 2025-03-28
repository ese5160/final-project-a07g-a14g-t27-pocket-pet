/**************************************************************************//**
 * @file        SerialConsole.c
 * @ingroup     Serial Console
 * @brief       This file contains the code necessary to run the CLI and Serial Debugger.
 *              It initializes a UART channel and uses it to receive commands from the user
 *              as well as print debug information.
 * @details     The code in this file will:
 *              - Initialize a SERCOM port to operate as a UART channel at 115200 baud, 8N1.
 *              - Register callbacks for asynchronous reading and writing of characters.
 *              - Initialize the CLI and Debug Logger data structures.
 * @copyright   
 * @author      
 * @date        January 26, 2019
 * @version     0.1
 *****************************************************************************/

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include "SerialConsole.h"

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define RX_BUFFER_SIZE 512    /**< Size of the RX character buffer in bytes */
#define TX_BUFFER_SIZE 512    /**< Size of the TX character buffer in bytes */

/******************************************************************************/
/* Structures and Enumerations                                                */
/******************************************************************************/
cbuf_handle_t cbufRx;         /**< Circular buffer handler for receiving characters */
cbuf_handle_t cbufTx;         /**< Circular buffer handler for transmitting characters */

char latestRx;                /**< Holds the latest character received */
char latestTx;                /**< Holds the latest character to be transmitted */

/******************************************************************************/
/* Callback Declarations                                                      */
/******************************************************************************/
void usart_write_callback(struct usart_module *const usart_module); /**< Callback for when writing is complete */
void usart_read_callback(struct usart_module *const usart_module);  /**< Callback for when reading is complete */

/******************************************************************************/
/* Local Function Declarations                                                */
/******************************************************************************/
static void configure_usart(void);
static void configure_usart_callbacks(void);

/******************************************************************************/
/* Global Variables                                                           */
/******************************************************************************/
struct usart_module usart_instance;        /**< USART instance structure */
char rxCharacterBuffer[RX_BUFFER_SIZE];      /**< Buffer to store received characters */
char txCharacterBuffer[TX_BUFFER_SIZE];      /**< Buffer to store characters to be sent */
enum eDebugLogLevels currentDebugLevel = LOG_INFO_LVL; /**< Default debug level */
SemaphoreHandle_t xSemaphore = NULL;         /**< Semaphore for synchronizing access to the UART */

/******************************************************************************/
/* Global Functions                                                           */
/******************************************************************************/

/**************************************************************************//**
 * @brief Initializes the UART and registers callbacks.
 *
 * This function initializes circular buffers for RX and TX, configures the USART,
 * registers callbacks, creates a semaphore for synchronization, sets the SERCOM interrupt
 * priority, and starts the USART read job.
 *
 * @return None.
 *****************************************************************************/
void InitializeSerialConsole(void)
{
    /* Initialize circular buffers for RX and TX */
    cbufRx = circular_buf_init((uint8_t *)rxCharacterBuffer, RX_BUFFER_SIZE);
    cbufTx = circular_buf_init((uint8_t *)txCharacterBuffer, TX_BUFFER_SIZE);

    /* Configure the USART and register callbacks */
    configure_usart();
    configure_usart_callbacks();

    /* Create a binary semaphore for synchronizing access to the UART */
    xSemaphore = xSemaphoreCreateBinary();
    configASSERT(xSemaphore);

    /* Set the interrupt priority for SERCOM4 */
    NVIC_SetPriority(SERCOM4_IRQn, 10);

    /* Kick off constant reading of characters */
    usart_read_buffer_job(&usart_instance, (uint8_t *)&latestRx, 1);

    // Additional initialization calls can be added here.
}

/**************************************************************************//**
 * @brief Deinitializes the UART.
 *
 * This function disables the USART.
 *
 * @return None.
 *****************************************************************************/
void DeinitializeSerialConsole(void)
{
    usart_disable(&usart_instance);
}

/**************************************************************************//**
 * @brief Writes a string to the UART.
 *
 * This function sends a null-terminated string over the UART. It places each
 * character into the transmit circular buffer and initiates a write job if the USART is available.
 *
 * @param[in] string Pointer to the null-terminated string to send.
 *
 * @return None.
 *****************************************************************************/
void SerialConsoleWriteString(char *string)
{
    if (string != NULL)
    {
        for (size_t iter = 0; iter < strlen(string); iter++)
        {
            circular_buf_put(cbufTx, string[iter]);
        }

        if (usart_get_job_status(&usart_instance, USART_TRANSCEIVER_TX) == STATUS_OK)
        {
            circular_buf_get(cbufTx, (uint8_t *)&latestTx); // Retrieve a character if TX is free.
            usart_write_buffer_job(&usart_instance, (uint8_t *)&latestTx, 1);
        }
    }
}

/**************************************************************************//**
 * @brief Reads a character from the RX buffer.
 *
 * This function retrieves a character from the RX circular buffer in a thread-safe manner.
 *
 * @param[out] rxChar Pointer where the received character will be stored.
 *
 * @return Returns -1 if the buffer is empty, otherwise the character read.
 *****************************************************************************/
int SerialConsoleReadCharacter(uint8_t *rxChar)
{
    vTaskSuspendAll();
    int a = circular_buf_get(cbufRx, (uint8_t *)rxChar);
    xTaskResumeAll();
    return a;
}

/**************************************************************************//**
 * @brief Gets the current debug log level.
 *
 * @return The current debug level.
 *****************************************************************************/
enum eDebugLogLevels getLogLevel(void)
{
    return currentDebugLevel;
}

/**************************************************************************//**
 * @brief Sets the debug log level.
 *
 * This function sets the debug log level.
 *
 * @param[in] debugLevel The debug level to set.
 *
 * @return None.
 *****************************************************************************/
void setLogLevel(enum eDebugLogLevels debugLevel)
{
    currentDebugLevel = debugLevel;
}

/**************************************************************************//**
 * @brief Logs a message at the specified debug level.
 *
 * This function formats a debug message and sends it over the UART if the specified
 * debug level is enabled.
 *
 * @param[in] level  The debug level for the message.
 * @param[in] format The format string for the message.
 * @param[in] ...    Variable arguments for the format string.
 *
 * @return None.
 *****************************************************************************/
void LogMessage(enum eDebugLogLevels level, const char *format, ...)
{
    // Todo: Implement Debug Logger.
    if (level < currentDebugLevel || level >= N_DEBUG_LEVELS)
    {
        return; // Do not log if level is lower than current or invalid.
    }

    char buffer[512]; // Buffer to hold the formatted string.
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    SerialConsoleWriteString(buffer);
}

/******************************************************************************/
/* Local Functions                                                          */
/******************************************************************************/

/**************************************************************************//**
 * @brief Configures the USART.
 *
 * This function sets up the USART configuration parameters such as baud rate,
 * multiplexer settings, and pin multiplexing, then initializes and enables the USART.
 *
 * @return None.
 *****************************************************************************/
static void configure_usart(void)
{
    struct usart_config config_usart;
    usart_get_config_defaults(&config_usart);

    config_usart.baudrate = 115200;
    config_usart.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
    config_usart.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
    config_usart.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
    config_usart.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
    config_usart.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
    while (usart_init(&usart_instance, EDBG_CDC_MODULE, &config_usart) != STATUS_OK)
    {
        // Optionally add error handling here.
    }

    usart_enable(&usart_instance);
}

/**************************************************************************//**
 * @brief Registers USART callbacks.
 *
 * This function registers and enables the USART callbacks for both transmit and
 * receive events.
 *
 * @return None.
 *****************************************************************************/
static void configure_usart_callbacks(void)
{
    usart_register_callback(&usart_instance, usart_write_callback, USART_CALLBACK_BUFFER_TRANSMITTED);
    usart_register_callback(&usart_instance, usart_read_callback, USART_CALLBACK_BUFFER_RECEIVED);
    usart_enable_callback(&usart_instance, USART_CALLBACK_BUFFER_TRANSMITTED);
    usart_enable_callback(&usart_instance, USART_CALLBACK_BUFFER_RECEIVED);
}

/******************************************************************************/
/* Callback Functions                                                       */
/******************************************************************************/

/**************************************************************************//**
 * @brief Callback for USART receive.
 *
 * This function is invoked when the USART has received the requested number of characters.
 * It stores the received character into the RX circular buffer, restarts the read job, and
 * gives a semaphore to unblock any tasks waiting for input.
 *
 * @param[in] usart_module Pointer to the USART module structure.
 *
 * @return None.
 *****************************************************************************/
void usart_read_callback(struct usart_module *const usart_module)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    circular_buf_put(cbufRx, latestRx);
    usart_read_buffer_job(&usart_instance, (uint8_t *)&latestRx, 1); // Restart reading
    if (xSemaphore != NULL)
    {
        xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**************************************************************************//**
 * @brief Callback for USART transmit.
 *
 * This function is invoked when the USART has finished transmitting the requested bytes.
 * It retrieves the next character from the TX circular buffer and starts another write job if available.
 *
 * @param[in] usart_module Pointer to the USART module structure.
 *
 * @return None.
 *****************************************************************************/
void usart_write_callback(struct usart_module *const usart_module)
{
    if (circular_buf_get(cbufTx, (uint8_t *)&latestTx) != -1)
    {
        usart_write_buffer_job(&usart_instance, (uint8_t *)&latestTx, 1);
    }
}
