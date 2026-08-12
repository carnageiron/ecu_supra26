#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "Config.h"

namespace Display
{
    void init();
    void drawStaticUI();
    void showBootScreen();
    void updateBootStatus(const char* component, bool success);

    void drawSpeed(uint16_t speed);
    void drawMotorTorque(int16_t torque);
    void drawLastCANID(uint32_t id);
    void drawFrameCounter(uint32_t count);
    void drawPrecharge(bool active);
    void drawLogo();

    void drawBatteryTemp(int16_t tempC);
    void drawMCTemp(int16_t tempC);
    void drawPackVoltage(uint16_t volts);
    void drawPackCurrent(int16_t amps);

    void drawSOC(uint8_t soc);
    void drawSOH(uint8_t soh);
    void drawBattery(uint8_t soc);

    void drawCANStatus(bool online);

    void drawProgressBar(int x, int y, int w, int h,
                         uint8_t percent, uint16_t barColor);
    void drawBatteryIcon(int x, int y, int w, int h, uint8_t percent);
    uint16_t getPercentColor(uint8_t percent);
}

#endif // DISPLAY_H
