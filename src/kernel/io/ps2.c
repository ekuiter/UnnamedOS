/*
 * PS/2 Controller - handles PS/2 keyboard, PS/2 mouse, speaker and reboots
 *
 * http://wiki.osdev.org/%228042%22_PS/2_Controller
 * http://wiki.osdev.org/Keyboard_Controller
 * http://www.lowlevel.eu/wiki/Keyboard_Controller
 * http://wiki.osdev.org/PS/2_Mouse
 * http://wiki.osdev.org/Mouse_Input
 * http://lowlevel.eu/wiki/PS/2-Maus
 * 
 */

#include <common.h>
#include <io/ps2.h>

// the controller's I/O ports
#define PS2_DATA           0x60 // write device commands, read device/PS/2 results
#define PS2_CMD            0x64 // write PS/2 commands, read ps2_status_t

// device port indication (see header file)
#define ANY_PORT              0 // don't care about the port used

// PS/2 commands
#define TEST_PS2           0xAA
#define READ_CONFIG        0x20 // see ps2_config_t
#define WRITE_CONFIG       0x60
#define READ_OUTPUT_PORT   0xD0 // see ps2_output_port_t
#define WRITE_OUTPUT_PORT  0xD1
#define DISABLE_PORT_1     0xAD
#define DISABLE_PORT_2     0xA7
#define ENABLE_PORT_1      0xAE
#define ENABLE_PORT_2      0xA8
#define TEST_PORT_1        0xAB
#define TEST_PORT_2        0xA9
#define SEND_TO_PORT_2     0xD4 // indicates we want to send to port 2 via PS2_DATA
#define PULSE_OUTPUT_LINES(b3, b2, b1, b0) /* used to reset the CPU */ \
    (0xF0 + !b3 * 0b1000 + !b2 * 0b100 + !b1 * 0b10 + !b0 * 0b1)

// PS/2 results
#define TEST_PS2_PASSED    0x55
#define TEST_PORT_PASSED   0x00

// device commands
#define DEVICE_RESET       0xFF
#define DEVICE_ENABLE      0xF4
#define DEVICE_DISABLE     0xF5
#define DEVICE_IDENTIFY    0xF2

// device results
#define DEVICE_ACK         0xFA
#define DEVICE_TEST_PASSED 0xAA

typedef union {
    struct {
        uint8_t outbuf_full  : 1; // set if a PS/2 device sent data
        uint8_t inbuf_full   : 1; // clear if we are allowed to send data
        uint8_t system       : 1; // set if system passed POST tests
        uint8_t cmd_data     : 1; // whether sent value is data or command
        uint8_t reserved     : 1;
        uint8_t outbuf_port2 : 1; // whether port 2 sent data
        uint8_t timeout_err  : 1;
        uint8_t parity_err   : 1;
    } bits;
    uint8_t byte;
} ps2_status_t;

typedef union {
    struct {
        uint8_t port1_intr   : 1; // whether port 1 fires IRQ1
        uint8_t port2_intr   : 1; // whether port 2 fires IRQ12
        uint8_t system       : 1; // set if system passed POST tests
        uint8_t reserved     : 1;
        uint8_t port1_clock  : 1; // whether port 1 clock is disabled
        uint8_t port2_clock  : 1; // whether port 2 clock is disabled
        uint8_t port1_transl : 1; // whether port 1 translates scancode sets
        uint8_t reserved2    : 1;
    } bits;
    uint8_t byte;
} ps2_config_t;

typedef union {
    struct {
        uint8_t reset        : 1; // whether port 1 fires IRQ1
        uint8_t a20_gate     : 1; // whether port 2 fires IRQ12
        uint8_t reserved     : 2;
        uint8_t outbuf_port1 : 1; // whether port 1 sent data
        uint8_t outbuf_port2 : 1; // whether port 2 sent data
        uint8_t reserved2    : 2;
    } bits;
    uint8_t byte;
} ps2_output_port_t;

static uint8_t port2_supported = 1; // let's assume that port 2 (IRQ12) is supported

static inline uint8_t ps2_ready() {
    return !((ps2_status_t) inb(PS2_CMD)).bits.inbuf_full;
}

static inline int8_t ps2_available(uint8_t port) {
    ps2_status_t status = (ps2_status_t) inb(PS2_CMD);
    if (port == ANY_PORT) // whether data is ready for us to fetch (on any port)
        return status.bits.outbuf_full;
    if (port == PS2_PORT_1) // we are only interested in data sent from port 1
        return status.bits.outbuf_full && !status.bits.outbuf_port2;
    if (port == PS2_PORT_2) // or port 2
        return status.bits.outbuf_full && status.bits.outbuf_port2;
    return -1;
}

static inline void ps2_write(uint8_t io_port, uint8_t command) {
    while (!ps2_ready()); // wait until PS/2 controller accepts input
    outb(io_port, command);
}

static inline uint8_t ps2_read(uint8_t port) {
    while (!ps2_available(port)); // wait until PS/2 controller holds data for port
    return inb(PS2_DATA);
}

static inline uint8_t ps2_valid_port(uint8_t port) {
    if ((port != PS2_PORT_1 && port != PS2_PORT_2) || (port == PS2_PORT_2 && !port2_supported)) {
        println("%4aPS/2 port %d not supported%a", port);
        return 0;
    }
    return 1;
}

static void ps2_flush() {
    while (ps2_available(ANY_PORT))
        ps2_read(ANY_PORT); // discard any data stuck in PS/2 controller
}

static ps2_config_t ps2_read_config() {
    ps2_write(PS2_CMD, READ_CONFIG);
    return (ps2_config_t) ps2_read(ANY_PORT);
}

static void ps2_write_config(ps2_config_t config) {
    ps2_write(PS2_CMD, WRITE_CONFIG);
    ps2_write(PS2_DATA, config.byte);
}

static uint8_t ps2_test(uint8_t test_command, uint8_t test_result, const char* name) {
    ps2_write(PS2_CMD, test_command);
    if (ps2_read(ANY_PORT) != test_result) {
        println("%4afail%a. %s test failed.", name);
        return 0;
    }
    return 1;
}

static void ps2_reset_device(uint8_t port) {
    ps2_write_device(port, DEVICE_RESET);
    while (ps2_read_device(port) != DEVICE_TEST_PASSED); // TODO: might be an infinite loop!
}

static void ps2_detect_device(uint8_t port) {
    ps2_write_device(port, DEVICE_DISABLE);
    while (ps2_read_device(port) != DEVICE_ACK); // TODO: see above
    ps2_write_device(port, DEVICE_IDENTIFY);
    while (ps2_read_device(port) != DEVICE_ACK); // TODO: see above
    //while (1) println("%02x", ps2_read_device(port)); // TODO: add timeout
    ps2_write_device(port, DEVICE_ENABLE);
    while (ps2_read_device(port) != DEVICE_ACK); // TODO: see above
}

void ps2_init() {
    print("PS/2 init ... ");
    // We assume the PS/2 controller exists and USB Legacy Support is still active.
    
    ps2_write(PS2_CMD, DISABLE_PORT_1); // start by disabling devices,
    ps2_write(PS2_CMD, DISABLE_PORT_2); // preventing interference
    ps2_flush();
    
    ps2_config_t config = ps2_read_config();
    if (!config.bits.port2_clock) // ought to be 1 because we disabled port 2 above
        port2_supported = 0;
    // disable IRQ1, IRQ12 (for now) and scancode translation
    config.bits.port1_intr = config.bits.port2_intr = config.bits.port1_transl = 0;
    ps2_write_config(config);
    
    if (!ps2_test(TEST_PS2, TEST_PS2_PASSED, "PS/2")) return;
    
    if (port2_supported) {
        // we are not sure yet if port 2 is supported,
        ps2_write(PS2_CMD, ENABLE_PORT_2); // so let's find out by enabling it
        ps2_config_t config = ps2_read_config();
        if (config.bits.port2_clock) // ought to be 0 because we just enabled port 2
            port2_supported = 0;
        else // we know for sure port 2 is supported, so let's disable it again
            ps2_write(PS2_CMD, DISABLE_PORT_2);
    }
    
    if (                   !ps2_test(TEST_PORT_1, TEST_PORT_PASSED, "Port 1")) return;
    if (port2_supported && !ps2_test(TEST_PORT_2, TEST_PORT_PASSED, "Port 2")) return;
    
    config = ps2_read_config(); // initialization is finished,
    ps2_write(PS2_CMD, ENABLE_PORT_1); // so we enable devices and IRQs again
    config.bits.port1_intr = 1;
    if (port2_supported) {
        ps2_write(PS2_CMD, ENABLE_PORT_2);
        config.bits.port2_intr = 1;
    }
    ps2_write_config(config);
    
                         ps2_reset_device(PS2_PORT_1);
    if (port2_supported) ps2_reset_device(PS2_PORT_2);
    ps2_flush(); // clear output buffers (most likely mouse ID)
    
    ps2_detect_device(PS2_PORT_1);
    ps2_detect_device(PS2_PORT_2);
    // TODO: start appropriate drivers
    
    println("%2aok%a. Dual-channel %s.", port2_supported ? "supported" : "not supported");
}

void ps2_write_device(uint8_t port, uint8_t command) {
    if (!ps2_valid_port(port)) return;
    if (port == PS2_PORT_2)
        ps2_write(PS2_CMD, SEND_TO_PORT_2);
    ps2_write(PS2_DATA, command);
}

uint8_t ps2_read_device(uint8_t port) {
    if (!ps2_valid_port(port)) return 0;
    return ps2_read(port);
}

void ps2_reboot() {
    // pulse output port bit 0 which triggers a system reboot
    ps2_write(PS2_CMD, PULSE_OUTPUT_LINES(0, 0, 0, 1));
}
