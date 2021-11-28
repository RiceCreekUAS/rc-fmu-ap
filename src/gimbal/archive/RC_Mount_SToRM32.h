/*
  SToRM32 mount backend class
 */
#pragma once

#include <AP_HAL/AP_HAL.h>
#include <AP_AHRS/AP_AHRS.h>
#include <AP_HAL/utility/RingBuffer.h>

#include <AP_Math/AP_Math.h>
#include <AP_Common/AP_Common.h>
#include <RC_Channel/RC_Channel.h>
#include "RC_Mount_Backend.h"

#define AP_MOUNT_STORM32_RESEND_MS  1000    // resend angle targets to gimbal once per second
#define AP_MOUNT_STORM32_SEARCH_MS  60000   // search for gimbal for 1 minute after startup

class RC_Mount_SToRM32 : public RC_Mount_Backend
{

public:
    // Constructor
    RC_Mount_SToRM32(/*RC_Mount &frontend, RC_Mount::mount_state &state, uint8_t instance*/);

    // init - performs any required initialisation for this instance
    void init() override;

    // read messages from the gimbal
    void read_messages();

    // send the mavlink heartbeat message periodically
    void send_heartbeat();
    void set_imu_rate();
    
    // test: send set gimbal model message
    void set_gimbal_mode();
    void set_mount_configure();
    
    // update mount position - should be called periodically
    void update() override;

    // has_pan_control - returns true if this mount can control it's pan (required for multicopters)
    bool has_pan_control() const override;

    // set_mode - sets mount's mode
    void set_mode(enum MAV_MOUNT_MODE mode) override;

    // send_mount_status - called to allow mounts to send their status to GCS using the MOUNT_STATUS message
    void send_mount_status(mavlink_channel_t chan) override;

private:

    // send_do_mount_control - send a COMMAND_LONG containing a do_mount_control message
    void send_do_mount_control(float pitch_deg, float roll_deg, float yaw_deg, enum MAV_MOUNT_MODE mount_mode);

    // internal variables
    bool _found_gimbal = false;     // true once the driver has been initialised
    bool _receiving_imu = false;    // true when we start getting imu messages
    uint8_t _sysid;                 // sysid of gimbal
    uint8_t _compid;                // component id of gimbal
    mavlink_channel_t _chan;        // mavlink channel used to communicate with gimbal.  Currently hard-coded to Telem2
    uint32_t _last_send;            // system time of last do_mount_control sent to gimbal
    uint32_t _last_heartbeat;       // ssytem tim eof last heartbeat
    AP_HAL::UARTDriver *_port;
    // ByteBuffer *write_buf;
};