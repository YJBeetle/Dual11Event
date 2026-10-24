cmake_minimum_required(VERSION 3.28)

project(Dual11Event VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# iOS 控制器要链带 MaaIOSControllerCreate 的构建，即 MaaFramework 的 feat/ios-control-unit 分支。
# 官方 release 里还没有这个入口，所以这里不 FetchContent，只认本地 install 目录。
set(MAAFW_DIR "" CACHE PATH "Path to a MaaFramework install dir containing libMaaIOSControlUnit")
if(MAAFW_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${MAAFW_DIR}/share/cmake")
endif()

# MaaUtils 导出的是静态构建的接口依赖；我们链的是动态包，Boost 已经在库里解析完了，
# 但 CMake 仍要求这个名字存在，所以给个空的 INTERFACE 目标占位。
if(NOT TARGET Boost::regex)
    add_library(Boost::regex INTERFACE IMPORTED)
endif()

find_package(MaaFramework CONFIG REQUIRED)

file(GLOB_RECURSE SRC_LIST ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} PRIVATE
    MaaFramework::MaaFramework
    MaaFramework::MaaToolkit
)

if(MAAFW_DIR)
    set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_RPATH "${MAAFW_DIR}/bin")
endif()

# 资源整份拷进构建目录，二进制可以直接从 build/ 起跑
file(GLOB_RECURSE ASSET_FILES RELATIVE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/assets/*")
foreach(asset_file ${ASSET_FILES})
    configure_file("${PROJECT_SOURCE_DIR}/${asset_file}" "${CMAKE_BINARY_DIR}/${asset_file}" COPYONLY)
endforeach()
