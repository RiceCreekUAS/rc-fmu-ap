// a simple rate limiter class.  this is intentionly crude,
// approximate, simplistic to keep the code simple.  Primary use case
// is rate limiting telemetry messages which doesn't have to be exact
// if the desired hz is not quite an even diviser of 1000 (millis).

#pragma once

#include "setup_board.h"

class RateLimiter {

private:

    uint32_t timer = 0;
    uint32_t dt_millis = 1;
    
public:

    RateLimiter( float hz );
    ~RateLimiter();
    bool update();

};