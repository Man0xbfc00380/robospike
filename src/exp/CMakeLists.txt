cmake_minimum_required(VERSION 3.5)
project(exp)
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

# find dependencies
find_package(CUDA REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(rclcpp_spk REQUIRED)
find_package(std_srvs REQUIRED)
find_package(std_msgs REQUIRED)

# ----------------------- Pre-experiments -----------------------
# add_executable(exp0_interexe src/exp0_interexe.cpp)
# ament_target_dependencies(exp0_interexe rclcpp_spk std_srvs std_msgs)
# add_executable(exp0_intraexe src/exp0_intraexe.cpp)
# ament_target_dependencies(exp0_intraexe rclcpp_spk std_srvs std_msgs)
# add_executable(exp0_co_interexe src/exp0_co_interexe.cpp)
# ament_target_dependencies(exp0_co_interexe rclcpp_spk std_srvs std_msgs)
# add_executable(exp0_co_intraexe src/exp0_co_intraexe.cpp)
# ament_target_dependencies(exp0_co_intraexe rclcpp_spk std_srvs std_msgs)
# add_executable(exp0_multicast src/exp0_multicast.cpp)
# ament_target_dependencies(exp0_multicast rclcpp_spk std_srvs std_msgs)
# add_executable(exp0_cuda_cpu src/exp0_cuda_cpu.cpp)
# ament_target_dependencies(exp0_cuda_cpu rclcpp_spk std_srvs std_msgs)
# cuda_add_executable(exp0_cuda_gpu src/exp0_cuda_gpu.cpp src/exp0_cuda.cu)
# ament_target_dependencies(exp0_cuda_gpu rclcpp_spk std_srvs std_msgs)

# ------------------------- Experiments -------------------------

# Combination of coroutine & multi-executor
# GPU-PGO Final Result
# WCET Calculation
# Utilization
cuda_add_executable(exp1_co_multiexe src/exp1_co_multiexe.cpp src/exp2_gpu_pgo.cu)
ament_target_dependencies(exp1_co_multiexe rclcpp_spk std_srvs std_msgs)

# PGO Variation
cuda_add_executable(exp2_gpu_pgo src/exp2_gpu_pgo.cpp src/exp2_gpu_pgo.cu)
ament_target_dependencies(exp2_gpu_pgo rclcpp_spk std_srvs std_msgs)

# Multiple Executor
cuda_add_executable(exp6_co_multiexe src/exp6_co_multiexe.cpp src/exp2_gpu_pgo.cu)
ament_target_dependencies(exp6_co_multiexe rclcpp_spk std_srvs std_msgs)

ament_package()
