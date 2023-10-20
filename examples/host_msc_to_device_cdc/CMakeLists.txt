set(target_name host_msc_to_device_cdc)
add_executable(${target_name})

target_sources(${target_name} PRIVATE
 host_msc_to_device_cdc.c
 usb_descriptors.c
 # can use 'tinyusb_pico_pio_usb' library later when pico-sdk is updated
 ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
 ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
 ${PICO_TINYUSB_PATH}/lib/fatfs/source/ff.c
 ${PICO_TINYUSB_PATH}/lib/fatfs/source/ffsystem.c
 ${PICO_TINYUSB_PATH}/lib/fatfs/source/ffunicode.c
 )

 # Suppress warnings on fatfs
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(
    ${PICO_TINYUSB_PATH}/lib/fatfs/source/ff.c
    PROPERTIES
    COMPILE_FLAGS "-Wno-conversion -Wno-cast-qual"
    )
endif ()

 # print memory usage, enable all warnings
target_link_options(${target_name} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_name} PRIVATE -Wall -Wextra)

# use tinyusb implementation
target_compile_definitions(${target_name} PRIVATE PIO_USB_USE_TINYUSB)

# needed so tinyusb can find tusb_config.h
target_include_directories(${target_name} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_TINYUSB_PATH}/lib/fatfs/source/
    )

target_link_libraries(${target_name} PRIVATE pico_stdlib pico_pio_usb tinyusb_device tinyusb_host)
pico_add_extra_outputs(${target_name})

