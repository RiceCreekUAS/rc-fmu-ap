#include <AP_HAL/AP_HAL.h>
#include <AP_BattMonitor/AP_BattMonitor_Analog.h>

#include "setup_board.h"
#include "power.h"

// fixme/configurable
static float amps_offset = 0.0;

void power_t::init() {
    PropertyNode config_node("/config/power");
    power_node = PropertyNode("/sensors/power");
    PropertyNode("/sensors/power");
    hal.scheduler->delay(1000);
    batt_volt_divider = AP_BATT_VOLTDIVIDER_DEFAULT;
    if ( config_node.hasChild("batt_volt_divider") ) {
        batt_volt_divider = config_node.getDouble("batt_volt_divider");
    }
    console->printf("Battery volt pin: %d\n", AP_BATT_VOLT_PIN);
    console->printf("Battery current pin: %d\n", AP_BATT_CURR_PIN);
    console->printf("Volt divider: %.2f\n", batt_volt_divider);
    console->printf("Amp per volt: %.2f\n", AP_BATT_CURR_AMP_PERVOLT_DEFAULT);
    _volt_pin_analog_source = hal.analogin->channel(AP_BATT_VOLT_PIN);
    _curr_pin_analog_source = hal.analogin->channel(AP_BATT_CURR_PIN);
}

void power_t::update() {
    // avionics voltage
    avionics_v = hal.analogin->board_voltage();
    power_node.setDouble("avionics_v", avionics_v);
    
    // battery volts / amps
    battery_volts = _volt_pin_analog_source->voltage_average()
        * batt_volt_divider;
    battery_amps = (_curr_pin_analog_source->voltage_average() - amps_offset)
        * AP_BATT_CURR_AMP_PERVOLT_DEFAULT;
    power_node.setDouble("battery_volts", battery_volts);
    power_node.setDouble("battery_amps", battery_amps);
}
