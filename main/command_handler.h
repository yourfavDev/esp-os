/**
 * @file command_handler.h
 * @brief Processes string-based commands for controlling the device.
 *
 * This module is responsible for parsing and executing commands received
 * from an external interface, such as the BLE service. It acts as a
 * dispatcher, calling the appropriate functions from other modules based
 * on the command received.
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

/**
 * @brief Processes a command string.
 *
 * Parses the command and its arguments, then executes the corresponding action.
 *
 * @param command The null-terminated command string to process.
 */
void command_handler_process(const char *command);

#endif // COMMAND_HANDLER_H
