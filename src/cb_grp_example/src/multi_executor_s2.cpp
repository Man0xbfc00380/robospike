#include <chrono>
#include <memory>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/syscall.h>
#include <mutex>
#include <coroutine>
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executor.hpp"
#include "std_srvs/srv/empty.hpp"
#include "std_msgs/msg/string.hpp"
#include "cospike/coroutine.hpp"
#include "cospike/generator.hpp"
#include "cospike/task.hpp"
#include "cospike/task_void.hpp"
#include "cospike/executor.hpp"
#include "cospike/awaiter.hpp"
#include "cospike/io_utils.hpp"
#include "cospike/channel.hpp"

using namespace std::chrono_literals;

using std::placeholders::_1;

#define gettid() syscall(__NR_gettid)

#define USE_INTRA_PROCESS_COMMS true 

#define DUMMY_LOAD_ITER	1000
#define THREAD_SIZE     3

timeval starting_time;

int dummy_load_calib = 1;

void dummy_load(int load_ms) {
    int i, j;
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
}

void dummy_load_sleep(int load_ms) {
    int i, j;
    // Do sth.
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    // Wait for the machine
    rclcpp::sleep_for(1500ms);
    // Do sth. Further
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
}

namespace cb_chain_demo
{
class StartNode : public rclcpp::Node
{
public:
    StartNode(const std::string node_name, const std::string pub_topic, int exe_time, int period, bool end_flag) 
        : Node(node_name, rclcpp::NodeOptions().use_intra_process_comms(USE_INTRA_PROCESS_COMMS)), count_(0), exe_time_(exe_time), period_(period), end_flag_(end_flag)
    {
        publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);

        if (period_ == 3000)
            timer_ = this->create_wall_timer(3000ms, std::bind(&StartNode::timer_callback, this));
        else
            timer_ = this->create_wall_timer(2000ms, std::bind(&StartNode::timer_callback, this));
    }

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
private:
    size_t count_;
    int exe_time_;
    int period_;
    timeval ctime, ftime, create_timer, latency_time;
    bool end_flag_;

    void show_time(timeval ftime, timeval ctime) 
    {
        int duration_us = (ftime.tv_sec - starting_time.tv_sec) * 1000000 + (ftime.tv_usec - starting_time.tv_usec);
        long tv_sec = duration_us / 1000000;
        long tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [Bgn] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);

        duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
        tv_sec = duration_us / 1000000;
        tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [End] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
    }

    void timer_callback()
    {
        gettimeofday(&ftime, NULL);

        dummy_load(100);

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = std::to_string(count_++);

        gettimeofday(&ctime, NULL);
        
        publisher_->publish(message);
        show_time(ftime, ctime);
    }
};

class IntermediateNode : public rclcpp::Node
{
public:
    IntermediateNode(const std::string node_name, const std::string sub_topic, const std::string pub_topic, int exe_time, bool end_flag) 
        : Node(node_name), count_(0), exe_time_(exe_time), end_flag_(end_flag)
    {                        
        subscription_ = this->create_subscription<std_msgs::msg::String>(sub_topic, 1, std::bind(&IntermediateNode::callback, this, _1));
        if (pub_topic != "") publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
private:
    size_t count_;
    int exe_time_;
    timeval ctime, ftime;
    double latency;
    bool end_flag_;

    void show_time(timeval ftime, timeval ctime) 
    {
        int duration_us = (ftime.tv_sec - starting_time.tv_sec) * 1000000 + (ftime.tv_usec - starting_time.tv_usec);
        long tv_sec = duration_us / 1000000;
        long tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [Bgn] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);

        duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
        tv_sec = duration_us / 1000000;
        tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [End] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
    }

    void callback(const std_msgs::msg::String::SharedPtr msg) {

        gettimeofday(&ftime, NULL);
        
        // Dummy with sleep (IO)
        dummy_load_sleep(exe_time_);
        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);
    }
};
}   // namespace cb_group_demo

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    // Warm-up: dummy_load_calib
    while (1) {
        timeval ctime, ftime;
        int duration_us;
        gettimeofday(&ctime, NULL);
        dummy_load(100); // 100ms
        gettimeofday(&ftime, NULL);
        duration_us = (ftime.tv_sec - ctime.tv_sec) * 1000000 + (ftime.tv_usec - ctime.tv_usec);
        if (abs(duration_us - 100 * 1000) < 500) {
            break;
        }
        dummy_load_calib = 100 * 1000 * dummy_load_calib / duration_us;
        if (dummy_load_calib <= 0) dummy_load_calib = 1;
    }

    // Define graph: c1 -> [c2, c3, c4]
    // std::make_shared --> return the ptr & allocate the memory space on heap
    auto c1_t_cb_0 = std::make_shared<cb_chain_demo::StartNode>("Timer_callback1", "c1", 100, 2000, false);
    auto c1_r_cb_1 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback11", "c1", "", 100, true);
    auto c1_r_cb_2 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback12", "c1", "", 100, true);
    auto c1_r_cb_3 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback13", "c1", "", 100, true);

    auto c1_r_cb_4 = std::make_shared<cb_chain_demo::StartNode>("Timer_callback2", "c2", 100, 3000, false);
    auto c1_r_cb_5 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback21", "c2", "", 100, true);
    auto c1_r_cb_6 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback22", "c2", "", 100, true);

    // Create executors
    int number_of_threads = 4;
    rclcpp::executors::MultiThreadedExecutor exec1(rclcpp::executor::ExecutorArgs(), number_of_threads, true);
    
    // Allocate callbacks to executors
    exec1.add_node(c1_t_cb_0);
    exec1.add_node(c1_r_cb_1);
    exec1.add_node(c1_r_cb_2);
    exec1.add_node(c1_r_cb_3);
    exec1.add_node(c1_r_cb_4);
    exec1.add_node(c1_r_cb_5);
    exec1.add_node(c1_r_cb_6);

    // Record Starting Time:
    gettimeofday(&starting_time, NULL);

    // Spin lock
    exec1.spin();

    // Remove Extra-node
    exec1.remove_node(c1_t_cb_0);
    exec1.remove_node(c1_r_cb_1);
    exec1.remove_node(c1_r_cb_2);
    exec1.remove_node(c1_r_cb_3);
    exec1.remove_node(c1_r_cb_4);
    exec1.remove_node(c1_r_cb_5);
    exec1.remove_node(c1_r_cb_6);

    // Shutdown
    rclcpp::shutdown();
    return 0;
}