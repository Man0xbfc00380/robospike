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
    sleep(1);
    // Do sth. Further
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
}

void co_dummy_load(int load_ms) {
    int i, j;
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
}

void co_dummy_load_sleep(int load_ms) {
    int i, j;
    // Do sth.
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
    // Wait for the machine
    sleep(1);
    // Do sth. Further
    for (j = 0; j < dummy_load_calib * load_ms; j++)
        for (i = 0 ; i < DUMMY_LOAD_ITER; i++) 
            __asm__ volatile ("nop");
}

Task<int, SharedThreadPoolExecutor> simple_task2() {
    using namespace std::chrono_literals;
    debug("[CoSpike] simple_task 002 bgn");
    std::this_thread::sleep_for(1s); // blocking sleep
    debug("[CoSpike] simple_task 002 end");
    co_return 2;
}

Task<int, SharedThreadPoolExecutor> simple_task3() {
    using namespace std::chrono_literals;
    debug("[CoSpike] simple_task 003 bgn");
    // FIXME: Avoid extra thread
    // --> Merge the schduler into the SharedThreadPoolExecutor
    // --> Can the schduler be suspended
    co_await 2s; // unblocking sleep
    debug("[CoSpike] simple_task 003 end");
    co_return 3;
}

/**
 * [Hint] Note that using a non-blocking pending caller concatenation requires
 * the use of a SharedLooperExecutor or NewThreadExecutor, not a NoopExecutor.
 * Orthewise more serious problems will occur.
 * 
 * The main reason lies in 
 *    (1) blocking in the main program for maintenance of the survival of 
 *        the blocking and re-execution of the call executor should be considered
 *        at the same time.
 *    (2) when TaskPromise wants to deconstruct, if the NoopExecutor & AsyncExecutor
 *        block the code in task:hpp 
 *            class TaskPromise --> void notify_callbacks()
 *            --> for (...) { callback(value); }
 */
Task<int, SharedThreadPoolExecutor> simple_task() {
    using namespace std::chrono_literals;
    debug("[CoSpike] simple_task all bgn");
    auto result2 = co_await simple_task2();
    auto result3 = co_await simple_task3();
    debug("[CoSpike] simple_task all end");
    co_return 1 + result2 + result3;
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

        if (period_ == 10000)
            timer_ = this->create_wall_timer(10000ms, std::bind(&StartNode::timer_callback, this));
        else
            timer_ = this->create_wall_timer( 2000ms, std::bind(&StartNode::timer_callback, this));

        gettimeofday(&create_timer, NULL);
        RCLCPP_INFO(this->get_logger(), "Create wall timer [PID: %ld] at %ld (s) %ld (us)", gettid(), create_timer.tv_sec, create_timer.tv_usec);
    }

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
private:
    size_t count_;
    int exe_time_;
    int period_;
    timeval ctime, ftime, create_timer, latency_time;
    bool end_flag_;

    void timer_callback()
    {
        std::string name = this->get_name();            
        gettimeofday(&ctime, NULL);
        dummy_load(exe_time_);
        auto message = std_msgs::msg::String();
        message.data = std::to_string(count_++);
        gettimeofday(&ftime, NULL);
        RCLCPP_INFO(this->get_logger(), "Start call [PID: %ld] back at %ld (s) %ld (us)", gettid(), ftime.tv_sec, ftime.tv_usec);

        publisher_->publish(message);
    }

    Task<void, SharedThreadPoolExecutor> co_timer_callback()
    {
        // [modification] dummy_load -> co_dummy_load
        // dummy_load(exe_time_);
        co_dummy_load(exe_time_);

        // message
        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = std::to_string(count_++);

        // record time
        gettimeofday(&ftime, NULL);
        RCLCPP_INFO(this->get_logger(), "Start call [PID: %ld] back at %ld (s) %ld (us)", gettid(), ftime.tv_sec, ftime.tv_usec);
        
        // publish data
        // <Q> is it possble to use channel?
        publisher_->publish(message);
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

    void callback(const std_msgs::msg::String::SharedPtr msg) {
        
        gettimeofday(&ftime, NULL);
        std::string name = this->get_name();

        // Dummy with sleep (IO)
        dummy_load_sleep(exe_time_);

        auto message = std_msgs::msg::String();
        message.data = msg->data;

        RCLCPP_INFO(this->get_logger(), "Intermediate call [PID: %ld] back at %ld (s) %ld (us)", gettid(), ftime.tv_sec, ftime.tv_usec);
        if (publisher_) publisher_->publish(message);
    }       

    Task<void, SharedThreadPoolExecutor> co_inode_callback()
    {
        // [modification] dummy_load -> co_dummy_load
        // dummy_load(exe_time_);
        co_dummy_load(exe_time_);

        // message
        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = std::to_string(count_++);

        // record time
        gettimeofday(&ftime, NULL);
        RCLCPP_INFO(this->get_logger(), "Start call [PID: %ld] back at %ld (s) %ld (us)", gettid(), ftime.tv_sec, ftime.tv_usec);
        
        // publish data
        // <Q> is it possble to use channel?
        publisher_->publish(message);
    } 
};
}   // namespace cb_group_demo

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "PID: %ld run in ROS2.", gettid());
    SharedThreadPoolExecutor stpExecutor;
    stpExecutor.executor_init(THREAD_SIZE);

    printf("[CoSpike] Let Coroutine Run!\n");
    auto simpleTask = simple_task();
    debug("[CoSpike] main: line after simple_task()");

    // Obatin the result in a sync way
    // --> blocking avoids the main function 
    //     ends before LooperExecutor
    try {
        auto i = simpleTask.get_result();
        debug("[CoSpike] Final simpleTask.get_result:", i);
    } catch (std::exception &e) {
        debug("[CoSpike] Error: ", e.what());
    }

    // Warm-up: dummy_load_calib
    while (1) {
        timeval ctime, ftime;
        int duration_us;
        gettimeofday(&ctime, NULL);
        dummy_load(100); // 100ms
        gettimeofday(&ftime, NULL);
        duration_us = (ftime.tv_sec - ctime.tv_sec) * 1000000 + (ftime.tv_usec - ctime.tv_usec);
        RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "dummy_load_calib: %d (duration_us: %d ns)", dummy_load_calib, duration_us);
        if (abs(duration_us - 100 * 1000) < 500) {
            break;
        }
        dummy_load_calib = 100 * 1000 * dummy_load_calib / duration_us;
        if (dummy_load_calib <= 0) dummy_load_calib = 1;
    }

    // Define graph: c1 -> [c2, c3, c4]
    // std::make_shared --> return the ptr & allocate the memory space on heap
    auto c1_t_cb_0 = std::make_shared<cb_chain_demo::StartNode>("Timer_callback", "c1", 100, 1000, false);
    auto c1_r_cb_1 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback1", "c1", "", 100, true);
    auto c1_r_cb_2 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback2", "c1", "", 100, true);
    auto c1_r_cb_3 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback3", "c1", "", 100, true);
    auto c1_r_cb_4 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback4", "c1", "", 100, true);
    auto c1_r_cb_5 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback5", "c1", "", 100, true);
    auto c1_r_cb_6 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback6", "c1", "", 100, true);

    // Define graph: c1 -> c2 -> c3 -> c4
    // auto c1_r_cb_1 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback1", "c1", "c2", 1000, true);
    // auto c1_r_cb_2 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback2", "c2", "c3", 1000, true);
    // auto c1_r_cb_3 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback3", "c3", "c4", 1000, true);
    // auto c1_r_cb_4 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback4", "c4", "c5", 100, true);
    // auto c1_r_cb_5 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback5", "c5", "c6", 100, true);
    // auto c1_r_cb_6 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback6", "c6", "c7", 100, true);

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