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

#ifndef _RASPBERRY_MINIUART_H_
#define _RASPBERRY_MINIUART_H_

#include "rabbits/component/slave.h"

#define READ_BUF_SIZE           256

#define AUX_IRQ                 0
#define AUX_ENABLES             1
#define AUX_MU_IO_REG           16
#define AUX_MU_IER_REG          17
#define AUX_MU_IIR_REG          18
#define AUX_MU_LCR_REG          19
#define AUX_MU_MCR_REG          20
#define AUX_MU_LSR_REG          21
#define AUX_MU_MSR_REG          22
#define AUX_MU_SCRATCH          23
#define AUX_MU_CNTL_REG         24
#define AUX_MU_STAT_REG         25
#define AUX_MU_BAUD_REG         26

typedef enum
{
    TX_IDLE, TX_EMPTY, RX_OVERRUN, DATA_RDY, RX_IDLE
} uart_state;

typedef struct
{
    unsigned long uart_enabled;
    unsigned long uart_rx_enable;
    unsigned long uart_tx_enable;

    unsigned long int_pending;
    unsigned long int_level;
    unsigned long rxint_enable;
    unsigned long txint_enable;

    unsigned long fifo_rxmode;
    unsigned long fifo_txmode;

    unsigned long dlab;
    unsigned long baudrate;
    unsigned long data_size; /* 0 if the UART works in 7 bit mode
     3 if the UART works in 8 bit mode
     */
    unsigned long uart_break;
    unsigned long uart_rts;
    unsigned long uart_rts_lvl;
    unsigned long uart_rts_autoflow;
    unsigned long uart_rts_autoflow_lvl;

    unsigned long uart_cts;
    unsigned long uart_cts_lvl;
    unsigned long uart_cts_autoflow;

    unsigned long uart_baudrate_count;

    unsigned long uart_scratch;

    uart_state uart_fsm;

    uint8_t read_buf[READ_BUF_SIZE];
    int read_pos;
    int read_count;
    int read_trigger;
} tty_state;

class raspberry_miniuart: public Slave
{
public:
    SC_HAS_PROCESS (raspberry_miniuart);
    raspberry_miniuart(sc_core::sc_module_name _name);
    virtual ~raspberry_miniuart();

private:
    void cb_write(uint32_t ofs, uint8_t be, uint8_t *data, bool &bErr);
    void cb_read(uint32_t ofs, uint8_t be, uint8_t *data, bool &bErr);

    sc_core::sc_event irq_update;
    //void irq_update ();
    void read_thread();
    void irq_update_thread();

    void raspberry_miniuart_init_register(void);

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
