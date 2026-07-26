//
// Created by all on 16. 12. 2025.
//
#include <stdio.h>
#include "ibus.h"
#include "pico/stdlib.h"
#include "pico/sync.h"

//#define DBG
#define RX_SBUS_IN_PIN 21     //UART1 RX
#define SBUS_NEXT_FRAME_TIMEGAP 3000 //us cca 7ms chodia frame, a jeden trva 32*10*1/115200 = 2.7ms

typedef enum
{
    IBUS_STATE_IDLE,
    IBUS_STATE_HEADING_RECEIVED,
    IBUS_STATE_COMMAND_RECEIVED,
    IBUS_STATE_DATA_RECEIVED_LOW,
    IBUS_STATE_DATA_RECEIVED_HIGH,
    IBUS_STATE_CHECK_CRC_LOW,
    IBUS_STATE_CHECK_CRC_HIGH
}ibus_state;

volatile ibus_state IBUS_rx_state = IBUS_STATE_IDLE;
volatile bool IBUS_data_received = false;
static volatile uint32_t time_next;
volatile uint16_t channel_data[14];
uint16_t channel_data_saved[14];
static bool first_data_received = false;

/*
 *  supports max 14 channels (with messagelength of 0x20 there is room for 14 channels)

  Example set of bytes coming over the iBUS line for setting servos:
    20 40 DB 5 DC 5 54 5 DC 5 E8 3 D0 7 D2 5 E8 3 DC 5 DC 5 DC 5 DC 5 DC 5 DC 5 DA F3
  Explanation
    Protocol length: 20
    Command code: 40
    Channel 0: DB 5  -> value 0x5DB
    Channel 1: DC 5  -> value 0x5Dc
    Channel 2: 54 5  -> value 0x554
    Channel 3: DC 5  -> value 0x5DC
    Channel 4: E8 3  -> value 0x3E8
    Channel 5: D0 7  -> value 0x7D0
    Channel 6: D2 5  -> value 0x5D2
    Channel 7: E8 3  -> value 0x3E8
    Channel 8: DC 5  -> value 0x5DC
    Channel 9: DC 5  -> value 0x5DC
    Channel 10: DC 5 -> value 0x5DC
    Channel 11: DC 5 -> value 0x5DC
    Channel 12: DC 5 -> value 0x5DC
    Channel 13: DC 5 -> value 0x5DC
    Checksum: DA F3 -> calculated by adding up all previous bytes, total must be FFFF
 */


void IBUS_rx_int_handler(void) {
    static uint8_t idx = 0;
    static uint32_t chksum = 0;
    uint32_t tmp = (uart1_hw->dr) & 0xFF;
    uint32_t dt = time_us_32() - time_next;
    time_next = time_us_32() + SBUS_NEXT_FRAME_TIMEGAP;

    if ( ((int32_t) dt) > 0) {

        if (tmp == 0x20) {
            IBUS_rx_state = IBUS_STATE_HEADING_RECEIVED;
            chksum = 0x20;
        }
        else {
            IBUS_rx_state = IBUS_STATE_IDLE;
        }
    }
    else{
        switch (IBUS_rx_state) {
            case IBUS_STATE_HEADING_RECEIVED:
                if (tmp == 0x40) {
                    chksum += 0x40;
                    IBUS_rx_state = IBUS_STATE_COMMAND_RECEIVED;
                    idx = 0;
                }
                else {
                    IBUS_rx_state = IBUS_STATE_IDLE;
                }

                break;
            case IBUS_STATE_COMMAND_RECEIVED:
            case IBUS_STATE_DATA_RECEIVED_LOW:
                if (IBUS_data_received) {
                    //az ked sa precitaju
                    IBUS_rx_state = IBUS_STATE_IDLE;
                }
                else {
                    chksum += tmp;
                    channel_data[idx] = (uint16_t) tmp;
                    IBUS_rx_state = IBUS_STATE_DATA_RECEIVED_HIGH;
                }
                break;
            case IBUS_STATE_DATA_RECEIVED_HIGH:
                chksum += tmp;
                channel_data[idx++] += (uint16_t) (tmp << 8);
                IBUS_rx_state = IBUS_STATE_DATA_RECEIVED_LOW;
                if (idx == 14) {
                    IBUS_rx_state = IBUS_STATE_CHECK_CRC_LOW;
                }
                break;
            case IBUS_STATE_CHECK_CRC_LOW:
                chksum += tmp;
                IBUS_rx_state = IBUS_STATE_CHECK_CRC_HIGH;
                break;
            case IBUS_STATE_CHECK_CRC_HIGH:
                chksum += tmp<<8;
            //printf("chk: %x/n",chksum);
                if (chksum == 0xFFFF ) {
                    IBUS_data_received = true;
                }
                else {
                    IBUS_data_received = false;
                }
                IBUS_rx_state = IBUS_STATE_IDLE;
                break;
            default:
            ;
        }
    }
}

/*
 * set uart rx pin uart1
 * 115200 baud, fifo disabled
 */
void ibus_init(void) {
    gpio_set_function(RX_SBUS_IN_PIN,GPIO_FUNC_UART);
    uart_init(uart1,115200);
    uart_set_fifo_enabled(uart1,false);
    irq_set_exclusive_handler(UART1_IRQ, IBUS_rx_int_handler);
    uart_set_irqs_enabled(uart1,true,false);
    irq_set_enabled(UART1_IRQ,true);
    time_next = time_us_32() + SBUS_NEXT_FRAME_TIMEGAP;  //3ms delay for next frame
}


/*
 * ak su k dispozicii data ulozi ich
 */
void ibus_service(void) {
    static uint32_t cnt = 0;

    //save store channel data
    if (IBUS_data_received) {
        first_data_received = true;
        uint32_t int_flags = save_and_disable_interrupts();
        for(uint8_t i=0;i<14;i++) {
            channel_data_saved[i] = channel_data[i];
        }
        restore_interrupts(int_flags);
        cnt++;

#ifdef DBG
        if (cnt == 20){
            cnt = 0;
            for(uint8_t i=0;i<14;i++) {
                printf("%02d: %4d ",i+1,channel_data_saved[i]);
            }
            printf("\n");
        }
#endif
        IBUS_data_received = false;
    }
}

uint16_t ibus_get_channel (ibus_channel chan) {
    return channel_data_saved[chan];
}

bool ibus_data_valid (void) {
    return first_data_received;
}