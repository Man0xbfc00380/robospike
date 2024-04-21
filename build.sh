rm -rf ./build
rm -rf ./install
rm -rf ./log
colcon build --packages-select rclcpp_spk
colcon build --packages-select cb_grp_example