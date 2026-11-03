cmake_minimum_required(VERSION 3.20)
project(airboat_ui_backend LANGUAGES C)

if(NOT ESP_GSP_COMPONENT_DIR AND DEFINED ENV{ESP_GSP_COMPONENT_DIR})
    set(ESP_GSP_COMPONENT_DIR "$ENV{ESP_GSP_COMPONENT_DIR}")
endif()
if(NOT ESP_GSP_COMPONENT_DIR)
    foreach(candidate IN ITEMS
            "${CMAKE_CURRENT_LIST_DIR}/../../../managed_components/espressif__esp-gsp")
        get_filename_component(candidate "${candidate}" ABSOLUTE)
        if(EXISTS "${candidate}/tools/sim_bridge/CMakeLists.txt")
            set(ESP_GSP_COMPONENT_DIR "${candidate}")
            break()
        endif()
    endforeach()
endif()
if(NOT EXISTS "${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge/CMakeLists.txt")
    message(FATAL_ERROR
        "Install espressif/esp-gsp first, or set ESP_GSP_COMPONENT_DIR")
endif()

add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(airboat_ui_backend
    SOURCES ../app_airboat_ui.c ../../app_motor_control/app_motor_control_core.c
        ../../app_motor_control/app_motor_control_inbox.c
        ../../app_motor_control/app_motor_control_task_pc.c
        ../../app_motor_safety/app_motor_safety.c platform_pc.c
    INCLUDE_DIRECTORIES ../include ../private ../../app_motor_control/include
        ../../app_motor_control/private ../../app_motor_safety/include
    SCENES ../ui/main.json)
