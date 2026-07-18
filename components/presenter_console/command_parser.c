/*
 * SPDX-License-Identifier: MIT
 */

#include "presenter_console.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    presenter_command_type_t type;
} named_command_t;

static const named_command_t s_commands[] = {
    {"next", PRESENTER_COMMAND_NEXT},
    {"previous", PRESENTER_COMMAND_PREVIOUS},
    {"start", PRESENTER_COMMAND_START},
    {"start-current", PRESENTER_COMMAND_START_CURRENT},
    {"stop", PRESENTER_COMMAND_STOP},
    {"black", PRESENTER_COMMAND_BLACK},
    {"white", PRESENTER_COMMAND_WHITE},
    {"first", PRESENTER_COMMAND_FIRST},
    {"last", PRESENTER_COMMAND_LAST},
};

static char *trim(char *text)
{
    while (isspace((unsigned char)*text)) {
        ++text;
    }
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static void lowercase(char *text)
{
    for (; *text != '\0'; ++text) {
        *text = (char)tolower((unsigned char)*text);
    }
}

presenter_console_parsed_t presenter_console_parse(const char *line)
{
    presenter_console_parsed_t parsed = {.result = CONSOLE_PARSE_ERROR_UNSUPPORTED};
    if (line == NULL) {
        return parsed;
    }

    const size_t length = strnlen(line, PRESENTER_CONSOLE_MAX_LINE_LENGTH + 1U);
    if (length > PRESENTER_CONSOLE_MAX_LINE_LENGTH) {
        parsed.result = CONSOLE_PARSE_ERROR_TOO_LONG;
        return parsed;
    }

    char buffer[PRESENTER_CONSOLE_MAX_LINE_LENGTH + 1U];
    memcpy(buffer, line, length + 1U);
    char *command = trim(buffer);
    if (*command == '\0') {
        parsed.result = CONSOLE_PARSE_EMPTY;
        return parsed;
    }
    lowercase(command);

    if (strcmp(command, "status") == 0) {
        parsed.result = CONSOLE_PARSE_STATUS;
        return parsed;
    }
    if (strcmp(command, "help") == 0) {
        parsed.result = CONSOLE_PARSE_HELP;
        return parsed;
    }
    for (size_t i = 0; i < sizeof(s_commands) / sizeof(s_commands[0]); ++i) {
        if (strcmp(command, s_commands[i].name) == 0) {
            parsed.result = CONSOLE_PARSE_COMMAND;
            parsed.command_type = s_commands[i].type;
            return parsed;
        }
    }

    if (strncmp(command, "goto", 4) == 0) {
        char *number = command + 4;
        if (!isspace((unsigned char)*number)) {
            parsed.result = CONSOLE_PARSE_ERROR_SLIDE_NUMBER;
            return parsed;
        }
        while (isspace((unsigned char)*number)) {
            ++number;
        }
        if (*number == '\0') {
            parsed.result = CONSOLE_PARSE_ERROR_SLIDE_NUMBER;
            return parsed;
        }
        for (const char *p = number; *p != '\0'; ++p) {
            if (!isdigit((unsigned char)*p)) {
                parsed.result = CONSOLE_PARSE_ERROR_SLIDE_NUMBER;
                return parsed;
            }
        }
        const unsigned long value = strtoul(number, NULL, 10);
        if (value < 1 || value > 9999) {
            parsed.result = CONSOLE_PARSE_ERROR_SLIDE_NUMBER;
            return parsed;
        }
        parsed.result = CONSOLE_PARSE_COMMAND;
        parsed.command_type = PRESENTER_COMMAND_GOTO_SLIDE;
        parsed.slide_number = (uint16_t)value;
        return parsed;
    }

    return parsed;
}
