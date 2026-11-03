cmake_minimum_required(VERSION 3.20)
project(app_motor_safety_host C)

enable_testing()
add_executable(app_motor_safety_test
    ../app_motor_safety.c
    test_app_motor_safety.c)
target_include_directories(app_motor_safety_test PRIVATE ../include)
add_test(NAME app_motor_safety_test COMMAND app_motor_safety_test)

add_executable(airboat_motor_console
    ../app_motor_safety.c
    motor_console.c)
target_include_directories(airboat_motor_console PRIVATE ../include)
