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

# executable
add_executable(client_node src/client.cpp)
ament_target_dependencies(client_node rclcpp_spk std_srvs std_msgs)
add_executable(service_node src/service.cpp)
ament_target_dependencies(service_node rclcpp_spk std_srvs std_msgs)
add_executable(chain src/chain.cpp)
ament_target_dependencies(chain rclcpp_spk std_srvs std_msgs)

ament_package()
