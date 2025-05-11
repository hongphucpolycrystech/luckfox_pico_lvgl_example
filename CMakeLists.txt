cmake_minimum_required(VERSION 3.10)

set(SDK_PATH $ENV{LUCKFOX_SDK_PATH})
set(STAGING_DIR $ENV{STAGING_DIR_PATH})

set(CMAKE_C_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc")
set(CMAKE_CXX_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-g++")
set(OpenCV_DIR "${CMAKE_SOURCE_DIR}/lib/opencv4/cmake/opencv4")
find_package(OpenCV REQUIRED)

project(ircam)

include_directories( 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uclibc/drm
    ${CMAKE_SOURCE_DIR}/include/uclibc/cjson
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/lib/lvgl
    ${CMAKE_SOURCE_DIR}/lib/lv_drivers
    # ${CMAKE_SOURCE_DIR}/custom 
    # ${CMAKE_SOURCE_DIR}/generated 
    # ${CMAKE_SOURCE_DIR}/generated/guider_customer_fonts 
    # ${CMAKE_SOURCE_DIR}/generated/guider_fonts
    # ${CMAKE_SOURCE_DIR}/generated/images 
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/ui/components
    ${CMAKE_SOURCE_DIR}/ui/fonts
    ${OpenCV_INCLUDE_DIRS}
)
link_directories(${CMAKE_SOURCE_DIR}/lib/uclibc/libdrm
                    ${CMAKE_SOURCE_DIR}/lib/uclibc/libcjson
                    ${STAGING_DIR}/usr/lib  )
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 
SET(CMAKE_INSTALL_RPATH "/usr/lib")


# lvgl
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/lvgl EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/lv_drivers EXCLUDE_FROM_ALL)


file(GLOB SRC ${CMAKE_SOURCE_DIR}/src/*.c
              ${CMAKE_SOURCE_DIR}/src/*.cpp
              ${CMAKE_SOURCE_DIR}/ui/*.c
              ${CMAKE_SOURCE_DIR}/ui/components/*.c
              ${CMAKE_SOURCE_DIR}/ui/fonts/*.c
              ${CMAKE_SOURCE_DIR}/ui/screens/*.c
            #  ${CMAKE_SOURCE_DIR}/custom/*.c
            # ${CMAKE_SOURCE_DIR}/generated/*.c
            # ${CMAKE_SOURCE_DIR}/generated/images/*.c
            # ${CMAKE_SOURCE_DIR}/generated/guider_fonts/*.c
)
# SET(UI_SOURCES 
#     ui/components/ui_comp_btnclick.c
#     ui/components/ui_comp.c
#     ui/screens/ui_Screen1.c
#     ui/ui.c
#     ui/components/ui_comp_hook.c
#     ui/ui_helpers.c)

# add_library(ui ${UI_SOURCES})

add_executable(${PROJECT_NAME} 
            ${SRC}            
    )
# debug
add_compile_options(-fPIC -Wall -O3 -g0)
# warnning
add_compile_options(
    -Wall -Wshadow -Wundef -Wmissing-prototypes -Wno-discarded-qualifiers -Wall -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith 
    -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess 
    -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic 
    -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated -Wempty-body -Wtype-limits -Wshift-negative-value -Wstack-usage=2048 
    -Wno-unused-value -Wno-unused-parameter -Wno-missing-field-initializers -Wuninitialized -Wmaybe-uninitialized -Wall -Wextra -Wno-unused-parameter 
    -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wpointer-arith -Wno-cast-qual -Wmissing-prototypes 
    -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wno-discarded-qualifiers -Wformat-security -Wno-ignored-qualifiers -Wno-sign-compare -std=c99
)


    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/uclibc/drm
        ${CMAKE_CURRENT_SOURCE_DIR}/include/uclibc/cjson
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/lvgl/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/lvgl/font
        ${STAGING_DIR}/usr/include/pcap
        ${STAGING_DIR}/usr/include
    )

message(STATUS "OpenCV_LIBS=${OpenCV_LIBS}")
target_link_libraries(${PROJECT_NAME}
    #PRIVATE lvgl_examples
    PRIVATE lvgl::drivers
    pthread
    drm
    cjson    
    lvgl
    pcap
    nl-3 
    nl-genl-3
    ${OpenCV_LIBS}
)



install(TARGETS ${PROJECT_NAME}  DESTINATION bin)