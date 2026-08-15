#pragma once

// ANSI Escape Codes for Terminal styling
#define ANSI_RESET          "\x1B[0m"
#define ANSI_BOLD           "\x1B[1m"
#define ANSI_DIM            "\x1B[2m"
#define ANSI_UNDERLINE      "\x1B[4m"
#define ANSI_BLINK          "\x1B[5m"
#define ANSI_REVERSE        "\x1B[7m"

// Foreground Colors
#define ANSI_FG_BLACK       "\x1B[30m"
#define ANSI_FG_RED         "\x1B[31m"
#define ANSI_FG_GREEN       "\x1B[32m"
#define ANSI_FG_YELLOW      "\x1B[33m"
#define ANSI_FG_BLUE        "\x1B[34m"
#define ANSI_FG_MAGENTA     "\x1B[35m"
#define ANSI_FG_CYAN        "\x1B[36m"
#define ANSI_FG_WHITE       "\x1B[37m"

#define ANSI_FG_BRIGHT_BLACK   "\x1B[90m"
#define ANSI_FG_BRIGHT_RED     "\x1B[91m"
#define ANSI_FG_BRIGHT_GREEN   "\x1B[92m"
#define ANSI_FG_BRIGHT_YELLOW  "\x1B[93m"
#define ANSI_FG_BRIGHT_BLUE    "\x1B[94m"
#define ANSI_FG_BRIGHT_MAGENTA "\x1B[95m"
#define ANSI_FG_BRIGHT_CYAN    "\x1B[96m"
#define ANSI_FG_BRIGHT_WHITE   "\x1B[97m"

// Background Colors
#define ANSI_BG_BLACK       "\x1B[40m"
#define ANSI_BG_RED         "\x1B[41m"
#define ANSI_BG_GREEN       "\x1B[42m"
#define ANSI_BG_YELLOW      "\x1B[43m"
#define ANSI_BG_BLUE        "\x1B[44m"
#define ANSI_BG_MAGENTA     "\x1B[45m"
#define ANSI_BG_CYAN        "\x1B[46m"
#define ANSI_BG_WHITE       "\x1B[47m"

#define ANSI_BG_BRIGHT_BLUE "\x1B[104m"
#define ANSI_BG_DARK_GRAY   "\x1B[100m"

// Screen Control
#define ANSI_CLEAR_SCREEN   "\x1B[2J"
#define ANSI_CURSOR_HOME    "\x1B[H"
#define ANSI_HIDE_CURSOR    "\x1B[?25l"
#define ANSI_SHOW_CURSOR    "\x1B[?25h"
