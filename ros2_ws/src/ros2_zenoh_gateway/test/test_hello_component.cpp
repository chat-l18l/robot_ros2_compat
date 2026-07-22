#include <gtest/gtest.h>

#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <string>

#include "ros2_zenoh_gateway/hello_component.hpp"

namespace
{

class RclcppContext final
{
public:
    RclcppContext()
    {
        rclcpp::init(0, nullptr);
    }

    ~RclcppContext()
    {
        rclcpp::shutdown();
    }

    RclcppContext(const RclcppContext&) = delete;
    RclcppContext& operator=(const RclcppContext&) = delete;
    RclcppContext(RclcppContext&&) = delete;
    RclcppContext& operator=(RclcppContext&&) = delete;
};

TEST(HelloComponentTest, constructs_with_expected_node_name)
{
    const RclcppContext context;
    const auto component = std::make_unique<robot::gateway::HelloComponent>(rclcpp::NodeOptions{});

    EXPECT_EQ(component->get_name(), std::string{"ros2_zenoh_gateway"});
}

}  // namespace
