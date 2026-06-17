# RTOS-Based Environmental Monitor
Personal Embedded Projects — STM32F407 Discovery Board

---

## UART Terminal Goals

- Enable USART2 in CubeMX, routed to a USB-to-UART adapter on PA2 (TX) and PA3 (RX)
- Implement interrupt-driven UART RX — no blocking `HAL_UART_Receive`
- Build a minimal command parser: receive a string, compare against known commands (`READ`, `HELP`, `CLEAR`), respond accordingly
- Echo characters back as they are typed so it feels like a real terminal
- Test with PuTTY, Tera Term, or the VS Code Serial Monitor at 115200 baud

---

## UART Architecture

### Overview

USART2 is configured at 115200 baud, 8N1 with no hardware flow control. Rather than polling or blocking on receive, the driver uses HAL's interrupt mode so the CPU is free while waiting for input — the interrupt fires only when a byte arrives.

### Receive Flow

A single byte `rxByte` is the live receive target. At startup, `HAL_UART_Receive_IT` arms the peripheral to interrupt when one byte lands in `rxByte`. Each time a byte arrives, the HAL calls `HAL_UART_RxCpltCallback`, where three things happen:

1. If the byte is a printable character, it is echoed back immediately and appended to `rx_buffer`
2. If the byte is `\r` (carriage return, sent when the user presses Enter), a `\r\n` is echoed to advance the terminal to a new line, `rx_buffer` is null-terminated, and `processCommand` is called
3. `HAL_UART_Receive_IT` is re-armed for the next byte before returning

This one-byte-at-a-time interrupt pattern means no bytes are lost and the main loop is never blocked waiting for input.

### Command Parser

`processCommand` receives the null-terminated string from `rx_buffer` and uses `strcmp` to match against known commands:

| Command | Response |
|---------|----------|
| `READ`  | `Reading sensors...` |
| `HELP`  | `Commands: READ, HELP, CLEAR` |
| `CLEAR` | ANSI escape sequence to clear the terminal (`\033[2J\033[H`) |
| anything else | `Unknown command` |

### Echo Design

Characters are echoed inside the interrupt callback before being stored. When `\r` arrives the echo is upgraded to `\r\n` so the terminal cursor drops to a new line before the response is printed. This gives the feel of a normal terminal without any client-side local echo setting required.
