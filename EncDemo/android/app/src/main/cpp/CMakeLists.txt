cmake_minimum_required(VERSION 3.6)

set(NATIVE_PATH ${PROJECT_SOURCE_DIR}/../../../../../..)


message("wanglijun start compile...")
message(${PROJECT_SOURCE_DIR})


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(__src_lst
    encoder_auto_test.cpp
)

set(__lib_name
    enc_auto_test)

#增加libopenh264.so库
set (__lib_lst "${NATIVE_PATH}/3rd/android/lib/${ANDROID_ABI}/libopenh264.so")
set (__include_dir ${NATIVE_PATH}/3rd/android/inc)

add_library(openh264 SHARED IMPORTED GLOBAL)
set_target_properties (openh264
     PROPERTIES IMPORTED_LOCATION  ${__lib_lst}
     INTERFACE_INCLUDE_DIRECTORIES "${__include_dir}"
     )

add_library(${__lib_name} SHARED
    ${__src_lst})

target_link_libraries (${__lib_name}
   -landroid
   -llog
   openh264
)
set(lib_install_dir ${CMAKE_CURRENT_LIST_DIR}/../lib)

add_custom_command (
    TARGET ${__lib_name} POST_BUILD
    COMMAND [ -d ${PROJECT_SOURCE_DIR}/../jniLibs ] || mkdir ${PROJECT_SOURCE_DIR}/../jniLibs
    COMMAND [ -d ${PROJECT_SOURCE_DIR}/../jniLibs/arm64-v8a ] || mkdir ${PROJECT_SOURCE_DIR}/../jniLibs/arm64-v8a
    COMMAND [ -d ${PROJECT_SOURCE_DIR}/../jniLibs/armeabi-v7a ] || mkdir ${PROJECT_SOURCE_DIR}/../jniLibs/armeabi-v7a
    COMMAND cp ${NATIVE_PATH}/3rd/android/lib/arm64-v8a/libopenh264.so  ${PROJECT_SOURCE_DIR}/../jniLibs/arm64-v8a/
    COMMAND cp ${NATIVE_PATH}/3rd/android/lib/armeabi-v7a/libopenh264.so  ${PROJECT_SOURCE_DIR}/../jniLibs/armeabi-v7a/
    USES_TERMINAL
)
message("wanglijun install dir")
message(${lib_install_dir})
install(TARGETS ${__lib_name} LIBRARY DESTINATION ${lib_install_dir} )
