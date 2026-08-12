#include "Display.h"
#include <TFT_eSPI.h>

static TFT_eSPI tft = TFT_eSPI();

static void drawTelemValue(int row, const char* text, uint16_t color)
{
    const int y = TELEM_ROW_Y(row) + TELEM_VALUE_Y_OS;
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(color, COLOR_BG);
    tft.setTextFont(4);
    tft.setTextPadding(tft.textWidth("-8888 XY", 4));
    tft.drawString(text, TELEM_VALUE_X, y);
    tft.setTextPadding(0);
}

//============================================================
void Display::init()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(COLOR_BG);
}

//============================================================
static int bootLineY = 160;

void Display::showBootScreen()
{
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.setTextFont(4);
    tft.drawString("4ZE RACING", SCREEN_W / 2, 30);
    tft.setTextColor(COLOR_TEXT_SECONDARY);
    tft.setTextFont(2);
    tft.drawString("Formula Student EV Dashboard", SCREEN_W / 2, 70);
    tft.setTextColor(COLOR_TEXT_DIM);
    tft.drawString("v6.0  |  Production Build (3.3V CAN)", SCREEN_W / 2, 95);
    tft.drawFastHLine(100, 130, SCREEN_W - 200, COLOR_DIVIDER);
    bootLineY = 160;
}

void Display::updateBootStatus(const char* component, bool success)
{
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT_SECONDARY);
    tft.drawString(component, 120, bootLineY);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(success ? COLOR_GREEN : COLOR_RED);
    tft.drawString(success ? "[  OK  ]" : "[ FAIL ]", 370, bootLineY);
    bootLineY += 26;
}

//============================================================
void Display::drawStaticUI()
{
    tft.fillScreen(COLOR_BG);

    tft.drawFastVLine(VDIV_X, 0, SCREEN_H, COLOR_DIVIDER);

    for (int i = 1; i <= 3; i++)
        tft.drawFastHLine(8, TELEM_ROW_Y(i), LEFT_W - 16, COLOR_DIVIDER);

    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("BATT TEMP", TELEM_LABEL_X, TELEM_ROW_Y(0) + TELEM_LABEL_Y_OS);
    tft.drawString("MC TEMP",   TELEM_LABEL_X, TELEM_ROW_Y(1) + TELEM_LABEL_Y_OS);
    tft.drawString("VOLTAGE",   TELEM_LABEL_X, TELEM_ROW_Y(2) + TELEM_LABEL_Y_OS);
    tft.drawString("CURRENT",   TELEM_LABEL_X, TELEM_ROW_Y(3) + TELEM_LABEL_Y_OS);

    tft.drawFastHLine(0, LEFT_HDIV_Y, LEFT_W, COLOR_ACCENT_DIM);

    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("SOC", BAR_LABEL_X, SOC_BAR_Y + BAR_H / 2);
    tft.drawString("SOH", BAR_LABEL_X, SOH_BAR_Y + BAR_H / 2);

    tft.drawRoundRect(BAR_X - 1, SOC_BAR_Y - 1, BAR_W + 2, BAR_H + 2, BAR_RADIUS, COLOR_BAR_BORDER);
    tft.drawRoundRect(BAR_X - 1, SOH_BAR_Y - 1, BAR_W + 2, BAR_H + 2, BAR_RADIUS, COLOR_BAR_BORDER);

    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("CAN:", CAN_STATUS_X, CAN_STATUS_Y);

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft.setTextFont(4);
    tft.drawString("km/h", SPEED_CX, SPEED_UNIT_Y);

    tft.drawFastHLine(RIGHT_X + 1, RIGHT_HDIV_Y, RIGHT_W - 1, COLOR_ACCENT_DIM);

    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("TORQUE", BOTTOM_LEFT_X, TORQUE_LBL_Y);
    tft.drawString("Frm:", BOTTOM_LEFT_X, BOTTOM_ROW2_Y);

    drawLogo();
}

//============================================================
void Display::drawSpeed(uint16_t speed)
{
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG);
    tft.setTextFont(8);
    tft.setTextPadding(tft.textWidth("888", 8));
    tft.drawNumber(speed, SPEED_CX, SPEED_NUM_Y, 8);
    tft.setTextPadding(0);
}

//============================================================
void Display::drawPrecharge(bool active)
{
    const uint16_t color = active ? COLOR_GREEN : COLOR_RED;
    tft.fillCircle(PRE_IND_CX - 50, PRE_IND_Y, PRE_DOT_R, color);
    tft.drawCircle(PRE_IND_CX - 50, PRE_IND_Y, PRE_DOT_R + 2, color);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(color, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextPadding(tft.textWidth("PRECHARGE", 2));
    tft.drawString("PRECHARGE", PRE_IND_CX - 38, PRE_IND_Y);
    tft.setTextPadding(0);
}

//============================================================
void Display::drawLogo()
{
    const int x = LOGO_CX - LOGO_W / 2;
    const int y = LOGO_Y;
    tft.fillRoundRect(x, y, LOGO_W, LOGO_H, 4, COLOR_ACCENT);
    tft.fillRoundRect(x + 2, y + 2, LOGO_W - 4, LOGO_H - 4, 3, COLOR_BG);
    tft.drawFastVLine(x + 6,  y + 5, LOGO_H - 10, COLOR_ACCENT);
    tft.drawFastVLine(x + 9,  y + 5, LOGO_H - 10, COLOR_ACCENT);
    tft.drawFastVLine(x + LOGO_W - 7,  y + 5, LOGO_H - 10, COLOR_ACCENT);
    tft.drawFastVLine(x + LOGO_W - 10, y + 5, LOGO_H - 10, COLOR_ACCENT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.setTextFont(4);
    tft.drawString("4ZE", LOGO_CX, y + LOGO_H / 2);
}

//============================================================
void Display::drawMotorTorque(int16_t torque)
{
    char buf[12]; snprintf(buf, sizeof(buf), "%d Nm", torque);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.setTextFont(4);
    tft.setTextPadding(tft.textWidth("-999 Nm", 4));
    tft.drawString(buf, BOTTOM_LEFT_X + 65, TORQUE_VAL_Y);
    tft.setTextPadding(0);
}

//============================================================
void Display::drawLastCANID(uint32_t id)
{
    char buf[10]; snprintf(buf, sizeof(buf), "0x%03lX", (unsigned long)id);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextPadding(tft.textWidth("0xFFF", 2));
    tft.drawString(buf, BOTTOM_RIGHT_X, BOTTOM_ROW2_Y);
    tft.setTextPadding(0);
}

//============================================================
void Display::drawFrameCounter(uint32_t count)
{
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextPadding(tft.textWidth("99999999", 2));
    tft.drawNumber(count, BOTTOM_LEFT_X + 38, BOTTOM_ROW2_Y);
    tft.setTextPadding(0);
}

//============================================================
void Display::drawBatteryTemp(int16_t tempC)
{
    char buf[10]; snprintf(buf, sizeof(buf), "%d \xB0""C", tempC);
    uint16_t c = COLOR_GREEN;
    if (tempC > 50) c = COLOR_RED; else if (tempC > 40) c = COLOR_YELLOW;
    drawTelemValue(0, buf, c);
}

void Display::drawMCTemp(int16_t tempC)
{
    char buf[10]; snprintf(buf, sizeof(buf), "%d \xB0""C", tempC);
    uint16_t c = COLOR_GREEN;
    if (tempC > 80) c = COLOR_RED; else if (tempC > 60) c = COLOR_YELLOW;
    drawTelemValue(1, buf, c);
}

void Display::drawPackVoltage(uint16_t volts)
{
    char buf[10]; snprintf(buf, sizeof(buf), "%u V", volts);
    drawTelemValue(2, buf, COLOR_TEXT_PRIMARY);
}

void Display::drawPackCurrent(int16_t amps)
{
    char buf[10]; snprintf(buf, sizeof(buf), "%d A", amps);
    drawTelemValue(3, buf, COLOR_TEXT_PRIMARY);
}

//============================================================
void Display::drawSOC(uint8_t soc)
{
    drawProgressBar(BAR_X, SOC_BAR_Y, BAR_W, BAR_H, soc, getPercentColor(soc));
    char buf[6]; snprintf(buf, sizeof(buf), "%3d%%", soc);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextPadding(tft.textWidth("100%", 2));
    tft.drawString(buf, BAR_PCT_X, SOC_BAR_Y + BAR_H / 2);
    tft.setTextPadding(0);
}

void Display::drawSOH(uint8_t soh)
{
    drawProgressBar(BAR_X, SOH_BAR_Y, BAR_W, BAR_H, soh, getPercentColor(soh));
    char buf[6]; snprintf(buf, sizeof(buf), "%3d%%", soh);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextPadding(tft.textWidth("100%", 2));
    tft.drawString(buf, BAR_PCT_X, SOH_BAR_Y + BAR_H / 2);
    tft.setTextPadding(0);
}

void Display::drawBattery(uint8_t soc)
{
    drawBatteryIcon(BATT_ICON_X, BATT_ICON_Y, BATT_ICON_W, BATT_ICON_H, soc);
}

//============================================================
void Display::drawCANStatus(bool online)
{
    const int dotX = CAN_STATUS_X + 34, textX = CAN_STATUS_X + 44;
    tft.fillCircle(dotX, CAN_STATUS_Y, 4, online ? COLOR_GREEN : COLOR_RED);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(online ? COLOR_GREEN : COLOR_RED, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextPadding(tft.textWidth("OFFLINE", 2));
    tft.drawString(online ? "ONLINE" : "OFFLINE", textX, CAN_STATUS_Y);
    tft.setTextPadding(0);
}

//============================================================
void Display::drawProgressBar(int x, int y, int w, int h, uint8_t percent, uint16_t barColor)
{
    if (percent > 100) percent = 100;
    const int fillW = (w * percent) / 100;
    if (fillW > 0) tft.fillRect(x, y, fillW, h, barColor);
    if (fillW < w) tft.fillRect(x + fillW, y, w - fillW, h, COLOR_BAR_BG);
}

void Display::drawBatteryIcon(int x, int y, int w, int h, uint8_t percent)
{
    if (percent > 100) percent = 100;
    const int bodyW = w - 8, termW = 5, termH = h / 3, pad = 3;
    tft.fillRect(x, y, w, h, COLOR_BG);
    tft.drawRoundRect(x, y, bodyW, h, 3, COLOR_TEXT_SECONDARY);
    tft.fillRoundRect(x + bodyW, y + (h - termH) / 2, termW + 2, termH, 1, COLOR_TEXT_SECONDARY);
    const int innerW = bodyW - pad * 2, innerH = h - pad * 2;
    const int fillW = (innerW * percent) / 100;
    if (fillW > 0)  tft.fillRect(x + pad, y + pad, fillW, innerH, getPercentColor(percent));
    if (fillW < innerW) tft.fillRect(x + pad + fillW, y + pad, innerW - fillW, innerH, COLOR_BAR_BG);
}

uint16_t Display::getPercentColor(uint8_t p)
{
    if (p > 60) return COLOR_GREEN;
    if (p > 30) return COLOR_YELLOW;
    return COLOR_RED;
}
