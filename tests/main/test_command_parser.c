/*
 * SPDX-License-Identifier: MIT
 */

#include "presenter_console.h"
#include "unity.h"

static void assert_command(const char *text, presenter_command_type_t expected)
{
    const presenter_console_parsed_t parsed = presenter_console_parse(text);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_COMMAND, parsed.result);
    TEST_ASSERT_EQUAL(expected, parsed.command_type);
}

TEST_CASE("parser accepts every allowlisted named command", "[parser]")
{
    assert_command("next", PRESENTER_COMMAND_NEXT);
    assert_command("previous", PRESENTER_COMMAND_PREVIOUS);
    assert_command("start", PRESENTER_COMMAND_START);
    assert_command("start-current", PRESENTER_COMMAND_START_CURRENT);
    assert_command("stop", PRESENTER_COMMAND_STOP);
    assert_command("black", PRESENTER_COMMAND_BLACK);
    assert_command("white", PRESENTER_COMMAND_WHITE);
    assert_command("first", PRESENTER_COMMAND_FIRST);
    assert_command("last", PRESENTER_COMMAND_LAST);
}

TEST_CASE("parser is case insensitive and trims whitespace", "[parser]")
{
    assert_command("  NeXt  ", PRESENTER_COMMAND_NEXT);
    assert_command("START-CURRENT", PRESENTER_COMMAND_START_CURRENT);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_EMPTY, presenter_console_parse(" \t ").result);
}

TEST_CASE("parser validates goto range and syntax", "[parser]")
{
    presenter_console_parsed_t parsed = presenter_console_parse("goto 1");
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_COMMAND, parsed.result);
    TEST_ASSERT_EQUAL(PRESENTER_COMMAND_GOTO_SLIDE, parsed.command_type);
    TEST_ASSERT_EQUAL_UINT16(1, parsed.slide_number);

    parsed = presenter_console_parse("GOTO 9999");
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_COMMAND, parsed.result);
    TEST_ASSERT_EQUAL_UINT16(9999, parsed.slide_number);

    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_SLIDE_NUMBER, presenter_console_parse("goto 0").result);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_SLIDE_NUMBER, presenter_console_parse("goto 10000").result);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_SLIDE_NUMBER, presenter_console_parse("goto -5").result);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_SLIDE_NUMBER, presenter_console_parse("goto abc").result);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_SLIDE_NUMBER, presenter_console_parse("goto 12 13").result);
}

TEST_CASE("parser rejects unknown and overlong input", "[parser]")
{
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_UNSUPPORTED,
                      presenter_console_parse("send CTRL+ALT+DELETE").result);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_UNSUPPORTED,
                      presenter_console_parse("next now").result);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_ERROR_TOO_LONG,
                      presenter_console_parse(
                          "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx").result);
}

TEST_CASE("parser recognizes non-HID commands", "[parser]")
{
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_STATUS, presenter_console_parse("status").result);
    TEST_ASSERT_EQUAL(CONSOLE_PARSE_HELP, presenter_console_parse("help").result);
}
