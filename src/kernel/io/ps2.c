/*
 * PS/2 Controller - handles PS/2 keyboard, PS/2 mouse, speaker and reboots
 *
 * http://wiki.osdev.org/%228042%22_PS/2_Controller
 * http://wiki.osdev.org/Keyboard_Controller
 * http://www.lowlevel.eu/wiki/Keyboard_Controller
 */

#include <common.h>
#include <io/ps2.h>
#include <intr/pit.h>
#include <io/keyboard.h>
#include <io/mouse.h>

// the controller's I/O ports
#define PS2_DATA           0x60 // write device commands, read device/PS/2 results
#define PS2_CMD            0x64 // write PS/2 commands, read ps2_status_t

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
#define DEVICE_IDENTIFY    0xF2
#define DEVICE_ENABLE      0xF4
#define DEVICE_DISABLE     0xF5

// device results
#define DEVICE_ACK         0xFA
#define DEVICE_TEST_PASSED 0xAA

#define PS2_TIMEOUT        1000
#define HAS_PORT1             1 // every PS/2 controller has port 1, but port 2
#define HAS_PORT2          port2_supported // needs to be detected first

typedef union {
    struct {
        uint8_t outbuf_full  : 1; // set if a PS/2 device sent data
        uint8_t inbuf_full   : 1; // clear if we are allowed to send data
        uint8_t system       : 1; // set if system passed POST tests
        uint8_t cmd_data     : 1; // whether sent value is data or command
        uint8_t              : 1;
        uint8_t outbuf_port2 : 1; // whether port 2 sent data
        uint8_t timeout_err  : 1;
        uint8_t parity_err   : 1;
    } __attribute__((packed)) bits;
    uint8_t byte;
} ps2_status_t;

typedef union {
    struct {
        uint8_t reset        : 1; // whether port 1 fires IRQ1
        uint8_t a20_gate     : 1; // whether port 2 fires IRQ12
        uint8_t              : 2;
        uint8_t outbuf_port1 : 1; // whether port 1 sent data
        uint8_t outbuf_port2 : 1; // whether port 2 sent data
        uint8_t              : 2;
    } __attribute__((packed)) bits;
    uint8_t byte;
} ps2_output_port_t;

static uint8_t init_done = 0;
static uint8_t port2_supported = 1; // let's assume that port 2 (IRQ12) is supported
static ps2_error_t err = 0; // timeout error (we often ignore this)

static inline uint8_t ps2_ready() {
    return !((ps2_status_t) inb(PS2_CMD)).bits.inbuf_full;
}

static inline int8_t ps2_available(ps2_port_t port) {
    ps2_status_t status = (ps2_status_t) inb(PS2_CMD);
    if (port == PS2_ANY_PORT) // whether data is ready for us to fetch (on any port)
        return status.bits.outbuf_full;
    if (port == PS2_PORT_1) // we are only interested in data sent from port 1
        return status.bits.outbuf_full && !status.bits.outbuf_port2;
    if (port == PS2_PORT_2) // or port 2
        return status.bits.outbuf_full && status.bits.outbuf_port2;
    return -1;
}

static inline uint8_t ps2_write(uint8_t io_port, uint8_t command) {
    pit_timeout_t timeout = pit_make_timeout(PS2_TIMEOUT);
    // wait until PS/2 controller accepts input or a timeout occurs
    while (!ps2_ready() && !pit_timed_out(&timeout));
    if (pit_timed_out(&timeout)) {
        println("%4aPS/2 write timeout (%02x,%02x)%a", io_port, command);
        return 0;
    }
    outb(io_port, command);
    return 1;
}

static inline uint8_t ps2_read(ps2_port_t port, ps2_error_t* err) {
    pit_timeout_t timeout = pit_make_timeout(PS2_TIMEOUT);
    // wait until PS/2 controller holds data for port or a timeout occurs
    while (!ps2_available(port) && !pit_timed_out(&timeout));
    if (pit_timed_out(&timeout)) {
        println("%4aPS/2 port %d read timeout%a", port);
        *err = PS2_TIMEOUT_ERR;
        return 0;
    }
    *err = 0;
    return inb(PS2_DATA);
}

ps2_config_t ps2_read_config() {
    ps2_write(PS2_CMD, READ_CONFIG); // we tell the controller to send us
    return (ps2_config_t) ps2_read(PS2_ANY_PORT, &err); // the config byte
}

void ps2_write_config(ps2_config_t config) {
    ps2_write(PS2_CMD, WRITE_CONFIG);
    ps2_write(PS2_DATA, config.byte);
}

static uint8_t ps2_test(uint8_t test_command, uint8_t expected_result, const char* name) {
    ps2_write(PS2_CMD, test_command);
    uint8_t res = ps2_read(PS2_ANY_PORT, &err);
    if (res != expected_result) {
        print("%4awarning%a. %s test failed (%02x). ", name, res);
        return 0;
    } else return 1;
}

static inline uint8_t ps2_valid_port(ps2_port_t port) {
    if ((port != PS2_PORT_1 && port != PS2_PORT_2) || (port == PS2_PORT_2 && !HAS_PORT2)) {
        println("%4aPS/2 port %d not supported%a", port);
        return 0;
    } else return 1;
}

void ps2_flush() {
    while (ps2_available(PS2_ANY_PORT))
        ps2_read(PS2_ANY_PORT, &err); // discard any data stuck in PS/2 controller
}

static uint8_t ps2_write_device_no_ack(ps2_port_t port, uint8_t command) {
    if (!ps2_valid_port(port)) return 0;
    if (port == PS2_PORT_2)
        ps2_write(PS2_CMD, SEND_TO_PORT_2);
    ps2_write(PS2_DATA, command);
    return 1;
}

uint8_t ps2_write_device(ps2_port_t port, uint8_t command) {
    if (!ps2_write_device_no_ack(port, command))
        return 0;
    return ps2_read_device(port, &err) == DEVICE_ACK;
}

uint8_t ps2_read_device(ps2_port_t port, ps2_error_t* err) {
    if (!ps2_valid_port(port)) {
        *err = PS2_INVALID_PORT_ERR;
        return 0;
    }
    return ps2_read(port, err);
}

static uint8_t ps2_reset_device(ps2_port_t port) {
    ps2_write_device_no_ack(port, DEVICE_RESET);
    pit_timeout_t timeout = pit_make_timeout(PS2_TIMEOUT);
    while (ps2_read_device(port, &err) != DEVICE_TEST_PASSED && !pit_timed_out(&timeout));
    if (pit_timed_out(&timeout)) {
        print("%4afail%a. Port %d reset failed. ", port);
        return 0;
    } else return 1;
}

static void ps2_init_controller() {
    ps2_write(PS2_CMD, DISABLE_PORT_1); // start by disabling devices,
    ps2_write(PS2_CMD, DISABLE_PORT_2); // preventing interference
    ps2_flush();
    
    ps2_config_t config = ps2_read_config();
    if (!config.bits.port2_clock) // ought to be 1 because we disabled port 2 above
        HAS_PORT2 = 0;
    // disable IRQ1, IRQ12 (for now) and scancode translation
    config.bits.port1_intr = config.bits.port2_intr = config.bits.port1_transl = 0;
    ps2_write_config(config);
    
    ps2_test(TEST_PS2, TEST_PS2_PASSED, "PS/2");
    
    if (HAS_PORT2) {
        // we are not sure yet if port 2 is supported,
        ps2_write(PS2_CMD, ENABLE_PORT_2); // so let's find out by enabling it
        ps2_config_t config = ps2_read_config();
        if (config.bits.port2_clock) // ought to be 0 because we just enabled port 2
            HAS_PORT2 = 0;
        else // we know for sure port 2 is supported, so let's disable it again
            ps2_write(PS2_CMD, DISABLE_PORT_2);
    }
    
    if (HAS_PORT1) ps2_test(TEST_PORT_1, TEST_PORT_PASSED, "Port 1");
    if (HAS_PORT2) ps2_test(TEST_PORT_2, TEST_PORT_PASSED, "Port 2");
    if (HAS_PORT1) ps2_write(PS2_CMD, ENABLE_PORT_1);
    if (HAS_PORT2) ps2_write(PS2_CMD, ENABLE_PORT_2);
    
    config = ps2_read_config(); // controller init is done, so we enable devices/IRQs again   
    if (HAS_PORT1) config.bits.port1_intr = 1;
    if (HAS_PORT2) config.bits.port2_intr = 1;
    ps2_write_config(config);
}

static void ps2_init_devices() {
    if (HAS_PORT1) ps2_reset_device(PS2_PORT_1);
    if (HAS_PORT2) ps2_reset_device(PS2_PORT_2);
    ps2_flush(); // clear output buffer (most likely mouse ID)
    if (HAS_PORT1) ps2_write_device(PS2_PORT_1, DEVICE_DISABLE);    
    ps2_flush();
    if (HAS_PORT2) ps2_write_device(PS2_PORT_2, DEVICE_DISABLE);    
    ps2_flush();
    if (HAS_PORT1) keyboard_init(PS2_PORT_1); // To simplify things, we assume port 1
    if (HAS_PORT2) mouse_init(PS2_PORT_2); // is a keyboard and port 2 a mouse.
    if (HAS_PORT1) ps2_write_device(PS2_PORT_1, DEVICE_ENABLE);
    if (HAS_PORT2) ps2_write_device(PS2_PORT_2, DEVICE_ENABLE);
    init_done = 1;
    ps2_flush();
}

static cpu_state_t* ps2_handle_interrupt(cpu_state_t* cpu) {
    if (!init_done) return cpu; // do not interfere with polling code
    if (cpu->intr == ISR_IRQ(12) && !HAS_PORT2) {
        println("%4aIRQ12: not a PS/2 device%a");
        return cpu;
    }
    uint8_t data = inb(PS2_DATA);
    if (cpu->intr == ISR_IRQ(1))
        keyboard_handle_data(data);
    else if (cpu->intr == ISR_IRQ(12))
        mouse_handle_data(data);
    return cpu;
}

void ps2_init() {
    print("PS/2 init ... ");
    isr_register_handler(ISR_IRQ(1), ps2_handle_interrupt);
    isr_register_handler(ISR_IRQ(12), ps2_handle_interrupt);
    // We assume the PS/2 controller exists and USB Legacy Support is still active.
    // Disable IRQs and translation, self-test the controller and ports, and
    ps2_init_controller(); // determine whether port 2 is available.
    ps2_init_devices(); // Reset and initialize the devices.
    println("%2aok%a. %s channel, keyboard %s.",
            HAS_PORT2 ? "Dual" : "Single", HAS_PORT2 ? "and mouse" : "only");
}

void ps2_reboot() {
    // pulse output port bit 0 which triggers a system reboot
    ps2_write(PS2_CMD, PULSE_OUTPUT_LINES(0, 0, 0, 1));
}
