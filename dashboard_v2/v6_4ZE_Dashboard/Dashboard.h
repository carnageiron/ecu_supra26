#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>
#include "Config.h"

struct DashboardData
{
    uint16_t speed;           ///< km/h
    int16_t  motorTorque;     ///< Nm
    uint8_t  soc;             ///< 0-100 %
    uint8_t  soh;             ///< 0-100 %
    int16_t  batteryTemp;     ///< deg C
    int16_t  mcTemp;          ///< deg C
    uint16_t packVoltage;     ///< Volts
    int16_t  packCurrent;     ///< Amps
    bool     precharge;       ///< relay state
    bool     fault;           ///< fault flag
    bool     canOnline;       ///< CAN alive flag
    uint32_t frameCounter;    ///< total frame count
    uint32_t lastCANID;       ///< last CAN ID
};

class Dashboard
{
public:
    void init();
    DashboardData&       getData();
    const DashboardData& getData() const;
    void update();
    void forceFullRedraw();
    DashboardPage getCurrentPage() const;
    void setPage(DashboardPage page);

private:
    DashboardData  current_;
    DashboardData  previous_;
    DashboardPage  currentPage_;
    bool           forceRedraw_;
};

#endif // DASHBOARD_H
