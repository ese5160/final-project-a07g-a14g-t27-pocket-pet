/**************************************************************************//**
 * @file        CliThread.c
 * @brief       File for the CLI Thread handler using FreeRTOS and CLI.
 * @author      Eduardo Garcia
 * @date        2020-02-15
 *****************************************************************************/

/******************************************************************************/
/* Includes                                                                   */
/******************************************************************************/
#include "CliThread.h"

/******************************************************************************/
/* Defines                                                                    */
/******************************************************************************/
#define FIRMWARE_VERSION  "0.0.1"  /**< Firmware version string */

/******************************************************************************/
/* Variables                                                                  */
/******************************************************************************/
/// Welcome message to be displayed when the CLI starts.
static int8_t *const pcWelcomeMessage =
    "FreeRTOS CLI.\r\nType Help to view a list of registered commands.\r\n";

/// Clear screen command definition.
const CLI_Command_Definition_t xClearScreen =
{
    CLI_COMMAND_CLEAR_SCREEN,          /**< Command string */
    CLI_HELP_CLEAR_SCREEN,             /**< Help text */
    CLI_CALLBACK_CLEAR_SCREEN,         /**< Callback function */
    CLI_PARAMS_CLEAR_SCREEN            /**< Expected parameters */
};

/// Reset command definition.
static const CLI_Command_Definition_t xResetCommand =
{
    "reset",                           /**< Command name */
    "reset: Resets the device\r\n",    /**< Help text */
    (const pdCOMMAND_LINE_CALLBACK)CLI_ResetDevice, /**< Callback function pointer */
    0                                  /**< Number of expected parameters */
};

/// Version command definition.
static const CLI_Command_Definition_t xVersionCommand =
{
    "version",                         /**< Command name */
    "version:\r\n Prints the firmware version.\r\n", /**< Help text */
    CLI_VersionCommand,                /**< Callback function pointer */
    0                                  /**< Number of expected parameters */
};

/// Ticks command definition.
static const CLI_Command_Definition_t xTicksCommand =
{
    "ticks",                           /**< Command name */
    "ticks:\r\n Prints the number of ticks since the scheduler started.\r\n", /**< Help text */
    CLI_TicksCommand,                  /**< Callback function pointer */
    0                                  /**< Number of expected parameters */
};

/******************************************************************************/
/* Forward Declarations                                                       */
/******************************************************************************/
/**
 * @brief Blocks until a character is available from the UART.
 *
 * This function waits until the USART callback gives the semaphore,
 * then retrieves a character from the circular receive buffer.
 *
 * @param[out] character Pointer to a character variable where the received
 *                       character will be stored.
 */
static void FreeRTOS_read(char *character);

/******************************************************************************/
/* CLI Thread                                                                 */
/******************************************************************************/
/**
 * @brief Task that handles the Command Line Interface (CLI).
 *
 * This task registers CLI commands, waits for user input character by character,
 * and processes complete command strings when a newline is received.
 *
 * @param[in] pvParameters Pointer to task parameters (unused).
 */
void vCommandConsoleTask(void *pvParameters)
{
    /* Register CLI commands */
    FreeRTOS_CLIRegisterCommand(&xClearScreen);
    FreeRTOS_CLIRegisterCommand(&xResetCommand);
    FreeRTOS_CLIRegisterCommand(&xVersionCommand);
    FreeRTOS_CLIRegisterCommand(&xTicksCommand);

    uint8_t cRxedChar[2], cInputIndex = 0;
    BaseType_t xMoreDataToFollow;
    /* Input and output buffers are declared static to keep them off the stack. */
    static char pcOutputString[MAX_OUTPUT_LENGTH_CLI], pcInputString[MAX_INPUT_LENGTH_CLI];
    static char pcLastCommand[MAX_INPUT_LENGTH_CLI];
    static bool isEscapeCode = false;
    static char pcEscapeCodes[4];
    static uint8_t pcEscapeCodePos = 0;

    /* Send a welcome message to the user to indicate the connection. */
    SerialConsoleWriteString(pcWelcomeMessage);
    char rxChar;
    for (;;)
    {
        /* Read a single character. The task blocks until a character is received. */
        FreeRTOS_read((char *)cRxedChar);

        if (cRxedChar[0] == '\n' || cRxedChar[0] == '\r')
        {
            /* Newline received: process the complete command string. */
            SerialConsoleWriteString("\r\n");
            /* Save the last command */
            isEscapeCode = false;
            pcEscapeCodePos = 0;
            strncpy(pcLastCommand, pcInputString, MAX_INPUT_LENGTH_CLI - 1);
            pcLastCommand[MAX_INPUT_LENGTH_CLI - 1] = 0; // Ensure null termination

            /* Process command string using the CLI command interpreter */
            do
            {
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
                    pcInputString,        /**< Command string */
                    pcOutputString,       /**< Output buffer */
                    MAX_OUTPUT_LENGTH_CLI /**< Size of output buffer */
                );

                /* Output the generated response */
                pcOutputString[MAX_OUTPUT_LENGTH_CLI - 1] = 0;
                SerialConsoleWriteString(pcOutputString);

            } while (xMoreDataToFollow != pdFALSE);

            /* Clear the input buffer for the next command */
            cInputIndex = 0;
            memset(pcInputString, 0x00, MAX_INPUT_LENGTH_CLI);
        }
        else
        {
            /* Process each received character */
            if (true == isEscapeCode)
            {
                if (pcEscapeCodePos < CLI_PC_ESCAPE_CODE_SIZE)
                {
                    pcEscapeCodes[pcEscapeCodePos++] = cRxedChar[0];
                }
                else
                {
                    isEscapeCode = false;
                    pcEscapeCodePos = 0;
                }

                if (pcEscapeCodePos >= CLI_PC_MIN_ESCAPE_CODE_SIZE)
                {
                    /* If UP arrow is detected, show last command */
                    if (strcasecmp(pcEscapeCodes, "oa"))
                    {
                        sprintf(pcInputString, "%c[2K\r>", 27);
                        SerialConsoleWriteString(pcInputString);
                        cInputIndex = 0;
                        memset(pcInputString, 0x00, MAX_INPUT_LENGTH_CLI);
                        strncpy(pcInputString, pcLastCommand, MAX_INPUT_LENGTH_CLI - 1);
                        cInputIndex = (strlen(pcInputString) < MAX_INPUT_LENGTH_CLI - 1) ?
                                        strlen(pcLastCommand) : MAX_INPUT_LENGTH_CLI - 1;
                        SerialConsoleWriteString(pcInputString);
                    }

                    isEscapeCode = false;
                    pcEscapeCodePos = 0;
                }
            }
            else if (cRxedChar[0] == '\r')
            {
                /* Ignore carriage returns. */
            }
            else if (cRxedChar[0] == ASCII_BACKSPACE || cRxedChar[0] == ASCII_DELETE)
            {
                char erase[4] = {0x08, 0x20, 0x08, 0x00};
                SerialConsoleWriteString(erase);
                if (cInputIndex > 0)
                {
                    cInputIndex--;
                    pcInputString[cInputIndex] = 0;
                }
            }
            else if (cRxedChar[0] == ASCII_ESC)
            {
                isEscapeCode = true;  /**< Next characters are part of an escape sequence */
                pcEscapeCodePos = 0;
            }
            else
            {
                /* Regular character: add to input buffer and echo it */
                if (cInputIndex < MAX_INPUT_LENGTH_CLI)
                {
                    pcInputString[cInputIndex] = cRxedChar[0];
                    cInputIndex++;
                }
                cRxedChar[1] = 0;
                SerialConsoleWriteString((char *)&cRxedChar[0]);
            }
        }
    }
}

/**************************************************************************//**
 * @fn          static void FreeRTOS_read(char *character)
 * @brief       Blocks until a character is available from the UART.
 * @param[out]  character Pointer to the location where the received character is stored.
 * @return      None.
 *****************************************************************************/
static void FreeRTOS_read(char *character)
{
    /* Block until a character is available (the semaphore is given in the USART callback) */
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
    {
        /* Retrieve the character from the circular RX buffer */
        if (circular_buf_get(cbufRx, (uint8_t *)character) == -1)
        {
            /* In the unlikely event the buffer is empty, set the character to null */
            *character = '\0';
        }
    }
}

/******************************************************************************/
/* CLI Functions                                                              */
/******************************************************************************/
/**************************************************************************//**
 * @fn          BaseType_t xCliClearTerminalScreen(char *pcWriteBuffer, size_t xWriteBufferLen, 
 *                                                   const int8_t *pcCommandString)
 * @brief       Clears the terminal screen using VT100 escape sequences.
 * @param[out]  pcWriteBuffer Pointer to the buffer into which the output is written.
 * @param[in]   xWriteBufferLen Length of the write buffer.
 * @param[in]   pcCommandString The command string (unused).
 * @return      pdFALSE after the command has been processed.
 *****************************************************************************/
static char bufCli[CLI_MSG_LEN];
BaseType_t xCliClearTerminalScreen(char *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    char clearScreen = ASCII_ESC;
    snprintf(bufCli, CLI_MSG_LEN - 1, "%c[2J", clearScreen);
    snprintf(pcWriteBuffer, xWriteBufferLen, bufCli);
    return pdFALSE;
}

/**************************************************************************//**
 * @fn          BaseType_t CLI_ResetDevice(int8_t *pcWriteBuffer, size_t xWriteBufferLen, 
 *                                            const int8_t *pcCommandString)
 * @brief       Resets the device.
 * @param[out]  pcWriteBuffer Pointer to the output buffer.
 * @param[in]   xWriteBufferLen Size of the output buffer.
 * @param[in]   pcCommandString The command string (unused).
 * @return      pdFALSE after the device reset command is issued.
 *****************************************************************************/
BaseType_t CLI_ResetDevice(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    system_reset();
    return pdFALSE;
}

/**************************************************************************//**
 * @fn          BaseType_t CLI_VersionCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, 
 *                                               const int8_t *pcCommandString)
 * @brief       Prints the firmware version.
 * @param[out]  pcWriteBuffer Pointer to the output buffer.
 * @param[in]   xWriteBufferLen Size of the output buffer.
 * @param[in]   pcCommandString The command string (unused).
 * @return      pdFALSE after printing the version.
 *****************************************************************************/
BaseType_t CLI_VersionCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    snprintf((char *)pcWriteBuffer, xWriteBufferLen, "Firmware version: %s\r\n", FIRMWARE_VERSION);
    return pdFALSE;
}

/**************************************************************************//**
 * @fn          BaseType_t CLI_TicksCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, 
 *                                             const int8_t *pcCommandString)
 * @brief       Prints the number of ticks since the scheduler started.
 * @param[out]  pcWriteBuffer Pointer to the output buffer.
 * @param[in]   xWriteBufferLen Size of the output buffer.
 * @param[in]   pcCommandString The command string (unused).
 * @return      pdFALSE after printing the tick count.
 *****************************************************************************/
BaseType_t CLI_TicksCommand(int8_t *pcWriteBuffer, size_t xWriteBufferLen, const int8_t *pcCommandString)
{
    TickType_t ticks = xTaskGetTickCount();
    snprintf((char *)pcWriteBuffer, xWriteBufferLen, "Ticks: %lu\r\n", (unsigned long)ticks);
    return pdFALSE;
}
