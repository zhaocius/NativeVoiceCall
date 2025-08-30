cmake_minimum_required(VERSION 3.16)
project(NativeVoiceCall VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖包
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# 查找ALSA库
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(ALSA QUIET alsa)
endif()

if(NOT ALSA_FOUND)
    # 尝试直接查找ALSA库
    find_library(ALSA_LIBRARY NAMES asound)
    if(ALSA_LIBRARY)
        set(ALSA_FOUND TRUE)
        set(ALSA_LIBRARIES ${ALSA_LIBRARY})
    endif()
endif()

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 源文件
set(SOURCES
    src/udp_voice_call.cpp
)

# 创建共享库
add_library(voice_call SHARED ${SOURCES})

# 设置库属性
set_target_properties(voice_call PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER include/voice_call.h
)

# 链接库
target_link_libraries(voice_call
    Threads::Threads
)

# 链接ALSA库
if(ALSA_FOUND)
    target_link_libraries(voice_call ${ALSA_LIBRARIES})
    if(ALSA_INCLUDE_DIRS)
        target_include_directories(voice_call PRIVATE ${ALSA_INCLUDE_DIRS})
    endif()
endif()

# 安装规则
install(TARGETS voice_call
    EXPORT voice_callTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

# 导出目标
install(EXPORT voice_callTargets
    FILE voice_callTargets.cmake
    NAMESPACE voice_call::
    DESTINATION lib/cmake/voice_call
)

# 创建配置文件
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    voice_callConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/voice_callConfigVersion.cmake
    DESTINATION lib/cmake/voice_call
)

# 创建简化的配置文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/voice_callConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/voice_callConfig.cmake
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/voice_callConfig.cmake
    DESTINATION lib/cmake/voice_call
)
