#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <queue>

class EnemyMarkerNode : public rclcpp::Node
{
public:
    EnemyMarkerNode() : Node("enemy_marker_node"), queue_size_(10)
    {
        coords_sub_ = this->create_subscription<geometry_msgs::msg::PointStamped>(
            "target_point", 10, std::bind(&EnemyMarkerNode::coordsCallback, this, std::placeholders::_1));

        marker_pub_ = this->create_publisher<visualization_msgs::msg::Marker>("visualization_marker", 10);

        tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

        timer_ = this->create_wall_timer(
            std::chrono::seconds(1), std::bind(&EnemyMarkerNode::updateQueueAndPublishMarkers, this));

        RCLCPP_INFO(this->get_logger(), "Enemy Marker Node has been started.");
    }

private:
    void coordsCallback(const geometry_msgs::msg::PointStamped::SharedPtr msg)
    {
        geometry_msgs::msg::PointStamped transformed_coords;
        transformed_coords.header = msg->header;
        transformed_coords.point.x = msg->point.x;
        transformed_coords.point.y = -msg->point.y;
        transformed_coords.point.z = -msg->point.z;

        transformed_coords.header.frame_id = "camera_link";
        latest_coords_ = std::make_shared<geometry_msgs::msg::PointStamped>(transformed_coords);
    }

    void updateQueueAndPublishMarkers()
    {
        if (latest_coords_)
        {
            try
            {
                geometry_msgs::msg::PointStamped base_link_coords;
                rclcpp::sleep_for(std::chrono::milliseconds(50));
                base_link_coords = tf_buffer_->transform(*latest_coords_, "base_link", tf2::Duration(0));

                if (coords_queue_.size() >= queue_size_)
                {
                    coords_queue_.pop();
                }
                coords_queue_.push(base_link_coords);
                
                RCLCPP_INFO(this->get_logger(), "Coordinate in base_link frame: x=%.2f, y=%.2f, z=%.2f",
                base_link_coords.point.x, base_link_coords.point.y, base_link_coords.point.z);

                publishMarkers();
            }
            catch (tf2::TransformException &ex)
            {
                RCLCPP_WARN(this->get_logger(), "Transform failed: %s", ex.what());
            }
        }
    }

    void publishMarkers()
    {
        std::queue<geometry_msgs::msg::PointStamped> temp_queue = coords_queue_;
        geometry_msgs::msg::PointStamped previous_point;
        bool first_point = true;
        int id = 0;

        while (!temp_queue.empty())
        {
            geometry_msgs::msg::PointStamped current_point = temp_queue.front();
            temp_queue.pop();

            if (first_point)
            {
                previous_point = current_point;
                first_point = false;
                continue;
            }

            visualization_msgs::msg::Marker marker;
            marker.header.frame_id = "base_link";
            marker.header.stamp = this->get_clock()->now();
            marker.ns = "enemy_trajectory";
            marker.id = id++;
            marker.type = visualization_msgs::msg::Marker::ARROW;
            marker.action = visualization_msgs::msg::Marker::ADD;

            geometry_msgs::msg::Point start, end;
            start = previous_point.point;
            end = current_point.point;

            marker.points.push_back(start);
            marker.points.push_back(end);

            marker.color.r = 1.0;
            marker.color.g = 0.0;
            marker.color.b = 0.0;
            marker.color.a = 1.0;

            marker.scale.x = 0.05;
            marker.scale.y = 0.1;
            marker.scale.z = 0.1;

            marker_pub_->publish(marker);

            previous_point = current_point;
        }
    }

    std::queue<geometry_msgs::msg::PointStamped> coords_queue_;
    geometry_msgs::msg::PointStamped::SharedPtr latest_coords_;
    size_t queue_size_;
    rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr coords_sub_;
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_pub_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<EnemyMarkerNode>());
    rclcpp::shutdown();
    return 0;
}
