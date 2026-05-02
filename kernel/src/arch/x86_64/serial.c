/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2026 Omar Elghoul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <kiwi/tty.h>
#include <kiwi/arch/ioport.h>

#define SERIAL_PORT                 0x3F8
#define DATA_REG                    0
#define IRQ_ENABLE_REG              1
#define DIVISOR_LOW_REG             0   /* in DLAB mode */
#define DIVISOR_HIGH_REG            1   /* in DLAB mode */
#define IRQ_ID_REG                  2   /* read-only */
#define FIFO_CTRL_REG               2   /* write-only */
#define LINE_CTRL_REG               3
#define MODEM_CTRL_REG              4
#define LINE_STATUS_REG             5
#define MODEM_STATUS_REG            6
#define SCRATCH_REG                 7

#define DIVISOR_115200_BAUD         1
#define DIVISOR_57600_BAUD          2
#define DIVISOR_38400_BAUD          3
#define DIVISOR_19200_BAUD          6
#define DIVISOR_9600_BAUD           12

#define LINE_CTRL_DLAB              0x80
#define LINE_CTRL_BREAK_ENABLE      0x40
#define LINE_CTRL_STOP_BITS_1       0x00
#define LINE_CTRL_STOP_BITS_2       0x04
#define LINE_CTRL_DATA_BITS_5       0x00
#define LINE_CTRL_DATA_BITS_6       0x01
#define LINE_CTRL_DATA_BITS_7       0x02
#define LINE_CTRL_DATA_BITS_8       0x03

#define IRQ_ENABLE_READ_READY       0x01
#define IRQ_ENABLE_WRITE_READY      0x02
#define IRQ_ENABLE_LINE_STATUS      0x04
#define IRQ_ENABLE_MODEM_STATUS     0x08

#define FIFO_CTRL_ENABLE_FIFO       0x01
#define FIFO_CTRL_CLEAR_RECEIVE     0x02
#define FIFO_CTRL_CLEAR_TRANSMIT    0x04

#define MODEM_CTR_DTR               0x01
#define MODEM_CTR_RTS               0x02
#define MODEM_CTR_OUT1              0x04
#define MODEM_CTR_OUT2              0x08
#define MODEM_CTR_LOOPBACK          0x10

#define LINE_STATUS_READ_READY      0x01
#define LINE_STATUS_OVERRUN_ERROR   0x02
#define LINE_STATUS_PARITY_ERROR    0x04
#define LINE_STATUS_FRAMING_ERROR   0x08
#define LINE_STATUS_BREAK_INTERRUPT 0x10
#define LINE_STATUS_WRITE_READY     0x20

static int has_serial = -1;

static void init_serial(void) {
    arch_outport8(SERIAL_PORT + IRQ_ENABLE_REG, 0);
    arch_outport8(SERIAL_PORT + LINE_CTRL_REG, LINE_CTRL_DLAB);
    arch_outport8(SERIAL_PORT + DIVISOR_LOW_REG,
        DIVISOR_115200_BAUD & 0xFF);
    arch_outport8(SERIAL_PORT + DIVISOR_HIGH_REG,
        (DIVISOR_115200_BAUD >> 8) & 0xFF);
    arch_outport8(SERIAL_PORT + LINE_CTRL_REG,
        LINE_CTRL_DATA_BITS_8 | LINE_CTRL_STOP_BITS_1);
    arch_outport8(SERIAL_PORT + FIFO_CTRL_REG,
        FIFO_CTRL_ENABLE_FIFO | FIFO_CTRL_CLEAR_RECEIVE
        | FIFO_CTRL_CLEAR_TRANSMIT);

    /* test the serial port in loopback mode */
    arch_outport8(SERIAL_PORT + MODEM_CTRL_REG,
        MODEM_CTR_DTR | MODEM_CTR_RTS | MODEM_CTR_LOOPBACK);
    arch_outport8(SERIAL_PORT + DATA_REG, 0x44);    // test byte
    if(arch_inport8(SERIAL_PORT + DATA_REG) != 0x44) {
        has_serial = 0;
        return;
    }

    arch_outport8(SERIAL_PORT + MODEM_CTRL_REG,
        MODEM_CTR_DTR | MODEM_CTR_RTS | MODEM_CTR_OUT1 | MODEM_CTR_OUT2);
    has_serial = 1;
}

void serial_tty_putchar(char c) {
    u8 is_ready;
    if(has_serial == -1)
        init_serial();
    if(!has_serial)
        return;

    if(c == '\n')
        serial_tty_putchar('\r');
    do {
        is_ready = arch_inport8(SERIAL_PORT + LINE_STATUS_REG)
            & LINE_STATUS_WRITE_READY;
    } while(!is_ready);
    arch_outport8(SERIAL_PORT, c);
}
