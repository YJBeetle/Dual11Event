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

# 源码构建出来的 MaaFramework install 树不携带 OpenCV 的 CMake 配置，它住在 MaaDeps
# 的 vcpkg 目录里，而 MaaFramework 导出的 config 会 find_dependency(OpenCV)。官方 release
# 包是自足的，不需要这一段。优先用显式 MAADEPS_DIR，否则在常见相对位置里找带
# share/opencv*/OpenCVConfig.cmake 的那个。
set(MAADEPS_DIR "" CACHE PATH "MaaDeps vcpkg installed dir (…/MaaDeps/vcpkg/installed/maa-<arch>-<os>)")
if(MAAFW_DIR AND NOT MAADEPS_DIR)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(maa_os "osx")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(maa_os "windows")
    else()
        set(maa_os "linux")
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
        set(maa_arch "arm64")
    else()
        set(maa_arch "x64")
    endif()
    file(GLOB maadeps_candidates
        "${MAAFW_DIR}/source/MaaUtils/MaaDeps/vcpkg/installed/maa-${maa_arch}-${maa_os}"
        "${MAAFW_DIR}/../source/MaaUtils/MaaDeps/vcpkg/installed/maa-${maa_arch}-${maa_os}"
        "${MAAFW_DIR}/../../source/MaaUtils/MaaDeps/vcpkg/installed/maa-${maa_arch}-${maa_os}"
        "${MAAFW_DIR}/../../../source/MaaUtils/MaaDeps/vcpkg/installed/maa-${maa_arch}-${maa_os}")
    if(maadeps_candidates)
        list(GET maadeps_candidates 0 MAADEPS_DIR)
    endif()
endif()
if(MAADEPS_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${MAADEPS_DIR}")
    message(STATUS "Using MaaDeps from: ${MAADEPS_DIR}")
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
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${MAAFW_DIR}/bin"
        INSTALL_RPATH "@executable_path/lib"
    )
endif()

# 打包：可执行文件、SDK 的全部动态库、资源，三层装进一个目录，解压即用。
# 控制单元是 dlopen 的，必须和 libMaaFramework 放在同一个 lib/ 里。
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION assets
    PATTERN "model/ocr" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/model/ DESTINATION assets
    FILES_MATCHING PATTERN "*.onnx" PATTERN "keys.txt")
install(DIRECTORY ${MAAFW_DIR}/bin/ DESTINATION lib
    FILES_MATCHING PATTERN "*.dylib" PATTERN "*.so")

# 资源整份拷进构建目录，二进制可以直接从 build/ 起跑
file(GLOB_RECURSE ASSET_FILES RELATIVE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/assets/*")
foreach(asset_file ${ASSET_FILES})
    configure_file("${PROJECT_SOURCE_DIR}/${asset_file}" "${CMAKE_BINARY_DIR}/${asset_file}" COPYONLY)
endforeach()
