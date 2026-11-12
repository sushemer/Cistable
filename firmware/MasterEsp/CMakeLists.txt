# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/Apoyo/field_espnow"
                        "${CMAKE_CURRENT_LIST_DIR}/Apoyo/imu_bno085"
                        "${CMAKE_CURRENT_LIST_DIR}/Apoyo/components/esp32_BNO08x")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MasterEsp)
