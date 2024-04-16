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
#include "cospike/coexecutor.hpp"
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
ThreadPoolExecutor SharedThreadPoolExecutor::sharedThreadPoolExecutor;

void dummy_load(int load_ms, const char * name_str) {
    int i, j;
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
}

Task<int, NewThreadExecutor> dummy_load_sleep(int load_ms, const char * name_str) {
    int i, j;
    timeval ftime, ctime;
    // Do sth.
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    
    gettimeofday(&ftime, NULL);
    int duration_us = (ftime.tv_sec - starting_time.tv_sec) * 1000000 + (ftime.tv_usec - starting_time.tv_usec);
    long tv_sec = duration_us / 1000000;
    long tv_usec = duration_us - tv_sec * 1000000;
    RCLCPP_INFO(rclcpp::get_logger(name_str), "[PID: %ld] load before co_await [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);

    // Wait for the machine
    // rclcpp::sleep_for(1500ms);
    co_await 1500ms;

    gettimeofday(&ctime, NULL);
    duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
    tv_sec = duration_us / 1000000;
    tv_usec = duration_us - tv_sec * 1000000;
    RCLCPP_INFO(rclcpp::get_logger(name_str), "[PID: %ld] load after co_await [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);
    
    // Do sth. Further
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    co_return 1;
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
        name_ = node_name;
        if (period_ == 10000)
            timer_ = this->create_wall_timer(10000ms, std::bind(&StartNode::timer_callback, this));
        else
            timer_ = this->create_wall_timer( 2000ms, std::bind(&StartNode::timer_callback, this));
    }

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
private:
    std::string name_;
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

        dummy_load(100, this->name_.c_str());

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
        // create_subscription interface for sync callback
        subscription_ = this->create_subscription<std_msgs::msg::String>(sub_topic, 1, std::bind(&IntermediateNode::callback, this, _1));
        
        // TODO: create_subscription interface for async callback
        // subscription_ = this->create_subscription<std_msgs::msg::String>(sub_topic, 1, std::bind(&IntermediateNode::co_callback, this, _1));
        
        if (pub_topic != "") publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);
        this->name_ = node_name;
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
private:
    std::string name_;
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

        /* In-Node Blocking Style */
        auto dummy_task = dummy_load_sleep(exe_time_, this->name_.c_str());
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] Execute Subcriber Callback (Before blocking)", gettid());
        auto i = dummy_task.get_result();
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] Execute Subcriber Callback [%d] (After blocking)", gettid(), i);

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);
    }

    Task<int, NewThreadExecutor> co_callback(const std_msgs::msg::String::SharedPtr msg) {

        gettimeofday(&ftime, NULL);

        /* Non-Blocking Style */
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] Execute Subcriber Callback (Before co_await)", gettid());
        co_await 1500ms;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] Execute Subcriber Callback (After co_await)", gettid());

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);

        co_return 1;
    }
};
}   // namespace cb_group_demo

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    // Define graph: t1 -> [r11, r12, r13]
    // Define graph: c1 -> r21 -> r22
    // std::make_shared --> return the ptr & allocate the memory space on heap
    auto c1_t_cb_0 = std::make_shared<cb_chain_demo::StartNode>("Timer_callback1", "c1", 100, 2000, false);
    auto c1_r_cb_1 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback11", "c1", "", 100, true);
    // auto c1_r_cb_2 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback12", "c1", "", 100, true);
    // auto c1_r_cb_3 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback13", "c1", "", 100, true);

    // auto c1_r_cb_4 = std::make_shared<cb_chain_demo::StartNode>("Timer_callback2", "c2", 100, 3000, false);
    // auto c1_r_cb_5 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback21", "c2", "c3", 100, true);
    // auto c1_r_cb_6 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback22", "c3", "", 100, true);

    // Create executors
    int number_of_threads = 2;
    rclcpp::executors::DistrThreadedExecutor exec1(rclcpp::executor::ExecutorArgs(), number_of_threads, true);
    
    // Allocate callbacks to executors
    exec1.add_node(c1_t_cb_0);
    exec1.add_node(c1_r_cb_1);
    // exec1.add_node(c1_r_cb_2);
    // exec1.add_node(c1_r_cb_3);
    // exec1.add_node(c1_r_cb_4);
    // exec1.add_node(c1_r_cb_5);
    // exec1.add_node(c1_r_cb_6);

    // Record Starting Time:
    gettimeofday(&starting_time, NULL);

    // Spin lock
    exec1.spin();

    // Remove Extra-node
    exec1.remove_node(c1_t_cb_0);
    exec1.remove_node(c1_r_cb_1);
    // exec1.remove_node(c1_r_cb_2);
    // exec1.remove_node(c1_r_cb_3);
    // exec1.remove_node(c1_r_cb_4);
    // exec1.remove_node(c1_r_cb_5);
    // exec1.remove_node(c1_r_cb_6);

    // Shutdown
    rclcpp::shutdown();
    return 0;
}