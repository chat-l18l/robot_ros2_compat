#ifndef ROS2_ZENOH_GATEWAY__HELLO_COMPONENT_HPP_
#define ROS2_ZENOH_GATEWAY__HELLO_COMPONENT_HPP_

#include <rclcpp/node.hpp>
#include <rclcpp/node_options.hpp>

namespace robot::gateway
{

/// Minimal component used to prove that the ROS 2 package can build and load.
///
/// Ownership:
/// - The component owns all ROS resources that it creates.
/// - The constructor only borrows options for the duration of the call.
///
/// Thread safety:
/// - Construction must happen on one thread.
/// - The initial implementation contains no shared mutable state.
class HelloComponent final : public rclcpp::Node
{
public:
    /// Constructs a component named ros2_zenoh_gateway.
    ///
    /// Preconditions:
    /// - rclcpp has been initialized by the executable or component container.
    ///
    /// Ownership:
    /// - Does not retain a reference to options.
    explicit HelloComponent(const rclcpp::NodeOptions& options);
};

}  // namespace robot::gateway

#endif  // ROS2_ZENOH_GATEWAY__HELLO_COMPONENT_HPP_
