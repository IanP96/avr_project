# Connections

LED matrix to VCC/GND
AVR to laptop

B0-3 - buttons B0-3
B4-7 - SPI connection to LED matrix (B4 - SS)
D0 - serial TX (baud 19200)
D1 - serial RX (baud 19200)

After upload, programmer to serial TX/RX and laptop

# Marks

## Tier A
s - Game over
44/50

## Tier B
s - Search and destroy
cheating 2
18/30

# Ship byte

- The three least significant bits determine what is in that location, with 000 being no ship, and 001 through 110 enumerating the ship types.
- The next bit is 1 if the location contains the end of a ship.
- Then the next bit is 1 if the ship is horizontal, or 0 if the ship is vertical.
- This leaves three unused bits
- Bit 5 is fired-unfired
- Bit 6 is sunken
- Bit 7 is hit (fired and turn done, used for salvo)

# Terminal

*Row n corresponds to y=n*

1 - invalid move message
2-7 - lists of sunken ships

9 - Which player won, from game over

11 - pause msg

14 - GAME OVER
15 - Press a button or 's'/'S' to start a new game
16 - high score
17 - Com mode (basic or search & destroy)
18 - salvo mode
19 - msg for ships setup or placed in default positions

# At end

Test on lab computers (Microchip Studio)
Don't save code to C: drive on lab computer (see Ed)

## Feature summary
- For salvo mode, when a human is on their turn, fired spots become dark green
- Pausing does not pause human ship setup