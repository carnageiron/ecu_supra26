#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "Dashboard.h"

namespace CANManager
{
    bool     init();
    void     pollParameters();
    void     receive(DashboardData& data);
    void     checkTimeout(DashboardData& data);
    uint32_t getLastFrameTime();
}

#endif // CAN_MANAGER_H
