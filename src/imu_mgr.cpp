#include "imu_mgr.h"

#include "setup_board.h"

#include <AP_HAL/AP_HAL.h>

// Setup imu defaults:
void imu_mgr_t::defaults() {
    Eigen::Matrix3f strapdown3x3 = Eigen::Matrix3f::Identity();
    for ( int i = 0; i < 9; i++ ) {
        imu_calib_node.setFloat("strapdown", i, strapdown3x3.data()[i]);
    }
    strapdown = Eigen::Matrix4f::Identity();

    for ( int i = 0; i < 3; i++ ) {
        imu_calib_node.setFloat("accel_scale", i, 1.0);
    }
    for ( int i = 0; i < 3; i++ ) {
        imu_calib_node.setFloat("accel_translate", i, 0.0);
    }
    accel_affine = Eigen::Matrix4f::Identity();
    
    mag_affine = Eigen::Matrix4f::Identity();
    for ( int i = 0; i < 16; i++ ) {
        imu_calib_node.setFloat("mag_affine", i, mag_affine.data()[i]);
    }
}

// Update the R matrix (called after loading/receiving any new config message)
void imu_mgr_t::set_strapdown_calibration() {
    strapdown = Eigen::Matrix4f::Identity();
    for ( int i = 0; i < 3; i++ ) {
        for ( int j = 0; j < 3; j++ ) {
            strapdown(i,j) = imu_calib_node.getFloat("strapdown", i*3+j);
        }
    }
    Eigen::Matrix4f scale = Eigen::Matrix4f::Identity();
    for (int i = 0; i < 3; i++ ) {
        scale(i,i) = imu_calib_node.getFloat("accel_scale", i);
    }
    Eigen::Matrix4f translate = Eigen::Matrix4f::Identity();
    for (int i = 0; i < 3; i++ ) {
        // column major
        translate(i,3) = imu_calib_node.getFloat("accel_translate", i);
    }
    accel_affine = translate * strapdown * scale;
    console->printf("Accel affine calibration matrix:\n");
    for ( int i = 0; i < 4; i++ ) {
        console->printf("  ");
        for ( int j = 0; j < 4; j++ ) {
            console->printf("%.4f ", accel_affine(i,j));
        }
        console->printf("\n");
    }
    console->printf("IMU strapdown calibration matrix:\n");
    for ( int i = 0; i < 4; i++ ) {
        console->printf("  ");
        for ( int j = 0; j < 4; j++ ) {
            console->printf("%.4f ", strapdown(i,j));
        }
        console->printf("\n");
    }
}

// update the mag calibration matrix from the config structur
void imu_mgr_t::set_mag_calibration() {
    mag_affine = Eigen::Matrix4f::Identity();
    for ( int i = 0; i < 4; i++ ) {
        for ( int j = 0; j < 4; j++ ) {
            mag_affine(i,j) = imu_calib_node.getFloat("mag_affine", i*4+j);
        }
    }

    console->printf("Magnetometer affine matrix:\n");
    for ( int i = 0; i < 4; i++ ) {
        console->printf("  ");
        for ( int j = 0; j < 4; j++ ) {
            console->printf("%.4f ", mag_affine(i,j));
        }
        console->printf("\n");
    }
}

// configure the IMU settings and setup the ISR to aquire the data
void imu_mgr_t::setup() {
    imu_node = PropertyNode("/sensors/imu");
    imu_calib_node = PropertyNode("/config/imu/calibration");
    imu_hal.setup();
}

// query the imu and update the structures
void imu_mgr_t::update() {
    // static uint8_t accel_count = ins.get_accel_count();
    // static uint8_t gyro_count = ins.get_gyro_count();

    imu_hal.update();
    imu_millis = imu_hal.raw_millis;
    
    accels_raw << imu_hal.accel.x, imu_hal.accel.y, imu_hal.accel.z, 1.0;
    gyros_raw << imu_hal.gyro.x, imu_hal.gyro.y, imu_hal.gyro.z, 1.0;
    tempC = imu_hal.tempC;

    Eigen::Vector4f mags_precal;
    mags_precal << imu_hal.mag.x, imu_hal.mag.y, imu_hal.mag.z, 1.0;
    mags_raw = strapdown * mags_precal;
    
    accels_cal = accel_affine * accels_raw;
    gyros_cal = strapdown * gyros_raw;

    //accels_cal(0) = ax_cal.calibrate(accels_nocal(0), tempC);
    //accels_cal(1) = ay_cal.calibrate(accels_nocal(1), tempC);
    //accels_cal(2) = az_cal.calibrate(accels_nocal(2), tempC);
        
    mags_cal = mag_affine * mags_raw;
    
    if ( gyros_calibrated < 2 ) {
        calibrate_gyros();
    } else {
        gyros_cal.segment(0,3) -= gyro_startup_bias;
    }

    // publish
    imu_node.setUInt("millis", imu_millis);
    imu_node.setFloat("timestamp", imu_millis / 1000.0);
    imu_node.setFloat("ax_raw", accels_raw(0));
    imu_node.setFloat("ay_raw", accels_raw(1));
    imu_node.setFloat("az_raw", accels_raw(2));
    imu_node.setFloat("p_raw", gyros_raw(0));
    imu_node.setFloat("q_raw", gyros_raw(1));
    imu_node.setFloat("r_raw", gyros_raw(2));
    imu_node.setFloat("hx_raw", mags_raw(0));
    imu_node.setFloat("hy_raw", mags_raw(1));
    imu_node.setFloat("hz_raw", mags_raw(2));
    imu_node.setFloat("ax_mps2", accels_cal(0));
    imu_node.setFloat("ay_mps2", accels_cal(1));
    imu_node.setFloat("az_mps2", accels_cal(2));
    imu_node.setFloat("p_rps", gyros_cal(0));
    imu_node.setFloat("q_rps", gyros_cal(1));
    imu_node.setFloat("r_rps", gyros_cal(2));
    imu_node.setFloat("hx", mags_cal(0));
    imu_node.setFloat("hy", mags_cal(1));
    imu_node.setFloat("hz", mags_cal(2));
    imu_node.setFloat("tempC", tempC);
}

// stay alive for up to 15 seconds looking for agreement between a 1
// second low pass filter and a 0.1 second low pass filter.  If these
// agree (close enough) for 4 consecutive seconds, then we calibrate
// with the 1 sec low pass filter value.  If time expires, the
// calibration fails and we run with raw gyro values.
void imu_mgr_t::calibrate_gyros() {
    if ( gyros_calibrated == 0 ) {
        console->printf("Initialize gyro calibration: ");
        slow = gyros_cal.segment(0,3);
        fast = gyros_cal.segment(0,3);
        total_timer = AP_HAL::millis();
        good_timer = AP_HAL::millis();
        output_timer = AP_HAL::millis();
        gyros_calibrated = 1;
    }

    fast = 0.95 * fast + 0.05 * gyros_cal.segment(0,3);
    slow = 0.995 * fast + 0.005 * gyros_cal.segment(0,3);
    // use 'slow' filter value for calibration while calibrating
    gyro_startup_bias << slow;

    float max = (slow - fast).cwiseAbs().maxCoeff();
    if ( max > cutoff ) {
        good_timer = AP_HAL::millis();
    }
    if (  AP_HAL::millis() - output_timer >= 1000 ) {
        output_timer = AP_HAL::millis();
        if ( AP_HAL::millis() - good_timer < 1000 ) {
            console->printf("x");
        } else {
            console->printf("*");
        }
    }
    if ( AP_HAL::millis() - good_timer > 4100 || AP_HAL::millis() - total_timer > 15000 ) {
        console->printf("\n");
        // set gyro zero points from the 'slow' filter.
        gyro_startup_bias = slow;
        gyros_calibrated = 2;
        // update(); // update imu_calib values before anything else get's a chance to read them // FIXME???
        console->printf("Average gyro startup bias: %.4f %.4f %.4f\n",
                        gyro_startup_bias(0), gyro_startup_bias(1),
                        gyro_startup_bias(2));
        if ( AP_HAL::millis() - total_timer > 15000 ) {
            console->printf("gyro init: too much motion, using best average guess.\n");
        } else {
            console->printf("gyro init: success.\n");
        }
    }
}

// global shared instance
imu_mgr_t imu_mgr;
