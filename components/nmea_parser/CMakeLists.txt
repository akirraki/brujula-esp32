idf_component_register(SRCS "nmea_parser.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver
                                  esp_event)
