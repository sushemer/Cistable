# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(BNO08X_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/Apoyo/components/esp32_BNO08x")
if(NOT EXISTS "${BNO08X_COMPONENT_DIR}/CMakeLists.txt")
    set(BNO08X_COMPONENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../Apoyo/esp32_BNO08x")
endif()

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/Apoyo/field_espnow"
                        "${CMAKE_CURRENT_LIST_DIR}/Apoyo/imu_bno085"
                        "${BNO08X_COMPONENT_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MasterEsp)
