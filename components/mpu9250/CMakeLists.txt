idf_component_register(SRCS "mpu9250.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver
                                  moving_average)
