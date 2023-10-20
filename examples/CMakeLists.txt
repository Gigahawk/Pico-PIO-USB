
cmake_minimum_required(VERSION 3.13)
include (pico_sdk_import.cmake)
project(pio_usb)

pico_sdk_init()

if(DEFINED DP_PIN)
    message("Setting PIO_USB_DP_PIN_DEFAULT to ${DP_PIN}")
    add_compile_definitions(PIO_USB_DP_PIN_DEFAULT=${DP_PIN})
endif()

# add pico-pio-usb library for examples
set(PICO_PIO_USB_DIR "${CMAKE_CURRENT_LIST_DIR}/../")
# the second argument to add_subdirectory is needed here because this isn't
# a subdirectory, it's out of tree.
add_subdirectory(${PICO_PIO_USB_DIR} pico_pio_usb)

add_subdirectory(capture_hid_report)
add_subdirectory(usb_device)
add_subdirectory(host_hid_to_device_cdc)
add_subdirectory(test_ll)
