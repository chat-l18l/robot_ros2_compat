#include "ros2_zenoh_gateway/hello_component.hpp"

#include <rclcpp/logging.hpp>

namespace robot::gateway
{

HelloComponent::HelloComponent(const rclcpp::NodeOptions& options)
    : Node("ros2_zenoh_gateway", options)
{
    RCLCPP_INFO(get_logger(), "ROS 2 gateway component started");
}

}  // namespace robot::gateway
