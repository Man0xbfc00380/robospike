cmake_minimum_required(VERSION 3.5)
project(cb_grp_example)

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

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp_spk REQUIRED)
find_package(std_srvs REQUIRED)
find_package(std_msgs REQUIRED)

# ros2 executable
add_executable(mexe_node src/multi_executor.cpp)
ament_target_dependencies(mexe_node rclcpp_spk std_srvs std_msgs)
add_executable(sexe_node src/single_executor.cpp)
ament_target_dependencies(sexe_node rclcpp_spk std_srvs std_msgs)

# coroutine-based executable
add_executable(mcoexe_node src/multi_co_executor.cpp)
ament_target_dependencies(mcoexe_node rclcpp_spk std_srvs std_msgs)
add_executable(scoexe_node src/single_co_executor.cpp)
ament_target_dependencies(scoexe_node rclcpp_spk std_srvs std_msgs)

# coroutine example
add_executable(chain src/chain.cpp)
ament_target_dependencies(chain rclcpp_spk std_srvs std_msgs)

ament_package()
