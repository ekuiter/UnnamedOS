/*
 * PS/2 Mouse - fires IRQ12
 *
 * http://wiki.osdev.org/PS/2_Mouse
 * http://wiki.osdev.org/Mouse_Input
 * http://lowlevel.eu/wiki/PS/2-Maus
 */

#include <common.h>
#include <hardware/io/mouse.h>

// mouse commands
#define GET_STATUS      0xE9
#define SET_RESOLUTION  0xE8
#define SET_SAMPLE_RATE 0xF3

#define PACKET_LEN         3 // length of a mouse packet
#define FLAG_ALWAYS_ONE 0x08

#define ABS(x) (x < 0 ? -x : x)
#define SUM_TOO_LOW(coord, dcoord, min)  ((int16_t) event.coord + dcoord < min)
#define SUM_TOO_HIGH(coord, dcoord, max) ((int16_t) event.coord + dcoord > max)

typedef struct {
    uint8_t left : 1, right : 1, middle : 1, : 1, x_sign : 1,
            y_sign : 1, x_overflow : 1, y_overflow : 1;
    uint8_t x_movement;
    uint8_t y_movement;
} __attribute__((packed)) mouse_packet_t;

static ps2_port_t port = 0; // where the mouse is connected (almost always 2)
static uint8_t packet_buf[PACKET_LEN]; // receives packets from mouse
static size_t packet_len = 0; // where to write the next received packet byte
static mouse_event_t event = {0}; // information on the last mouse movement
static mouse_handler_t handler; // which function to call when a mouse event occurs
static uint16_t x_max = 1000, y_max = 750, // maximum intern mouse coordinates
    screen_width = IO_COLS - 1, screen_height = IO_ROWS - 1; // map intern to extern
static uint8_t dx_max = 150, dy_max = 150; // maximum delta to prevent outlier values

static uint8_t mouse_get_status(uint8_t* resolution, uint8_t* sample_rate) {
    ps2_write_device(port, GET_STATUS);
    ps2_error_t err;
    uint8_t flags = ps2_read_device(port, &err);
    // binary value->pixels per mm: 0->1, 1->2, 2->4, 3->8 (powers of 2)
    *resolution = 1 << ps2_read_device(port, &err);
    *sample_rate  = ps2_read_device(port, &err); // decimal value (per second)
    return flags;
}

static uint8_t mouse_resolution(uint8_t resolution) {
    if (resolution == 0) { // read resolution
        uint8_t sample_rate;
        mouse_get_status(&resolution, &sample_rate);
        return resolution;
    }
    else if (resolution == 1 || resolution == 2 || resolution == 4 ||
             resolution == 8) { // write resolution
        uint8_t resolution_value = 0;
        while (resolution >>= 1) // pixels per mm->binary value:
            resolution_value++; // 1->0, 2->1, 4->2, 8->3 (log 2)
        ps2_write_device(port, SET_RESOLUTION);
        ps2_write_device(port, resolution_value);
        return resolution;
    } else
        return 0; // invalid resolution given
}

static uint8_t mouse_sample_rate(uint8_t sample_rate) {
    if (sample_rate == 0) { // read sample rate
        uint8_t resolution;
        mouse_get_status(&resolution, &sample_rate);
        return sample_rate;
    }
    else if (sample_rate == 10 || sample_rate == 20 || sample_rate == 40  ||
             sample_rate == 60 || sample_rate == 80 || sample_rate == 100 ||
             sample_rate == 200) { // write sample rate
        ps2_write_device(port, SET_SAMPLE_RATE);
        ps2_write_device(port, sample_rate);
        return sample_rate;
    } else
        return 0; // invalid sample rate given
}

void mouse_init(ps2_port_t _port) {
    port = _port;
    mouse_resolution(4); // report 4 pixels per mm
    mouse_sample_rate(40); // 40 packets per second should suffice
}

void mouse_register_handler(mouse_handler_t _handler) {
    handler = _handler;
}

static uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; // Arduino map
}

static void mouse_process_packet(mouse_packet_t* packet) {
    packet_len = 0; // ready for receiving a new packet
    if (packet->x_overflow || packet->y_overflow)
        return; // ignore overflowing packets
    int16_t dx = packet->x_movement, // assume normal positive integer, so fill
            dy = packet->y_movement; // high byte up with 0: 0x00xm and 0x00ym
    if (packet->x_sign) // x_movement (xm) contains two's complement, we need to
        dx |= 0xFF00; // extend dx to a 2-byte negative number of form 0xFFxm
    if (packet->y_sign) // similar for y-axis
        dy |= 0xFF00;
    dy = -dy; // dy is positive when moving up, so we flip its sign
    if (ABS(dx) > dx_max || ABS(dy) > dy_max)
        return; // if a delta value is suspiciously high, ignore packet
    event.x = SUM_TOO_LOW(x, dx, 0) ? 0 : (SUM_TOO_HIGH(x, dx, x_max) ? x_max : event.x + dx),
    event.y = SUM_TOO_LOW(y, dy, 0) ? 0 : (SUM_TOO_HIGH(y, dy, y_max) ? y_max : event.y + dy);
    event.screen_x = map(event.x, 0, x_max, 0, screen_width);
    event.screen_y = map(event.y, 0, y_max, 0, screen_height);
    event.left   = packet->left;
    event.right  = packet->right;
    event.middle = packet->middle;
    if (handler) handler(event); // call our event handler if there is one
}

void mouse_handle_data(uint8_t data) {
    packet_buf[packet_len++] = data; // save packet byte
        
    if (packet_len == 1 && !(data & FLAG_ALWAYS_ONE)) // if the "always one"
        packet_len = 0; // flag is not set, something went wrong, so start over
    
    if (packet_len == PACKET_LEN) // if all bytes received, process packet
        mouse_process_packet((mouse_packet_t*) packet_buf);
}