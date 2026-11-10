target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    tftlcd.c
    screen_delay.c
    touch.c
    iic.c
    24cxx.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
