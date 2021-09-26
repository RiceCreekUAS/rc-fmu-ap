#include "util/lowpass.h"
#include "ground.h"

// initialize ground estimator variables
void ground_est_t::init() {
    airdata_node = PropertyNode( "/sensors/airdata" );
    nav_node = PropertyNode( "/filters/nav" );
    pos_node = PropertyNode( "/position" );
    ground_alt_filt.set_time_factor(30.0);
    ground_alt_calibrated = false;
}

void ground_est_t::update(float dt) {
    if ( nav_node.getInt("status") == 2 ) {
        // determine ground reference altitude.  Average nav altitude
        // over the most recent 30 seconds that we are !is_airborne
        if ( !ground_alt_calibrated and nav_node.getInt("status") == 2 ) {
            ground_alt_calibrated = true;
            ground_alt_filt.init( nav_node.getDouble("altitude_m") );
        }

        if ( !airdata_node.getBool("is_airborne") ) {
            // update ground altitude estimate while not airborne
            // fixme: create a settled (not moving) parameter and use that?
            ground_alt_filt.update( nav_node.getDouble("altitude_m"), dt );
            pos_node.setDouble( "altitude_ground_m",
                                ground_alt_filt.get_value() );
        }

        // fixme: should this move to airdata helper or stay here?
        float agl_m = nav_node.getDouble( "altitude_m" )
            - ground_alt_filt.get_value();
        pos_node.setDouble( "altitude_agl_m", agl_m );
    }
}
