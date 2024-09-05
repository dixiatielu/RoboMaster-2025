#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <image_transport/image_transport.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>

class ImageFpsNode : public rclcpp::Node
{
public:
    ImageFpsNode()
    : Node("image_fps_node"), frame_count_(0), fps_(0.0)
    {
        // 订阅器
        image_sub_ = image_transport::create_subscription(
            this, "image_raw", std::bind(&ImageFpsNode::imageCallback, this, std::placeholders::_1),
            "raw");

        // 发布器
        image_pub_ = image_transport::create_publisher(this, "output_image_topic");

        // 定时器，用于FPS计算
        timer_ = this->create_wall_timer(
            std::chrono::seconds(1),
            std::bind(&ImageFpsNode::timerCallback, this));

        RCLCPP_INFO(this->get_logger(), "Image FPS Processor Node has been started.");
    }

private:
    void imageCallback(const sensor_msgs::msg::Image::ConstSharedPtr msg)
    {
        // 将ROS图像消息转换为OpenCV图像
        cv_bridge::CvImagePtr cv_ptr;
        try {
            cv_ptr = cv_bridge::toCvCopy(msg, "bgr8");
        }
        catch (cv_bridge::Exception& e) {
            RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
            return;
        }

        // 计算帧数
        frame_count_++;

        // 在图像上绘制FPS
        std::string fps_text = "FPS: " + std::to_string(static_cast<int>(fps_));
        cv::putText(cv_ptr->image, fps_text, cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

        // 发布处理后的图像
        image_pub_.publish(cv_ptr->toImageMsg());
    }

    void timerCallback()
    {
        fps_ = frame_count_;
        frame_count_ = 0;
        RCLCPP_INFO(this->get_logger(), "Current FPS: %d", static_cast<int>(fps_));
    }

    image_transport::Subscriber image_sub_;
    image_transport::Publisher image_pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    int frame_count_;
    double fps_;
};

int main(int argc, char** argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ImageFpsNode>());
    rclcpp::shutdown();
    return 0;
}
