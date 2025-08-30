cmake_minimum_required(VERSION 3.10.2)

# 设置项目名称
project("nativevoicecall")

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加Android专用的语音通话实现和JNI
add_library(voice_call SHARED
    android_voice_call.cpp
    voice_call_jni.cpp
)

# 设置包含目录
target_include_directories(voice_call PRIVATE
    ../../../../../core/include
    ${JNI_INCLUDE_DIRS}
)

# 链接必要的库
target_link_libraries(voice_call
    android
    log
    OpenSLES
)
