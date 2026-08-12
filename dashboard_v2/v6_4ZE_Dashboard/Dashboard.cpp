#include "Dashboard.h"
#include "Display.h"

void Dashboard::init()
{
    current_.speed = 0; current_.motorTorque = 0;
    current_.soc = 0;   current_.soh = 98;
    current_.batteryTemp = 0; current_.mcTemp = 0;
    current_.packVoltage = 0; current_.packCurrent = 0;
    current_.precharge = false; current_.fault = false;
    current_.canOnline = false;
    current_.frameCounter = 0; current_.lastCANID = 0;

    previous_.speed = 0xFFFF; previous_.motorTorque = 0x7FFF;
    previous_.soc = 0xFF;     previous_.soh = 0xFF;
    previous_.batteryTemp = 0x7FFF; previous_.mcTemp = 0x7FFF;
    previous_.packVoltage = 0xFFFF; previous_.packCurrent = 0x7FFF;
    previous_.precharge = true; previous_.fault = true;
    previous_.canOnline = true;
    previous_.frameCounter = 0xFFFFFFFF; previous_.lastCANID = 0xFFFFFFFF;

    currentPage_ = DashboardPage::DRIVER;
    forceRedraw_ = true;
}

DashboardData& Dashboard::getData()             { return current_; }
const DashboardData& Dashboard::getData() const  { return current_; }

void Dashboard::update()
{
    if (currentPage_ != DashboardPage::DRIVER) { return; }
    const bool all = forceRedraw_;

    if (all || current_.speed != previous_.speed)
    {
        Display::drawSpeed(current_.speed);
        previous_.speed = current_.speed;
    }

    if (all || current_.motorTorque != previous_.motorTorque)
    {
        Display::drawMotorTorque(current_.motorTorque);
        previous_.motorTorque = current_.motorTorque;
    }

    if (all || current_.precharge != previous_.precharge)
    {
        Display::drawPrecharge(current_.precharge);
        previous_.precharge = current_.precharge;
    }

    if (all || current_.batteryTemp != previous_.batteryTemp)
    {
        Display::drawBatteryTemp(current_.batteryTemp);
        previous_.batteryTemp = current_.batteryTemp;
    }

    if (all || current_.mcTemp != previous_.mcTemp)
    {
        Display::drawMCTemp(current_.mcTemp);
        previous_.mcTemp = current_.mcTemp;
    }

    if (all || current_.packVoltage != previous_.packVoltage)
    {
        Display::drawPackVoltage(current_.packVoltage);
        previous_.packVoltage = current_.packVoltage;
    }

    if (all || current_.packCurrent != previous_.packCurrent)
    {
        Display::drawPackCurrent(current_.packCurrent);
        previous_.packCurrent = current_.packCurrent;
    }

    if (all || current_.soc != previous_.soc)
    {
        Display::drawSOC(current_.soc);
        Display::drawBattery(current_.soc);
        previous_.soc = current_.soc;
    }

    if (all || current_.soh != previous_.soh)
    {
        Display::drawSOH(current_.soh);
        previous_.soh = current_.soh;
    }

    if (all || current_.canOnline != previous_.canOnline)
    {
        Display::drawCANStatus(current_.canOnline);
        previous_.canOnline = current_.canOnline;
    }

    if (all || current_.frameCounter != previous_.frameCounter)
    {
        Display::drawFrameCounter(current_.frameCounter);
        previous_.frameCounter = current_.frameCounter;
    }

    if (all || current_.lastCANID != previous_.lastCANID)
    {
        Display::drawLastCANID(current_.lastCANID);
        previous_.lastCANID = current_.lastCANID;
    }

    forceRedraw_ = false;
}

void Dashboard::forceFullRedraw()               { forceRedraw_ = true; }
DashboardPage Dashboard::getCurrentPage() const { return currentPage_; }
void Dashboard::setPage(DashboardPage page)
{ if (page != currentPage_) { currentPage_ = page; forceRedraw_ = true; } }
