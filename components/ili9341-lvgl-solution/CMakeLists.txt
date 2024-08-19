idf_component_register(SRCS "ili9341-lvgl-solution.c"
                    INCLUDE_DIRS "include"
                    REQUIRES lvgl esp_lcd
                    PRIV_REQUIRES driver)
