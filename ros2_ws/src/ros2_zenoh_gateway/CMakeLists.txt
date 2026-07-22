cmake_minimum_required(VERSION 3.16)
project(ros2_zenoh_gateway LANGUAGES CXX)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)

add_library(hello_component SHARED
  src/hello_component.cpp
  src/register_hello_component.cpp
)

target_compile_features(hello_component PUBLIC cxx_std_17)
set_target_properties(hello_component PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(hello_component
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

ament_target_dependencies(hello_component
  rclcpp
  rclcpp_components
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(hello_component PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
  )
endif()

rclcpp_components_register_node(hello_component
  PLUGIN "robot::gateway::HelloComponent"
  EXECUTABLE hello_gateway
)

install(
  TARGETS hello_component
  EXPORT export_hello_component
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_hello_component test/test_hello_component.cpp)

  if(TARGET test_hello_component)
    target_compile_features(test_hello_component PRIVATE cxx_std_17)
    set_target_properties(test_hello_component PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(test_hello_component hello_component)
    ament_target_dependencies(test_hello_component rclcpp)
  endif()
endif()

ament_export_targets(export_hello_component HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp rclcpp_components)
ament_package()
