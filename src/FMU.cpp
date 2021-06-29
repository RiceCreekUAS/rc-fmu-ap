#include <AP_HAL/AP_HAL.h>
#include <AP_BoardConfig/AP_BoardConfig.h>

// test
#include <AP_Common/ExpandingString.h>

#include "setup_board.h"

// #include "airdata.h"
#include "comms.h"
#include "config.h"
#include "gps_mgr.h"
#include "imu_mgr.h"
#include "led.h"
#include "menu.h"
#include "nav_mgr.h"
#include "pilot.h"
#include "power.h"
#include "props2.h"

const AP_HAL::HAL& hal = AP_HAL::get_HAL();
static AP_BoardConfig BoardConfig;
AP_HAL::UARTDriver *console = hal.console;
// AP_HAL::UARTDriver *console = hal.serial(1); // telemetry 1

static PropertyNode config_nav_node;
static PropertyNode pilot_node;

static config_t config;
static gps_mgr_t gps_mgr;
static led_t led;
static menu_t menu;
static power_t power;

// -Wmissing-declarations requires these
void setup();
void loop();

void setup() {
    BoardConfig.init();         // setup any board specific drivers

    console->begin(57600);
    hal.scheduler->delay(2000); // give the electrons a chance to settle
    
    console->printf("\nRice Creek UAS FMU: Rev %d\n", FIRMWARE_REV);
    console->printf("You are seeing this message on the console interface.\n");
    console->printf("Sensor/config communication is on Serial1 @ %d baud (N81) no flow control.\n", DEFAULT_BAUD);

    // load config from sd card
    config.setup();
    if ( !config.load_json_config() ) {
        config.reset_defaults();
    }
    
    // The following code (when enabled) will force setting a specific
    // device serial number when the device boots:
    if ( false ) {
        config.set_serial_number(117);
    }
    config.read_serial_number();
    console->printf("Serial Number: %d\n", config.read_serial_number());
    hal.scheduler->delay(100);

    config_nav_node = PropertyNode("/config/nav"); // after config.setup()
    pilot_node = PropertyNode("/pilot");
    
    // if ( !config.read_storage() ) {
    //     console->printf("Resetting eeprom to default values.");
    //     config.reset_defaults();
    //     config.write_storage();
    // } else {
    //     console->printf("Successfully loaded config from eeprom storage.\n");
    // }
    
    // initialize the IMU and calibration matrices
    imu_mgr.setup();
    imu_mgr.set_strapdown_calibration();
    imu_mgr.set_accel_calibration();
    imu_mgr.set_mag_calibration();
    
    // initialize the pilot interface (RC in, out & mixer)
    pilot.setup();

    // initialize the gps receiver
    gps_mgr.setup();

    // initialize air data (marmot v1)
    // airdata.setup();
    
    // power sensing
    power.setup();
    
    // led status
    led.setup();

    // ekf init (just prints availability status)
    nav_mgr.setup();

    comms.setup();              // do this after gps initialization

    menu.setup();
    
    console->printf("Setup finished.\n");
    console->printf("Ready and transmitting...\n");
}

// main loop
void loop() {
    static uint32_t mainTimer = AP_HAL::millis();
    static uint32_t hbTimer = AP_HAL::millis();
    static uint32_t debugTimer = AP_HAL::millis();

    static uint32_t tempTimer = AP_HAL::millis();
    static uint32_t counter = 0;

    // this is the heartbeat of the system here (DT_MILLIS)
    if ( AP_HAL::millis() - mainTimer >= DT_MILLIS ) {
        if ( AP_HAL::millis() - mainTimer > DT_MILLIS ) {
            comms.main_loop_timer_misses++;
            mainTimer = AP_HAL::millis(); // catch up
            if ( comms.main_loop_timer_misses % 25 == 0 ) {
                console->printf("WARNING: main loop is not completing on time!\n");
            }
        } else {
            mainTimer += DT_MILLIS;
        }
        counter++;
        
        // 1. Sense motion
        imu_mgr.update();

        // 2. Check for gps updates
        gps_mgr.update();

        // 3. Estimate location and attitude
        if ( config_nav_node.getString("selected") != "none" ) {
            nav_mgr.update();
        }

        // 4. Send state to host computer
        if ( true) {
            comms.output_counter += comms.write_pilot_in_bin();
            comms.output_counter += comms.write_gps_bin();
            // comms.output_counter += comms.write_airdata_bin();
            comms.output_counter += comms.write_power_bin();
            // do a little extra dance with the return value because
            // write_status_info_bin() can reset comms.output_counter (but
            // that gets ignored if we do the math in one step)
            uint8_t result = comms.write_status_info_bin();
            comms.output_counter += result;
            if ( config_nav_node.getString("select") != "none" ) {
                comms.output_counter += comms.write_nav_bin();
            }
            // write imu message last: used as an implicit end of data
            // frame marker.
            comms.output_counter += comms.write_imu_bin();
            hal.scheduler->delay(1);
        }

        // 10hz human console output, (begins when gyros finish calibrating)
        if ( AP_HAL::millis() - debugTimer >= 100 ) {
            debugTimer = AP_HAL::millis();
            if ( imu_mgr.gyros_calibrated == 2 ) {
                menu.update();
                if ( menu.display_pilot ) { comms.write_pilot_in_ascii(); }
                if ( menu.display_gps ) { comms.write_gps_ascii(); }
                if ( menu.display_airdata ) { comms.write_airdata_ascii(); }
                if ( menu.display_imu ) { comms.write_imu_ascii(); }
                if ( menu.display_nav ) { comms.write_nav_ascii(); }
                if ( menu.display_act ) { comms.write_actuator_out_ascii(); }
            }
        }

        // 10 second heartbeat console output
        if ( AP_HAL::millis() - hbTimer >= 10000 ) {
            hbTimer = AP_HAL::millis();
            if ( imu_mgr.gyros_calibrated == 2 ) {
                comms.write_status_info_ascii();
                comms.write_power_ascii();
                float elapsed_sec = (AP_HAL::millis() - tempTimer) / 1000.0;
                console->printf("Available mem: %d bytes\n",
                                (unsigned int)hal.util->available_memory());
                console->printf("Performance = %.1f hz\n", counter/elapsed_sec);
                //PropertyNode("/").pretty_print();
                console->printf("\n");

                if ( false ) {
                    // system info
                    ExpandingString dma_info {};
                    ExpandingString mem_info {};
                    ExpandingString uart_info {};
                    ExpandingString thread_info {};
                    hal.util->dma_info(dma_info);
                    hal.util->mem_info(mem_info);
                    hal.util->uart_info(uart_info);
                    hal.util->thread_info(thread_info);
                    console->printf("dma info:\n%s\n", dma_info.get_string());
                    console->printf("mem info:\n%s\n", mem_info.get_string());
                    console->printf("uart info:\n%s\n", uart_info.get_string());
                    // console->printf("thread info:\n%s\n", thread_info.get_string());
                }
            }
        }
        
        // poll the pressure sensors
        // airdata.update();

        // read power values
        power.update();

        if ( pilot.read() ) {
            bool ap_state = pilot_node.getBool("ap_enabled");
            static bool last_ap_state = ap_state;
            if ( ap_state and !last_ap_state ) {
                console->printf("ap enabled\n");
            } else if ( !ap_state and last_ap_state ) {
                console->printf("ap disabled (manaul flight)\n");
            }
            last_ap_state = ap_state;
        }

        // read in any host commmands (config, inceptors, etc.)
        comms.read_commands();

        if ( pilot.changed ) {
            pilot.write();
        }

        // blink the led
        led.do_policy(imu_mgr.gyros_calibrated);
        led.update();
    }
}

AP_HAL_MAIN();