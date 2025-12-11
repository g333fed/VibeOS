# Raspberry Pi Zero 2W USB Driver

This document describes the USB host driver implementation for VibeOS on the Raspberry Pi Zero 2W.

## Hardware Overview

The Pi Zero 2W (BCM2710) uses the **Synopsys DesignWare USB 2.0 OTG Controller (DWC2)**. This is the same IP block used across all Raspberry Pi models.

### Key Hardware Details

| Property | Value |
|----------|-------|
| Controller | DWC2 (DesignWare Core USB 2.0) |
| Base Address | `0x3F980000` (BCM2710 peripheral space) |
| PHY Type | Internal UTMI+ (High-Speed capable) |
| PHY Clock | 60 MHz |
| Channels | 8 host channels |
| FIFO Depth | 4080 words (16KB) |
| Mode | Host mode only (OTG not used) |

### USB Port

The Pi Zero 2W has a single micro-USB OTG port. For host mode operation, you need a micro-USB OTG adapter that grounds the ID pin to force host mode.

## Register Map

The DWC2 register space is divided into logical blocks:

| Offset Range | Block | Description |
|--------------|-------|-------------|
| `0x000-0x0FF` | Global | Core config, interrupts, FIFO sizing |
| `0x400-0x4FF` | Host | Host config, port control |
| `0x500-0x6FF` | Channels | 8 host channels (0x20 bytes each) |
| `0xE00-0xEFF` | Power | Power and clock gating |
| `0x1000+` | FIFOs | Data FIFO access windows |

### Critical Global Registers

```c
GOTGCTL   (0x000) - OTG Control
GAHBCFG   (0x008) - AHB Configuration
GUSBCFG   (0x00C) - USB Configuration
GRSTCTL   (0x010) - Reset Control
GINTSTS   (0x014) - Interrupt Status
GINTMSK   (0x018) - Interrupt Mask
GRXSTSP   (0x020) - Receive Status Pop
GRXFSIZ   (0x024) - Receive FIFO Size
GNPTXFSIZ (0x028) - Non-periodic TX FIFO Size
GHWCFG1-4 (0x044-0x050) - Hardware Configuration (RO)
HPTXFSIZ  (0x100) - Host Periodic TX FIFO Size
```

### Host Registers

```c
HCFG      (0x400) - Host Configuration
HFIR      (0x404) - Host Frame Interval
HFNUM     (0x408) - Host Frame Number
HAINT     (0x414) - Host All Channels Interrupt
HAINTMSK  (0x418) - Host All Channels Interrupt Mask
HPRT0     (0x440) - Host Port Control and Status
```

### Channel Registers (per channel n)

```c
HCCHAR(n)   (0x500 + n*0x20) - Channel Characteristics
HCSPLT(n)   (0x504 + n*0x20) - Channel Split Control
HCINT(n)    (0x508 + n*0x20) - Channel Interrupt
HCINTMSK(n) (0x50C + n*0x20) - Channel Interrupt Mask
HCTSIZ(n)   (0x510 + n*0x20) - Channel Transfer Size
HCDMA(n)    (0x514 + n*0x20) - Channel DMA Address
```

## Initialization Sequence

### 1. Power On (Mailbox)

The USB controller must be powered on via the VideoCore mailbox before use:

```c
// Mailbox property tag 0x00028001 (Set Power State)
// Device ID 3 = USB HCD
// State = 3 (on + wait)
```

### 2. Core Soft Reset

```c
// Wait for AHB idle
while (!(GRSTCTL & GRSTCTL_AHBIDLE));

// Trigger soft reset
GRSTCTL = GRSTCTL_CSFTRST;

// Wait for reset complete (hardware clears bit)
while (GRSTCTL & GRSTCTL_CSFTRST);

// Wait for AHB idle again
while (!(GRSTCTL & GRSTCTL_AHBIDLE));
```

### 3. PHY Configuration

**CRITICAL**: The Pi uses an internal UTMI+ PHY running at 60MHz:

```c
uint32_t usbcfg = GUSBCFG;

// DO NOT set PHYSEL - Pi uses HS-capable PHY
usbcfg &= ~GUSBCFG_PHYSEL;

// Use UTMI+ interface (not ULPI)
usbcfg &= ~GUSBCFG_ULPI_UTMI_SEL;

// 8-bit UTMI+ interface
usbcfg &= ~GUSBCFG_PHYIF;

// Force host mode
usbcfg |= GUSBCFG_FORCEHOSTMODE;

GUSBCFG = usbcfg;
```

### 4. Host Configuration

**CRITICAL**: Use 30/60MHz clock setting, NOT 48MHz:

```c
// FSLSPCLKSEL = 0 for UTMI+ PHY (30/60 MHz)
// FSLSPCLKSEL = 1 is for dedicated FS PHY (48 MHz) - WRONG for Pi!
HCFG = HCFG_FSLSPCLKSEL_30_60;

// Frame interval for 60MHz clock
HFIR = 60000;  // 60MHz * 1ms = 60000 clocks per frame
```

### 5. FIFO Sizing

```c
GRXFSIZ = 256;                    // RX FIFO: 256 words (1024 bytes)
GNPTXFSIZ = (256 << 16) | 256;    // Non-periodic TX: size | start
HPTXFSIZ = (256 << 16) | 512;     // Periodic TX: size | start
```

### 6. Port Power On

```c
uint32_t hprt = HPRT0;
// Clear W1C bits to avoid accidents
hprt &= ~(HPRT0_PRTENA | HPRT0_PRTCONNDET | HPRT0_PRTENCHNG | HPRT0_PRTOVRCURRCHNG);
// Set power
hprt |= HPRT0_PRTPWR;
HPRT0 = hprt;
```

### 7. Device Detection and Reset

```c
// Wait for connection
while (!(HPRT0 & HPRT0_PRTCONNSTS));

// Assert reset for 50ms
hprt = HPRT0;
hprt &= ~(W1C bits);
hprt |= HPRT0_PRTRST;
HPRT0 = hprt;
msleep(50);

// Deassert reset
hprt &= ~HPRT0_PRTRST;
HPRT0 = hprt;

// Read speed from HPRT0.PRTSPD
// 0 = High Speed, 1 = Full Speed, 2 = Low Speed
```

## Control Transfers

Control transfers have three stages: SETUP, DATA (optional), STATUS.

### SETUP Stage

```c
// Configure channel
HCCHAR(ch) = (mps) |                           // Max packet size
             (0 << HCCHAR_EPNUM_SHIFT) |       // EP0
             (HCCHAR_EPTYPE_CTRL << 18) |      // Control
             (device_addr << 22);              // Device address

// Transfer size: 8 bytes, 1 packet, SETUP PID
HCTSIZ(ch) = 8 | (1 << 19) | (HCTSIZ_PID_SETUP << 29);

// Write SETUP packet to FIFO (2 words)
FIFO(ch) = setup_packet[0];
FIFO(ch) = setup_packet[1];

// Enable channel
HCCHAR(ch) |= HCCHAR_CHENA;

// Wait for completion
while (!(HCINT(ch) & HCINT_XFERCOMPL));
```

### DATA Stage (IN)

**CRITICAL**: For Full Speed devices, use MPS=64, not 8!

```c
// MPS = 64 for FS, 8 for LS
uint32_t mps = (device_speed == LOW_SPEED) ? 8 : 64;

HCCHAR(ch) = mps | HCCHAR_EPDIR | ...;  // IN direction
HCTSIZ(ch) = data_len | (pkt_count << 19) | (HCTSIZ_PID_DATA1 << 29);
HCCHAR(ch) |= HCCHAR_CHENA;

// Poll for data
while (!done) {
    if (GINTSTS & GINTSTS_RXFLVL) {
        uint32_t grxsts = GRXSTSP;  // Pop status
        int pktsts = (grxsts >> 17) & 0xF;
        int bcnt = (grxsts >> 4) & 0x7FF;

        if (pktsts == 2) {  // IN data packet
            // Read from FIFO
            for (int i = 0; i < (bcnt + 3) / 4; i++) {
                data[i] = FIFO(ch);
            }
        }
    }

    // CRITICAL: Re-enable channel after each packet!
    if (HCINT(ch) & HCINT_ACK) {
        HCINT(ch) = HCINT_ACK;
        if (bytes_received < total_len) {
            HCCHAR(ch) |= HCCHAR_CHENA;  // Get next packet
        }
    }
}
```

### STATUS Stage

```c
// Status direction is opposite of data (or IN if no data)
HCCHAR(ch) = mps | (status_in ? HCCHAR_EPDIR : 0) | ...;
HCTSIZ(ch) = 0 | (1 << 19) | (HCTSIZ_PID_DATA1 << 29);  // Zero-length
HCCHAR(ch) |= HCCHAR_CHENA;
```

## Common Pitfalls

### 1. Babble Error on IN Transfer

**Symptom**: HCINT shows bit 8 (BBLERR) set immediately after SETUP succeeds.

**Causes**:
1. **Wrong MPS**: Using MPS=8 for Full Speed devices. FS devices can send 64 bytes per packet. When byte 9 arrives, the controller flags babble.
   - **Fix**: Use MPS=64 for FS/HS devices, MPS=8 only for LS.

2. **Wrong PHY clock**: Using FSLSPCLKSEL=1 (48MHz) instead of 0 (60MHz). The controller's frame timer runs too fast, cutting off packets early.
   - **Fix**: Set FSLSPCLKSEL=0 for Pi's UTMI+ PHY.

### 2. Multi-Packet Transfer Timeout

**Symptom**: First packet received, then timeout waiting for more data. HCINT shows 0x00000000.

**Cause**: In slave mode, the controller doesn't automatically continue to the next packet. You must re-enable the channel.

**Fix**: After receiving ACK or IN_COMPLETE, check if more data is needed and re-enable the channel:

```c
if (hcint & HCINT_ACK) {
    HCINT(ch) = HCINT_ACK;
    if (bytes_transferred < data_len) {
        HCCHAR(ch) |= HCCHAR_CHENA;  // Continue transfer
    }
}
```

### 3. NAK Handling

**Symptom**: Transfer fails with NAK.

**Cause**: Device not ready. Common during enumeration.

**Fix**: Re-enable channel and retry (with limit):

```c
if (hcint & HCINT_NAK) {
    HCINT(ch) = HCINT_NAK;
    if (nak_count++ < 1000) {
        HCCHAR(ch) |= HCCHAR_CHENA;
    }
}
```

### 4. GRXSTSP Packet Status

When reading GRXSTSP, the packet status field indicates what's in the FIFO:

| pktsts | Meaning |
|--------|---------|
| 2 | IN data packet received |
| 3 | IN transfer complete |
| 5 | Data toggle error |
| 7 | Channel halted |

Only read FIFO data when pktsts=2 and bcnt>0.

## USB Enumeration

Standard enumeration sequence:

1. **Reset port** - 50ms reset pulse
2. **Get Device Descriptor (8 bytes)** - Learn max packet size
3. **Set Address** - Assign address 1
4. **Get Device Descriptor (18 bytes)** - Full descriptor at new address
5. **Get Configuration Descriptor** - Learn interfaces and endpoints
6. **Set Configuration** - Activate the device

For HID devices (keyboards/mice):
- bInterfaceClass = 3 (HID)
- bInterfaceProtocol = 1 (keyboard) or 2 (mouse)
- bInterfaceSubClass = 1 (boot protocol) for simple operation

## Future Work

### Interrupt Transfers (for HID)

HID devices report input via interrupt IN transfers. Implementation needed:

```c
// Configure channel for interrupt endpoint
HCCHAR(ch) = mps |
             (ep_num << 11) |
             HCCHAR_EPDIR |
             (HCCHAR_EPTYPE_INTR << 18) |
             (device_addr << 22);

// Poll periodically (based on endpoint bInterval)
HCTSIZ(ch) = 8 | (1 << 19) | (data_toggle << 29);
HCCHAR(ch) |= HCCHAR_CHENA;

// Read 8-byte HID report from FIFO
// Parse modifier keys and keycodes
```

### HID Report Parsing

Boot protocol keyboard reports are 8 bytes:
- Byte 0: Modifier keys (Ctrl, Shift, Alt, GUI)
- Byte 1: Reserved
- Bytes 2-7: Up to 6 simultaneous key codes

### Hub Support

For USB hubs, need to implement:
- Hub class requests (GET_PORT_STATUS, SET_PORT_FEATURE)
- Split transactions for FS/LS devices behind hub

## References

- Linux dwc2 driver: `drivers/usb/dwc2/`
- Synopsys DWC2 databook
- USB 2.0 specification
- BCM2835 ARM Peripherals guide (similar to BCM2710)
