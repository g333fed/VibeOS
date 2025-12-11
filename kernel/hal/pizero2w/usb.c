/*
 * Raspberry Pi Zero 2W USB (DWC2) Driver
 *
 * The Pi Zero 2W uses the Synopsys DesignWare USB 2.0 OTG controller (DWC2).
 * This driver implements Host mode for USB keyboards/mice.
 *
 * DWC2 base: 0x3F980000 (BCM2710 peripheral space)
 *
 * Reference: Linux dwc2 driver (drivers/usb/dwc2/)
 */

#include "../hal.h"
#include "../../printf.h"
#include "../../string.h"
#include "../../memory.h"

// Debug output
#define USB_DEBUG
#ifdef USB_DEBUG
#define usb_debug(...) printf(__VA_ARGS__)
#else
#define usb_debug(...) ((void)0)
#endif

// Peripheral base for Pi Zero 2W (BCM2710)
#define PERI_BASE       0x3F000000

// DWC2 USB Controller base
#define USB_BASE        (PERI_BASE + 0x980000)

// ============================================================================
// Global Registers (0x000 - 0x3FF)
// ============================================================================

// OTG Control and Status
#define GOTGCTL         (*(volatile uint32_t *)(USB_BASE + 0x000))
#define GOTGINT         (*(volatile uint32_t *)(USB_BASE + 0x004))

// AHB Configuration
#define GAHBCFG         (*(volatile uint32_t *)(USB_BASE + 0x008))
#define GAHBCFG_GLBL_INTR_EN    (1 << 0)    // Global interrupt enable
#define GAHBCFG_DMA_EN          (1 << 5)    // DMA enable
#define GAHBCFG_AHB_SINGLE      (1 << 23)   // AHB single transfer

// USB Configuration
#define GUSBCFG         (*(volatile uint32_t *)(USB_BASE + 0x00C))
#define GUSBCFG_PHYIF           (1 << 3)    // PHY Interface (0=8bit, 1=16bit)
#define GUSBCFG_ULPI_UTMI_SEL   (1 << 4)    // 0=UTMI+, 1=ULPI
#define GUSBCFG_PHYSEL          (1 << 6)    // 0=HS, 1=FS
#define GUSBCFG_FORCEHOSTMODE   (1 << 29)   // Force host mode
#define GUSBCFG_FORCEDEVMODE    (1 << 30)   // Force device mode

// Reset Control
#define GRSTCTL         (*(volatile uint32_t *)(USB_BASE + 0x010))
#define GRSTCTL_CSFTRST         (1 << 0)    // Core soft reset
#define GRSTCTL_RXFFLSH         (1 << 4)    // RxFIFO flush
#define GRSTCTL_TXFFLSH         (1 << 5)    // TxFIFO flush
#define GRSTCTL_TXFNUM_SHIFT    6           // TxFIFO number for flush
#define GRSTCTL_TXFNUM_ALL      (0x10 << 6) // Flush all TxFIFOs
#define GRSTCTL_AHBIDLE         (1 << 31)   // AHB master idle

// Interrupt Status and Mask
#define GINTSTS         (*(volatile uint32_t *)(USB_BASE + 0x014))
#define GINTMSK         (*(volatile uint32_t *)(USB_BASE + 0x018))
#define GINTSTS_CURMODE         (1 << 0)    // Current mode (0=device, 1=host)
#define GINTSTS_MODEMIS         (1 << 1)    // Mode mismatch
#define GINTSTS_SOF             (1 << 3)    // Start of frame
#define GINTSTS_RXFLVL          (1 << 4)    // RxFIFO non-empty
#define GINTSTS_NPTXFE          (1 << 5)    // Non-periodic TxFIFO empty
#define GINTSTS_USBSUSP         (1 << 11)   // USB suspend
#define GINTSTS_PRTINT          (1 << 24)   // Port interrupt
#define GINTSTS_HCHINT          (1 << 25)   // Host channel interrupt
#define GINTSTS_CONIDSTSCHNG    (1 << 28)   // Connector ID status change
#define GINTSTS_DISCONNINT      (1 << 29)   // Disconnect detected

// Receive Status (Read/Pop)
#define GRXSTSR         (*(volatile uint32_t *)(USB_BASE + 0x01C))  // Read
#define GRXSTSP         (*(volatile uint32_t *)(USB_BASE + 0x020))  // Pop

// FIFO Sizes
#define GRXFSIZ         (*(volatile uint32_t *)(USB_BASE + 0x024))  // Receive FIFO size
#define GNPTXFSIZ       (*(volatile uint32_t *)(USB_BASE + 0x028))  // Non-periodic Tx FIFO size
#define GNPTXSTS        (*(volatile uint32_t *)(USB_BASE + 0x02C))  // Non-periodic Tx FIFO status

// Hardware Configuration (Read-Only)
#define GHWCFG1         (*(volatile uint32_t *)(USB_BASE + 0x044))
#define GHWCFG2         (*(volatile uint32_t *)(USB_BASE + 0x048))
#define GHWCFG3         (*(volatile uint32_t *)(USB_BASE + 0x04C))
#define GHWCFG4         (*(volatile uint32_t *)(USB_BASE + 0x050))

// Host Periodic Tx FIFO Size
#define HPTXFSIZ        (*(volatile uint32_t *)(USB_BASE + 0x100))

// ============================================================================
// Host Mode Registers (0x400 - 0x7FF)
// ============================================================================

// Host Configuration
#define HCFG            (*(volatile uint32_t *)(USB_BASE + 0x400))
#define HCFG_FSLSPCLKSEL_30_60  0           // 30/60 MHz PHY clock
#define HCFG_FSLSPCLKSEL_48     1           // 48 MHz PHY clock
#define HCFG_FSLSUPP            (1 << 2)    // FS/LS only support

// Host Frame Interval/Number
#define HFIR            (*(volatile uint32_t *)(USB_BASE + 0x404))
#define HFNUM           (*(volatile uint32_t *)(USB_BASE + 0x408))

// Host All Channels Interrupt
#define HAINT           (*(volatile uint32_t *)(USB_BASE + 0x414))
#define HAINTMSK        (*(volatile uint32_t *)(USB_BASE + 0x418))

// Host Port Control and Status (Root Hub Port)
#define HPRT0           (*(volatile uint32_t *)(USB_BASE + 0x440))
#define HPRT0_PRTCONNSTS        (1 << 0)    // Port connect status
#define HPRT0_PRTCONNDET        (1 << 1)    // Port connect detected (W1C)
#define HPRT0_PRTENA            (1 << 2)    // Port enable
#define HPRT0_PRTENCHNG         (1 << 3)    // Port enable changed (W1C)
#define HPRT0_PRTOVRCURRACT     (1 << 4)    // Port overcurrent active
#define HPRT0_PRTOVRCURRCHNG    (1 << 5)    // Port overcurrent changed (W1C)
#define HPRT0_PRTRES            (1 << 6)    // Port resume
#define HPRT0_PRTSUSP           (1 << 7)    // Port suspend
#define HPRT0_PRTRST            (1 << 8)    // Port reset
#define HPRT0_PRTLNSTS_SHIFT    10          // Port line status
#define HPRT0_PRTLNSTS_MASK     (3 << 10)
#define HPRT0_PRTPWR            (1 << 12)   // Port power
#define HPRT0_PRTTSTCTL_SHIFT   13          // Port test control
#define HPRT0_PRTSPD_SHIFT      17          // Port speed
#define HPRT0_PRTSPD_MASK       (3 << 17)
#define HPRT0_PRTSPD_HIGH       0
#define HPRT0_PRTSPD_FULL       1
#define HPRT0_PRTSPD_LOW        2

// ============================================================================
// Host Channel Registers (0x500 + n*0x20, n=0-15)
// ============================================================================

#define HCCHAR(n)       (*(volatile uint32_t *)(USB_BASE + 0x500 + (n)*0x20))
#define HCSPLT(n)       (*(volatile uint32_t *)(USB_BASE + 0x504 + (n)*0x20))
#define HCINT(n)        (*(volatile uint32_t *)(USB_BASE + 0x508 + (n)*0x20))
#define HCINTMSK(n)     (*(volatile uint32_t *)(USB_BASE + 0x50C + (n)*0x20))
#define HCTSIZ(n)       (*(volatile uint32_t *)(USB_BASE + 0x510 + (n)*0x20))
#define HCDMA(n)        (*(volatile uint32_t *)(USB_BASE + 0x514 + (n)*0x20))

// HCCHAR bits
#define HCCHAR_MPS_MASK         0x7FF       // Max packet size
#define HCCHAR_EPNUM_SHIFT      11          // Endpoint number
#define HCCHAR_EPDIR            (1 << 15)   // Endpoint direction (1=IN)
#define HCCHAR_LSDEV            (1 << 17)   // Low-speed device
#define HCCHAR_EPTYPE_SHIFT     18          // Endpoint type
#define HCCHAR_EPTYPE_CTRL      0
#define HCCHAR_EPTYPE_ISOC      1
#define HCCHAR_EPTYPE_BULK      2
#define HCCHAR_EPTYPE_INTR      3
#define HCCHAR_MC_SHIFT         20          // Multi-count
#define HCCHAR_DEVADDR_SHIFT    22          // Device address
#define HCCHAR_ODDFRM           (1 << 29)   // Odd frame
#define HCCHAR_CHDIS            (1 << 30)   // Channel disable
#define HCCHAR_CHENA            (1 << 31)   // Channel enable

// HCINT bits (channel interrupts)
#define HCINT_XFERCOMPL         (1 << 0)    // Transfer complete
#define HCINT_CHHLTD            (1 << 1)    // Channel halted
#define HCINT_AHBERR            (1 << 2)    // AHB error
#define HCINT_STALL             (1 << 3)    // STALL response
#define HCINT_NAK               (1 << 4)    // NAK response
#define HCINT_ACK               (1 << 5)    // ACK response
#define HCINT_XACTERR           (1 << 7)    // Transaction error
#define HCINT_BBLERR            (1 << 8)    // Babble error
#define HCINT_FRMOVRUN          (1 << 9)    // Frame overrun
#define HCINT_DATATGLERR        (1 << 10)   // Data toggle error

// HCTSIZ bits
#define HCTSIZ_XFERSIZE_MASK    0x7FFFF     // Transfer size
#define HCTSIZ_PKTCNT_SHIFT     19          // Packet count
#define HCTSIZ_PKTCNT_MASK      (0x3FF << 19)
#define HCTSIZ_PID_SHIFT        29          // PID
#define HCTSIZ_PID_DATA0        0
#define HCTSIZ_PID_DATA1        2
#define HCTSIZ_PID_DATA2        1
#define HCTSIZ_PID_SETUP        3

// ============================================================================
// Power and Clock Gating
// ============================================================================

#define PCGCCTL         (*(volatile uint32_t *)(USB_BASE + 0xE00))

// ============================================================================
// Data FIFOs (0x1000 + n*0x1000)
// ============================================================================

#define FIFO(n)         (*(volatile uint32_t *)(USB_BASE + 0x1000 + (n)*0x1000))

// ============================================================================
// Mailbox for USB power control
// ============================================================================

#define MAILBOX_BASE    (PERI_BASE + 0x00B880)
#define MAILBOX_READ    (*(volatile uint32_t *)(MAILBOX_BASE + 0x00))
#define MAILBOX_STATUS  (*(volatile uint32_t *)(MAILBOX_BASE + 0x18))
#define MAILBOX_WRITE   (*(volatile uint32_t *)(MAILBOX_BASE + 0x20))

#define MAILBOX_FULL    0x80000000
#define MAILBOX_EMPTY   0x40000000
#define MAILBOX_CH_PROP 8

// USB device ID for mailbox power control
#define DEVICE_ID_USB_HCD   3

// ============================================================================
// USB Descriptors and Structures
// ============================================================================

// Standard USB request
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

// Device descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

// Configuration descriptor header
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

// Interface descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

// Endpoint descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

// HID descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bReportDescriptorType;
    uint16_t wReportDescriptorLength;
} __attribute__((packed)) usb_hid_descriptor_t;

// USB standard requests
#define USB_REQ_GET_STATUS          0
#define USB_REQ_CLEAR_FEATURE       1
#define USB_REQ_SET_FEATURE         3
#define USB_REQ_SET_ADDRESS         5
#define USB_REQ_GET_DESCRIPTOR      6
#define USB_REQ_SET_DESCRIPTOR      7
#define USB_REQ_GET_CONFIGURATION   8
#define USB_REQ_SET_CONFIGURATION   9

// USB descriptor types
#define USB_DESC_DEVICE             1
#define USB_DESC_CONFIGURATION      2
#define USB_DESC_STRING             3
#define USB_DESC_INTERFACE          4
#define USB_DESC_ENDPOINT           5
#define USB_DESC_HID                0x21
#define USB_DESC_HID_REPORT         0x22

// USB class codes
#define USB_CLASS_HID               3

// HID subclass/protocol
#define USB_HID_SUBCLASS_BOOT       1
#define USB_HID_PROTOCOL_KEYBOARD   1
#define USB_HID_PROTOCOL_MOUSE      2

// ============================================================================
// Driver State
// ============================================================================

static struct {
    int initialized;
    int num_channels;
    int device_connected;
    int device_speed;           // 0=HS, 1=FS, 2=LS
    int device_address;
    int max_packet_size;
    uint8_t data_toggle[16];    // Data toggle for each endpoint
} usb_state = {0};

// Mailbox buffer (16-byte aligned)
static volatile uint32_t __attribute__((aligned(16))) mbox_buf[36];

// ============================================================================
// Helper Functions
// ============================================================================

static inline void dmb(void) {
    asm volatile("dmb sy" ::: "memory");
}

static inline void dsb(void) {
    asm volatile("dsb sy" ::: "memory");
}

static void usleep(uint32_t us) {
    uint32_t count = us * 333;  // ~1GHz clock, ~3 cycles per iteration
    while (count--) {
        asm volatile("nop");
    }
}

static void msleep(uint32_t ms) {
    usleep(ms * 1000);
}

// ============================================================================
// Mailbox Functions
// ============================================================================

static void mbox_write(uint32_t channel, uint32_t data) {
    while (MAILBOX_STATUS & MAILBOX_FULL) {
        dmb();
    }
    dmb();
    MAILBOX_WRITE = (data & 0xFFFFFFF0) | (channel & 0xF);
    dmb();
}

static uint32_t mbox_read(uint32_t channel) {
    uint32_t data;
    while (1) {
        while (MAILBOX_STATUS & MAILBOX_EMPTY) {
            dmb();
        }
        dmb();
        data = MAILBOX_READ;
        dmb();
        if ((data & 0xF) == channel) {
            return data & 0xFFFFFFF0;
        }
    }
}

static uint32_t arm_to_bus(void *ptr) {
    return ((uint32_t)(uint64_t)ptr) | 0xC0000000;
}

// Power on/off USB controller via mailbox
static int usb_set_power(int on) {
    usb_debug("[USB] Setting power %s\n", on ? "ON" : "OFF");

    uint32_t idx = 0;
    mbox_buf[idx++] = 8 * 4;        // Message size
    mbox_buf[idx++] = 0;            // Request

    // Set power state tag
    mbox_buf[idx++] = 0x00028001;   // Tag: set power state
    mbox_buf[idx++] = 8;            // Value size
    mbox_buf[idx++] = 8;            // Request
    mbox_buf[idx++] = DEVICE_ID_USB_HCD;  // Device ID: USB HCD
    mbox_buf[idx++] = on ? 3 : 0;   // State: on + wait, or off

    mbox_buf[idx++] = 0;            // End tag

    dmb();
    mbox_write(MAILBOX_CH_PROP, arm_to_bus((void *)mbox_buf));
    mbox_read(MAILBOX_CH_PROP);
    dmb();

    if (mbox_buf[1] != 0x80000000) {
        printf("[USB] Power control failed: %08x\n", mbox_buf[1]);
        return -1;
    }

    uint32_t state = mbox_buf[6];
    if (on && (state & 0x3) != 1) {
        printf("[USB] USB did not power on: %08x\n", state);
        return -1;
    }

    usb_debug("[USB] Power %s successful\n", on ? "ON" : "OFF");
    return 0;
}

// ============================================================================
// Core Reset and Initialization
// ============================================================================

static int usb_core_reset(void) {
    usb_debug("[USB] Core reset...\n");

    // Wait for AHB master idle
    int timeout = 100000;
    while (!(GRSTCTL & GRSTCTL_AHBIDLE) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] Timeout waiting for AHB idle\n");
        return -1;
    }

    // Trigger core soft reset
    GRSTCTL = GRSTCTL_CSFTRST;
    dsb();

    // Wait for reset to complete (hardware clears the bit)
    timeout = 100000;
    while ((GRSTCTL & GRSTCTL_CSFTRST) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] Timeout waiting for reset complete\n");
        return -1;
    }

    // Wait for AHB idle again
    timeout = 100000;
    while (!(GRSTCTL & GRSTCTL_AHBIDLE) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] Timeout waiting for AHB idle after reset\n");
        return -1;
    }

    // Wait a bit for things to settle
    msleep(100);

    usb_debug("[USB] Core reset complete\n");
    return 0;
}

static int usb_flush_fifos(void) {
    // Flush all TxFIFOs
    GRSTCTL = GRSTCTL_TXFFLSH | GRSTCTL_TXFNUM_ALL;
    dsb();

    int timeout = 10000;
    while ((GRSTCTL & GRSTCTL_TXFFLSH) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] TxFIFO flush timeout\n");
        return -1;
    }

    // Flush RxFIFO
    GRSTCTL = GRSTCTL_RXFFLSH;
    dsb();

    timeout = 10000;
    while ((GRSTCTL & GRSTCTL_RXFFLSH) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] RxFIFO flush timeout\n");
        return -1;
    }

    msleep(1);
    return 0;
}

// ============================================================================
// Host Mode Setup
// ============================================================================

static int usb_init_host(void) {
    usb_debug("[USB] Initializing host mode...\n");

    // Read hardware config to determine capabilities
    uint32_t hwcfg2 = GHWCFG2;
    uint32_t hwcfg3 = GHWCFG3;
    uint32_t hwcfg4 = GHWCFG4;

    usb_state.num_channels = ((hwcfg2 >> 14) & 0xF) + 1;
    uint32_t fifo_depth = (hwcfg3 >> 16) & 0xFFFF;

    usb_debug("[USB] HWCFG2: %08x, HWCFG3: %08x, HWCFG4: %08x\n", hwcfg2, hwcfg3, hwcfg4);
    usb_debug("[USB] Channels: %d, FIFO depth: %u words\n", usb_state.num_channels, fifo_depth);

    // Configure USB for host mode
    // Pi Zero 2W uses the internal DWC2 PHY
    uint32_t usbcfg = GUSBCFG;

    usb_debug("[USB] Initial GUSBCFG: %08x\n", usbcfg);

    // Clear mode forcing bits first
    usbcfg &= ~(GUSBCFG_FORCEDEVMODE | GUSBCFG_FORCEHOSTMODE);

    // Don't set PHYSEL - Pi uses the integrated HS PHY in FS mode
    // PHYSEL=0 means use the high-speed capable PHY
    usbcfg &= ~GUSBCFG_PHYSEL;

    // Use UTMI+ interface (not ULPI)
    usbcfg &= ~GUSBCFG_ULPI_UTMI_SEL;

    // 8-bit UTMI+ interface
    usbcfg &= ~GUSBCFG_PHYIF;

    GUSBCFG = usbcfg;
    dsb();
    msleep(10);

    // Now force host mode
    usbcfg |= GUSBCFG_FORCEHOSTMODE;
    GUSBCFG = usbcfg;
    dsb();

    usb_debug("[USB] Final GUSBCFG: %08x\n", GUSBCFG);

    // Wait for host mode (can take up to 25ms per spec)
    msleep(50);

    if (!(GINTSTS & GINTSTS_CURMODE)) {
        printf("[USB] Failed to enter host mode\n");
        return -1;
    }

    usb_debug("[USB] Host mode active\n");

    // Configure FIFOs
    // RxFIFO: 256 words (1024 bytes) - receives all IN data
    // Non-periodic TxFIFO: 256 words (1024 bytes) - control/bulk OUT
    // Periodic TxFIFO: 256 words (1024 bytes) - interrupt/isochronous OUT
    GRXFSIZ = 256;
    GNPTXFSIZ = (256 << 16) | 256;      // Size | Start address
    HPTXFSIZ = (256 << 16) | 512;       // Size | Start address
    dsb();

    // Flush FIFOs after sizing
    usb_flush_fifos();

    // Host configuration
    // Pi uses UTMI+ PHY at 60MHz - use FSLSPCLKSEL=0 (30/60 MHz mode)
    HCFG = HCFG_FSLSPCLKSEL_30_60;
    dsb();

    // Frame interval for 60MHz PHY
    HFIR = 60000;
    dsb();

    // Configure AHB (no DMA for now, use slave mode)
    // Don't enable global interrupts yet - we're polling
    GAHBCFG = 0;
    dsb();

    // Clear all pending interrupts
    GINTSTS = 0xFFFFFFFF;

    // Enable relevant interrupts
    GINTMSK = GINTSTS_PRTINT |      // Port interrupt
              GINTSTS_HCHINT |      // Host channel interrupt
              GINTSTS_DISCONNINT |  // Disconnect
              GINTSTS_CONIDSTSCHNG; // Connector ID change
    dsb();

    usb_debug("[USB] Host initialization complete\n");
    return 0;
}

// ============================================================================
// Port Control
// ============================================================================

static int usb_port_power_on(void) {
    usb_debug("[USB] Powering on port...\n");

    // Read current port status (preserve certain bits, clear W1C bits)
    uint32_t hprt = HPRT0;

    // Clear W1C bits so we don't accidentally clear them
    hprt &= ~(HPRT0_PRTENA | HPRT0_PRTCONNDET | HPRT0_PRTENCHNG | HPRT0_PRTOVRCURRCHNG);

    // Set port power
    hprt |= HPRT0_PRTPWR;
    HPRT0 = hprt;
    dsb();

    // Wait for power to stabilize
    msleep(50);

    usb_debug("[USB] Port power on, HPRT0: %08x\n", HPRT0);
    return 0;
}

static int usb_port_reset(void) {
    usb_debug("[USB] Resetting port...\n");

    // Read port status
    uint32_t hprt = HPRT0;
    hprt &= ~(HPRT0_PRTENA | HPRT0_PRTCONNDET | HPRT0_PRTENCHNG | HPRT0_PRTOVRCURRCHNG);

    // Assert reset
    hprt |= HPRT0_PRTRST;
    HPRT0 = hprt;
    dsb();

    // Hold reset for 50ms (USB spec requires at least 10ms)
    msleep(50);

    // De-assert reset
    hprt = HPRT0;
    hprt &= ~(HPRT0_PRTENA | HPRT0_PRTCONNDET | HPRT0_PRTENCHNG | HPRT0_PRTOVRCURRCHNG);
    hprt &= ~HPRT0_PRTRST;
    HPRT0 = hprt;
    dsb();

    // Wait for port to become enabled
    msleep(20);

    hprt = HPRT0;
    usb_debug("[USB] After reset, HPRT0: %08x\n", hprt);

    if (!(hprt & HPRT0_PRTENA)) {
        printf("[USB] Port not enabled after reset\n");
        return -1;
    }

    // Get device speed
    usb_state.device_speed = (hprt & HPRT0_PRTSPD_MASK) >> HPRT0_PRTSPD_SHIFT;
    const char *speed_str[] = {"High", "Full", "Low"};
    usb_debug("[USB] Device speed: %s\n", speed_str[usb_state.device_speed]);

    // Configure HCFG and HFIR based on PHY type
    // Pi uses UTMI+ PHY which runs at 60MHz, even for FS/LS devices
    // FSLSPCLKSEL = 0 means 30/60 MHz (for UTMI+ HS PHY)
    // FSLSPCLKSEL = 1 means 48 MHz (for dedicated FS PHY - NOT on Pi)
    HCFG = HCFG_FSLSPCLKSEL_30_60;  // Must be 0 for Pi's UTMI+ PHY
    HFIR = 60000;  // 60MHz * 1ms = 60000 clocks per frame
    dsb();
    usb_debug("[USB] HCFG=%08x HFIR=%08x\n", HCFG, HFIR);

    return 0;
}

static int usb_wait_for_device(void) {
    usb_debug("[USB] Waiting for device connection...\n");

    // Check if already connected
    uint32_t hprt = HPRT0;
    if (hprt & HPRT0_PRTCONNSTS) {
        usb_debug("[USB] Device already connected\n");
        usb_state.device_connected = 1;
        return 0;
    }

    // Wait up to 5 seconds for connection
    for (int i = 0; i < 50; i++) {
        hprt = HPRT0;
        if (hprt & HPRT0_PRTCONNSTS) {
            usb_debug("[USB] Device connected!\n");
            usb_state.device_connected = 1;
            // Clear connect detect
            HPRT0 = (hprt & ~(HPRT0_PRTENA | HPRT0_PRTENCHNG | HPRT0_PRTOVRCURRCHNG)) | HPRT0_PRTCONNDET;
            return 0;
        }
        msleep(100);
    }

    printf("[USB] No device connected\n");
    return -1;
}

// ============================================================================
// Channel Control and Transfers
// ============================================================================

static void usb_halt_channel(int ch) {
    uint32_t hcchar = HCCHAR(ch);

    if (!(hcchar & HCCHAR_CHENA)) {
        return;  // Already disabled
    }

    // Disable channel
    hcchar |= HCCHAR_CHDIS;
    hcchar &= ~HCCHAR_CHENA;
    HCCHAR(ch) = hcchar;
    dsb();

    // Wait for channel halted
    int timeout = 10000;
    while (!(HCINT(ch) & HCINT_CHHLTD) && timeout--) {
        usleep(1);
    }

    // Clear interrupt
    HCINT(ch) = 0xFFFFFFFF;
}

// GRXSTSP packet status values
#define GRXSTS_PKTSTS_IN_DATA       2
#define GRXSTS_PKTSTS_IN_COMPLETE   3
#define GRXSTS_PKTSTS_TOGGLE_ERR    5
#define GRXSTS_PKTSTS_CH_HALTED     7

// Wait for channel to complete, handling NAKs with retries
static int usb_wait_for_channel(int ch, int max_retries) {
    for (int retry = 0; retry < max_retries; retry++) {
        int timeout = 50000;
        while (timeout--) {
            uint32_t hcint = HCINT(ch);

            if (hcint & HCINT_XFERCOMPL) {
                HCINT(ch) = 0xFFFFFFFF;
                return 0;  // Success
            }
            if (hcint & HCINT_STALL) {
                usb_debug("[USB] STALL\n");
                HCINT(ch) = 0xFFFFFFFF;
                return -1;
            }
            if (hcint & HCINT_XACTERR) {
                usb_debug("[USB] Transaction error (hcint=%08x)\n", hcint);
                HCINT(ch) = 0xFFFFFFFF;
                return -1;
            }
            if (hcint & HCINT_NAK) {
                // NAK - need to retry
                HCINT(ch) = HCINT_NAK;  // Clear NAK
                break;  // Break inner loop, retry
            }
            if (hcint & HCINT_CHHLTD) {
                HCINT(ch) = 0xFFFFFFFF;
                return 0;  // Channel halted, might be OK
            }
            usleep(1);
        }

        if (retry < max_retries - 1) {
            // Re-enable channel for retry
            uint32_t hcchar = HCCHAR(ch);
            HCCHAR(ch) = hcchar | HCCHAR_CHENA;
            dsb();
            usleep(1000);  // Small delay between retries
        }
    }

    usb_debug("[USB] Max retries exceeded\n");
    return -1;
}

// Control transfer (SETUP + optional DATA + STATUS)
static int usb_control_transfer(int device_addr, usb_setup_packet_t *setup,
                                void *data, int data_len, int data_in) {
    int ch = 0;  // Use channel 0 for control

    usb_debug("[USB] Control: addr=%d req=%02x val=%04x len=%d %s\n",
              device_addr, setup->bRequest, setup->wValue, data_len,
              data_in ? "IN" : "OUT");

    // Halt channel if active
    usb_halt_channel(ch);

    // Clear all channel interrupts
    HCINT(ch) = 0xFFFFFFFF;

    // Enable all interrupts for this channel
    HCINTMSK(ch) = HCINT_XFERCOMPL | HCINT_CHHLTD | HCINT_STALL |
                   HCINT_NAK | HCINT_ACK | HCINT_XACTERR | HCINT_DATATGLERR;

    // Configure channel for control endpoint
    // For initial enumeration (addr 0), use 64 for Full Speed, 8 for Low Speed
    // Full Speed devices can send up to 64 bytes per packet - using 8 causes babble errors
    uint32_t mps;
    if (device_addr == 0) {
        mps = (usb_state.device_speed == 2) ? 8 : 64;  // LS=8, FS/HS=64
    } else {
        mps = usb_state.max_packet_size;
        if (mps == 0) mps = 64;
    }

    uint32_t hcchar = (mps & HCCHAR_MPS_MASK) |
                      (0 << HCCHAR_EPNUM_SHIFT) |         // EP0
                      (HCCHAR_EPTYPE_CTRL << HCCHAR_EPTYPE_SHIFT) |
                      (device_addr << HCCHAR_DEVADDR_SHIFT) |
                      (1 << HCCHAR_MC_SHIFT);             // 1 transaction per frame

    if (usb_state.device_speed == 2) {  // Low-speed
        hcchar |= HCCHAR_LSDEV;
    }

    // ========== SETUP Stage ==========
    usb_debug("[USB] SETUP stage...\n");

    // Configure channel (OUT direction for SETUP)
    HCCHAR(ch) = hcchar;
    dsb();

    // Transfer size: 8 bytes, 1 packet, SETUP PID
    HCTSIZ(ch) = 8 | (1 << HCTSIZ_PKTCNT_SHIFT) | (HCTSIZ_PID_SETUP << HCTSIZ_PID_SHIFT);
    dsb();

    // Write SETUP packet to FIFO first (before enabling channel)
    uint32_t *setup32 = (uint32_t *)setup;
    FIFO(ch) = setup32[0];
    FIFO(ch) = setup32[1];
    dsb();

    // Now enable channel to start the transfer
    HCCHAR(ch) = hcchar | HCCHAR_CHENA;
    dsb();

    // Wait for SETUP completion
    if (usb_wait_for_channel(ch, 3) < 0) {
        usb_debug("[USB] SETUP failed\n");
        return -1;
    }
    usb_debug("[USB] SETUP complete\n");

    // ========== DATA Stage (if any) ==========
    int bytes_transferred = 0;

    if (data_len > 0 && data != NULL) {
        usb_debug("[USB] DATA stage (%d bytes)...\n", data_len);

        // Configure for data direction
        uint32_t data_hcchar = hcchar;
        if (data_in) {
            data_hcchar |= HCCHAR_EPDIR;  // IN
        }

        // Calculate packet count
        int pkt_count = (data_len + mps - 1) / mps;

        // Clear interrupts
        HCINT(ch) = 0xFFFFFFFF;

        // Configure channel
        HCCHAR(ch) = data_hcchar;
        dsb();

        // Transfer size, packet count, DATA1 PID (first data packet after SETUP is always DATA1)
        HCTSIZ(ch) = data_len | (pkt_count << HCTSIZ_PKTCNT_SHIFT) |
                     (HCTSIZ_PID_DATA1 << HCTSIZ_PID_SHIFT);
        dsb();

        if (data_in) {
            // IN transfer - enable channel, then read data from FIFO
            HCCHAR(ch) = data_hcchar | HCCHAR_CHENA;
            dsb();

            usb_debug("[USB] DATA IN: HCCHAR=%08x HCTSIZ=%08x\n", HCCHAR(ch), HCTSIZ(ch));

            // Wait and read data as it arrives
            int timeout = 100000;
            int done = 0;
            int nak_count = 0;
            int last_print = 0;
            while (!done && timeout--) {
                uint32_t gintsts = GINTSTS;
                uint32_t hcint = HCINT(ch);

                // Print status periodically
                if (timeout % 10000 == 0 && timeout != last_print) {
                    usb_debug("[USB] Waiting: GINTSTS=%08x HCINT=%08x HCCHAR=%08x\n",
                              gintsts, hcint, HCCHAR(ch));
                    last_print = timeout;
                }

                // Check for RxFIFO data
                if (gintsts & GINTSTS_RXFLVL) {
                    uint32_t grxsts = GRXSTSP;  // Pop and read status
                    int pktsts = (grxsts >> 17) & 0xF;
                    int bcnt = (grxsts >> 4) & 0x7FF;
                    int ch_num = grxsts & 0xF;

                    usb_debug("[USB] RX: grxsts=%08x ch=%d pktsts=%d bcnt=%d\n", grxsts, ch_num, pktsts, bcnt);

                    if (pktsts == GRXSTS_PKTSTS_IN_DATA && bcnt > 0) {
                        // Read data from FIFO
                        int words = (bcnt + 3) / 4;
                        uint32_t *data32 = (uint32_t *)((uint8_t *)data + bytes_transferred);
                        for (int i = 0; i < words; i++) {
                            data32[i] = FIFO(ch_num);
                        }
                        bytes_transferred += bcnt;
                        usb_debug("[USB] Read %d bytes (total %d)\n", bcnt, bytes_transferred);
                    } else if (pktsts == GRXSTS_PKTSTS_IN_COMPLETE) {
                        // Transfer complete indication - re-enable channel for next packet if needed
                        usb_debug("[USB] IN complete indication\n");

                        // Check if we need more data and channel isn't done
                        if (bytes_transferred < data_len && !(HCINT(ch) & HCINT_XFERCOMPL)) {
                            // Re-enable channel to receive next packet
                            HCCHAR(ch) = data_hcchar | HCCHAR_CHENA;
                            dsb();
                        }
                    }
                }

                // Check channel interrupt
                if (hcint & HCINT_XFERCOMPL) {
                    usb_debug("[USB] XFERCOMPL\n");
                    HCINT(ch) = HCINT_XFERCOMPL;
                    done = 1;
                }
                if (hcint & HCINT_CHHLTD) {
                    usb_debug("[USB] Channel halted, hcint=%08x\n", hcint);
                    HCINT(ch) = HCINT_CHHLTD;
                    // Check if transfer actually completed
                    if (bytes_transferred >= data_len || (HCTSIZ(ch) & HCTSIZ_XFERSIZE_MASK) == 0) {
                        done = 1;
                    } else {
                        // Need more data, re-enable channel
                        HCCHAR(ch) = data_hcchar | HCCHAR_CHENA;
                        dsb();
                    }
                }
                if (hcint & HCINT_BBLERR) {
                    usb_debug("[USB] Babble error - frame timing issue\n");
                    HCINT(ch) = 0xFFFFFFFF;
                    return -1;
                }
                if (hcint & (HCINT_STALL | HCINT_XACTERR)) {
                    usb_debug("[USB] DATA IN error: hcint=%08x\n", hcint);
                    HCINT(ch) = 0xFFFFFFFF;
                    return -1;
                }
                if (hcint & HCINT_NAK) {
                    nak_count++;
                    HCINT(ch) = HCINT_NAK;
                    if (nak_count < 1000) {
                        // Re-enable channel for retry
                        HCCHAR(ch) = data_hcchar | HCCHAR_CHENA;
                        dsb();
                    } else if (nak_count == 1000) {
                        usb_debug("[USB] Too many NAKs (%d)\n", nak_count);
                    }
                }
                if (hcint & HCINT_ACK) {
                    usb_debug("[USB] ACK received\n");
                    HCINT(ch) = HCINT_ACK;

                    // After ACK, re-enable channel for next packet if transfer not complete
                    if (bytes_transferred < data_len && !(HCINT(ch) & HCINT_XFERCOMPL)) {
                        HCCHAR(ch) = data_hcchar | HCCHAR_CHENA;
                        dsb();
                    }
                }

                usleep(1);
            }

            if (timeout <= 0) {
                usb_debug("[USB] DATA IN timeout (nak_count=%d)\n", nak_count);
                usb_debug("[USB] Final: GINTSTS=%08x HCINT=%08x HCCHAR=%08x HCTSIZ=%08x\n",
                          GINTSTS, HCINT(ch), HCCHAR(ch), HCTSIZ(ch));
                return -1;
            }
        } else {
            // OUT transfer - write data to FIFO, then enable channel
            uint32_t *data32 = (uint32_t *)data;
            int words = (data_len + 3) / 4;
            for (int i = 0; i < words; i++) {
                FIFO(ch) = data32[i];
            }
            dsb();

            // Enable channel
            HCCHAR(ch) = data_hcchar | HCCHAR_CHENA;
            dsb();

            // Wait for completion
            if (usb_wait_for_channel(ch, 3) < 0) {
                usb_debug("[USB] DATA OUT failed\n");
                return -1;
            }
            bytes_transferred = data_len;
        }

        usb_debug("[USB] DATA complete (%d bytes)\n", bytes_transferred);
    }

    // ========== STATUS Stage ==========
    usb_debug("[USB] STATUS stage...\n");

    // Status is opposite direction of data (or IN if no data)
    int status_in = (data_len > 0) ? !data_in : 1;

    uint32_t status_hcchar = hcchar;
    if (status_in) {
        status_hcchar |= HCCHAR_EPDIR;
    }

    // Clear interrupts
    HCINT(ch) = 0xFFFFFFFF;

    // Configure channel
    HCCHAR(ch) = status_hcchar;
    dsb();

    // Zero-length packet, DATA1 PID
    HCTSIZ(ch) = 0 | (1 << HCTSIZ_PKTCNT_SHIFT) | (HCTSIZ_PID_DATA1 << HCTSIZ_PID_SHIFT);
    dsb();

    // Enable channel
    HCCHAR(ch) = status_hcchar | HCCHAR_CHENA;
    dsb();

    // For IN status, we need to drain any RxFIFO data
    if (status_in) {
        int timeout = 50000;
        while (timeout--) {
            if (GINTSTS & GINTSTS_RXFLVL) {
                uint32_t grxsts = GRXSTSP;  // Pop and discard
                (void)grxsts;
            }
            uint32_t hcint = HCINT(ch);
            if (hcint & (HCINT_XFERCOMPL | HCINT_CHHLTD)) {
                HCINT(ch) = 0xFFFFFFFF;
                break;
            }
            if (hcint & (HCINT_STALL | HCINT_XACTERR)) {
                usb_debug("[USB] STATUS error: hcint=%08x\n", hcint);
                HCINT(ch) = 0xFFFFFFFF;
                return -1;
            }
            if (hcint & HCINT_NAK) {
                HCINT(ch) = HCINT_NAK;
                HCCHAR(ch) = status_hcchar | HCCHAR_CHENA;
                dsb();
            }
            usleep(1);
        }
    } else {
        // OUT status
        if (usb_wait_for_channel(ch, 3) < 0) {
            usb_debug("[USB] STATUS OUT failed\n");
            return -1;
        }
    }

    usb_debug("[USB] Control transfer complete, %d bytes\n", bytes_transferred);
    return bytes_transferred;
}

// ============================================================================
// USB Enumeration
// ============================================================================

static int usb_get_device_descriptor(int addr, usb_device_descriptor_t *desc) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,  // Device to host, standard, device
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DESC_DEVICE << 8) | 0,
        .wIndex = 0,
        .wLength = sizeof(usb_device_descriptor_t)
    };

    return usb_control_transfer(addr, &setup, desc, sizeof(usb_device_descriptor_t), 1);
}

static int usb_set_address(int addr) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x00,  // Host to device, standard, device
        .bRequest = USB_REQ_SET_ADDRESS,
        .wValue = addr,
        .wIndex = 0,
        .wLength = 0
    };

    return usb_control_transfer(0, &setup, NULL, 0, 0);
}

static int usb_set_configuration(int addr, int config) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x00,
        .bRequest = USB_REQ_SET_CONFIGURATION,
        .wValue = config,
        .wIndex = 0,
        .wLength = 0
    };

    return usb_control_transfer(addr, &setup, NULL, 0, 0);
}

static int usb_get_configuration_descriptor(int addr, uint8_t *buf, int len) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DESC_CONFIGURATION << 8) | 0,
        .wIndex = 0,
        .wLength = len
    };

    return usb_control_transfer(addr, &setup, buf, len, 1);
}

static int usb_enumerate_device(void) {
    usb_debug("[USB] Enumerating device...\n");

    // First, get device descriptor with address 0 (limited to 8 bytes initially)
    usb_device_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));

    // Get first 8 bytes to learn max packet size
    usb_state.max_packet_size = 8;  // Start with minimum
    int ret = usb_get_device_descriptor(0, &desc);
    if (ret < 8) {
        printf("[USB] Failed to get device descriptor (got %d bytes)\n", ret);
        return -1;
    }

    usb_state.max_packet_size = desc.bMaxPacketSize0;
    usb_debug("[USB] Device descriptor: VID=%04x PID=%04x MaxPacket=%d\n",
              desc.idVendor, desc.idProduct, desc.bMaxPacketSize0);

    // Set device address (use address 1)
    msleep(10);
    ret = usb_set_address(1);
    if (ret < 0) {
        printf("[USB] Failed to set address\n");
        return -1;
    }
    usb_state.device_address = 1;
    msleep(10);

    // Get full device descriptor at new address
    ret = usb_get_device_descriptor(1, &desc);
    if (ret < (int)sizeof(desc)) {
        printf("[USB] Failed to get full device descriptor\n");
        return -1;
    }

    usb_debug("[USB] Device: USB%x.%x Class=%d VID=%04x PID=%04x\n",
              desc.bcdUSB >> 8, (desc.bcdUSB >> 4) & 0xF,
              desc.bDeviceClass, desc.idVendor, desc.idProduct);

    // Get configuration descriptor
    uint8_t config_buf[256];
    ret = usb_get_configuration_descriptor(1, config_buf, sizeof(config_buf));
    if (ret < 9) {
        printf("[USB] Failed to get config descriptor\n");
        return -1;
    }

    usb_config_descriptor_t *config = (usb_config_descriptor_t *)config_buf;
    usb_debug("[USB] Config: %d interfaces, total length %d\n",
              config->bNumInterfaces, config->wTotalLength);

    // Parse interfaces to find HID devices
    int offset = config->bLength;
    while (offset < config->wTotalLength) {
        uint8_t len = config_buf[offset];
        uint8_t type = config_buf[offset + 1];

        if (type == USB_DESC_INTERFACE) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)&config_buf[offset];
            usb_debug("[USB] Interface %d: Class=%d SubClass=%d Protocol=%d\n",
                      iface->bInterfaceNumber, iface->bInterfaceClass,
                      iface->bInterfaceSubClass, iface->bInterfaceProtocol);

            if (iface->bInterfaceClass == USB_CLASS_HID) {
                usb_debug("[USB] Found HID device!\n");
                if (iface->bInterfaceProtocol == USB_HID_PROTOCOL_KEYBOARD) {
                    usb_debug("[USB] -> Boot keyboard\n");
                } else if (iface->bInterfaceProtocol == USB_HID_PROTOCOL_MOUSE) {
                    usb_debug("[USB] -> Boot mouse\n");
                }
            }
        }

        offset += len;
        if (len == 0) break;  // Prevent infinite loop
    }

    // Set configuration
    ret = usb_set_configuration(1, config->bConfigurationValue);
    if (ret < 0) {
        printf("[USB] Failed to set configuration\n");
        return -1;
    }

    usb_debug("[USB] Device configured!\n");
    return 0;
}

// ============================================================================
// Public API
// ============================================================================

int hal_usb_init(void) {
    printf("[USB] Initializing DWC2 USB controller...\n");

    // Power on USB
    if (usb_set_power(1) < 0) {
        return -1;
    }

    // Give power time to stabilize
    msleep(100);

    // Core reset
    if (usb_core_reset() < 0) {
        return -1;
    }

    // Initialize host mode
    if (usb_init_host() < 0) {
        return -1;
    }

    // Power on port
    if (usb_port_power_on() < 0) {
        return -1;
    }

    // Wait for device connection
    if (usb_wait_for_device() < 0) {
        printf("[USB] No USB device found - continuing without USB\n");
        return 0;  // Not fatal
    }

    // Reset port (and get device speed)
    if (usb_port_reset() < 0) {
        return -1;
    }

    // Enumerate device
    if (usb_enumerate_device() < 0) {
        printf("[USB] Device enumeration failed\n");
        return -1;
    }

    usb_state.initialized = 1;
    printf("[USB] USB initialization complete!\n");
    return 0;
}

// Placeholder for keyboard polling (will be expanded)
int hal_usb_keyboard_poll(uint8_t *report, int report_len) {
    if (!usb_state.initialized || !usb_state.device_connected) {
        return -1;
    }

    // TODO: Implement interrupt transfer for HID reports
    return -1;
}
