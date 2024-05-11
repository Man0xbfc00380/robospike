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

#include "cuda_runtime.h"
#include "device_types.h"
#include "exp/exp0_cuda.cuh"

using namespace std::chrono_literals;
using std::placeholders::_1;
#define gettid() syscall(__NR_gettid)
#define USE_INTRA_PROCESS_COMMS true 
#define DUMMY_LOAD_ITER	1000
#define THREAD_SIZE     3

timeval starting_time;
int dummy_load_calib = 1;

void dummy_load(int load_ms, const char * name_str) {
    int i, j;
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    rclcpp::sleep_for(100ms);
}

int dummy_load_sleep(int load_ms, const char * name_str) {
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

    // Wait for the machine
    rclcpp::sleep_for(450ms);

    gettimeofday(&ctime, NULL);
    duration_us = (ctime.tv_sec - starting_time.tv_sec) * 1000000 + (ctime.tv_usec - starting_time.tv_usec);
    tv_sec = duration_us / 1000000;
    tv_usec = duration_us - tv_sec * 1000000;
    
    // Do sth. Further
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    return 1;
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
        timer_ = this->create_wall_timer(600ms, std::bind(&StartNode::timer_callback, this));
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
        // subscription_ = this->create_subscription<std_msgs::msg::String>(false, sub_topic, 1, std::bind(&IntermediateNode::callback, this, _1));
        
        // create_subscription interface for async callback
        subscription_ = this->create_subscription<std_msgs::msg::String>(true, sub_topic, 10, std::bind(&IntermediateNode::co_callback, this, _1));
        
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

    int callback(const std_msgs::msg::String::SharedPtr msg) {

        gettimeofday(&ftime, NULL);

        float a[128]; a[0] = 1.0; a[1] = 1.1; a[2] = 1.2;
        float b[128]; b[0] = 2.0; b[1] = 2.1; b[2] = 3.2;
        auto res = matAdd(a, b, 128);
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] a[] [%f - %f - %f]", gettid(), a[0], a[1], a[2]);
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] b[] [%f - %f - %f]", gettid(), b[0], b[1], b[2]);
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] Res [%f - %f - %f]", gettid(), res[0], res[1], res[2]);

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);

        return 1;
    }

    Task<int, RosCoExecutor> co_callback(const std_msgs::msg::String::SharedPtr msg) {

        /* Non-Blocking Style */
        // co_await 450ms;        
        int N = 200000000;
        int duration_us;
        long tv_sec, tv_usec;

        //^^^^^ CPU Run It
        N = 10000;
        int* cpu_a = new int[N];
        for (int i = 0; i < N; i++) {
            cpu_a[i] = i;
        }
        gettimeofday(&ftime, NULL);
        for (size_t idx = 0; idx < N; idx++)
        {
            int item = cpu_a[idx];
            for (int i = 0; i < 100; i++) {
                cpu_a[idx] += item;
                // ^^^^^ Extra code to evaluate time
                if (idx > 0) {
                    cpu_a[idx] += cpu_a[idx - 1];
                }
            }
            cpu_a[idx] += 2;
        }
        gettimeofday(&ctime, NULL);
        duration_us = (ctime.tv_sec - ftime.tv_sec) * 1000000 + (ctime.tv_usec - ftime.tv_usec);
        tv_sec = duration_us / 1000000;
        tv_usec = duration_us - tv_sec * 1000000;
        RCLCPP_INFO(this->get_logger(), "[PID: %ld] [kernelCPU] [s: %ld] [us: %ld]", gettid(), tv_sec, tv_usec);


        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;
        if (publisher_) publisher_->publish(message);
        // show_time(ftime, ctime);
        co_return 1;
    }
};
}

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    // Define graph: c1_timer -> c1_callback
    // std::make_shared --> return the ptr & allocate the memory space on heap
    auto c1_timer = std::make_shared<cb_chain_demo::StartNode>("Timer_callback1", "c1", 100, 2000, false);
    auto c1_r_cbk = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback11", "c1", "", 100, true);
    
    // Create executors
    int number_of_threads = 2;
    rclcpp::executors::ExecutorNodelet exec1(rclcpp::executor::ExecutorArgs(), number_of_threads, true);
    
    std::queue<Task<int, RosCoExecutor> > task_queue;
    
    // Allocate callbacks to executors
    exec1.add_node(c1_timer);
    exec1.add_node(c1_r_cbk);

    // Record Starting Time:
    gettimeofday(&starting_time, NULL);

    // Spin lock
    exec1.spin();

    // Remove Extra-node
    exec1.remove_node(c1_timer);
    exec1.remove_node(c1_r_cbk);

    // Shutdown
    rclcpp::shutdown();
    return 0;
}