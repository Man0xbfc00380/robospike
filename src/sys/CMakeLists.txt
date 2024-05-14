cmake_minimum_required(VERSION 3.5)
project(sys)
set(CMAKE_VERBOSE_MAKEFILEON ON)
include_directories(include)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++20
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

add_compile_options(-fcoroutines)
add_compile_options(-g) # debug for GDB

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# tensorrt
set(CMAKE_CUDA_COMPILER /usr/local/cuda/bin/nvcc)
enable_language(CUDA)

# find dependencies
find_package(CUDA REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(rclcpp_spk REQUIRED)
find_package(std_srvs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(realsense2 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread system)

include_directories(include/common include/imgui ${OpenCV_INCLUDE_DIRS} ${realsense2_INCLUDE_DIRS} include/sdk_sagittarius_arm /usr/lib ${Boost_INCLUDE_DIRS} /usr/local/cuda/include include/src include/plugin ${OpenCV_INCLUDE_DIRS})
link_directories(include/sdk_sagittarius_arm /usr/lib ${Boost_INCLUDE_DIRS} /usr/local/cuda/lib64)
file(GLOB_RECURSE SRCS include/src/*.cpp include/src/*.cu)
file(GLOB_RECURSE PLUGIN_SRCS include/plugin/*.cu)
cuda_add_library(myplugins SHARED ${PLUGIN_SRCS})
target_link_libraries(myplugins nvinfer cudart)
cuda_add_executable(test_sys src/test_sys.cpp src/example.h ${SRCS})
target_link_libraries(test_sys ${DEPENDENCIES} ${realsense2_LIBRARY} ${OpenCV_LIBS} sagittarius_sdk ${Boost_LIBRARIES} nvinfer cudart myplugins) # ${PLUGIN_SRCS}
ament_target_dependencies(test_sys rclcpp_spk std_srvs std_msgs ${DEPENDENCIES} ${realsense2_LIBRARY} ${OpenCV_LIBS}) 

ament_package()
