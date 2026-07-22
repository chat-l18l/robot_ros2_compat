# ROS 2 Zenoh gateway

This package starts with the smallest useful ROS 2 component. The current
component only logs that it was constructed. It intentionally has no publishers,
subscriptions, timers or Zenoh dependency yet.

## Build and test

From the repository root:

~~~bash
pixi run build
pixi run test-result
~~~

## Run as a standalone component executable

The registration macro generates the hello_gateway executable:

~~~bash
pixi shell
source ros2_ws/install/setup.bash
ros2 run ros2_zenoh_gateway hello_gateway
~~~

Expected log message:

~~~text
ROS 2 gateway component started
~~~

Stop the spinning node with Ctrl+C.

## Load into a component container

Terminal 1:

~~~bash
pixi shell
source ros2_ws/install/setup.bash
ros2 run rclcpp_components component_container
~~~

Terminal 2:

~~~bash
pixi shell
source ros2_ws/install/setup.bash
ros2 component load /ComponentManager \
  ros2_zenoh_gateway \
  robot::gateway::HelloComponent
~~~

The next increments will add a ROS publisher, a subscriber, pure message mapping
and finally a concrete Zenoh session. Abstractions are added only when a real
ownership, lifetime or testing boundary requires them.
