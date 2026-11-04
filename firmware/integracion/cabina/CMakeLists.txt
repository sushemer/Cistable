cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components"
    "${CMAKE_CURRENT_LIST_DIR}/../../componentes")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cistable_cabina_integrada)

if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "Hardware vigente ESP32-S3: usar build/s3/... y sdkconfig S3; builds clasicos son historicos")
endif()
