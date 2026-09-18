# ONT-S207CW-62TS-SE / Binardat 2G06-04210GSM

## Overview

ONT-S207CW-62TS-SE and Binardat 2G06-04210GSM are **managed switches with identical hardware**:
- **CPU:** RTL8372N (confirmed via serial: "Detecting CPU: RTL8372N")
- **Flash:** GD25Q128E (16MB)
- **Ports:** 4x 2.5G RJ45 + 2x 10G SFP+
- **Console:** 115200 baud (RTLPlayground firmware) / 9600 baud (stock firmware)

**Note:** These devices use **PCB-SWTG024AS-A-2.0.1** but with different port mapping and LED configuration than the stock PCB-SWTG024AS-A-2.0.1 firmware. Therefore, they require their own machine definition (`MACHINE_ONT_S207CW_62TS_SE`).

## Device Photos

### Binardat 2G06-04210GSM

**Front View:**
<img src="photos/ONT-S207CW-Binardat-2G06-04210GSM/binardat-2g06-04210gsm-front.jpg" width="400" />

**Back View:**
<img src="photos/ONT-S207CW-Binardat-2G06-04210GSM/binardat-2g06-04210gsm-back.jpg" width="400" />

**PCB View:**
<img src="photos/ONT-S207CW-Binardat-2G06-04210GSM/binardat-2g06-04210gsm-pcb.jpg" width="400" />

*Photos courtesy of [andreas5232](https://github.com/andreas5232) from [emesix/ONT-S207CW-62TS-SE#1](https://github.com/emesix/ONT-S207CW-62TS-SE/issues/1)*

## Hardware Specification

| Feature | Value |
|---|---|
| **CPU** | RTL8372N |
| **Flash** | GD25Q128E (16MB) |
| **RAM** | Integrated in RTL8372N |
| **RJ45 Ports** | 4x 2.5GBase-T (Ports 1-4) |
| **SFP+ Ports** | 2x 10G (Ports 5-6) |
| **Console** | UART (115200 baud for RTLPlayground, 9600 baud for stock) |
| **Stock Web UI IP** | 192.168.2.1 |
| **Loader Mode IP** | 192.168.10.247 |

## Port Layout

```
┌─────────────────────────────────────────────┐
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐   ┌─────┐ ┌─────┐  │
│  │RJ45 │ │RJ45 │ │RJ45 │ │RJ45 │   │SFP+ │ │SFP+ │  │
│  │  1  │ │  2  │ │  3  │ │  4  │   │  5  │ │  6  │  │
│  └─────┘ └─────┘ └─────┘ └─────┘   └─────┘ └─────┘  │
│                                                     │
│  [System LEDs]                                      │
└─────────────────────────────────────────────┘
```

## LED Behavior

### RJ45 Ports (Physical 1-4, Logical 0-3)

| LED Color | Speed | Physical LED Position |
|---|---|---|
| **Green** | 2.5G | Right LED |
| **Orange** | 1G / 100M / 10M | Left LED |
| **Off** | No link | - |

### SFP+ Ports (Physical 5-6, Logical 4-5)

| LED Color | Speed | Physical LED Position |
|---|---|---|
| **Green** | 10G | Right LED |
| **Orange** | 2.5G / 1G | Left LED |
| **Off** | No link / No SFP module | - |

## RTLPlayground Configuration

### machine.h

```c
// #define MACHINE_PCB_SWTG024AS_A_2_0_1  // Different port/LED mapping
#define MACHINE_ONT_S207CW_62TS_SE
// ONT-S207CW-62TS-SE and Binardat 2G06-04210GSM
// RTL8372N, 4x2.5G RJ45 + 2x10G SFP+, GD25Q128E (16MB)
// Physical ports: 1-4 = RJ45, 5-6 = SFP
// Logical ports: 0-3 = RJ45 (phys 1-4), 4-5 = SFP (phys 5-6)
// LED colors: RJ45 = Green@2.5G, Orange@1G/100M/10M; SFP = Green@10G, Orange@1G/2.5G
// LED mux: Original firmware values (0x08144040, 0x1037f309, ...)
```

### machine.c Configuration

```c
#elif defined MACHINE_ONT_S207CW_62TS_SE
__code const struct machine machine = {
    .machine_name = "ONT-S207CW-62TS-SE / Binardat 2G06-04210GSM",
    .isRTL8373 = 0,                    // RTL8372N
    .mac_flash_offset = 0x1FC000,      // MAC address storage offset
    .min_port = 0,                    // First logical port
    .max_port = 5,                    // Last logical port (0-5 = 6 ports)
    .n_sfp = 2,
    // Physical to logical port mapping:
    // Physical port 1 -> Logical port 0 (RJ45)
    // Physical port 2 -> Logical port 1 (RJ45)
    // Physical port 3 -> Logical port 2 (RJ45)
    // Physical port 4 -> Logical port 3 (RJ45)
    // Physical port 5 -> Logical port 4 (SFP on SDS0)
    // Physical port 6 -> Logical port 5 (SFP on SDS1)
    .log_to_phys_port = {1, 2, 3, 4, 5, 6, 0, 0, 0},
    .phys_to_log_port = {0, 1, 2, 3, 4, 5, 0, 0, 0},
    .is_sfp = {0, 0, 0, 0, 1, 1, 0, 0, 0},  // Logical ports 4,5 are SFP

    // SFP port on SDS0 / logical port 4 / physical port 5
    .sfp_port[0].pin_detect = GPIO37,
    .sfp_port[0].pin_los = GPIO_NA,
    .sfp_port[0].pin_tx_disable = GPIO_NA,
    .sfp_port[0].sds = 0,
    .sfp_port[0].i2c = { .sda = GPIO41_I2C_SDA3_MDIO1, .scl = GPIO40_I2C_SCL3_MDC1 },

    // SFP port on SDS1 / logical port 5 / physical port 6
    .sfp_port[1].pin_detect = GPIO38,
    .sfp_port[1].pin_los = GPIO_NA,
    .sfp_port[1].pin_tx_disable = GPIO_NA,
    .sfp_port[1].sds = 1,
    .sfp_port[1].i2c = { .sda = GPIO39_I2C_SDA4, .scl = GPIO40_I2C_SCL3_MDC1 },

    .reset_pin = GPIO_NA,
    .high_leds = { .mux = LED_28_SYS | LED_29, .enable = LED_27 | LED_28_SYS | LED_29 },
    .port_led_set = { 0, 0, 0, 0, 1, 1, 0, 0, 0 },  // RJ45=SET0, SFP=SET1

    /* LED Configuration from original firmware:
     * RJ45 ports (logical 0-3, physical 1-4): SET0
     *   LED0 (Green): 2.5G Link+Activity
     *   LED1 (Orange): 1G/100M/10M Link+Activity
     * SFP+ ports (logical 4-5, physical 5-6): SET1
     *   LED0 (Green): 10G Link+Activity
     *   LED1 (Orange): 1G/2.5G Link+Activity
     */
    .led_sets = {
        { // SET0: RJ45 Ports
          LEDS_2G5 | LEDS_LINK | LEDS_ACT,                 // Green: 2.5G
          LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT, // Orange: 1G/100M/10M
          0,
          0 },
        { // SET1: SFP+ Ports
          LEDS_10G | LEDS_LINK | LEDS_ACT,               // Green: 10G
          LEDS_2G5 | LEDS_1G | LEDS_LINK | LEDS_ACT,   // Orange: 1G/2.5G
          0,
          0 },
    },
    .led_mux_custom = 1,
    // LED mux configuration extracted from original firmware
    // Corresponds to register values:
    // RTL837X_REG_LED_GLB_MUX_1: 0x08144040
    // RTL837X_REG_LED_GLB_MUX_2: 0x1037f309
    // RTL837X_REG_LED_GLB_MUX_3: 0x12454391
    // RTL837X_REG_LED_GLB_MUX_4: 0x19616555
    // RTL837X_REG_LED_GLB_MUX_5: 0x1c79d65a
    // RTL837X_REG_LED_GLB_MUX_6: 0x0002181d
    .led_mux = { 0x08, 0x14, 0x40, 0x40, 0x10, 0x37, 0xf3, 0x09,
                0x12, 0x45, 0x43, 0x91, 0x19, 0x61, 0x65, 0x55,
                0x1c, 0x79, 0xd6, 0x5a, 0x00, 0x02, 0x18, 0x1d,
                0x00, 0x00, 0x00, 0x00 },
};
```

## Port to Logical Mapping

| Physical Port | Logical Port | Type | LED Set | Web UI Display |
|---|---|---|---|---|
| 1 | 0 | RJ45 | 0 | Port 1 |
| 2 | 1 | RJ45 | 0 | Port 2 |
| 3 | 2 | RJ45 | 0 | Port 3 |
| 4 | 3 | RJ45 | 0 | Port 4 |
| 5 | 4 | SFP+ (SDS0) | 1 | Port 5 |
| 6 | 5 | SFP+ (SDS1) | 1 | Port 6 |

**Important:** The web UI will show ports in the correct physical order (1-4 RJ45, 5-6 SFP) because the logical-to-physical mapping preserves the order.

## Firmware Files

| File | Size | Purpose |
|---|---|---|
| `rtlplayground-ONT_S207CW_62TS_SE-v11.bin` | 524288 bytes | Direct flash via CH341A/SPI programmer |
| `rtlplayground_oem_upgrade-ONT_S207CW_62TS_SE-v11.bin` | 540710 bytes | OEM upgrade via web UI (Loader Mode) |

### Firmware Size Check
- **Flash chip:** GD25Q128E = 16MB = 16777216 bytes
- **Firmware size:** ~512KB-528KB
- **Status:** ✅ **Plenty of space available** - No size issues with either old or new web UI

## Flashing Instructions

### Prerequisites
- **GD25Q128E** flash chip (16MB)
- **CH341A** SPI programmer (recommended)
- **flashrom** tool (Linux) or **Flashrom GUI** (Windows)

### Via CH341A (Direct Flash)

**Linux (flashrom):**
```bash
# Install flashrom first
sudo apt install flashrom

# Flash the firmware (may need sudo)
flashrom -p ch341a_spi -c "GD25Q128E/GD25B128E/GD25R128E/GD25Q127C" -w rtlplayground-ONT_S207CW_62TS_SE-v11.bin --noverify-all
```

**Alternative command (if above fails):**
```bash
flashrom -p ch341a_spi -c "GD25Q128E" -w rtlplayground-ONT_S207CW_62TS_SE-v11.bin
```

### Via Loader Mode (Web UI Upgrade)

The OEM upgrade file can be flashed via the device's loader mode:

1. **Enter Loader Mode:**
   - Power off the switch
   - Hold the reset button
   - Power on while holding reset
   - Release after ~5 seconds
   - Switch should be accessible at `192.168.10.247`

2. **Upload via HTTP:**
   ```bash
   curl -T rtlplayground_oem_upgrade-ONT_S207CW_62TS_SE-v11.bin http://192.168.10.247/firmware
   ```

3. **Or via Web Browser:**
   - Access `http://192.168.10.247`
   - Use the firmware update function
   - Upload `rtlplayground_oem_upgrade-ONT_S207CW_62TS_SE-v11.bin`
   - Wait for completion and reboot

### Via RTLPlayground Web UI (After First Flash)

Once RTLPlayground firmware is running:
1. Access web interface at `http://192.168.2.1`
2. Navigate to **System > Firmware Update**
3. Upload the `.bin` file
4. Wait for flash completion and reboot

## Recovery

If flashing fails and the switch does not boot:

### Method 1: Loader Mode Recovery
- The loader mode always remains accessible
- Switch boots to `192.168.10.247` when in loader mode
- Flash via: `curl -T firmware.bin http://192.168.10.247/firmware`

### Method 2: Serial Recovery (if loader mode fails)
- Connect UART: 115200 baud, 8N1
- Use CH341A to flash directly:
  ```bash
  flashrom -p ch341a_spi -c "GD25Q128E" -w firmware.bin
  ```

### Method 3: SOIC8 Clip
- Connect SOIC8 clip to CH341A
- Use flashrom as above
- Chip: GD25Q128E (16MB)

## Stock Firmware Information

| Property | Value |
|---|---|
| **Default IP** | 192.168.2.1 (ONT) / 192.168.10.247 (Loader Mode) |
| **Default Login** | admin/admin (may vary by OEM) |
| **Console Baud** | 9600 (stock) / 115200 (RTLPlayground) |
| **CPU String** | "RTL8372N" |
| **Loader Mode IP** | 192.168.10.247 |

## Known Issues & Fixes

### ✅ Issue: LEDs showing wrong colors (FIXED in v11)
**Symptom:** All RJ45 ports show green at all speeds, SFP LEDs incorrect
**Root Cause:** Wrong LED mux configuration and LED set assignments
**Fix:** Use v11 firmware with:
- Original firmware LED mux values (extracted from stock firmware)
- Correct LED set to physical port mapping
- Proper LED color logic (Green=2.5G/10G, Orange=1G/2.5G/100M/10M)

### ✅ Issue: Port order reversed in web UI (FIXED in v11)
**Symptom:** Web UI shows ports as 4321 instead of 1234
**Root Cause:** Incorrect log_to_phys_port and phys_to_log_port mapping
**Fix:** Corrected mapping to preserve physical port order

### ✅ Issue: No network connectivity after flash (FIXED)
**Symptom:** Switch boots but no ping response
**Root Cause:** Wrong CPU type configuration
**Fix:** Ensure `.isRTL8373 = 0` (this is RTL8372N, not RTL8373)

### ✅ Issue: SFP modules not detected (FIXED)
**Symptom:** SFP ports not working
**Root Cause:** Wrong SFP port SDS assignments
**Fix:** Correct SDS0/SDS1 assignments and GPIO pins

## Version History

| Version | Date | Changes | Status |
|---|---|---|---|
| v3 | 2026-09-14 | Initial PCB_SWTG024AS_A_2_0_1 config | ❌ Port order wrong, LEDs wrong |
| v10 | 2026-09-17 | LED colors corrected | ⚠️ LEDs better, port order still wrong |
| v11 | 2026-09-18 | **Separate machine def, correct port mapping, original LED mux** | ✅ All working |

## Notes

- This device shares PCB-SWTG024AS-A-2.0.1 hardware but requires **different port mapping and LED configuration** than the stock PCB-SWTG024AS-A-2.0.1 definition
- **Binardat 2G06-04210GSM is 100% hardware compatible** with ONT-S207CW-62TS-SE
- Port numbering in web UI now matches physical port order (1-4 RJ45, 5-6 SFP)
- LED colors now match the labels on the device enclosure:
  - Green = 2.5G (RJ45) / 10G (SFP)
  - Orange = 1G/100M/10M (RJ45) / 1G/2.5G (SFP)
- The **Loader Mode** always remains accessible at 192.168.10.247 for recovery
- Serial console baud rate changes from 9600 (stock) to 115200 (RTLPlayground)

## References

- [GitHub Issue: emesix/ONT-S207CW-62TS-SE#1](https://github.com/emesix/ONT-S207CW-62TS-SE/issues/1) - Original photos and discussion
- [PR: logicog/RTLPlayground#465](https://github.com/logicog/RTLPlayground/pull/465) - Feedback and improvements
