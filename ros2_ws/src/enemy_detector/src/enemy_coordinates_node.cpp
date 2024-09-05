#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <queue>

class EnemyCoordinatesNode : public rclcpp::Node
{
public:
    EnemyCoordinatesNode() : Node("enemy_coordinates_node"), queue_size_(10)
    {
        // 初始化subscription
        coords_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
            "target_point", 10, std::bind(&EnemyCoordinatesNode::coordsCallback, this, std::placeholders::_1));

        // 初始化TF2 Buffer和Transform Listener
        tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        // 定时器：每秒更新队列
        timer_ = this->create_wall_timer(
            std::chrono::seconds(1), std::bind(&EnemyCoordinatesNode::updateQueue, this));

        RCLCPP_INFO(this->get_logger(), "Enemy Coordinates Node has been started.");
    }

private:
    // 回调函数：接收坐标数据
    void coordsCallback(const geometry_msgs::msg::PointStamped::SharedPtr msg)
    {
        // 转换左手坐标系到右手坐标系
        geometry_msgs::msg::PointStamped transformed_coords;
        transformed_coords.header = msg->header;
        transformed_coords.point.x = msg->point.x;  // x轴保持不变
        transformed_coords.point.y = -msg->point.y; // y轴取负
        transformed_coords.point.z = -msg->point.z; // z轴取负

        transformed_coords.header.frame_id = "camera_link"; 

        latest_coords_ = std::make_shared<geometry_msgs::msg::PointStamped>(transformed_coords);
    }

    // 定时器回调函数：每秒更新队列
    void updateQueue()
    {
        if (latest_coords_)
        {
            try
            {
                // 将坐标从相机坐标系转换到base_link坐标系
                geometry_msgs::msg::PointStamped base_link_coords;
                base_link_coords = tf_buffer_->transform(*latest_coords_, "base_link", tf2::durationFromSec(0.2));

                if (coords_queue_.size() >= queue_size_)
                {
                    coords_queue_.pop(); // 移除最老的元素
                }
                coords_queue_.push(base_link_coords); // 添加转换后的坐标

                RCLCPP_INFO(this->get_logger(), "Coordinate in base_link frame: x=%.2f, y=%.2f, z=%.2f",
                            base_link_coords.point.x, base_link_coords.point.y, base_link_coords.point.z);
            }
            catch (tf2::TransformException &ex)
            {
                RCLCPP_WARN(this->get_logger(), "Transform failed: %s", ex.what());
            }
        }
    }

    // 队列和其他成员变量
    std::queue<geometry_msgs::msg::PointStamped> coords_queue_;
    geometry_msgs::msg::PointStamped::SharedPtr latest_coords_;
    size_t queue_size_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr coords_sub_;
    rclcpp::TimerBase::SharedPtr timer_;

    // TF2相关变量
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<EnemyCoordinatesNode>());
    rclcpp::shutdown();
    return 0;
}
