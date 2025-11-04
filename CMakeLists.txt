cmake_minimum_required(VERSION 3.10)

# 设置交叉编译
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 交叉编译器路径（请根据实际情况修改）
set(CMAKE_C_COMPILER /usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/local/arm/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-g++)

# 项目名称
project(lvgl)

# 使用的 C/C++ 标准
set(CMAKE_C_STANDARD 99)   # LVGL 官方支持 C99 及以上
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找线程库
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 包含 lvgl 子项目
add_subdirectory(lvgl)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR})

# 添加可执行文件，包含 main.c、client.c 和 mouse_cursor_icon.c
add_executable(main
    main.c
    client.c
    mouse_cursor_icon.c
)

# 如果需要使用 DRM 驱动，请取消注释下面两行
# include(${CMAKE_CURRENT_LIST_DIR}/lvgl/tests/FindLibDRM.cmake)
# include_directories(${Libdrm_INCLUDE_DIRS})

# 如果在 PC 上运行，可以取消注释 SDL2 相关内容
# find_package(SDL2)
# find_package(SDL2_image)
# include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

# 链接库
target_link_libraries(main
    lvgl
    lvgl::examples
    lvgl::demos
    lvgl::thorvg
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${Libdrm_LIBRARIES}
    m
    Threads::Threads
)

# 自定义目标：运行程序
add_custom_target(run
    COMMAND ${EXECUTABLE_OUTPUT_PATH}/main
    DEPENDS main
)