cmake_minimum_required(VERSION 3.5)
project(exp)
set(CMAKE_VERBOSE_MAKEFILEON ON)

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

# distributed executable
add_executable(exp0_interexe src/exp0_interexe.cpp)
ament_target_dependencies(exp0_interexe rclcpp_spk std_srvs std_msgs)

add_executable(exp0_intraexe src/exp0_intraexe.cpp)
ament_target_dependencies(exp0_intraexe rclcpp_spk std_srvs std_msgs)

add_executable(exp0_co_interexe src/exp0_co_interexe.cpp)
ament_target_dependencies(exp0_co_interexe rclcpp_spk std_srvs std_msgs)

add_executable(exp0_co_intraexe src/exp0_co_intraexe.cpp)
ament_target_dependencies(exp0_co_intraexe rclcpp_spk std_srvs std_msgs)

add_executable(exp1_multicast src/exp1_multicast.cpp)
ament_target_dependencies(exp1_multicast rclcpp_spk std_srvs std_msgs)

ament_package()
