#include <chrono>
#include <memory>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/syscall.h>
#include <mutex>
#include <coroutine>
#include <vector>
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executor.hpp"
#include "std_srvs/srv/empty.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float32_multi_array.hpp"

#include "cospike/coroutine.hpp"
#include "cospike/generator.hpp"
#include "cospike/task.hpp"
#include "cospike/task_void.hpp"
#include "cospike/coexecutor.hpp"
#include "cospike/awaiter.hpp"
#include "cospike/io_utils.hpp"
#include "cospike/channel.hpp"

#include <librealsense2/rs.h>
#include <librealsense2/h/rs_pipeline.h>
#include <librealsense2/h/rs_frame.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "example.h"
#include <librealsense2/rs.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include "sdk_sagittarius_arm/sdk_sagittarius_arm_real.h"
#include "sdk_sagittarius_arm/sdk_sagittarius_arm_log.h"
#include "sdk_sagittarius_arm/modern_robotics.h"

#include "cuda_utils.h"
#include "logging.h"
#include "utils.h"
#include "preprocess.h"
#include "postprocess.h"
#include "model.h"
#include <cmath>

using namespace std::chrono_literals;
using std::placeholders::_1;
#define gettid() syscall(__NR_gettid)
#define USE_INTRA_PROCESS_COMMS true 
#define DUMMY_LOAD_ITER	1000
#define THREAD_SIZE     3
#define STREAM          RS2_STREAM_COLOR  // rs2_stream is a types of data provided by RealSense device           //
#define FORMAT          RS2_FORMAT_RGB8   // rs2_format identifies how binary data is encoded within a frame      //
#define WIDTH           640               // Defines the number of columns for each frame                         //
#define HEIGHT          480               // Defines the number of lines for each frame                           //
#define FPS             30                // Defines the rate of frames per second                                //
#define STREAM_INDEX    0                 // Defines the stream index, used for multiple streams of the same type //

timeval starting_time;
int dummy_load_calib = 1;

void run_exe(rclcpp::executors::ExecutorNodelet* exe) {
    exe->spin();
}

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

// std::vector<cv::Mat> get_frames_color_depth(rs2::pipeline pipe, rs2::align align_to) {
//     rs2::frameset frames = pipe.wait_for_frames();
//     frames = align_to.process(frames);
//     rs2::video_frame depth = frames.get_depth_frame();
//     cv::Mat depth_image = cv::Mat(cv::Size(640,480), CV_16U, (void*)depth.get_data(), cv::Mat::AUTO_STEP);
//     rs2::video_frame color = frames.get_color_frame();
//     cv::Mat color_image = cv::Mat(cv::Size(640,480), CV_8UC3, (void*)color.get_data(), cv::Mat::AUTO_STEP);
//     cv::imwrite("Depth Image.jpg", depth_image);
//     cv::imwrite("Color Image.jpg", color_image);

//     std::vector<cv::Mat> result;
//     result.push_back(depth_image);
//     result.push_back(color_image);
//     return result;
// }

std_msgs::msg::Float32MultiArray get_frames_color_depth(rs2::pipeline pipe, rs2::align align_to) {
    rs2::frameset frames = pipe.wait_for_frames();
    frames = align_to.process(frames);
    rs2::video_frame depth = frames.get_depth_frame();
    const uint8_t* depth_ptr = (const uint8_t*)(depth.get_data());
    rs2::video_frame color = frames.get_color_frame();
    const uint8_t* color_ptr = (const uint8_t*)(color.get_data());

    std_msgs::msg::Float32MultiArray array_data;
    array_data.data.resize(640*480*2);
    for (size_t i=0; i<640*480; ++i) {
        array_data.data[i] = static_cast<float> (*(depth_ptr+i));
    }
    for (size_t i=0; i<640*480; ++i) {
        array_data.data[i+640*480] = static_cast<float> (*(color_ptr+i));
    }
    array_data.layout.dim.resize(3);
    array_data.layout.dim[0].label = "height";
    array_data.layout.dim[0].size = 480;
    array_data.layout.dim[0].stride = 640*2;
    array_data.layout.dim[1].label = "width";
    array_data.layout.dim[1].size = 640;
    array_data.layout.dim[1].stride = 2;
    array_data.layout.dim[2].label = "channels";
    array_data.layout.dim[2].size = 2;
    array_data.layout.dim[2].stride = 1;

    return array_data;
}

std::vector<float> parseFloats(const std::string& input) {
    std::vector<float> result;
    std::istringstream iss(input);
    float num;
    char comma;

    while (iss >> num) {
        result.push_back(num);
        if (iss >> comma) {
            if (comma != ',') {
                throw std::runtime_error("Expected a comma");
            }
        }
    }
    return result;
}

namespace nvinfer1 {
    static Logger gLogger;
    const static int kOutputSize = kMaxNumOutputBbox * sizeof(Detection) / sizeof(float) + 1;

    void prepare_buffers(ICudaEngine* engine, float** gpu_input_buffer, float** gpu_output_buffer, float** cpu_output_buffer) {
        assert(engine->getNbBindings() == 2);
        // In order to bind the buffers, we need to know the names of the input and output tensors.
        // Note that indices are guaranteed to be less than IEngine::getNbBindings()
        const int inputIndex = engine->getBindingIndex(kInputTensorName);
        const int outputIndex = engine->getBindingIndex(kOutputTensorName);
        assert(inputIndex == 0);
        assert(outputIndex == 1);
        // Create GPU buffers on device
        CUDA_CHECK(cudaMalloc((void**)gpu_input_buffer, kBatchSize * 3 * kInputH * kInputW * sizeof(float)));
        CUDA_CHECK(cudaMalloc((void**)gpu_output_buffer, kBatchSize * kOutputSize * sizeof(float)));

        *cpu_output_buffer = new float[kBatchSize * kOutputSize];
    }

    void infer(IExecutionContext& context, cudaStream_t& stream, void** gpu_buffers, float* output, int batchsize) {
        context.enqueue(batchsize, gpu_buffers, stream, nullptr);
        CUDA_CHECK(cudaMemcpyAsync(output, gpu_buffers[1], batchsize * kOutputSize * sizeof(float), cudaMemcpyDeviceToHost, stream));
        cudaStreamSynchronize(stream);
    }

    void deserialize_engine(std::string& engine_name, IRuntime** runtime, ICudaEngine** engine, IExecutionContext** context) {
        std::ifstream file(engine_name, std::ios::binary);
        if (!file.good()) {
            std::cerr << "read " << engine_name << " error!" << std::endl;
            assert(false);
        }
        size_t size = 0;
        file.seekg(0, file.end);
        size = file.tellg();
        file.seekg(0, file.beg);
        char* serialized_engine = new char[size];
        assert(serialized_engine);
        file.read(serialized_engine, size);
        file.close();

        *runtime = createInferRuntime(gLogger);
        assert(*runtime);
        *engine = (*runtime)->deserializeCudaEngine(serialized_engine, size);
        assert(*engine);
        *context = (*engine)->createExecutionContext();
        assert(*context);
        delete[] serialized_engine;
    }

    std::string inference_img() {
        cudaSetDevice(kGpuId);

        std::string engine_name = "/home/leshannx/hongyi/ros2_ws/experiment/demo_ws/src/sys/model/yolov5s.engine";
        std::string img_file = "/home/leshannx/hongyi/ros2_ws/experiment/demo_ws/src/sys/model/images/bus.jpg";

        // Deserialize the engine from file
        IRuntime* runtime = nullptr;
        ICudaEngine* engine = nullptr;
        IExecutionContext* context = nullptr;
        deserialize_engine(engine_name, &runtime, &engine, &context);
        cudaStream_t stream;
        CUDA_CHECK(cudaStreamCreate(&stream));

        // Init CUDA preprocessing
        cuda_preprocess_init(kMaxInputImageSize);

        // Prepare cpu and gpu buffers
        float* gpu_buffers[2];
        float* cpu_output_buffer = nullptr;
        prepare_buffers(engine, &gpu_buffers[0], &gpu_buffers[1], &cpu_output_buffer);

        std::vector<cv::Mat> img_batch;
        std::vector<std::string> img_name_batch;
        cv::Mat img = cv::imread(img_file);
        img_batch.push_back(img);
        img_name_batch.push_back(img_file);

        // Preprocess
        cuda_batch_preprocess(img_batch, gpu_buffers[0], kInputW, kInputH, stream);

        // Run inference
        auto start = std::chrono::system_clock::now();
        infer(*context, stream, (void**)gpu_buffers, cpu_output_buffer, kBatchSize);
        auto end = std::chrono::system_clock::now();
        std::cout << "inference time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

        // NMS
        std::vector<std::vector<Detection>> res_batch;
        batch_nms(res_batch, cpu_output_buffer, img_batch.size(), kOutputSize, kConfThresh, kNmsThresh);

        // Draw bounding boxes
        draw_bbox(img_batch, res_batch);

        // Save images
        for (size_t j = 0; j < img_batch.size(); j++) {
            cv::imwrite("_" + img_name_batch[j], img_batch[j]);
        }

        // Release stream and buffers
        cudaStreamDestroy(stream);
        CUDA_CHECK(cudaFree(gpu_buffers[0]));
        CUDA_CHECK(cudaFree(gpu_buffers[1]));
        delete[] cpu_output_buffer;
        cuda_preprocess_destroy();
        // Destroy the engine
        context->destroy();
        engine->destroy();
        runtime->destroy();

        std::string pos_img= "0.3, 0, 0";
        return pos_img;
    }
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

class RealSenseNode : public rclcpp::Node
{
public:
    RealSenseNode(const std::string node_name, const std::string sub_topic, const std::string pub_topic, int exe_time, bool end_flag) 
        : Node(node_name), count_(0), exe_time_(exe_time), end_flag_(end_flag)
    {                        
        // create_subscription interface for sync callback
        subscription_ = this->create_subscription<std_msgs::msg::String>(false, sub_topic, 1, std::bind(&RealSenseNode::callback, this, std::placeholders::_1));
        
        if (pub_topic != "") publisher_ = this->create_publisher<std_msgs::msg::Float32MultiArray>(pub_topic, 1);
        this->name_ = node_name;

        pipeline_ = new rs2::pipeline;
        rs2::config cfg;

        cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);
        cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);
        pipeline_->start(cfg);
        align_to_ = new rs2::align(RS2_STREAM_COLOR);
    }

    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr publisher_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
private:
    std::string name_;
    size_t count_;
    int exe_time_;
    timeval ctime, ftime;
    double latency;
    bool end_flag_;
    rs2::pipeline* pipeline_;
    rs2::align* align_to_;

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
      
        dummy_load_sleep(exe_time_, this->name_.c_str());

        std::string name = this->get_name();
        std_msgs::msg::Float32MultiArray message = get_frames_color_depth(*pipeline_, *align_to_);

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);

        return 1;
    }
};

class YoloNode : public rclcpp::Node
{
public:
    YoloNode(const std::string node_name, const std::string sub_topic, const std::string pub_topic, int exe_time, bool end_flag) 
        : Node(node_name), count_(0), exe_time_(exe_time), end_flag_(end_flag)
    {                        
        subscription_ = this->create_subscription<std_msgs::msg::Float32MultiArray>(false, sub_topic, 1, std::bind(&YoloNode::callback, this, std::placeholders::_1));
        
        if (pub_topic != "") publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);
        this->name_ = node_name;
    }

    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::Subscription<std_msgs::msg::Float32MultiArray>::SharedPtr subscription_;
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

    int callback(const std_msgs::msg::Float32MultiArray::SharedPtr msg) {

        gettimeofday(&ftime, NULL);

        dummy_load_sleep(exe_time_, this->name_.c_str());

        std::string position = nvinfer1::inference_img();

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = position;

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);

        return 1;
    }
};

class ArmNode : public rclcpp::Node
{
public:
    ArmNode(const std::string node_name, const std::string sub_topic, const std::string pub_topic, int exe_time, bool end_flag) 
        : Node(node_name), count_(0), exe_time_(exe_time), end_flag_(end_flag)
    {                        
        // create_subscription interface for sync callback
        subscription_ = this->create_subscription<std_msgs::msg::String>(false, sub_topic, 1, std::bind(&ArmNode::callback, this, std::placeholders::_1));
        
        if (pub_topic != "") publisher_ = this->create_publisher<std_msgs::msg::String>(pub_topic, 1);
        this->name_ = node_name;

        // Config Sagittarius_Arm
        float joint_positions[7] = {0, 0, 0, 0, 0, 1.576, 0};
        log_set_level(3);

        sar_ptr_ = new sdk_sagittarius_arm::SagittariusArmReal("/dev/ttyACM0", 1000000, 500, 5);
        sgr_kinematics_ptr_ = new sdk_sagittarius_arm::SagittariusArmKinematics(0,0,0);
        sar_ptr_->SetFreeAfterDestructor(false);
        std::cout<<"[Arm] init and sleep!"<<std::endl;
        sar_ptr_->arm_set_gripper_linear_position(0.0);      //设置夹爪的角度
        sar_ptr_->SetAllServoRadian(joint_positions);         //设置6个舵机的弧度
        sleep(1);
        std::cout<<"[Arm] SetAllServoRadian!"<<std::endl;
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
    rs2_pipeline* pipeline_;
    rs2_error* e_;
    sdk_sagittarius_arm::SagittariusArmKinematics* sgr_kinematics_ptr_;
    sdk_sagittarius_arm::SagittariusArmReal* sar_ptr_;

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

        std::string pos_string = msg->data;
        std::cout<<"[Arm] get message: "<<pos_string<<std::endl;

        std::vector<float> nums = parseFloats(pos_string);

        float joint_positions[7];
        if(sgr_kinematics_ptr_->getIKinThetaEuler(nums[0], nums[1], nums[2], 0, 0, 0, joint_positions))
        {
            std::cout<<"[Arm] IK True!"<<std::endl;
            sar_ptr_->SetAllServoRadian(joint_positions);
            sleep(1/30);
            std::cout<<"SetAllServoRadian!"<<std::endl;
        }

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = "***ArmNode callback!***";

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);

        return 1;
    }
};

class IntermediateNode : public rclcpp::Node
{
public:
    IntermediateNode(const std::string node_name, const std::string sub_topic, const std::string pub_topic, int exe_time, bool end_flag) 
        : Node(node_name), count_(0), exe_time_(exe_time), end_flag_(end_flag)
    {                        
        // create_subscription interface for sync callback
        subscription_ = this->create_subscription<std_msgs::msg::String>(false, sub_topic, 1, std::bind(&IntermediateNode::callback, this, std::placeholders::_1));
        
        // create_subscription interface for async callback
        // subscription_ = this->create_subscription<std_msgs::msg::String>(true, sub_topic, 1, std::bind(&IntermediateNode::co_callback, this, std::placeholders::_1));
        
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

        dummy_load_sleep(exe_time_, this->name_.c_str());

        std::string name = this->get_name();
        auto message = std_msgs::msg::String();
        message.data = msg->data;

        gettimeofday(&ctime, NULL);

        if (publisher_) publisher_->publish(message);
        show_time(ftime, ctime);

        return 1;
    }
};
}   // namespace cb_group_demo

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    // Define graph: t1 -> [r11, r12, r13]
    // Define graph: c1 -> r21 -> r22
    // std::make_shared --> return the ptr & allocate the memory space on heap

    // c1
    auto c1_timer = std::make_shared<cb_chain_demo::StartNode>("Timer_callback1", "c1", 100, 2000, false);
    auto c1_r_cb_1 = std::make_shared<cb_chain_demo::RealSenseNode>("RealSense_callback", "c1", "r1", 100, true);
    auto c1_r_cb_2 = std::make_shared<cb_chain_demo::YoloNode>("Yolo_callback", "r1", "y1", 100, true);
    auto c1_r_cb_3 = std::make_shared<cb_chain_demo::ArmNode>("Arm_callback", "y1", "a1", 100, true);
    auto c1_r_cb_4 = std::make_shared<cb_chain_demo::IntermediateNode>("Regular_callback11", "a1", "", 100, true);

    // Create executors
    rclcpp::executors::ExecutorNodelet exec1(rclcpp::executor::ExecutorArgs(), 3, true);
    
    // Allocate callbacks to executors
    exec1.add_node(c1_timer);
    exec1.add_node(c1_r_cb_1);
    exec1.add_node(c1_r_cb_2);
    exec1.add_node(c1_r_cb_3);
    exec1.add_node(c1_r_cb_4);

    // Record Starting Time:
    gettimeofday(&starting_time, NULL);

    // Spin lock
    run_exe(&exec1);
    
    // Record Starting Time:
    gettimeofday(&starting_time, NULL);

    exec1.spin();

    // Remove Extra-node
    exec1.remove_node(c1_timer);
    exec1.remove_node(c1_r_cb_1);
    exec1.remove_node(c1_r_cb_2);
    exec1.remove_node(c1_r_cb_3);
    exec1.remove_node(c1_r_cb_4);

    // Shutdown
    rclcpp::shutdown();
    return 0;
}