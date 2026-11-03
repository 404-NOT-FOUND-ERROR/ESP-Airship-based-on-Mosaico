idf_component_register(SRCS "app_motor_control_core.c" "app_motor_control_inbox.c"
    "app_motor_control_task_idf.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private"
    REQUIRES app_motor_safety
    PRIV_REQUIRES freertos)
