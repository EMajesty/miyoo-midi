# Miyoo Mini+ native MIDI

Experimental native MIDI hardware support for the Miyoo Mini+ handheld,
targeting OnionOS on the SigmaStar SSD202D SoC.

The CON1 header exposes UART0 as PM_UART_RX/PM_UART_TX; Linux provides it as
`/dev/ttyS0`. This repository contains small tools for removing
`console=ttyS0,115200` from the U-Boot environment and configuring the freed
UART with Linux `termios2`/`BOTHER` at the MIDI rate of 31250 baud.

Native MIDI OUT has been tested successfully with real note-on/note-off
messages and an M-VAVE FM1 MIDI input. The current prototype is a TRS MIDI
Type-A output powered from CON1's approximately 3.0 V supply, with a 33 ohm
supply-side resistor and a 10 ohm TX-side resistor.

The approximately 3.0 V interface is experimental. A properly buffered 3.3 V
output is planned. MIDI IN is not implemented yet.

## Warning

Modifying `/dev/mtd0` can brick the device. Preserve original flash and U-Boot
environment backups, verify the expected erase block before writing, and
always perform byte-for-byte readback verification afterward.
