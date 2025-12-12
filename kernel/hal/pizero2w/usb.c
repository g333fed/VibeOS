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

// Debug output levels
// 0 = errors only, 1 = key events, 2 = verbose
#define USB_DEBUG_LEVEL 1

#if USB_DEBUG_LEVEL >= 2
#define usb_debug(...) printf(__VA_ARGS__)
#else
#define usb_debug(...) ((void)0)
#endif

#if USB_DEBUG_LEVEL >= 1
#define usb_info(...) printf(__VA_ARGS__)
#else
#define usb_info(...) ((void)0)
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

// HID class requests
#define USB_HID_SET_PROTOCOL        0x0B
#define USB_HID_SET_IDLE            0x0A
#define USB_HID_PROTOCOL_BOOT       0
#define USB_HID_PROTOCOL_REPORT     1

// USB Hub class
#define USB_CLASS_HUB               9

// Hub descriptor
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t  bPwrOn2PwrGood;        // Time in 2ms intervals
    uint8_t  bHubContrCurrent;
    uint8_t  DeviceRemovable[8];    // Variable length, max 8 bytes for 64 ports
} __attribute__((packed)) usb_hub_descriptor_t;

// Hub class requests
#define USB_REQ_GET_HUB_STATUS      0
#define USB_REQ_GET_PORT_STATUS     0
#define USB_REQ_SET_PORT_FEATURE    3
#define USB_REQ_CLEAR_PORT_FEATURE  1

// Hub port features
#define USB_PORT_FEAT_CONNECTION    0
#define USB_PORT_FEAT_ENABLE        1
#define USB_PORT_FEAT_SUSPEND       2
#define USB_PORT_FEAT_OVER_CURRENT  3
#define USB_PORT_FEAT_RESET         4
#define USB_PORT_FEAT_POWER         8
#define USB_PORT_FEAT_LOWSPEED      9
#define USB_PORT_FEAT_C_CONNECTION  16
#define USB_PORT_FEAT_C_ENABLE      17
#define USB_PORT_FEAT_C_SUSPEND     18
#define USB_PORT_FEAT_C_OVER_CURRENT 19
#define USB_PORT_FEAT_C_RESET       20

// Hub port status bits
#define USB_PORT_STAT_CONNECTION    (1 << 0)
#define USB_PORT_STAT_ENABLE        (1 << 1)
#define USB_PORT_STAT_SUSPEND       (1 << 2)
#define USB_PORT_STAT_OVERCURRENT   (1 << 3)
#define USB_PORT_STAT_RESET         (1 << 4)
#define USB_PORT_STAT_POWER         (1 << 8)
#define USB_PORT_STAT_LOW_SPEED     (1 << 9)
#define USB_PORT_STAT_HIGH_SPEED    (1 << 10)

// Hub descriptor type
#define USB_DESC_HUB                0x29

// ============================================================================
// Driver State
// ============================================================================

// USB device info
typedef struct {
    int address;
    int speed;                  // 0=HS, 1=FS, 2=LS
    int max_packet_size;
    int is_hub;
    int hub_ports;              // Number of ports if hub
    int parent_hub;             // Address of parent hub (0 = root)
    int parent_port;            // Port on parent hub
} usb_device_t;

#define MAX_USB_DEVICES 8

static struct {
    int initialized;
    int num_channels;
    int device_connected;
    int device_speed;           // 0=HS, 1=FS, 2=LS
    int next_address;           // Next address to assign
    uint8_t data_toggle[16];    // Data toggle for each endpoint

    // Device tracking
    usb_device_t devices[MAX_USB_DEVICES];
    int num_devices;

    // Keyboard info (if found)
    int keyboard_addr;
    int keyboard_ep;            // Interrupt endpoint
    int keyboard_mps;           // Max packet size for interrupt EP
    int keyboard_interval;      // Polling interval
} usb_state = {0};

// Forward declarations for interrupt-driven keyboard
static void usb_irq_handler(void);
void usb_start_keyboard_transfer(void);

// Mailbox buffer (16-byte aligned)
static volatile uint32_t __attribute__((aligned(16))) mbox_buf[36];

// DMA buffer for USB transfers (32-byte aligned for cache, 512 bytes for max transfer)
// DMA buffers - must be 64-byte aligned for Cortex-A53 cache line size
static uint8_t __attribute__((aligned(64))) dma_buffer[512];

// ============================================================================
// Helper Functions
// ============================================================================

static inline void dmb(void) {
    asm volatile("dmb sy" ::: "memory");
}

static inline void dsb(void) {
    asm volatile("dsb sy" ::: "memory");
}

/*
 * Cache maintenance operations for AArch64
 * Required because DWC2 DMA does not see CPU L1 cache updates!
 * This is why USB works on QEMU but not real hardware.
 */
static void clean_data_cache_range(uintptr_t start, size_t length) {
    uintptr_t line_size;
    // Read cache line size from CTR_EL0
    asm volatile("mrs %0, ctr_el0" : "=r" (line_size));
    // Extract DminLine (bits [19:16]), encoded as log2(words)
    uint32_t dminline = (line_size >> 16) & 0xF;
    // Convert to bytes: 4 * 2^dminline
    size_t step = 4 << dminline;

    uintptr_t addr = start & ~(step - 1);
    uintptr_t end = start + length;

    // Clean data cache by virtual address to point of coherency
    while (addr < end) {
        asm volatile("dc cvac, %0" :: "r" (addr));
        addr += step;
    }
    asm volatile("dsb sy" ::: "memory");
}

static void invalidate_data_cache_range(uintptr_t start, size_t length) {
    uintptr_t line_size;
    asm volatile("mrs %0, ctr_el0" : "=r" (line_size));
    uint32_t dminline = (line_size >> 16) & 0xF;
    size_t step = 4 << dminline;

    uintptr_t addr = start & ~(step - 1);
    uintptr_t end = start + length;

    // Invalidate data cache by virtual address to point of coherency
    while (addr < end) {
        asm volatile("dc ivac, %0" :: "r" (addr));
        addr += step;
    }
    asm volatile("dsb sy" ::: "memory");
}

static void usleep(uint32_t us) {
    // Use ARM generic timer for accurate delays
    uint64_t freq, start, target;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    asm volatile("mrs %0, cntpct_el0" : "=r"(start));
    target = start + (freq * us / 1000000);
    uint64_t now;
    do {
        asm volatile("mrs %0, cntpct_el0" : "=r"(now));
    } while (now < target);
}

static void msleep(uint32_t ms) {
    // Use ARM generic timer for accurate delays
    uint64_t freq, start, target;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    asm volatile("mrs %0, cntpct_el0" : "=r"(start));
    target = start + (freq * ms / 1000);
    uint64_t now;
    do {
        asm volatile("mrs %0, cntpct_el0" : "=r"(now));
    } while (now < target);
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

    usb_debug("[USB] Reading GRSTCTL...\n");
    uint32_t val = GRSTCTL;
    usb_debug("[USB] GRSTCTL = %08x\n", val);

    // Wait for AHB master idle
    usb_debug("[USB] Waiting for AHB idle...\n");
    int timeout = 100000;
    while (!(GRSTCTL & GRSTCTL_AHBIDLE) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] Timeout waiting for AHB idle\n");
        return -1;
    }
    usb_debug("[USB] AHB idle OK\n");

    // Trigger core soft reset
    usb_debug("[USB] Triggering soft reset...\n");
    GRSTCTL = GRSTCTL_CSFTRST;
    dsb();
    usb_debug("[USB] Soft reset triggered\n");

    // Wait for reset to complete (hardware clears the bit)
    usb_debug("[USB] Waiting for reset complete...\n");
    timeout = 100000;
    while ((GRSTCTL & GRSTCTL_CSFTRST) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] Timeout waiting for reset complete\n");
        return -1;
    }
    usb_debug("[USB] Reset complete\n");

    // Wait for AHB idle again
    usb_debug("[USB] Waiting for AHB idle again...\n");
    timeout = 100000;
    while (!(GRSTCTL & GRSTCTL_AHBIDLE) && timeout--) {
        usleep(1);
    }
    if (timeout <= 0) {
        printf("[USB] Timeout waiting for AHB idle after reset\n");
        return -1;
    }
    usb_debug("[USB] AHB idle again OK\n");

    // Wait a bit for things to settle
    usb_debug("[USB] Settling...\n");
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

    // Configure AHB for DMA mode (interrupts enabled later after handler registered)
    // QEMU's DWC2 emulation only supports DMA mode, not slave mode
    GAHBCFG = GAHBCFG_DMA_EN;
    dsb();
    usb_debug("[USB] DMA mode enabled (GAHBCFG=%08x)\n", GAHBCFG);

    // Clear all pending interrupts
    GINTSTS = 0xFFFFFFFF;

    // Enable relevant interrupts
    // NOTE: SOF is NOT enabled - it fires 1000x/sec and kills performance
    // Keyboard polling is driven by timer tick instead
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
    usb_info("[USB] Device speed: %s\n", speed_str[usb_state.device_speed]);

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


// Wait for DMA transfer to complete
static int usb_wait_for_dma_complete(int ch, int max_retries) {
    for (int retry = 0; retry < max_retries; retry++) {
        int timeout = 100000;
        while (timeout--) {
            uint32_t hcint = HCINT(ch);

            if (hcint & HCINT_XFERCOMPL) {
                HCINT(ch) = 0xFFFFFFFF;
                return 0;  // Success
            }
            if (hcint & HCINT_CHHLTD) {
                // Channel halted - check why
                if (hcint & (HCINT_XFERCOMPL | HCINT_ACK)) {
                    HCINT(ch) = 0xFFFFFFFF;
                    return 0;  // Transfer actually completed
                }
                if (hcint & HCINT_NAK) {
                    // NAK - need to retry
                    HCINT(ch) = 0xFFFFFFFF;
                    break;  // Break to retry loop
                }
                if (hcint & (HCINT_STALL | HCINT_XACTERR | HCINT_BBLERR | HCINT_AHBERR)) {
                    usb_debug("[USB] Transfer error: hcint=%08x\n", hcint);
                    HCINT(ch) = 0xFFFFFFFF;
                    return -1;
                }
                // Other halt reason - assume done
                HCINT(ch) = 0xFFFFFFFF;
                return 0;
            }
            if (hcint & HCINT_AHBERR) {
                usb_debug("[USB] AHB error (bad DMA address?)\n");
                HCINT(ch) = 0xFFFFFFFF;
                return -1;
            }
            if (hcint & HCINT_STALL) {
                usb_debug("[USB] STALL\n");
                HCINT(ch) = 0xFFFFFFFF;
                return -1;
            }
            if (hcint & HCINT_BBLERR) {
                usb_debug("[USB] Babble error\n");
                HCINT(ch) = 0xFFFFFFFF;
                return -1;
            }
            if (hcint & HCINT_XACTERR) {
                usb_debug("[USB] Transaction error\n");
                HCINT(ch) = 0xFFFFFFFF;
                return -1;
            }

            usleep(1);
        }

        if (retry < max_retries - 1) {
            usb_debug("[USB] Retry %d/%d\n", retry + 1, max_retries);
            // Re-enable channel for retry
            uint32_t hcchar = HCCHAR(ch);
            hcchar |= HCCHAR_CHENA;
            hcchar &= ~HCCHAR_CHDIS;
            HCCHAR(ch) = hcchar;
            dsb();
            usleep(1000);
        }
    }

    usb_debug("[USB] Transfer timeout after %d retries\n", max_retries);
    return -1;
}

// Control transfer using DMA (SETUP + optional DATA + STATUS)
static int usb_control_transfer(int device_addr, usb_setup_packet_t *setup,
                                void *data, int data_len, int data_in) {
    int ch = 0;  // Use channel 0 for control

    usb_debug("[USB] Control: addr=%d req=%02x val=%04x len=%d %s\n",
              device_addr, setup->bRequest, setup->wValue, data_len,
              data_in ? "IN" : "OUT");

    // Halt channel if active
    usb_halt_channel(ch);

    // Configure channel for control endpoint
    // Look up device to get MPS and speed
    uint32_t mps = 64;  // Default for FS/HS
    int dev_speed = usb_state.device_speed;

    if (device_addr == 0) {
        mps = (usb_state.device_speed == 2) ? 8 : 64;  // LS=8, FS/HS=64
    } else {
        // Find device in our list
        for (int i = 0; i < usb_state.num_devices; i++) {
            if (usb_state.devices[i].address == device_addr) {
                mps = usb_state.devices[i].max_packet_size;
                dev_speed = usb_state.devices[i].speed;
                break;
            }
        }
        if (mps == 0) mps = 64;
    }

    uint32_t hcchar_base = (mps & HCCHAR_MPS_MASK) |
                           (0 << HCCHAR_EPNUM_SHIFT) |         // EP0
                           (HCCHAR_EPTYPE_CTRL << HCCHAR_EPTYPE_SHIFT) |
                           (device_addr << HCCHAR_DEVADDR_SHIFT) |
                           (1 << HCCHAR_MC_SHIFT);             // 1 transaction per frame

    if (dev_speed == 2) {  // Low-speed
        hcchar_base |= HCCHAR_LSDEV;
    }

    // ========== SETUP Stage (DMA) ==========
    usb_debug("[USB] SETUP stage (DMA)...\n");

    // Copy SETUP packet to DMA buffer
    memcpy(dma_buffer, setup, 8);
    // CRITICAL: Flush CPU cache so DMA controller sees the data!
    clean_data_cache_range((uintptr_t)dma_buffer, 8);
    dsb();

    // Clear all channel interrupts
    HCINT(ch) = 0xFFFFFFFF;

    // Enable interrupts for this channel
    HCINTMSK(ch) = HCINT_XFERCOMPL | HCINT_CHHLTD | HCINT_STALL |
                   HCINT_NAK | HCINT_ACK | HCINT_XACTERR | HCINT_BBLERR | HCINT_AHBERR;

    // Set DMA address (bus address)
    HCDMA(ch) = arm_to_bus(dma_buffer);
    dsb();

    // Configure channel (OUT direction for SETUP)
    HCCHAR(ch) = hcchar_base;
    dsb();

    // Transfer size: 8 bytes, 1 packet, SETUP PID
    HCTSIZ(ch) = 8 | (1 << HCTSIZ_PKTCNT_SHIFT) | (HCTSIZ_PID_SETUP << HCTSIZ_PID_SHIFT);
    dsb();

    usb_debug("[USB] SETUP: HCDMA=%08x HCCHAR=%08x HCTSIZ=%08x\n",
              HCDMA(ch), HCCHAR(ch), HCTSIZ(ch));

    // Enable channel to start the transfer
    HCCHAR(ch) = hcchar_base | HCCHAR_CHENA;
    dsb();

    // Wait for SETUP completion
    if (usb_wait_for_dma_complete(ch, 5) < 0) {
        usb_debug("[USB] SETUP failed\n");
        return -1;
    }
    usb_debug("[USB] SETUP complete\n");

    // ========== DATA Stage (if any) ==========
    int bytes_transferred = 0;

    if (data_len > 0 && data != NULL) {
        usb_debug("[USB] DATA stage (%d bytes, %s)...\n", data_len, data_in ? "IN" : "OUT");

        if (data_len > (int)sizeof(dma_buffer)) {
            usb_debug("[USB] Data too large for DMA buffer\n");
            return -1;
        }

        // Configure for data direction
        uint32_t data_hcchar = hcchar_base;
        if (data_in) {
            data_hcchar |= HCCHAR_EPDIR;  // IN
            // Clear DMA buffer for IN transfer
            memset(dma_buffer, 0, data_len);
            // Invalidate cache - ensures we don't hold stale lines that could
            // be evicted into the buffer while DMA is writing
            invalidate_data_cache_range((uintptr_t)dma_buffer, data_len);
        } else {
            // Copy data to DMA buffer for OUT transfer
            memcpy(dma_buffer, data, data_len);
            // Flush cache so DMA controller sees the data
            clean_data_cache_range((uintptr_t)dma_buffer, data_len);
        }
        dsb();

        // Calculate packet count
        int pkt_count = (data_len + mps - 1) / mps;
        if (pkt_count == 0) pkt_count = 1;

        // Clear interrupts
        HCINT(ch) = 0xFFFFFFFF;

        // Set DMA address
        HCDMA(ch) = arm_to_bus(dma_buffer);
        dsb();

        // Configure channel
        HCCHAR(ch) = data_hcchar;
        dsb();

        // Transfer size, packet count, DATA1 PID (first data after SETUP is always DATA1)
        HCTSIZ(ch) = data_len | (pkt_count << HCTSIZ_PKTCNT_SHIFT) |
                     (HCTSIZ_PID_DATA1 << HCTSIZ_PID_SHIFT);
        dsb();

        usb_debug("[USB] DATA: HCDMA=%08x HCCHAR=%08x HCTSIZ=%08x\n",
                  HCDMA(ch), HCCHAR(ch), HCTSIZ(ch));

        // Enable channel
        HCCHAR(ch) = data_hcchar | HCCHAR_CHENA;
        dsb();

        // Wait for completion
        if (usb_wait_for_dma_complete(ch, 10) < 0) {
            usb_debug("[USB] DATA stage failed\n");
            return -1;
        }

        if (data_in) {
            // Invalidate cache to ensure CPU reads fresh data from RAM
            invalidate_data_cache_range((uintptr_t)dma_buffer, data_len);

            // Copy received data from DMA buffer
            // Calculate actual bytes received from HCTSIZ
            uint32_t remaining = HCTSIZ(ch) & HCTSIZ_XFERSIZE_MASK;
            bytes_transferred = data_len - remaining;
            if (bytes_transferred > 0) {
                memcpy(data, dma_buffer, bytes_transferred);
            }
            usb_debug("[USB] DATA IN: received %d bytes\n", bytes_transferred);
        } else {
            bytes_transferred = data_len;
            usb_debug("[USB] DATA OUT: sent %d bytes\n", bytes_transferred);
        }
    }

    // ========== STATUS Stage ==========
    usb_debug("[USB] STATUS stage...\n");

    // Status is opposite direction of data (or IN if no data)
    int status_in = (data_len > 0) ? !data_in : 1;

    uint32_t status_hcchar = hcchar_base;
    if (status_in) {
        status_hcchar |= HCCHAR_EPDIR;
    }

    // Clear interrupts
    HCINT(ch) = 0xFFFFFFFF;

    // Set DMA address (zero-length, but still need valid address)
    HCDMA(ch) = arm_to_bus(dma_buffer);
    dsb();

    // Configure channel
    HCCHAR(ch) = status_hcchar;
    dsb();

    // Zero-length packet, DATA1 PID
    HCTSIZ(ch) = 0 | (1 << HCTSIZ_PKTCNT_SHIFT) | (HCTSIZ_PID_DATA1 << HCTSIZ_PID_SHIFT);
    dsb();

    usb_debug("[USB] STATUS: HCDMA=%08x HCCHAR=%08x HCTSIZ=%08x\n",
              HCDMA(ch), HCCHAR(ch), HCTSIZ(ch));

    // Enable channel
    HCCHAR(ch) = status_hcchar | HCCHAR_CHENA;
    dsb();

    // Wait for completion
    if (usb_wait_for_dma_complete(ch, 5) < 0) {
        usb_debug("[USB] STATUS failed\n");
        return -1;
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

// Forward declaration for recursive enumeration
static int usb_enumerate_device_at(int parent_addr, int port, int speed);

// Get hub descriptor
static int usb_get_hub_descriptor(int addr, usb_hub_descriptor_t *desc) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0xA0,  // Device to host, class, device
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DESC_HUB << 8) | 0,
        .wIndex = 0,
        .wLength = sizeof(usb_hub_descriptor_t)
    };

    return usb_control_transfer(addr, &setup, desc, sizeof(usb_hub_descriptor_t), 1);
}

// Get hub port status
static int usb_get_port_status(int hub_addr, int port, uint32_t *status) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0xA3,  // Device to host, class, other (port)
        .bRequest = USB_REQ_GET_PORT_STATUS,
        .wValue = 0,
        .wIndex = port,
        .wLength = 4
    };

    return usb_control_transfer(hub_addr, &setup, status, 4, 1);
}

// Set hub port feature
static int usb_set_port_feature(int hub_addr, int port, int feature) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x23,  // Host to device, class, other (port)
        .bRequest = USB_REQ_SET_PORT_FEATURE,
        .wValue = feature,
        .wIndex = port,
        .wLength = 0
    };

    return usb_control_transfer(hub_addr, &setup, NULL, 0, 0);
}

// Clear hub port feature
static int usb_clear_port_feature(int hub_addr, int port, int feature) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x23,  // Host to device, class, other (port)
        .bRequest = USB_REQ_CLEAR_PORT_FEATURE,
        .wValue = feature,
        .wIndex = port,
        .wLength = 0
    };

    return usb_control_transfer(hub_addr, &setup, NULL, 0, 0);
}

// Enumerate devices on a hub
static int usb_enumerate_hub(int hub_addr, int num_ports) {
    usb_info("[USB] Enumerating hub at addr %d with %d ports\n", hub_addr, num_ports);

    for (int port = 1; port <= num_ports; port++) {
        usb_debug("[USB] Hub port %d: powering on...\n", port);

        // Power on port
        if (usb_set_port_feature(hub_addr, port, USB_PORT_FEAT_POWER) < 0) {
            usb_debug("[USB] Failed to power on port %d\n", port);
            continue;
        }

        // Wait for power good (hub descriptor says how long in 2ms units)
        msleep(100);

        // Get port status
        uint32_t status = 0;
        if (usb_get_port_status(hub_addr, port, &status) < 0) {
            usb_debug("[USB] Failed to get port %d status\n", port);
            continue;
        }

        usb_debug("[USB] Port %d status: %08x\n", port, status);

        // Check if device connected
        if (!(status & USB_PORT_STAT_CONNECTION)) {
            usb_debug("[USB] Port %d: no device\n", port);
            continue;
        }

        usb_info("[USB] Port %d: device connected!\n", port);

        // Reset port
        if (usb_set_port_feature(hub_addr, port, USB_PORT_FEAT_RESET) < 0) {
            usb_debug("[USB] Failed to reset port %d\n", port);
            continue;
        }

        // Wait for reset to complete
        msleep(50);

        // Get port status again
        if (usb_get_port_status(hub_addr, port, &status) < 0) {
            usb_debug("[USB] Failed to get port %d status after reset\n", port);
            continue;
        }

        usb_debug("[USB] Port %d after reset: %08x\n", port, status);

        // Clear reset change
        usb_clear_port_feature(hub_addr, port, USB_PORT_FEAT_C_RESET);

        // Check if port is enabled
        if (!(status & USB_PORT_STAT_ENABLE)) {
            usb_debug("[USB] Port %d: not enabled after reset\n", port);
            continue;
        }

        // Determine device speed
        int speed = 1;  // Default Full Speed
        if (status & USB_PORT_STAT_LOW_SPEED) {
            speed = 2;  // Low Speed
        } else if (status & USB_PORT_STAT_HIGH_SPEED) {
            speed = 0;  // High Speed
        }

        const char *speed_names[] = {"High", "Full", "Low"};
        usb_debug("[USB] Port %d: %s speed device\n", port, speed_names[speed]);

        // Enumerate the device on this port
        msleep(10);  // Recovery time
        usb_enumerate_device_at(hub_addr, port, speed);
    }

    return 0;
}

// Enumerate a device (generic - works for root or hub port)
static int usb_enumerate_device_at(int parent_addr, int port, int speed) {
    usb_debug("[USB] Enumerating device (parent=%d, port=%d, speed=%d)...\n",
              parent_addr, port, speed);

    if (usb_state.num_devices >= MAX_USB_DEVICES) {
        usb_debug("[USB] Too many devices!\n");
        return -1;
    }

    // Get device descriptor at address 0
    usb_device_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));

    // Temporarily set speed for address 0 transfers
    int old_speed = usb_state.device_speed;
    usb_state.device_speed = speed;

    int ret = usb_get_device_descriptor(0, &desc);
    if (ret < 8) {
        usb_debug("[USB] Failed to get device descriptor (got %d bytes)\n", ret);
        usb_state.device_speed = old_speed;
        return -1;
    }

    usb_debug("[USB] Device descriptor: VID=%04x PID=%04x MaxPacket=%d\n",
              desc.idVendor, desc.idProduct, desc.bMaxPacketSize0);

    // Assign address
    int new_addr = ++usb_state.next_address;
    msleep(10);

    ret = usb_set_address(new_addr);
    if (ret < 0) {
        usb_debug("[USB] Failed to set address %d\n", new_addr);
        usb_state.device_speed = old_speed;
        return -1;
    }
    msleep(10);

    // Create device entry
    usb_device_t *dev = &usb_state.devices[usb_state.num_devices++];
    dev->address = new_addr;
    dev->speed = speed;
    dev->max_packet_size = desc.bMaxPacketSize0;
    dev->parent_hub = parent_addr;
    dev->parent_port = port;
    dev->is_hub = 0;
    dev->hub_ports = 0;

    // Get full device descriptor at new address
    ret = usb_get_device_descriptor(new_addr, &desc);
    if (ret < (int)sizeof(desc)) {
        usb_debug("[USB] Failed to get full device descriptor\n");
        return -1;
    }

    usb_debug("[USB] Device %d: USB%x.%x Class=%d VID=%04x PID=%04x\n",
              new_addr, desc.bcdUSB >> 8, (desc.bcdUSB >> 4) & 0xF,
              desc.bDeviceClass, desc.idVendor, desc.idProduct);

    // Get configuration descriptor
    uint8_t config_buf[256];
    ret = usb_get_configuration_descriptor(new_addr, config_buf, sizeof(config_buf));
    if (ret < 9) {
        usb_debug("[USB] Failed to get config descriptor\n");
        return -1;
    }

    usb_config_descriptor_t *config = (usb_config_descriptor_t *)config_buf;
    usb_debug("[USB] Config: %d interfaces, total length %d\n",
              config->bNumInterfaces, config->wTotalLength);

    // Check if this is a hub (device class or interface class)
    int is_hub = (desc.bDeviceClass == USB_CLASS_HUB);
    int found_keyboard = 0;
    int keyboard_ep = 0;
    int keyboard_mps = 8;
    int keyboard_interval = 10;
    int keyboard_interface = 0;

    // Parse interfaces
    int offset = config->bLength;
    while (offset < config->wTotalLength && offset < (int)sizeof(config_buf)) {
        uint8_t len = config_buf[offset];
        if (len == 0) break;

        uint8_t type = config_buf[offset + 1];

        if (type == USB_DESC_INTERFACE) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)&config_buf[offset];
            usb_debug("[USB] Interface %d: Class=%d SubClass=%d Protocol=%d\n",
                      iface->bInterfaceNumber, iface->bInterfaceClass,
                      iface->bInterfaceSubClass, iface->bInterfaceProtocol);

            if (iface->bInterfaceClass == USB_CLASS_HUB) {
                is_hub = 1;
            } else if (iface->bInterfaceClass == USB_CLASS_HID) {
                if (iface->bInterfaceProtocol == USB_HID_PROTOCOL_KEYBOARD) {
                    usb_info("[USB] Found HID boot keyboard!\n");
                    found_keyboard = 1;
                    keyboard_interface = iface->bInterfaceNumber;
                } else if (iface->bInterfaceProtocol == USB_HID_PROTOCOL_MOUSE) {
                    usb_debug("[USB] Found HID boot mouse\n");
                }
            }
        } else if (type == USB_DESC_ENDPOINT && found_keyboard && keyboard_ep == 0) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&config_buf[offset];
            if ((ep->bmAttributes & 0x03) == 3 && (ep->bEndpointAddress & 0x80)) {
                // Interrupt IN endpoint
                keyboard_ep = ep->bEndpointAddress & 0x0F;
                keyboard_mps = ep->wMaxPacketSize;
                keyboard_interval = ep->bInterval;
                usb_debug("[USB] Keyboard interrupt EP: %d, MPS=%d, interval=%d\n",
                          keyboard_ep, keyboard_mps, keyboard_interval);
            }
        }

        offset += len;
    }

    // Set configuration
    ret = usb_set_configuration(new_addr, config->bConfigurationValue);
    if (ret < 0) {
        usb_debug("[USB] Failed to set configuration\n");
        return -1;
    }

    usb_debug("[USB] Device %d configured!\n", new_addr);

    // Handle hub
    if (is_hub) {
        dev->is_hub = 1;

        // Get hub descriptor
        usb_hub_descriptor_t hub_desc;
        ret = usb_get_hub_descriptor(new_addr, &hub_desc);
        if (ret >= 7) {
            dev->hub_ports = hub_desc.bNbrPorts;
            usb_info("[USB] Hub has %d ports\n", hub_desc.bNbrPorts);

            // Enumerate downstream devices
            usb_enumerate_hub(new_addr, hub_desc.bNbrPorts);
        } else {
            usb_debug("[USB] Failed to get hub descriptor\n");
        }
    }

    // Save keyboard info and configure HID protocol
    if (found_keyboard && keyboard_ep > 0) {
        usb_state.keyboard_addr = new_addr;
        usb_state.keyboard_ep = keyboard_ep;
        usb_state.keyboard_mps = keyboard_mps;
        usb_state.keyboard_interval = keyboard_interval;

        // SET_PROTOCOL: Switch to Boot Protocol (0) for simple 8-byte reports
        // This is CRITICAL - without it, keyboard stays in Report Protocol mode
        usb_setup_packet_t set_protocol = {
            .bmRequestType = 0x21,  // Host to device, Class, Interface
            .bRequest = USB_HID_SET_PROTOCOL,
            .wValue = USB_HID_PROTOCOL_BOOT,  // 0 = Boot Protocol
            .wIndex = keyboard_interface,
            .wLength = 0
        };
        ret = usb_control_transfer(new_addr, &set_protocol, NULL, 0, 0);
        if (ret < 0) {
            usb_info("[USB] SET_PROTOCOL failed (may be OK for boot keyboards)\n");
        } else {
            usb_info("[USB] SET_PROTOCOL to Boot Protocol OK\n");
        }

        // SET_IDLE: Set idle rate to 0 (only report on change)
        // This reduces USB traffic - keyboard only sends data when key state changes
        usb_setup_packet_t set_idle = {
            .bmRequestType = 0x21,  // Host to device, Class, Interface
            .bRequest = USB_HID_SET_IDLE,
            .wValue = 0,  // Idle rate = 0 (indefinite)
            .wIndex = keyboard_interface,
            .wLength = 0
        };
        ret = usb_control_transfer(new_addr, &set_idle, NULL, 0, 0);
        if (ret < 0) {
            usb_debug("[USB] SET_IDLE failed (OK, not all keyboards support it)\n");
        } else {
            usb_debug("[USB] SET_IDLE OK\n");
        }

        usb_info("[USB] Keyboard ready at addr %d EP %d\n", new_addr, keyboard_ep);
    }

    return 0;
}

// Main enumeration entry point (for root device)
static int usb_enumerate_device(void) {
    usb_state.next_address = 0;
    usb_state.num_devices = 0;
    usb_state.keyboard_addr = 0;

    return usb_enumerate_device_at(0, 0, usb_state.device_speed);
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
    printf("[USB] Waiting 100ms for power stabilize...\n");
    msleep(100);
    printf("[USB] Power stabilized, starting core reset...\n");

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

    if (usb_state.keyboard_addr) {
        printf("[USB] Keyboard at address %d, endpoint %d\n",
               usb_state.keyboard_addr, usb_state.keyboard_ep);

        // Register USB interrupt handler for keyboard
        extern void hal_irq_register_handler(uint32_t irq, void (*handler)(void));
        extern void hal_irq_enable_irq(uint32_t irq);

        #define IRQ_VC_USB  17  // VideoCore bank1 IRQ 9

        // Clear any pending interrupts first
        GINTSTS = 0xFFFFFFFF;
        dsb();

        // Enable host channel 1 interrupts (keyboard channel)
        HAINTMSK = (1 << 1);
        dsb();

        // Register handler with Pi interrupt controller
        hal_irq_register_handler(IRQ_VC_USB, usb_irq_handler);
        hal_irq_enable_irq(IRQ_VC_USB);

        // NOW enable global interrupts in DWC2 (handler is ready)
        GAHBCFG = GAHBCFG_DMA_EN | GAHBCFG_GLBL_INTR_EN;
        dsb();

        printf("[USB] IRQ setup: IRQ=%d GAHBCFG=%08x GINTMSK=%08x HAINTMSK=%08x\n",
               IRQ_VC_USB, GAHBCFG, GINTMSK, HAINTMSK);

        // Start first keyboard transfer
        usb_start_keyboard_transfer();

        // Debug: check state after starting transfer
        printf("[USB] After start: GINTSTS=%08x HAINT=%08x HCCHAR(1)=%08x HCINT(1)=%08x\n",
               GINTSTS, HAINT, HCCHAR(1), HCINT(1));
    }

    return 0;
}

// DMA buffer for interrupt transfers (separate from control transfers)
// Interrupt transfer DMA buffer - 64-byte aligned for Cortex-A53 cache line
static uint8_t __attribute__((aligned(64))) intr_dma_buffer[64];

// Data toggle for interrupt endpoint
static int keyboard_data_toggle = 0;

#ifdef PI_DEBUG_MODE
// Debug counters for USB polling
static int usb_poll_count = 0;
static int usb_nak_count = 0;
static int usb_timeout_count = 0;
static int usb_success_count = 0;
static int usb_error_count = 0;
#endif

// Interrupt-driven keyboard state
static volatile uint8_t kbd_report_buf[8];
static volatile int kbd_report_ready = 0;
static volatile int kbd_transfer_pending = 0;
static volatile uint32_t kbd_last_frame = 0;  // Frame number of last transfer

// Port recovery state (set by IRQ, handled by timer)
static volatile int port_reset_pending = 0;
static volatile uint32_t port_reset_start_tick = 0;

// Forward declaration for ISR to call
static void usb_restart_keyboard_transfer(void);

// Debug: count interrupts
static volatile int usb_irq_count = 0;
static volatile int usb_kbd_irq_count = 0;
static volatile int usb_kbd_data_count = 0;
static volatile int usb_kbd_nak_count = 0;

// USB IRQ handler - called when DWC2 generates an interrupt
// PURELY interrupt-driven: SOF schedules polls, channel IRQs handle completions
static void usb_irq_handler(void) {
    uint32_t gintsts = GINTSTS;
    usb_irq_count++;

    // Debug: print first few interrupts and then periodically
    if (usb_irq_count <= 5 || usb_irq_count % 500 == 0) {
        printf("[USB-IRQ] #%d GINTSTS=%08x\n", usb_irq_count, gintsts);
    }

    // NOTE: SOF interrupt is now DISABLED - we use timer-based polling instead
    // This avoids 1000 interrupts/sec just for scheduling

    // Port interrupt - check what changed and react accordingly
    // WARNING: PRTENA is W1C - writing 1 DISABLES the port!
    if (gintsts & GINTSTS_PRTINT) {
        uint32_t hprt = HPRT0;
        printf("[USB-IRQ] Port interrupt! HPRT0=%08x\n", hprt);

        // Check what happened
        int port_enabled = (hprt & HPRT0_PRTENA) ? 1 : 0;
        int port_connected = (hprt & HPRT0_PRTCONNSTS) ? 1 : 0;
        int enable_changed = (hprt & HPRT0_PRTENCHNG) ? 1 : 0;
        int connect_changed = (hprt & HPRT0_PRTCONNDET) ? 1 : 0;

        printf("[USB-IRQ] connected=%d enabled=%d conn_chg=%d ena_chg=%d\n",
               port_connected, port_enabled, connect_changed, enable_changed);

        // Clear W1C status bits (but NOT PRTENA!)
        uint32_t hprt_write = hprt & ~HPRT0_PRTENA;
        HPRT0 = hprt_write;
        dsb();

        // React to port changes
        if (enable_changed && !port_enabled && port_connected) {
            // Port got disabled but device still connected - need to re-reset!
            printf("[USB-IRQ] Port disabled! Scheduling re-reset...\n");

            // Assert reset
            hprt = HPRT0;
            hprt &= ~(HPRT0_PRTENA | HPRT0_PRTCONNDET | HPRT0_PRTENCHNG | HPRT0_PRTOVRCURRCHNG);
            hprt |= HPRT0_PRTRST;
            HPRT0 = hprt;
            dsb();

            // Set flag for timer to complete the reset (can't block 50ms in IRQ)
            port_reset_pending = 1;
            port_reset_start_tick = 0;  // Will be set by timer
            kbd_transfer_pending = 0;   // Stop keyboard polling during reset
        }

        if (connect_changed && !port_connected) {
            // Device disconnected
            printf("[USB-IRQ] Device disconnected!\n");
            usb_state.device_connected = 0;
            usb_state.keyboard_addr = 0;
            kbd_transfer_pending = 0;
        }

        printf("[USB-IRQ] After handling: HPRT0=%08x\n", HPRT0);
    }

    // Host channel interrupt
    if (gintsts & GINTSTS_HCHINT) {
        uint32_t haint = HAINT;

        if (usb_irq_count <= 10) {
            printf("[USB-IRQ] Channel interrupt! HAINT=%08x\n", haint);
        }

        for (int ch = 0; ch < 16; ch++) {
            if (haint & (1 << ch)) {
                uint32_t hcint = HCINT(ch);

                // Channel 1 = keyboard interrupt transfers
                // PURELY interrupt-driven: we just handle completion here
                // SOF interrupt schedules the next poll - NO immediate restart
                if (ch == 1 && usb_state.keyboard_addr != 0) {
                    usb_kbd_irq_count++;
                    if (usb_kbd_irq_count <= 10 || usb_kbd_irq_count % 500 == 0) {
                        printf("[USB-IRQ] KBD ch1 #%d HCINT=%08x\n",
                               usb_kbd_irq_count, hcint);
                    }

                    if (hcint & HCINT_XFERCOMPL) {
                        // Transfer complete with data
                        keyboard_data_toggle = !keyboard_data_toggle;

                        // CRITICAL: Invalidate cache to read fresh DMA data
                        invalidate_data_cache_range((uintptr_t)intr_dma_buffer, 8);

                        uint32_t remaining = HCTSIZ(1) & HCTSIZ_XFERSIZE_MASK;
                        int received = 8 - remaining;
                        if (received > 0) {
                            memcpy((void*)kbd_report_buf, intr_dma_buffer, 8);
                            kbd_report_ready = 1;
                            usb_kbd_data_count++;
                            printf("[USB-IRQ] KBD DATA! %d bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                   received,
                                   intr_dma_buffer[0], intr_dma_buffer[1], intr_dma_buffer[2], intr_dma_buffer[3],
                                   intr_dma_buffer[4], intr_dma_buffer[5], intr_dma_buffer[6], intr_dma_buffer[7]);
                        }
                    }
                    else if ((hcint & HCINT_CHHLTD) && (hcint & HCINT_ACK)) {
                        // Got ACK with halt - data received
                        keyboard_data_toggle = !keyboard_data_toggle;

                        // CRITICAL: Invalidate cache to read fresh DMA data
                        invalidate_data_cache_range((uintptr_t)intr_dma_buffer, 8);

                        uint32_t remaining = HCTSIZ(1) & HCTSIZ_XFERSIZE_MASK;
                        int received = 8 - remaining;
                        if (received > 0) {
                            memcpy((void*)kbd_report_buf, intr_dma_buffer, 8);
                            kbd_report_ready = 1;
                            usb_kbd_data_count++;
                            printf("[USB-IRQ] KBD DATA! %d bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                   received,
                                   intr_dma_buffer[0], intr_dma_buffer[1], intr_dma_buffer[2], intr_dma_buffer[3],
                                   intr_dma_buffer[4], intr_dma_buffer[5], intr_dma_buffer[6], intr_dma_buffer[7]);
                        }
                    }
                    else if (hcint & HCINT_NAK) {
                        // NAK = no data available (normal for HID when no key pressed)
                        // Don't restart here - timer tick will schedule the next poll
                        usb_kbd_nak_count++;
                        if (usb_kbd_nak_count <= 5 || usb_kbd_nak_count % 500 == 0) {
                            printf("[USB-IRQ] KBD NAK #%d\n", usb_kbd_nak_count);
                        }
                    }
                    else if (hcint & (HCINT_STALL | HCINT_XACTERR | HCINT_BBLERR)) {
                        // Error - log it
                        printf("[USB-IRQ] KBD ERROR HCINT=%08x\n", hcint);
                    }
                    // Note: CHHLTD alone (without NAK/ACK/XFERCOMPL) can happen - just means halt

                    // Mark transfer complete - timer tick will schedule the next one
                    kbd_transfer_pending = 0;
                    kbd_last_frame = HFNUM & 0xFFFF;

                    // Clear channel interrupt
                    HCINT(ch) = 0xFFFFFFFF;
                    continue;  // Skip the HCINT clear below
                }

                // Clear this channel's interrupts (for non-keyboard channels)
                HCINT(ch) = 0xFFFFFFFF;
            }
        }
    }

    // Clear global interrupt status
    GINTSTS = gintsts;
}

// Debug counter for restart
static volatile int usb_restart_count = 0;

// Internal: configure and start a keyboard transfer on channel 1
// Called from both initial start and ISR restart
static void usb_do_keyboard_transfer(void) {
    int ch = 1;
    int ep = usb_state.keyboard_ep;
    int addr = usb_state.keyboard_addr;

    usb_restart_count++;

    // Check if channel is still enabled (shouldn't be!)
    uint32_t old_hcchar = HCCHAR(ch);
    if (old_hcchar & HCCHAR_CHENA) {
        printf("[USB-XFER] ERROR: Channel still enabled! HCCHAR=%08x\n", old_hcchar);
        return;
    }

    kbd_transfer_pending = 1;

    // Configure channel for interrupt IN endpoint
    uint32_t mps = 64;  // Full speed max
    uint32_t hcchar = (mps & HCCHAR_MPS_MASK) |
                      (ep << HCCHAR_EPNUM_SHIFT) |
                      HCCHAR_EPDIR |                              // IN direction
                      (HCCHAR_EPTYPE_INTR << HCCHAR_EPTYPE_SHIFT) |
                      (addr << HCCHAR_DEVADDR_SHIFT) |
                      (1 << HCCHAR_MC_SHIFT);

    // Odd/even frame scheduling
    uint32_t fnum = HFNUM & 0xFFFF;
    if (fnum & 1) {
        hcchar |= HCCHAR_ODDFRM;
    }

    // Clear DMA buffer and invalidate cache for receive
    memset(intr_dma_buffer, 0, 8);
    // CRITICAL: Invalidate cache so DMA writes go directly to RAM
    invalidate_data_cache_range((uintptr_t)intr_dma_buffer, 8);
    dsb();

    // Configure channel interrupts
    // Only enable CHHLTD (channel halted) - we check HCINT for details
    // This minimizes interrupts: one per transfer, not one per NAK
    HCINT(ch) = 0xFFFFFFFF;
    HCINTMSK(ch) = HCINT_CHHLTD | HCINT_XACTERR | HCINT_BBLERR;
    HCDMA(ch) = arm_to_bus(intr_dma_buffer);
    HCCHAR(ch) = hcchar;

    // Transfer size: 8 bytes, 1 packet, DATA0/DATA1 toggle
    uint32_t pid = keyboard_data_toggle ? HCTSIZ_PID_DATA1 : HCTSIZ_PID_DATA0;
    HCTSIZ(ch) = 8 | (1 << HCTSIZ_PKTCNT_SHIFT) | (pid << HCTSIZ_PID_SHIFT);
    dsb();

    // Record frame number for throttling
    kbd_last_frame = HFNUM & 0xFFFF;

    // Enable channel - transfer starts, interrupt fires on completion
    HCCHAR(ch) = hcchar | HCCHAR_CHENA;
    dsb();

    if (usb_restart_count <= 5) {
        printf("[USB-XFER] #%d started: HCCHAR=%08x HCINT=%08x frame=%d\n",
               usb_restart_count, HCCHAR(ch), HCINT(ch), kbd_last_frame);
    }
}

// Called from ISR to restart transfer (channel already halted)
static void usb_restart_keyboard_transfer(void) {
    usb_do_keyboard_transfer();
}

// Called from init to start the first transfer
void usb_start_keyboard_transfer(void) {
    if (kbd_transfer_pending) {
        printf("[USB] start_keyboard_transfer: already pending\n");
        return;
    }
    if (usb_state.keyboard_addr == 0) {
        printf("[USB] start_keyboard_transfer: no keyboard\n");
        return;
    }

    // If channel is still active, request disable (shouldn't happen normally)
    if (HCCHAR(1) & HCCHAR_CHENA) {
        printf("[USB] start_keyboard_transfer: channel active, disabling\n");
        HCCHAR(1) |= HCCHAR_CHDIS;
        dsb();
        return;  // Will be restarted by ISR when halt completes
    }

    printf("[USB] Starting first keyboard transfer! addr=%d ep=%d\n",
           usb_state.keyboard_addr, usb_state.keyboard_ep);
    usb_do_keyboard_transfer();
    printf("[USB] First transfer started, pending=%d\n", kbd_transfer_pending);
}

// Called from timer tick (every 10ms) to schedule keyboard polls
// This replaces SOF-based polling which was too expensive (1000 IRQs/sec)
// Also handles port reset recovery
static uint32_t tick_counter = 0;

void hal_usb_keyboard_tick(void) {
    tick_counter++;

    // Handle port reset recovery (set by port IRQ)
    if (port_reset_pending) {
        if (port_reset_start_tick == 0) {
            // First tick after reset asserted - record start time
            port_reset_start_tick = tick_counter;
            printf("[USB-TICK] Port reset started at tick %u\n", tick_counter);
            return;
        }

        // Wait 5 ticks (50ms) then de-assert reset
        if (tick_counter - port_reset_start_tick >= 5) {
            printf("[USB-TICK] De-asserting port reset...\n");

            uint32_t hprt = HPRT0;
            hprt &= ~(HPRT0_PRTENA | HPRT0_PRTCONNDET | HPRT0_PRTENCHNG | HPRT0_PRTOVRCURRCHNG);
            hprt &= ~HPRT0_PRTRST;  // De-assert reset
            HPRT0 = hprt;
            dsb();

            // Wait for port to become enabled (check in future ticks)
            port_reset_pending = 2;  // Phase 2: waiting for enable
            port_reset_start_tick = tick_counter;
        }
        return;
    }

    // Phase 2: Wait for port to enable after reset
    if (port_reset_pending == 2) {
        uint32_t hprt = HPRT0;
        if (hprt & HPRT0_PRTENA) {
            printf("[USB-TICK] Port re-enabled! HPRT0=%08x\n", hprt);
            port_reset_pending = 0;
            // Note: Would need to re-enumerate device here for full recovery
            // For now, just resume polling - device should still respond
        } else if (tick_counter - port_reset_start_tick >= 10) {
            // Timeout - port didn't enable
            printf("[USB-TICK] Port enable timeout! HPRT0=%08x\n", hprt);
            port_reset_pending = 0;
        }
        return;
    }

    // Normal keyboard polling
    if (!usb_state.initialized || !usb_state.device_connected) {
        return;
    }
    if (usb_state.keyboard_addr == 0) {
        return;
    }
    if (kbd_transfer_pending) {
        return;  // Transfer in progress, wait for it to complete
    }

    // Channel should be idle - if not, something is wrong
    if (HCCHAR(1) & HCCHAR_CHENA) {
        // Channel still active but not pending - force disable
        HCCHAR(1) |= HCCHAR_CHDIS;
        dsb();
        return;
    }

    // Start a new transfer
    usb_do_keyboard_transfer();
}

// Interrupt transfer for HID reports (legacy polling version - keep for control transfers)
static int usb_interrupt_transfer(int device_addr, int ep, void *data, int data_len, int dev_speed) {
    int ch = 1;  // Use channel 1 for interrupt transfers

    // Halt channel if active
    usb_halt_channel(ch);

    // Clear all channel interrupts
    HCINT(ch) = 0xFFFFFFFF;

    // Enable interrupts for this channel
    HCINTMSK(ch) = HCINT_XFERCOMPL | HCINT_CHHLTD | HCINT_STALL |
                   HCINT_NAK | HCINT_ACK | HCINT_XACTERR | HCINT_BBLERR | HCINT_AHBERR;

    // Configure channel for interrupt IN endpoint
    uint32_t mps = (data_len < 64) ? data_len : 64;

    uint32_t hcchar = (mps & HCCHAR_MPS_MASK) |
                      (ep << HCCHAR_EPNUM_SHIFT) |
                      HCCHAR_EPDIR |                              // IN direction
                      (HCCHAR_EPTYPE_INTR << HCCHAR_EPTYPE_SHIFT) |
                      (device_addr << HCCHAR_DEVADDR_SHIFT) |
                      (1 << HCCHAR_MC_SHIFT);                     // 1 transaction per frame

    if (dev_speed == 2) {  // Low-speed
        hcchar |= HCCHAR_LSDEV;
    }

    // Use odd/even frame for interrupt scheduling
    uint32_t fnum = HFNUM & 0xFFFF;
    if (fnum & 1) {
        hcchar |= HCCHAR_ODDFRM;
    }

    // Clear DMA buffer and invalidate cache for receive
    memset(intr_dma_buffer, 0, data_len);
    invalidate_data_cache_range((uintptr_t)intr_dma_buffer, data_len);
    dsb();

    // Set DMA address
    HCDMA(ch) = arm_to_bus(intr_dma_buffer);
    dsb();

    // Configure channel
    HCCHAR(ch) = hcchar;
    dsb();

    // Transfer size, 1 packet, alternating DATA0/DATA1
    uint32_t pid = keyboard_data_toggle ? HCTSIZ_PID_DATA1 : HCTSIZ_PID_DATA0;
    HCTSIZ(ch) = data_len | (1 << HCTSIZ_PKTCNT_SHIFT) | (pid << HCTSIZ_PID_SHIFT);
    dsb();

    // Enable channel
    HCCHAR(ch) = hcchar | HCCHAR_CHENA;
    dsb();

    // Wait for completion (short timeout since interrupt transfers should be quick)
    int timeout = 10000;
    while (timeout--) {
        uint32_t hcint = HCINT(ch);

        if (hcint & HCINT_XFERCOMPL) {
            // Success - toggle data toggle for next transfer
            keyboard_data_toggle = !keyboard_data_toggle;
            HCINT(ch) = 0xFFFFFFFF;

            // Invalidate cache to read fresh DMA data
            invalidate_data_cache_range((uintptr_t)intr_dma_buffer, data_len);

            // Copy data out
            uint32_t remaining = HCTSIZ(ch) & HCTSIZ_XFERSIZE_MASK;
            int received = data_len - remaining;
            if (received > 0) {
                memcpy(data, intr_dma_buffer, received);
            }
#ifdef PI_DEBUG_MODE
            usb_success_count++;
#endif
            return received;
        }
        if (hcint & HCINT_CHHLTD) {
            if (hcint & HCINT_ACK) {
                // Got ACK - data received
                keyboard_data_toggle = !keyboard_data_toggle;
                HCINT(ch) = 0xFFFFFFFF;

                // Invalidate cache to read fresh DMA data
                invalidate_data_cache_range((uintptr_t)intr_dma_buffer, data_len);

                uint32_t remaining = HCTSIZ(ch) & HCTSIZ_XFERSIZE_MASK;
                int received = data_len - remaining;
                if (received > 0) {
                    memcpy(data, intr_dma_buffer, received);
                }
#ifdef PI_DEBUG_MODE
                usb_success_count++;
#endif
                return received;
            }
            if (hcint & HCINT_NAK) {
                // NAK = no data available, not an error
                HCINT(ch) = 0xFFFFFFFF;
#ifdef PI_DEBUG_MODE
                usb_nak_count++;
#endif
                return 0;
            }
            // Other halt reason
#ifdef PI_DEBUG_MODE
            usb_error_count++;
            printf("[USB] HCINT error: 0x%08x\n", hcint);
#endif
            HCINT(ch) = 0xFFFFFFFF;
            return -1;
        }
        if (hcint & HCINT_NAK) {
            // NAK without halt - just means no data
            HCINT(ch) = 0xFFFFFFFF;
#ifdef PI_DEBUG_MODE
            usb_nak_count++;
#endif
            return 0;
        }
        if (hcint & (HCINT_STALL | HCINT_XACTERR | HCINT_BBLERR | HCINT_AHBERR)) {
#ifdef PI_DEBUG_MODE
            usb_error_count++;
            printf("[USB] Error HCINT: 0x%08x (STALL=%d XACTERR=%d BBLERR=%d AHBERR=%d)\n",
                   hcint,
                   (hcint & HCINT_STALL) ? 1 : 0,
                   (hcint & HCINT_XACTERR) ? 1 : 0,
                   (hcint & HCINT_BBLERR) ? 1 : 0,
                   (hcint & HCINT_AHBERR) ? 1 : 0);
#endif
            HCINT(ch) = 0xFFFFFFFF;
            return -1;
        }

        usleep(1);
    }

    // Timeout - halt channel
#ifdef PI_DEBUG_MODE
    usb_timeout_count++;
#endif
    usb_halt_channel(ch);
    return -1;
}

// Poll keyboard for HID report - PURE interrupt-driven, NO polling
// The ISR populates kbd_report_buf and sets kbd_report_ready
// This function ONLY checks if data is ready - it does NOT poll hardware
int hal_usb_keyboard_poll(uint8_t *report, int report_len) {
    if (!usb_state.initialized || !usb_state.device_connected) {
        return -1;
    }

    if (usb_state.keyboard_addr == 0) {
        return -1;
    }

    // Just check if ISR has delivered data
    if (kbd_report_ready) {
        kbd_report_ready = 0;
        int len = (report_len < 8) ? report_len : 8;
        memcpy(report, (void*)kbd_report_buf, len);
        return len;
    }

    return 0;  // No data available
}

#ifdef PI_DEBUG_MODE
// USB Keyboard Debug Loop - for debugging USB keyboard on real hardware
void usb_keyboard_debug_loop(void) {
    printf("[DEBUG] USB Keyboard Debug Loop\n");
    printf("[DEBUG] Keyboard: addr=%d EP=%d MPS=%d\n",
           usb_state.keyboard_addr, usb_state.keyboard_ep,
           usb_state.keyboard_mps);

    if (usb_state.keyboard_addr == 0) {
        printf("[DEBUG] ERROR: No keyboard detected!\n");
        printf("[DEBUG] Hanging...\n");
        while (1) {
            asm volatile("wfi");
        }
    }

    printf("[DEBUG] Press keys - watching for HID reports...\n");
    printf("[DEBUG] Legend: . = poll, [HID] = data received\n\n");

    uint8_t report[8];
    uint8_t prev[8] = {0};
    int loop = 0;

    while (1) {
        usb_poll_count++;
        int ret = hal_usb_keyboard_poll(report, 8);
        loop++;

        // Print status every 1000 polls
        if (loop % 1000 == 0) {
            printf(".");  // Show we're alive
        }

        // Print detailed stats every 10000 polls
        if (loop % 10000 == 0) {
            printf("\n[STATS] Poll=%d NAK=%d TO=%d OK=%d ERR=%d\n",
                   usb_poll_count, usb_nak_count, usb_timeout_count,
                   usb_success_count, usb_error_count);
        }

        if (ret > 0) {
            // Got data! Print hex dump
            printf("\n[HID] Got %d bytes: ", ret);
            for (int i = 0; i < 8; i++) {
                printf("%02x ", report[i]);
            }
            printf("\n");

            // Decode modifiers
            if (report[0]) {
                printf("  Mods: ");
                if (report[0] & 0x22) printf("SHIFT ");
                if (report[0] & 0x11) printf("CTRL ");
                if (report[0] & 0x44) printf("ALT ");
                if (report[0] & 0x88) printf("GUI ");
                printf("\n");
            }

            // Show keycodes
            for (int i = 2; i < 8; i++) {
                if (report[i]) {
                    printf("  Key[%d]: 0x%02x", i-2, report[i]);
                    // Common HID codes
                    if (report[i] >= 0x04 && report[i] <= 0x1D) {
                        printf(" (%c)", 'a' + report[i] - 0x04);
                    } else if (report[i] == 0x28) {
                        printf(" (Enter)");
                    } else if (report[i] == 0x2C) {
                        printf(" (Space)");
                    } else if (report[i] == 0x2A) {
                        printf(" (Backspace)");
                    }
                    printf("\n");
                }
            }
            memcpy(prev, report, 8);
        } else if (ret < 0) {
            printf("\n[USB] Transfer error: ret=%d\n", ret);
        }
        // ret == 0 means NAK (no data) - normal, don't print

        // Small delay between polls (roughly 10ms)
        for (volatile int i = 0; i < 100000; i++);
    }
}
#endif
