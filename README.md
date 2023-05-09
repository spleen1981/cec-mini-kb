# cec-mini-kb
Small utility to use a CEC remote controller as a mini keyboard in Linux (libcec+uinput), with the only ambition of being able to navigate user interfaces.

### Usage
Typical usage of the tool is within a service loaded at startup or launched with `systemd-run` when needed.

`-p, --poweroff "<command>"` option allows a custom shell command (e.g. `shutdown -P now`) to be called when system standby event (e.g. turning off the monitor/tv from remote) is received.

`-a, --adapter <num>` specify the adapter to use (0-9). Default adapter is 0.

`-b, --bindings <file>` specify the bindings map file name. See **map.txt** example file.

### Key bindings
 * REMOTE SELECT -> KEY ENTER
 * REMOTE UP -> KEY UP
 * REMOTE DOWN -> KEY DOWN
 * REMOTE LEFT -> KEY LEFT
 * REMOTE RIGH -> KEY RIGHT
 * REMOTE EXIT -> KEY BACKSPACE
 * REMOTE BLUE -> KEY ESC
 * REMOTE RED -> KEY LEFT SHIFT
 * REMOTE GREEN -> KEY SPACE
 * REMOTE YELLOW -> KEY DELETE
 * REMOTE {0 to 9} -> KEY {0 to 9} (or ABC DEF ... WXYZ with multiple presses, like old cellphones)
 * REMOTE FORWARD -> KEY TAB
 * REMOTE BACKFORWARD -> KEY LEFTSHIFT + KEY TAB
