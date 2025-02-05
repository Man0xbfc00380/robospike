# RoboSpike

A coroutine-based rclcpp for more efficient scheduling in ROS 2. 

## Step 0: Download ROS 2 Dashing

Reference: https://docs.ros.org/en/dashing/Installation.html

Please compile from source code and use a compiler supporting C++20. Version conflict problems may occur during this process. The corresponding solutions will be summarized later in the documentation.

## Step 1: Clone the repo

```bash
git clone https://github.com/Man0xbfc00380/robospike.git

# Please set LD_LIBRARY_PATH to your own PATH before source
source setenv.sh
```

## Step 2: Build and Run

```bash
bash build.sh
./build/exp/exe_nodelet
```

H. Li, Q. Yang, S. Ma, R. Zhao and X. Ji, "RoboSpike: Fully Utilizing the Heterogeneous System with Subcallback Scheduling in ROS 2," in IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, doi: 10.1109/TCAD.2025.3538615. keywords: {ROS;Heterogeneity;Executor;Subcallback},
