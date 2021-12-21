# cec-mini-kb
Small utility to use a CEC remote controller as a mini keyboard in Linux (libcec+uinput), with the only ambition of being able to navigate user interfaces.

### Usage
Typical usage of the tool is within a service loaded at startup or launched with `systemd-run` when needed.

### Key bindings
 * REMOTE SELECT -> KEY_ENTER
 * REMOTE UP -> KEY UP
 * REMOTE DOWN -> KEY DOWN
 * REMOTE LEFT -> KEY_LEFT
 * REMOTE RIGH -> KEY RIGHT
 * REMOTE EXIT -> KEY BACKSPACE
 * REMOTE BLUE -> KEY ESC
 * REMOTE RED -> KEY LEFTSHIT
 * REMOTE GREEN -> KEY SPACE
 * REMOTE YELLOW -> KEY DELETE
