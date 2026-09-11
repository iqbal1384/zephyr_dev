#ifndef SMP_RS485_TRANSPORT_H_
#define SMP_RS485_TRANSPORT_H_

/**
 * Brings up the RS485 UART/DE hardware and registers the custom mcumgr SMP
 * transport on top of it. Must be called once at startup, after which mcumgr
 * commands (image upload/list/test/confirm, echo, reset) can be received over
 * the RS485 link.
 */
int smp_rs485_transport_init(void);

#endif /* SMP_RS485_TRANSPORT_H_ */
