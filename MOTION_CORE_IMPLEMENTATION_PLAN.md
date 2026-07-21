# Motion Core Implementation Plan

Status: proposed  
Target: ESP32-P4, FreeRTOS, Ethernet, Zenoh, C  
ROS 2 compatibility target: Clearpath Jackal J100 and Husky A300 high-level motion interface

## 1. Goal

Implement the smallest safe interface required to drive, turn, stop and report wheel odometry while remaining compatible with the high-level Clearpath ROS 2 API.

The first implementation does **not** drive the motors. It receives, validates and logs motion commands. Motor output is enabled only after the message contract, watchdog and safety tests pass.

## 2. Scope

### Phase 1: required motion interface

| Direction | ROS 2 topic | ROS 2 type | Purpose | Priority |
|---|---|---|---|---|
| Input | `/cmd_vel` | `geometry_msgs/msg/TwistStamped` | Requested vehicle velocity | P0 |
| Output | `/platform/odom` | `nav_msgs/msg/Odometry` | Raw wheel odometry | P0 |
| Output | `/platform/emergency_stop` | `std_msgs/msg/Bool` | Hardware emergency-stop state | P0 |
| Output | `/platform/cmd_vel` | `geometry_msgs/msg/TwistStamped` | Command accepted after validation and limiting | P1 |
| Output | `/tf` | `tf2_msgs/msg/TFMessage` | `odom` to `base_link` transform | P1 |
| Output | `/joint_states` | `sensor_msgs/msg/JointState` | Wheel state for visualization and tools | P2 |

Encoder samples and the command watchdog are internal firmware interfaces, not ROS 2 topics.

### Deferred

- Battery, current and power telemetry
- Motor and controller temperatures
- Controller diagnostics
- Lighting, fans and pin I/O
- GPS, IMU and LiDAR
- Filtered odometry
- Jackal low-level `Drive` and `DriveFeedback`
- Husky Lynx motor-specific interfaces
- Direct PWM or per-motor command topics

Only one command authority is allowed in Phase 1: `/cmd_vel`. A second low-level motor command path would create ambiguous ownership and can bypass the safety gate.

## 3. System boundary

The implementation is split into two layers:

1. **ROS 2 to Zenoh adapter**
   - Exposes exact Clearpath-compatible ROS 2 topics and message types.
   - Translates ROS 2 messages to versioned Zenoh wire messages.
   - Translates firmware state back to ROS 2 messages.

2. **ESP32-P4 motion core**
   - Uses small fixed-size C structures.
   - Does not depend on ROS 2 dynamic message structures.
   - Validates commands and owns the watchdog and safety state machine.
   - Converts vehicle velocity to left/right targets.
   - Reads encoders and calculates raw odometry.

The exact ROS 2-to-Zenoh mapping is the next design step and must be approved before handler implementation.

## 4. Required behavior

### Motion command semantics

- `linear.x`: forward velocity in metres per second.
- `angular.z`: yaw rate in radians per second.
- `linear.y`: must be zero for the skid-steer base.
- All other linear and angular axes must be zero in Phase 1.
- Zero `linear.x` and zero `angular.z` request a normal stop.
- `NaN`, infinity, malformed messages and unsupported non-zero axes are rejected.
- Limits and rejection/clamping policy must be explicit configuration.

### Stop paths

1. **Normal stop**: a valid zero velocity command.
2. **Communication stop**: command watchdog expires.
3. **Emergency stop**: hardware e-stop removes or inhibits drive independently of ROS 2 and Zenoh.
4. **Fault stop**: controller, encoder or internal safety fault moves the state machine to `FAULT`.

On boot, reset, reconnect or decode failure, motor targets remain zero.

### Safety state machine

Required states:

- `STOPPED`
- `ACTIVE`
- `TIMEOUT`
- `ESTOP`
- `FAULT`

Only a fresh, valid command may transition from `STOPPED` to `ACTIVE`. Network messages may never clear a physical e-stop. Recovery rules from `ESTOP` and `FAULT` must be explicit and tested.

The watchdog uses monotonic FreeRTOS receive time, not the ROS timestamp. Initial proposed timeout: 500 ms, to be validated by measurement.

## 5. Skid-steer conversion

For requested linear velocity `v`, angular velocity `omega` and effective track width `b`:

```text
left_velocity  = v - (omega * b / 2)
right_velocity = v + (omega * b / 2)
```

The effective track width is configurable because tyre scrub and terrain make it differ from the mechanical track width.

When either side exceeds its configured limit, preserve curvature by scaling both sides with the same factor. Do not clamp each side independently.

## 6. Firmware modules

Proposed small modules and responsibilities:

| Module | Responsibility |
|---|---|
| `motion_command` | Decode-independent command representation, validation and limiting |
| `motion_state` | Safety state machine and transition reasons |
| `motion_watchdog` | Monotonic command-age tracking |
| `skid_steer` | Vehicle twist to left/right target conversion |
| `encoder_state` | Four encoder samples, validity and wrap handling |
| `odometry` | Left/right aggregation and planar pose integration |
| `motion_log` | Rate-limited event and counter logging |
| `zenoh_motion_adapter` | Zenoh key subscriptions/publications and payload conversion |
| `motor_output` | Existing motor-controller integration behind the safety gate |

Initial handlers:

```c
void motion_command_handle(const MotionCommand *command);
void wheel_encoder_state_handle(const WheelEncoderState *state);
void emergency_stop_handle(bool active);

void motion_watchdog_update(uint64_t monotonic_time_us);
bool motion_command_validate(const MotionCommand *command,
                             const MotionLimits *limits,
                             MotionValidationResult *result);
void motion_state_log(const MotionStateSnapshot *snapshot);
```

Function signatures are provisional until the Zenoh wire contract and existing robot code have been inspected.

## 7. Implementation milestones

### M0 — Existing-code inventory and baseline

- [ ] Identify reusable Ethernet and Zenoh initialization code.
- [ ] Identify existing motor command, encoder, e-stop and logging interfaces.
- [ ] Record ESP-IDF, FreeRTOS and Zenoh-Pico versions.
- [ ] Record current task priorities, stack sizes and update frequencies.
- [ ] Capture current safe-stop behavior as tests or a reproducible bug/behavior report.
- [ ] Document vehicle limits, wheel radius, mechanical track width and encoder resolution.

Exit criterion: interfaces and hardware assumptions are documented; no production behavior changed.

### M1 — ROS 2 to Zenoh message contract

- [ ] Decide whether ROS 2 messages are carried as CDR or translated into custom fixed-size Zenoh payloads.
- [ ] Define Zenoh keys for commands, accepted commands, odometry and e-stop state.
- [ ] Define byte order, field sizes, units, version, sequence number and timestamps.
- [ ] Define QoS/congestion behavior and publisher ownership.
- [ ] Define compatibility rules for future message versions.
- [ ] Add golden payload examples and encode/decode tests.

Exit criterion: one reviewed contract maps every Phase 1 ROS 2 field to a Zenoh field or an explicit constant/default.

### M2 — Receive, validate and log; motors disabled

- [ ] Subscribe to the motion command key.
- [ ] Decode into a fixed-size `MotionCommand`.
- [ ] Validate version, length, finite values, axes, sequence and age.
- [ ] Log changes immediately and counters once per second.
- [ ] Count valid, rejected, stale, duplicate and malformed messages.
- [ ] Keep motor output unconditionally zero.
- [ ] Publish or log the accepted command after limiting.

Exit criterion: replayed valid and invalid commands produce deterministic logs and zero motor output.

### M3 — Safety gate and watchdog

- [ ] Implement the five-state safety state machine.
- [ ] Implement the monotonic receive watchdog.
- [ ] Integrate physical e-stop input.
- [ ] Integrate existing controller fault input.
- [ ] Make every unsafe transition command zero before returning.
- [ ] Require explicit recovery from `FAULT` and safe recovery from `ESTOP`.

Exit criterion: all stop-path tests pass with motor output replaced by a test spy/fake.

### M4 — Skid-steer target calculation

- [ ] Implement and unit-test the left/right equations.
- [ ] Configure effective track width and velocity limits.
- [ ] Preserve curvature when scaling excessive targets.
- [ ] Add acceleration/deceleration limiting only if required by measured hardware behavior.
- [ ] Connect targets to the existing motor layer behind the safety gate.

Exit criterion: bench test with wheels unloaded demonstrates forward, reverse, turn and stop with correct signs.

### M5 — Encoder ingestion and raw odometry

- [ ] Read all four encoders with timestamps and validity.
- [ ] Handle counter wrap, reset, impossible jumps and stale samples.
- [ ] Derive one robust velocity and displacement per side from two motors.
- [ ] Specify behavior when the two encoders on one side disagree.
- [ ] Integrate `x`, `y`, yaw, linear velocity and yaw rate.
- [ ] Define covariance values based on measurement rather than arbitrary zeros.
- [ ] Publish odometry through Zenoh.

Exit criterion: measured straight distance and rotation remain inside agreed tolerances.

### M6 — ROS 2 compatibility adapter

- [ ] Receive `geometry_msgs/msg/TwistStamped` on `/cmd_vel`.
- [ ] Publish `nav_msgs/msg/Odometry` on `/platform/odom`.
- [ ] Publish `std_msgs/msg/Bool` on `/platform/emergency_stop`.
- [ ] Publish accepted `TwistStamped` on `/platform/cmd_vel`.
- [ ] Add `/tf` after frame ownership is agreed.
- [ ] Verify topic names, frame IDs, units, timestamps and QoS against Clearpath behavior.

Exit criterion: standard ROS 2 tools can command and observe the base without robot-specific knowledge.

### M7 — Hardware acceptance

- [ ] Test at low configured speed in a secured area.
- [ ] Test normal zero-command stop.
- [ ] Disconnect Ethernet during movement and measure stop latency.
- [ ] Stop command publishing while the link remains up and measure watchdog stop latency.
- [ ] Activate physical e-stop during movement and measure response.
- [ ] Reboot the ESP32-P4 and gateway; verify no unintended motion.
- [ ] Measure straight-line distance, turn angle and repeatability.
- [ ] Save logs and test results in the repository.

Exit criterion: all safety and motion acceptance criteria pass and results are recorded.

## 8. Minimum test set

### Unit tests

- [ ] Valid forward command.
- [ ] Valid reverse command.
- [ ] In-place rotation.
- [ ] Combined translation and rotation.
- [ ] Zero command.
- [ ] Curvature-preserving saturation.
- [ ] Reject `NaN`, positive/negative infinity and unsupported axes.
- [ ] Reject wrong version and wrong payload length.
- [ ] Watchdog boundary before, at and after timeout.
- [ ] Every safety-state transition and prohibited transition.
- [ ] Encoder positive/negative movement and wraparound.
- [ ] Odometry straight line, arc and in-place rotation.

### Integration tests

- [ ] ROS 2 command reaches the ESP32-P4 log with matching values and sequence.
- [ ] Accepted command is returned with applied limits.
- [ ] Missing Zenoh samples trigger timeout.
- [ ] Ethernet loss triggers timeout.
- [ ] Duplicate, delayed and out-of-order samples follow the contract.
- [ ] E-stop state reaches ROS 2 while hardware remains authoritative.
- [ ] Odometry fields, frames, signs and units match the contract.

### Hardware tests

- [ ] Motors cannot energize during boot or network reconnect.
- [ ] Motor directions match positive `linear.x` and `angular.z` conventions.
- [ ] Physical e-stop works without ROS 2 or Zenoh.
- [ ] Maximum observed communication-stop latency is within the agreed limit.
- [ ] Encoder disconnect or implausible disagreement produces defined safe behavior.

## 9. Logging requirements

Log immediately:

- Safety-state changes and reason.
- First valid command after inactivity.
- Rejected or malformed payloads.
- Watchdog expiration.
- E-stop changes.
- Encoder/controller faults.

Log a summary at a limited rate, initially once per second:

- Current requested and accepted velocity.
- Command age.
- Valid/rejected/stale/duplicate counters.
- Left/right target and measured velocity.
- Current safety state.

Do not print every 20-100 Hz sample; logging must not disturb real-time control.

## 10. Decisions required before implementation

- [ ] Direct ROS 2 CDR over Zenoh versus a custom fixed-size wire format.
- [ ] Zenoh key namespace.
- [ ] Required command rate and watchdog timeout.
- [ ] Maximum linear/angular velocity and acceleration.
- [ ] Reject versus clamp policy.
- [ ] ROS time source and behavior before clock synchronization.
- [ ] E-stop reset and fault recovery procedure.
- [ ] Encoder aggregation policy for two motors per side.
- [ ] Ownership of `odom -> base_link` TF.
- [ ] Exact compatibility baseline: Jackal J100 only first, then Husky additions.

## 11. Definition of done for the first implementation slice

The first slice is complete when:

- `/cmd_vel` is translated to the agreed Zenoh wire message.
- The ESP32-P4 receives, validates and rate-limited logs the command.
- Invalid and stale commands are rejected with counters and reasons.
- The watchdog demonstrably reaches `TIMEOUT`.
- Motor output is demonstrably held at zero in all cases.
- Host-side unit tests and an ESP32-P4 integration test pass.
- The message contract and test evidence are committed.

Only after this slice passes do we enable skid-steer calculation and connect the existing motor output.

## 12. Related document

See [`CLEARPATH_ROS2_COMPATIBILITY_TODO.md`](CLEARPATH_ROS2_COMPATIBILITY_TODO.md) for the complete Jackal/Husky ROS 2 compatibility backlog and message inventory.
