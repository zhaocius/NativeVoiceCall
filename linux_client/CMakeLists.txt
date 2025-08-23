cmake_minimum_required(VERSION 3.16)
project(VoiceCallClient VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 源文件
set(SOURCES
    src/main.cpp
)

# 创建可执行文件
add_executable(voice_call_client ${SOURCES})

# 添加核心库子目录
add_subdirectory(${CMAKE_SOURCE_DIR}/../core core)

# 链接核心库
target_link_libraries(voice_call_client
    voice_call
)

# 设置包含目录
target_include_directories(voice_call_client PRIVATE
    ${CMAKE_SOURCE_DIR}/../core/include
)

# 安装规则
install(TARGETS voice_call_client
    RUNTIME DESTINATION bin
)

# 复制依赖库到输出目录（如果需要）
if(UNIX AND NOT APPLE)
    add_custom_command(TARGET voice_call_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:voice_call>
        $<TARGET_FILE_DIR:voice_call_client>
    )
endif()
