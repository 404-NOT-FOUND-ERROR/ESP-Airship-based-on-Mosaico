idf_component_register(SRCS "app_imu.c" "app_imu_model.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private"
    REQUIRES esp_common
    PRIV_REQUIRES esp-mosaico-bsp esp_timer freertos log)
