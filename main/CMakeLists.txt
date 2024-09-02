idf_component_register(SRCS "brujula.c"
                            "gps.c"
                            "rest_server.c"
                            "wifi_ap.c"
                            "indev.c"
                            "tasks.c"
                            "storage.c"
                            "display_measurements.c"
                    INCLUDE_DIRS ".")

# Define the directory where your web files are located
set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../frontend")

# Check if the web directory exists
if(EXISTS ${WEB_SRC_DIR})
    # Create a SPIFFS image from the web directory
    spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR} doesn't exist. Please create this directory and add your web files.")
endif()

spiffs_create_partition_image(caldata ../reset FLASH_IN_PROJECT)