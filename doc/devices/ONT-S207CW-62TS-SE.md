# ONT-S207CW-62TS-SE / Binardat 2G06-04210GSM / Binardat 2G06-04210GS

## Overview

ONT-S207CW-62TS-SE, Binardat 2G06-04210GSM (managed), and Binardat 2G06-04210GS (unmanaged) are **switches with identical hardware**:
- **CPU:** RTL8372N (confirmed via serial: "Detecting CPU: RTL8372N")
- **Flash:** GD25Q128E (16MB)
- **Ports:** 4x 2.5G RJ45 + 2x 10G SFP+
- **Console:** 115200 baud (RTLPlayground firmware) / 9600 baud (stock firmware)

**Note:** These devices use **PCB-SWTG024AS-A-2.0.1** hardware. The `MACHINE_ONT_S207CW_62TS_SE` definition uses a **custom configuration** with:
- Corrected port mapping (physical ports 1-4 = RJ45, 5-6 = SFP)
- Optimized LED sets for different port wiring (RJ45 Ports 1-2 have reversed LED wiring)
- Custom LED mux configuration from PCB-SWTG024AS-A-2.0.1

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

### RJ45 Ports (Physical 1-4)

| LED Color | Speed | Physical LED Position |
|---|---|---|
| **Green** | 2.5G | Right LED |
| **Orange** | 1G / 100M / 10M | Left LED |
| **Off** | No link | - |

### SFP+ Ports (Physical 5-6)

| LED Color | Speed | Physical LED Position |
|---|---|---|
| **Green** | 10G | Right LED |
| **Orange** | 2.5G / 1G | Left LED |
| **Off** | No link / No SFP module | - |

## RTLPlayground Configuration

### machine.h

```c
#define MACHINE_ONT_S207CW_62TS_SE
// ONT-S207CW-62TS-SE and Binardat 2G06-04210GSM
// RTL8372N, 4x2.5G RJ45 + 2x10G SFP+, GD25Q128E (16MB)
// Custom configuration with corrected port mapping and LED behavior
// Physical ports: 1-4 = RJ45, 5 = SFP (SDS0), 6 = SFP (SDS1)
// Logical ports: 3 = SFP (SDS0), 4-7 = RJ45, 8 = SFP (SDS1)
```

### machine.c Configuration

Custom configuration for ONT-S207CW-62TS-SE:

```c
#elif defined MACHINE_ONT_S207CW_62TS_SE
__code const struct machine machine = {
    .machine_name = "ONT-S207CW-62TS-SE / Binardat 2G06-04210GSM",
    .isRTL8373 = 0,
    .mac_flash_offset = 0x1FC000,
    .min_port = 3,
    .max_port = 8,
    .n_sfp = 2,
    .log_to_phys_port = {0, 0, 0, 5, 4, 3, 2, 1, 6},
    .phys_to_log_port = {7, 6, 5, 4, 3, 8, 0, 0, 0},
    .is_sfp = {0, 0, 0, 1, 0, 0, 0, 0, 2},
    
    // SFP port on SDS0 / logical port 3
    .sfp_port[0].pin_detect = GPIO37,
    .sfp_port[0].pin_los = GPIO_NA,
    .sfp_port[0].pin_tx_disable = GPIO_NA,
    .sfp_port[0].sds = 0,
    .sfp_port[0].i2c = I2CBUS( GPIO41_I2C_SDA3_MDIO1, GPIO40_I2C_SCL3_MDC1 ),
    
    // SFP port on SDS1 / logical port 8
    .sfp_port[1].pin_detect = GPIO38,
    .sfp_port[1].pin_los = GPIO_NA,
    .sfp_port[1].pin_tx_disable = GPIO_NA,
    .sfp_port[1].sds = 1,
    .sfp_port[1].i2c = I2CBUS( GPIO39_I2C_SDA4, GPIO40_I2C_SCL3_MDC1 ),
    
    .reset_pin = GPIO_NA,
    .high_leds = { .mux = LED_28_SYS | LED_29, .enable = LED_27 | LED_28_SYS | LED_29 },
    .port_led_set = { 0, 0, 0, 3, 0, 0, 2, 2, 3},
    .led_sets = {
        {
            // Set 0: RJ45 Ports 3-4 (normal wiring)
            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
            LEDS_2G5 | LEDS_LINK | LEDS_ACT,
            LEDS_DUPLEX,
            0
        },
        {
            // Set 1: Unused
            LEDS_2G5 | LEDS_1G | LEDS_100M | LEDS_LINK | LEDS_ACT,
            LEDS_10G | LEDS_LINK | LEDS_ACT,
            LEDS_2G5 | LEDS_LINK,
            LEDS_COL | LEDS_DUPLEX
        },
        {
            // Set 2: RJ45 Ports 1-2 (reversed wiring)
            LEDS_2G5 | LEDS_LINK | LEDS_ACT,
            LEDS_1G | LEDS_100M | LEDS_10M | LEDS_LINK | LEDS_ACT,
            LEDS_DUPLEX,
            0
        },
        {
            // Set 3: SFP Ports 5-6 (reversed wiring for Port 6)
            LEDS_10G | LEDS_LINK | LEDS_ACT,
            LEDS_2G5 | LEDS_1G | LEDS_LINK | LEDS_ACT,
            LEDS_2G5 | LEDS_LINK,
            LEDS_COL | LEDS_DUPLEX
        },
    },
    .led_mux_custom = 1,
    .led_mux = {
        0x00,0x01,0x04,0x05,0x08,0x09,0x0c,0x3f,0x0d,0x10,0x11,0x0e,0x14,0x11,0x12,0x15,
        0x15,0x16,0x18,0x19,0x1a,0x19,0x1d,0x1e,0x1c,0x1d,0x20,0x21
    },
};
```

## Port to Logical Mapping

Custom mapping for ONT-S207CW-62TS-SE:

| Physical Port | Logical Port | Type | LED Set |
|---|---|---|---|
| 1 | 7 | RJ45 | 2 |
| 2 | 6 | RJ45 | 2 |
| 3 | 5 | RJ45 | 0 |
| 4 | 4 | RJ45 | 0 |
| 5 | 3 | SFP+ (SDS0) | 3 |
| 6 | 8 | SFP+ (SDS1) | 3 |

**Note:** Ports 1-2 (RJ45) have reversed LED wiring and use LED-Set 2. SFP Ports 5-6 use LED-Set 3 to account for wiring differences.

## Firmware Files

| File | Size | Purpose |
|---|---|---|
| `ONT_S207CW_62TS_SE-v22.bin` | 524288 bytes | Direct flash via CH341A/SPI programmer |
| `rtlplayground_oem_upgrade-ONT_S207CW_62TS_SE-v22.bin` | 540710 bytes | OEM upgrade via web UI (Loader Mode) |

### Firmware Size Check
- **Flash chip:** GD25Q128E = 16MB = 16777216 bytes
- **Firmware size:** 524288 bytes (512KB)
- **Status:** ✅ **Plenty of space available** - No size issues with old or new web UI

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
flashrom -p ch341a_spi -c "GD25Q128E/GD25B128E/GD25R128E/GD25Q127C" -w ONT_S207CW_62TS_SE-v22.bin --noverify-all
```

**Alternative command (if above fails):**
```bash
flashrom -p ch341a_spi -c "GD25Q128E" -w ONT_S207CW_62TS_SE-v22.bin
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
   curl -T rtlplayground_oem_upgrade-ONT_S207CW_62TS_SE-v22.bin http://192.168.10.247/firmware
   ```

3. **Or via Web Browser:**
   - Access `http://192.168.10.247`
   - Use the firmware update function
   - Upload `rtlplayground_oem_upgrade-ONT_S207CW_62TS_SE-v22.bin`
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

## Configuration Details

- **CPU:** RTL8372N (`.isRTL8373 = 0`)
- **SFP:** SDS0 on logical port 3 (GPIO37), SDS1 on logical port 8 (GPIO38)
- **Port Mapping:** Physical 1-4 → Logical 7-4, Physical 5-6 → Logical 3,8
- **LED Wiring:** RJ45 Ports 1-2 and SFP Port 6 have reversed LED pins (orange/green swapped)
- **LED Sets:** 4 sets total - Set 0/1 unused, Set 2 for RJ45 1-2, Set 3 for SFP 5-6

## Notes

- ONT-S207CW-62TS-SE, Binardat 2G06-04210GSM (managed), and Binardat 2G06-04210GS (unmanaged) use **identical PCB-SWTG024AS-A-2.0.1 hardware**
- Web UI shows correct physical port order (1-2-3-4 for RJ45, 5-6 for SFP)
- Loader Mode always remains accessible at `192.168.10.247` for recovery
- Serial console: 9600 baud (stock) → 115200 baud (RTLPlayground)

## References

- [GitHub Issue: emesix/ONT-S207CW-62TS-SE#1](https://github.com/emesix/ONT-S207CW-62TS-SE/issues/1) - Original photos and discussion
- [PR: logicog/RTLPlayground#465](https://github.com/logicog/RTLPlayground/pull/465) - Feedback and improvements
