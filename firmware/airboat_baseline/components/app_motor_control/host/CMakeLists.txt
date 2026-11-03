cmake_minimum_required(VERSION 3.20)
project(app_motor_control_host LANGUAGES C)

add_executable(app_motor_control_test
    ../app_motor_control_core.c
    ../app_motor_control_inbox.c
    ../app_motor_control_task_pc.c
    ../../app_motor_safety/app_motor_safety.c
    test_app_motor_control.c)
target_include_directories(app_motor_control_test PRIVATE
    ../include ../private ../../app_motor_safety/include)
