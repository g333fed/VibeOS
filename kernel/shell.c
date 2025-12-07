/*
 * VibeOS Shell
 *
 * Simple command-line shell with built-in commands.
 */

#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "string.h"
#include "printf.h"
#include "fb.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 16

// Command buffer
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_pos = 0;

// External memory functions
extern size_t memory_free(void);
extern size_t memory_used(void);

// ============ Command Handlers ============

static void cmd_help(void) {
    console_puts("Available commands:\n");
    console_puts("  ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("help");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts(" - Show available commands\n");

    console_puts("  ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("clear");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts(" - Clear the screen\n");

    console_puts("  ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("echo");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts(" - Print arguments to screen\n");

    console_puts("  ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("version");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts(" - Show VibeOS version\n");

    console_puts("  ");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("mem");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts(" - Show memory information\n");
}

static void cmd_clear(void) {
    console_clear();
}

static void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        console_puts(argv[i]);
        if (i < argc - 1) {
            console_putc(' ');
        }
    }
    console_putc('\n');
}

static void cmd_version(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("VibeOS");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts(" v0.1 - aarch64\n");
    console_puts("Built for QEMU virt machine\n");
    console_puts("The vibes are immaculate.\n");
}

static void cmd_memory(void) {
    size_t free_bytes = memory_free();
    size_t used_bytes = memory_used();

    uint32_t free_mb = (uint32_t)(free_bytes / 1024 / 1024);

    console_puts("Memory:\n");
    printf("  Used: %u bytes\n", (uint32_t)used_bytes);
    printf("  Free: %u MB\n", free_mb);
}

// ============ Shell Core ============

// Parse command line into argc/argv
static int parse_command(char *cmd, char *argv[], int max_args) {
    int argc = 0;
    char *p = cmd;

    while (*p && argc < max_args) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        // Found start of argument
        argv[argc++] = p;

        // Find end of argument
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }

        // Null-terminate this argument
        if (*p) {
            *p++ = '\0';
        }
    }

    return argc;
}

// Simple string comparison that definitely works
static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return (*a == *b);
}

// Execute a command
static void execute_command(char *cmd) {
    char *argv[MAX_ARGS];
    int argc = parse_command(cmd, argv, MAX_ARGS);

    if (argc == 0) {
        return;  // Empty command
    }

    // Match commands directly
    if (str_eq(argv[0], "help")) {
        cmd_help();
    } else if (str_eq(argv[0], "clear")) {
        cmd_clear();
    } else if (str_eq(argv[0], "echo")) {
        cmd_echo(argc, argv);
    } else if (str_eq(argv[0], "version")) {
        cmd_version();
    } else if (str_eq(argv[0], "mem")) {
        cmd_memory();
    } else {
        // Command not found
        console_set_color(COLOR_RED, COLOR_BLACK);
        console_puts("Unknown command: ");
        console_set_color(COLOR_WHITE, COLOR_BLACK);
        console_puts(argv[0]);
        console_puts("\nType 'help' for available commands.\n");
    }
}

// Print the shell prompt
static void print_prompt(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_puts("vibe");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts("> ");
}

void shell_init(void) {
    cmd_pos = 0;
    cmd_buffer[0] = '\0';
}

void shell_run(void) {
    shell_init();

    console_puts("\n");
    console_set_color(COLOR_AMBER, COLOR_BLACK);
    console_puts("Welcome to VibeOS Shell!\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_puts("Type 'help' for available commands.\n\n");

    print_prompt();

    while (1) {
        int c = keyboard_getc();

        if (c < 0) {
            // No key available, keep polling
            continue;
        }

        if (c == '\n' || c == '\r') {
            // Enter pressed - execute command
            console_putc('\n');
            cmd_buffer[cmd_pos] = '\0';

            if (cmd_pos > 0) {
                execute_command(cmd_buffer);
            }

            // Reset for next command
            cmd_pos = 0;
            cmd_buffer[0] = '\0';
            print_prompt();

        } else if (c == '\b' || c == 127) {
            // Backspace
            if (cmd_pos > 0) {
                cmd_pos--;
                console_putc('\b');
            }

        } else if (c >= 32 && c < 127) {
            // Printable character
            if (cmd_pos < MAX_CMD_LEN - 1) {
                cmd_buffer[cmd_pos++] = (char)c;
                console_putc((char)c);
            }
        }
    }
}
