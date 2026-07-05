cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS components/esurfing_core)

project(esp32_esurfing)

spiffs_create_partition_image(spiffs spiffs_data FLASH_IN_PROJECT)
