"""Keyboard remote for the Citroen radio over USB serial (Windows).

Arrow keys and a few others act like the steering-wheel buttons and the setup
button, and everything the radio prints is shown live.

Usage:  python tools/serial_remote.py [PORT]      (default COM9)
Needs:  pip install pyserial   (PlatformIO's Python already has it)
"""

import msvcrt
import sys
import threading

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM9"

# Keys sent to the radio. 1-8 are the physical steering-wheel buttons with their
# factory functions; if a button was reassigned in the menu, it does its new action.
SPECIAL = {  # second code after the 0x00 / 0xE0 prefix Windows gives arrow keys
    "H": ("2", "PRÉCÉDENT (up)"),
    "P": ("1", "SUIVANT (down)"),
    "M": ("5", "VOLUME +"),
    "K": ("6", "VOLUME -"),
}
KEYS = {
    "\r": ("3", "SÉLECTION (enter)"),
    "\x08": ("4", "MENU (back)"),
    " ": ("7", "LECTURE/PAUSE"),
    "x": ("8", "MUET"),
    "+": ("5", "VOLUME +"),
    "-": ("6", "VOLUME -"),
    "\t": ("m", "setup button (open / back)"),
}
PASS_THROUGH = "12345678mLwcsl"  # the radio's own serial keys

HELP = f"""
Citroen radio remote on {PORT}
  Up / Down      Prev / Next (move in the menu)
  Left / Right   Volume - / +  (change a value)   also - and +
  Enter          Select (confirm)
  Backspace      Menu key (back)
  Space          Play / Pause
  x              Mute
  Tab or m       Setup button: open the menu / back
  1..8           Steering-wheel button 1..8
  L w c s l      Learn, raw readings, clear, source, LOUD
  h              This help
  Esc / Ctrl+C   Quit
"""


def reader(port):
    while port.is_open:
        try:
            line = port.readline()
        except (serial.SerialException, TypeError):
            break
        if line:
            print("  " + line.decode(errors="replace").rstrip(), flush=True)


def main():
    port = serial.Serial()
    port.port = PORT
    port.baudrate = 115200
    port.timeout = 0.2
    port.dtr = False  # don't reset the board when the port opens
    port.rts = False
    try:
        port.open()
    except serial.SerialException as e:
        sys.exit(f"Can't open {PORT}: {e}\nClose the PlatformIO serial monitor first.")

    threading.Thread(target=reader, args=(port,), daemon=True).start()
    print(HELP, flush=True)
    try:
        while True:
            ch = msvcrt.getwch()
            if ch in ("\x00", "\xe0"):
                send = SPECIAL.get(msvcrt.getwch())
            elif ch in ("\x1b", "\x03"):
                break
            elif ch == "h":
                print(HELP, flush=True)
                continue
            elif ch in KEYS:
                send = KEYS[ch]
            elif ch in PASS_THROUGH:
                send = (ch, f"key {ch}")
            else:
                send = None
            if send:
                port.write(send[0].encode())
                print(f"> {send[1]}", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        port.close()


if __name__ == "__main__":
    main()
