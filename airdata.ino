#include "src/BME280/BME280.h"   // onboard barometer

#if defined AURA_V2
# include "src/MS4525DO/MS4525DO.h"
#elif defined MARMOT_V1
# include "src/AMS5915/AMS5915.h"
#endif

#if defined OLD_BFS_AIRDATA
# include "Wire.h"
#endif

#if defined AURA_V2
 BME280 bme(0x76, &Wire);
#elif defined MARMOT_V1
 BME280 bme(26);
#endif

#if defined OLD_BFS_AIRDATA
 const uint8_t airDataAddr = 0x22;
 volatile uint8_t airDataBuff[8]; 
#endif

#if defined AURA_V2
 MS4525DO dPress(0x28, &Wire);
#elif defined MARMOT_V1
 AMS5915 dPress(0x27, &Wire1, AMS5915_0020_D);
 AMS5915 sPress(0x26, &Wire1, AMS5915_1200_B);
#endif

int bme_status = -1;
float bme_press, bme_temp, bme_hum;

void airdata_setup() {
    bme_status = bme.begin();
    if ( bme_status < 0 ) {
        Serial.println("BME280 initialization unsuccessful");
        Serial.println("Check wiring or try cycling power");
        delay(1000);
    } else {
        Serial.println("BME280 driver ready.");
    }
    
    dPress.begin();
    #if defined MARMOT_V1
     sPress.begin();
    #endif
}

void airdata_update() {
    // onboard static pressure sensor
    if ( bme_status >= 0 ) {
        // get the pressure (Pa), temperature (C),
        // and humidity data (%RH) all at once
        bme.getData(&bme_press,&bme_temp,&bme_hum);
    }

    bool result;
    
    // external differential pressure sensor
    result = dPress.getData(&airdata_diffPress_pa, &airdata_temp_C);
    if ( !result ) {
        Serial.println("Error while reading dPress sensor.");
        airdata_error_count++;
    }
    #if defined MARMOT_V1
     // external differential pressure sensor
     result;
     float tmp;
     result = sPress.getData(&airdata_staticPress_pa, &tmp);
     if ( !result ) {
         Serial.println("Error while reading sPress sensor.");
         airdata_error_count++;
     }
    #endif
     
    #if defined OLD_BFS_AIRDATA
     // gather air data from external BFS board
     Wire.requestFrom(airDataAddr, sizeof(airDataBuff));
     int i = 0;
     while ( Wire.available() ) {
         airDataBuff[i] = Wire.read();
         i++;
     }
     uint8_t *p = airDataBuff;
     airdata_staticPress_pa = *(float *)p; p += 4;
     airdata_diffPress_pa = *(float *)p;
    #endif
}
