// ins wrapper class

#pragma once

#include "setup_board.h"
#include "props2.h"
#include "nav/nav_structs.h"
#include "nav/ekf15.h"
#include "nav/ekf15_mag.h"

class nav_mgr_t {
    
private:
    bool ekf_inited = false;
    unsigned long int gps_last_millis = 0;
    EKF15 ekf;
    EKF15_mag ekf_mag;
    PropertyNode config_nav_node;
    PropertyNode gps_node;
    PropertyNode imu_node;
    PropertyNode nav_node;
    
public:
    NAVdata data;
    uint8_t status;             // 0 = uninitted, 1 = no gps, 2 = 0k
    void init();
    void configure();
    void update();
    void reinit();              // request the filter reinit itself
};

extern nav_mgr_t nav_mgr;
