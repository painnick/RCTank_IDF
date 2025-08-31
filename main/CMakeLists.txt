set(srcs "main.c" "my_flatform.c" "rc_tank.c" "dfplayer.c")

set(requires "bluepad32" "btstack" "driver" "nvs_flash" "esp_driver_mcpwm" "esp_driver_ledc")

idf_component_register(SRCS "${srcs}"
        INCLUDE_DIRS "."
        REQUIRES "${requires}")
