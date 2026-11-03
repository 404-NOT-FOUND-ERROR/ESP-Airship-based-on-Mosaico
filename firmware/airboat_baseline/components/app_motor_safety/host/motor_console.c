// SPDX-License-Identifier: Apache-2.0

#include "app_motor_safety.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t now_ms;

static void print_snapshot(const app_motor_safety_snapshot_t *snapshot)
{
    printf("t=%" PRIu32 " armed=%s estop=%s fresh=%s age=%" PRIu32
           " P=%u/%u Y=%u/%u L=%u/%u\n",
           now_ms, snapshot->armed ? "yes" : "no",
           snapshot->emergency_stop_latched ? "latched" : "clear",
           snapshot->command_fresh ? "yes" : "no", snapshot->command_age_ms,
           snapshot->motor[APP_MOTOR_SAFETY_PROPULSION].input_a_permille,
           snapshot->motor[APP_MOTOR_SAFETY_PROPULSION].input_b_permille,
           snapshot->motor[APP_MOTOR_SAFETY_YAW].input_a_permille,
           snapshot->motor[APP_MOTOR_SAFETY_YAW].input_b_permille,
           snapshot->motor[APP_MOTOR_SAFETY_LIFT].input_a_permille,
           snapshot->motor[APP_MOTOR_SAFETY_LIFT].input_b_permille);
}

static size_t split_line(char *line, char **tokens, size_t capacity)
{
    size_t count = 0;
    char *token = strtok(line, " \t\r\n");
    while (token) {
        if (count == capacity) {
            return capacity + 1;
        }
        tokens[count++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    return count;
}

static bool parse_signed(const char *token, int16_t *out)
{
    char *end = NULL;
    errno = 0;
    const long value = strtol(token, &end, 10);
    if (errno || end == token || *end != '\0' || value < INT16_MIN ||
        value > INT16_MAX) {
        return false;
    }
    *out = (int16_t)value;
    return true;
}

static const char *result_name(app_motor_safety_result_t result)
{
    switch (result) {
    case APP_MOTOR_SAFETY_OK:
        return "OK";
    case APP_MOTOR_SAFETY_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case APP_MOTOR_SAFETY_NO_MEMORY:
        return "NO_MEMORY";
    case APP_MOTOR_SAFETY_INVALID_STATE:
        return "INVALID_STATE";
    }
    return "UNKNOWN";
}

static void print_result(const char *action, app_motor_safety_result_t result)
{
    printf("%s: %s\n", action, result_name(result));
}

static void show_state(app_motor_safety_handle_t handle)
{
    app_motor_safety_snapshot_t snapshot;
    if (app_motor_safety_tick(handle, now_ms, &snapshot) != APP_MOTOR_SAFETY_OK) {
        fputs("state read failed\n", stderr);
        return;
    }
    print_snapshot(&snapshot);
}

static void show_help(void)
{
    puts("Commands:");
    puts("  arm                         enable command processing");
    puts("  cmd <propulsion> <yaw> <lift>  submit -1000..1000");
    puts("  advance <milliseconds>     advance simulated clock and tick");
    puts("  disarm                     clear command and stop");
    puts("  estop                      latch emergency stop");
    puts("  clear                      clear emergency stop while disarmed");
    puts("  state                      print the current snapshot");
    puts("  help                       show this list");
    puts("  quit                       exit");
}

int main(void)
{
    app_motor_safety_handle_t handle = NULL;
    if (app_motor_safety_create(&handle) != APP_MOTOR_SAFETY_OK) {
        fputs("could not create motor safety controller\n", stderr);
        return 1;
    }

    puts("AirBoat motor safety dry run (no GPIO/PWM; simulated clock)");
    show_help();
    show_state(handle);

    char line[160];
    while (fgets(line, sizeof(line), stdin)) {
        char *tokens[5];
        const size_t count = split_line(line, tokens, 5);
        if (count == 0) {
            continue;
        }
        const char *command = tokens[0];
        if (strcmp(command, "help") == 0) {
            show_help();
        } else if (strcmp(command, "state") == 0) {
            show_state(handle);
        } else if (strcmp(command, "arm") == 0) {
            print_result("arm", app_motor_safety_arm(handle));
            show_state(handle);
        } else if (strcmp(command, "disarm") == 0) {
            print_result("disarm", app_motor_safety_disarm(handle));
            show_state(handle);
        } else if (strcmp(command, "estop") == 0) {
            print_result("estop", app_motor_safety_latch_emergency_stop(handle));
            show_state(handle);
        } else if (strcmp(command, "clear") == 0) {
            print_result("clear", app_motor_safety_clear_emergency_stop(handle));
            show_state(handle);
        } else if (strcmp(command, "advance") == 0) {
            if (count != 2) {
                puts("usage: advance <milliseconds>");
                continue;
            }
            char *end = NULL;
            errno = 0;
            const unsigned long value = strtoul(tokens[1], &end, 10);
            if (errno || end == tokens[1] || *end != '\0' ||
                value > INT32_MAX) {
                puts("milliseconds must be in 0..2147483647");
                continue;
            }
            const uint32_t delta = (uint32_t)value;
            now_ms += delta;
            show_state(handle);
        } else if (strcmp(command, "cmd") == 0) {
            if (count != 4) {
                puts("usage: cmd <propulsion> <yaw> <lift>");
                continue;
            }
            int16_t propulsion;
            int16_t yaw;
            int16_t lift;
            if (!parse_signed(tokens[1], &propulsion) ||
                !parse_signed(tokens[2], &yaw) || !parse_signed(tokens[3], &lift)) {
                puts("axis values must be signed 16-bit integers");
                continue;
            }
            const app_motor_safety_command_t value = {
                .axis = {propulsion, yaw, lift}};
            print_result("cmd", app_motor_safety_submit(handle, &value, now_ms));
            show_state(handle);
        } else if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
            break;
        } else {
            puts("unknown command; enter help");
        }
    }

    app_motor_safety_disarm(handle);
    app_motor_safety_destroy(&handle);
    return 0;
}
