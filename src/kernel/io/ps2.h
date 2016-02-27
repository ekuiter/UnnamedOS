#ifndef IO_KBC_H
#define IO_KBC_H

// device port indication (see implementation file)

#define PS2_PORT_1 1 // port 1 is usually the keyboard
#define PS2_PORT_2 2 // port 2, if supported, is usually the mouse

void ps2_init();
void ps2_write_device(uint8_t port, uint8_t command);
uint8_t ps2_read_device(uint8_t port);
void ps2_reboot();

#endif
