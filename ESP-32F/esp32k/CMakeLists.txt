# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(ESP_IDF_LOCAL_PATH "/Users/GeorgeSystem/Develop/Environment/esp/esp-idf-v5.5.4")
set(ESP_IDF_LOCAL_PYTHON "/Users/george/.espressif/python_env/idf5.5_py3.11_env/bin/python")
set(ESP_IDF_LOCAL_PYTHON_BIN "/Users/george/.espressif/python_env/idf5.5_py3.11_env/bin")
set(ESP_IDF_LOCAL_TOOLCHAIN_BIN "/Users/george/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin")
set(ESP_IDF_LOCAL_GDB_BIN "/Users/george/.espressif/tools/xtensa-esp-elf-gdb/16.3_20250913/xtensa-esp-elf-gdb/bin")
set(ESP_IDF_LOCAL_ULP_BIN "/Users/george/.espressif/tools/esp32ulp-elf/2.38_20240113/esp32ulp-elf/bin")
set(ESP_IDF_LOCAL_OPENOCD_BIN "/Users/george/.espressif/tools/openocd-esp32/v0.12.0-esp32-20251215/openocd-esp32/bin")
set(ESP_IDF_LOCAL_ROM_ELF_DIR "/Users/george/.espressif/tools/esp-rom-elfs/20241011/")

if(NOT DEFINED ENV{IDF_PATH})
    set(ENV{IDF_PATH} "${ESP_IDF_LOCAL_PATH}")
endif()

set(ENV{ESP_ROM_ELF_DIR} "${ESP_IDF_LOCAL_ROM_ELF_DIR}")
set(ENV{PATH} "${ESP_IDF_LOCAL_PYTHON_BIN}:${ESP_IDF_LOCAL_TOOLCHAIN_BIN}:${ESP_IDF_LOCAL_GDB_BIN}:${ESP_IDF_LOCAL_ULP_BIN}:${ESP_IDF_LOCAL_OPENOCD_BIN}:${ESP_IDF_LOCAL_PATH}/tools:$ENV{PATH}")
set(PYTHON "${ESP_IDF_LOCAL_PYTHON}" CACHE FILEPATH "ESP-IDF Python interpreter")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(esp32k)
