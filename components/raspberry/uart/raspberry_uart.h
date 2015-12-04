/*
 *  This file is part of Rabbits
 *  Copyright (C) 2015  Clement Deschamps and Luc Michel
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _RASPBERRY_UART_H_
#define _RASPBERRY_UART_H_

#include <rabbits/component/slave.h>

#define READ_BUF_SIZE           256

#define AMBA_CID 0xB105F00D
#define PID 0x00041011

#define UART_DR                 0
#define UART_RSRECR             2
#define UART_FR                 6
#define UART_ILPR               8
#define UART_IBRD               9
#define UART_FBRD               10
#define UART_LCRH               11
#define UART_CR                 12
#define UART_IFLS               13
#define UART_IMSC               14
#define UART_RIS                15
#define UART_MIS                16
#define UART_ICR                17
#define UART_DMACR              18
#define UART_ITCR               19
#define UART_ITIP               20
#define UART_ITOP               21
#define UART_TDR                22
#define UART_PID0            0x3F8
#define UART_PID1            0x3F9
#define UART_PID2            0x3FA
#define UART_PID3            0x3FB
#define UART_CID0            0x3FC
#define UART_CID1            0x3FD
#define UART_CID2            0x3FE
#define UART_CID3            0x3FF

#define UART_MASK_TX(reg)       ((reg >> 5) & 1)
#define UART_MASK_RX(reg)       ((reg >> 4) & 1)

typedef struct
{

    unsigned long int_pending;
    unsigned long int_rx;
    unsigned long int_tx;

    uint8_t uart_enabled;
    uint8_t uart_rx_enable;
    uint8_t uart_tx_enable;
    uint8_t uart_loopback;

    uint16_t uart_baudrate_divisor;
    uint8_t uart_frac_baudrate_divisor;

    uint8_t lcrh;
    unsigned long data_size;

    uint8_t cts_enable;
    uint8_t rts_enable;

    uint8_t rx_irq_lvl;
    uint8_t tx_irq_lvl;

    uint16_t irq_mask;

    uint8_t read_single;
    uint8_t read_buf[READ_BUF_SIZE];
    int read_pos;
    int read_count;
    int read_trigger;
} tty_state;

class raspberry_uart: public Slave
{
public:
    SC_HAS_PROCESS (raspberry_uart);
    raspberry_uart(sc_core::sc_module_name _name);
    raspberry_uart(std::string name, ComponentParameters &params);
    virtual ~raspberry_uart();

private:
    void bus_cb_read(uint64_t addr, uint8_t *data, unsigned int len,
            bool &bErr);
    void bus_cb_write(uint64_t addr, uint8_t *data, unsigned int len,
            bool &bErr);

    sc_core::sc_event irq_update;
    //void irq_update ();
    void read_thread();
    void irq_update_thread();

    void raspberry_uart_init_register(void);

public:
    //ports
    sc_core::sc_out<bool> irq_line;

private:
    sc_core::sc_event evRead;

    int pin;
    int pout;
    tty_state state;
};

#endif

/*
 * Vim standard variables
 * vim:set ts=4 expandtab tw=80 cindent syntax=c:
 *
 * Emacs standard variables
 * Local Variables:
 * mode: c
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
