#ifndef WIFIREG_H
#define WIFIREG_H

#include "wifihal.h"

#define WIFIHAL_ROMBASE			(0x80000)
#define WIFIHAL_REG_RESPONSE	(0x00008)
#define WIFIHAL_REG_PAYLOAD		(0x00004)
#define WIFIHAL_REG_CMD			(0x00000)

static inline int _wifihal_isopen(wifihal_t *h) { return h->iobase != NULL; }

static inline void _wifireg_espoff(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = 0;
}

static inline void _wifireg_espon(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = (1 << 0);
}
static inline void _wifireg_imask(wifihal_t *h) { _wifireg_espon(h); }

static inline void _wifireg_wrie(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = (1 << 1);
}

static inline void _wifireg_rdie(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = (1 << 2);
}

static inline int _wifireg_wrreq(wifihal_t *h) {
	return ((*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE])) >> 22) & 1;
}

static inline int _wifireg_rdreq(wifihal_t *h) {
	return ((*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE])) >> 23) & 1;
}

static inline void _wifireg_tx4(wifihal_t *h, Ptr buf) {
	*(uint32_t*)(&h->iobase[WIFIHAL_REG_PAYLOAD]) = *(uint32_t*)buf;
}

static inline void _wifireg_tx2(wifihal_t *h, Ptr buf) {
	*(uint16_t*)(&h->iobase[WIFIHAL_REG_PAYLOAD]) = *(uint16_t*)buf;
}

static inline void _wifireg_tx1(wifihal_t *h, Ptr buf) {
	*(uint8_t*)(&h->iobase[WIFIHAL_REG_PAYLOAD]) = *(uint8_t*)buf;
}

static inline void _wifireg_rx4(wifihal_t *h, Ptr buf) {
	*(uint32_t*)buf = *(uint32_t*)(&h->iobase[WIFIHAL_REG_RESPONSE]);
}

static inline void _wifireg_txcmd(wifihal_t *h, wificmd_t cmd) {
	uint32_t temp = ((cmd.id & 0xFF)     << 24) |
		  			((cmd.arg0 & 0xFF)   << 16) |
					((cmd.arg1 & 0xFFFF) << 00);
	_wifireg_tx4(h, (Ptr)&temp);
}

static inline void _wifireg_rx2(wifihal_t *h, Ptr buf) {
	*(uint16_t*)buf = *(uint16_t*)(&h->iobase[WIFIHAL_REG_RESPONSE]);
}

static inline void _wifireg_rx1(wifihal_t *h, Ptr buf) {
	*(uint8_t*)buf = *(uint8_t*)(&h->iobase[WIFIHAL_REG_RESPONSE]);
}

#endif
