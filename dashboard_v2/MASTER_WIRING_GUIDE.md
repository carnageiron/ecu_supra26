# 4ZE Racing Formula Student EV — Hardware Wiring & System Architecture (v6.0)

This master guide covers the complete hardware wiring for all three nodes on your CAN bus system:
1. **Node 1**: Arduino Nano CAN Simulator (Emulates APPS, Orion BMS, and Bamocar)
2. **Node 2**: ESP32 Data Logger (SPI SD Card + MCP2515 CAN)
3. **Node 3**: ESP32 Dashboard (ILI9488 TFT Display + MCP2515 CAN)

---

## 1. Node 1: Arduino Nano CAN Simulator Wiring

| MCP2515 Pin | Arduino Nano Pin | Note |
|---|---|---|
| **VCC** | **5V / 3.3V** | Power |
| **GND** | **GND** | Connect to **Shared Common GND Bus** |
| **CS** | **Pin D10** | Hardware SPI CS |
| **MOSI (SI)** | **Pin D11** | Hardware SPI MOSI |
| **MISO (SO)** | **Pin D12** | Hardware SPI MISO |
| **SCK** | **Pin D13** | Hardware SPI SCK |
| **CAN_H** | **CAN_H Bus Line** | Parallel CAN High |
| **CAN_L** | **CAN_L Bus Line** | Parallel CAN Low |
| **J1 Jumper** | **ENABLED** | 120 Ω Termination Resistor |

---

## 2. Node 2: ESP32 Data Logger Wiring

| Component | Component Pin | ESP32 DevKit V1 Pin | Bus |
|---|---|---|---|
| **MCP2515** | **VCC** | **3.3V / 5V** | Power |
| **MCP2515** | **GND** | **GND** | **Shared Common GND Bus** |
| **MCP2515** | **CS** | **GPIO 5** | VSPI |
| **MCP2515** | **SCK** | **GPIO 18** | VSPI |
| **MCP2515** | **MISO (SO)** | **GPIO 19** | VSPI |
| **MCP2515** | **MOSI (SI)** | **GPIO 23** | VSPI |
| **MCP2515** | **CAN_H** | **CAN_H Bus Line** | Parallel CAN High |
| **MCP2515** | **CAN_L** | **CAN_L Bus Line** | Parallel CAN Low |
| **SD Card** | **VCC** | **5V (VIN)** | Power |
| **SD Card** | **GND** | **GND** | **Shared Common GND Bus** |
| **SD Card** | **CS** | **GPIO 15** | HSPI |
| **SD Card** | **SCK** | **GPIO 14** | HSPI |
| **SD Card** | **MISO** | **GPIO 2** | HSPI |
| **SD Card** | **MOSI** | **GPIO 13** | HSPI |

---

## 3. Node 3: ESP32 Dashboard Wiring

| Component | Component Pin | ESP32 DevKit V1 Pin | Bus |
|---|---|---|---|
| **MCP2515** | **VCC** | **3.3V / 5V** | Power |
| **MCP2515** | **GND** | **GND** | **Shared Common GND Bus** |
| **MCP2515** | **CS** | **GPIO 5** | VSPI |
| **MCP2515** | **SCK** | **GPIO 18** | VSPI |
| **MCP2515** | **MISO (SO)** | **GPIO 19** | VSPI |
| **MCP2515** | **MOSI (SI)** | **GPIO 23** | VSPI |
| **MCP2515** | **CAN_H** | **CAN_H Bus Line** | Parallel CAN High |
| **MCP2515** | **CAN_L** | **CAN_L Bus Line** | Parallel CAN Low |
| **TFT ILI9488**| **VCC** | **5V / 3.3V** | Power |
| **TFT ILI9488**| **GND** | **GND** | **Shared Common GND Bus** |
| **TFT ILI9488**| **CS** | **GPIO 15** | HSPI |
| **TFT ILI9488**| **DC / RS** | **GPIO 27** | Control |
| **TFT ILI9488**| **RST** | **GPIO 33** | Reset |
| **TFT ILI9488**| **SCK / CLK** | **GPIO 14** | HSPI |
| **TFT ILI9488**| **MOSI** | **GPIO 13** | HSPI |

---

## 4. Physical Parallel CAN Bus Schematic

```
  CAN_H  ======================================================= Wire 1
  CAN_L  ======================================================= Wire 2
  GND    ======================================================= Wire 3 (SHARED GND)
               | | |                  | | |                  | | |
               | | |                  | | |                  | | |
       +-------+-+-+-------+  +-------+-+-+-------+  +-------+-+-+-------+
       | Arduino Nano      |  | ESP32 Data Logger |  | ESP32 Dashboard   |
       | CAN Simulator     |  | Node (SD Card)    |  | Node (TFT Screen) |
       +-------------------+  +-------------------+  +-------------------+
```
