#include "wifihal.h"

#define WIFIHAL_DT_TIMESLICE	(5)
#define WIFIHAL_DT_MAXTRIES		(255)

static inline int _wifihal_isopen(wifihal_t *h) {
	return h->iobase != NULL;
}

static inline void _wifihal_espoff(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = 0x00000000;
}

static inline void _wifihal_espon(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = 0x00000001;
}
static inline void _wifihal_imask(wifihal_t *h) { _wifihal_espon(h); }

static inline void _wifihal_wrie(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = 0x00000002;
}

static inline void _wifihal_rdie(wifihal_t *h) {
	*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE]) = 0x00000004;
}

static inline int _wifihal_wrreq(wifihal_t *h) {
	return ((*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE])) >> 22) & 1;
}

static inline int _wifihal_rdreq(wifihal_t *h) {
	return ((*(uint32_t*)(&h->iobase[WIFIHAL_ROMBASE])) >> 23) & 1;
}

static inline void _wifihal_tx4(wifihal_t *h, Ptr buf) {
	*(uint32_t*)(&h->iobase[WIFIHAL_REG_PAYLOAD]) = *(uint32_t*)buf;
}

static inline void _wifihal_tx3(wifihal_t *h, Ptr buf) {
	_wifihal_tx4(h, buf);
}

static inline void _wifihal_tx2(wifihal_t *h, Ptr buf) {
	*(uint16_t*)(&h->iobase[WIFIHAL_REG_PAYLOAD]) = *(uint16_t*)buf;
}

static inline void _wifihal_tx1(wifihal_t *h, Ptr buf) {
	*(uint8_t*)(&h->iobase[WIFIHAL_REG_PAYLOAD]) = *(uint8_t*)buf;
}

static inline void _wifihal_rx4(wifihal_t *h, Ptr buf) {
	*(uint32_t*)buf = *(uint32_t*)(&h->iobase[WIFIHAL_REG_RESPONSE]);
}

static inline void _wifihal_txcmd(wifihal_t *h, wificmdword_t cmd) {
	uint32_t temp;
	temp = ((cmd.id & 0xFF)     << 24) |
		   ((cmd.arg0 & 0xFF)   << 16) |
		   ((cmd.arg1 & 0xFFFF) << 00);
	_wifihal_tx4(h, (Ptr)&temp);
}

static inline void _wifihal_rx3(wifihal_t *h, Ptr buf) {
	_wifihal_rx4(h, buf);
}

static inline void _wifihal_rx2(wifihal_t *h, Ptr buf) {
	*(uint16_t*)buf = *(uint16_t*)(&h->iobase[WIFIHAL_REG_RESPONSE]);
}

static inline void _wifihal_rx1(wifihal_t *h, Ptr buf) {
	*(uint8_t*)buf = *(uint8_t*)(&h->iobase[WIFIHAL_REG_RESPONSE]);
}


#define _WIFIHAL_DT_WAIT(condition) if (!condition) { \
	uint32_t now; \
	timeout1 = 0; timeout2 = 0; \
	for (int i = 0; !condition; i++) { \
		now = TickCount(); \
		timeout1 = now > end; \
		timeout2 = i >= WIFIHAL_DT_MAXTRIES; \
		if (timeout1 || timeout2) { break; } \
	} \
	if (timeout1 || timeout2) { break; } \
}

// Sets IRQ masks correctly depending on transfer phase
static void _wifihal_setphasemask(wifihal_t *h) {
	switch (h->phase) {
	case WIFIHAL_PHASE_IDLE:
	_wifihal_imask(h); // Mask all IRQs
	break;

	case WIFIHAL_PHASE_TXDATA:
	case WIFIHAL_PHASE_TXCMD:
	_wifihal_wrie(h); // Unmask only write request IRQ
	break;

	case WIFIHAL_PHASE_RXRESULT:
	case WIFIHAL_PHASE_RXDATA:
	_wifihal_rdie(h); // Unmask only read request IRQ
	break;
	}
}

void _wifihal_dt(wifihal_t *h) {
	uint32_t start = TickCount();
	uint32_t end = start + WIFIHAL_DT_TIMESLICE;
	int timeout1 = 0, timeout2 = 0;

	switch (h->phase) {
	case WIFIHAL_PHASE_IDLE:
	break;

	case WIFIHAL_PHASE_TXDATA:
	while (h->cmd.txlength > 3) { 
		_WIFIHAL_DT_WAIT(_wifihal_wrreq(h));
		// Send 4 bytes
		_wifihal_tx4(h, h->cmd.txdata);
		h->cmd.txdata += 4;
		h->cmd.txlength -= 4;
	}
	_WIFIHAL_DT_WAIT(_wifihal_wrreq(h));
	// Send remaining 1/2/3 bytes
	switch (h->cmd.txlength) {
		case 1: _wifihal_tx1(h, h->cmd.txdata); break;
		case 2: _wifihal_tx2(h, h->cmd.txdata); break;
		case 3: _wifihal_tx3(h, h->cmd.txdata); break;
		default: break;
	}
	h->cmd.txdata += h->cmd.txlength;
	h->cmd.txlength = 0;
	h->phase++;

	case WIFIHAL_PHASE_TXCMD:
	_WIFIHAL_DT_WAIT(_wifihal_wrreq(h));
	// Send command
	_wifihal_txcmd(h, h->cmd.cmd);
	h->phase++;

	case WIFIHAL_PHASE_RXRESULT:
	_WIFIHAL_DT_WAIT(_wifihal_rdreq(h));
	// Receive result and store in h->result
	uint32_t temp;
	_wifihal_rx4(h, (Ptr)&temp);
	h->result.code =   (temp >> 24) & 0xFF;
	h->result.value =  (temp >> 16) & 0xFF;
	h->result.length = (temp >> 00) & 0xFFFF;
	// Store result length in h->cmd.txlength temporarily
	h->cmd.txlength = h->result.length;
	h->phase++;

	case WIFIHAL_PHASE_RXDATA:
	while (h->cmd.txlength > 3) { 
		_WIFIHAL_DT_WAIT(_wifihal_rdreq(h));
		// Receive 4 bytes
		_wifihal_rx4(h, h->cmd.rxdata);
		h->cmd.rxdata += h->cmd.txlength;
		h->cmd.txlength -= 4;
	}
	if (h->cmd.txlength > 0) {
		_WIFIHAL_DT_WAIT(_wifihal_rdreq(h));
		// Receive remaining 1/2/3 bytes
		switch (h->cmd.txlength) {
			case 1: _wifihal_rx1(h, h->cmd.rxdata); break;
			case 2: _wifihal_rx2(h, h->cmd.rxdata); break;
			case 3: _wifihal_rx3(h, h->cmd.rxdata); break;
			default: break;
		}
		_wifihal_rx4(h, h->cmd.rxdata);
		h->cmd.rxdata += h->cmd.txlength;
		h->cmd.txlength = 0;
	}
	h->phase = WIFIHAL_PHASE_IDLE;
	}

	if (timeout1 || timeout2) { _wifihal_imask(h); }
	else { _wifihal_setphasemask(h); }

	return;
}

void _wifihal_isr(wifihal_t *h) {
	//TODO: Install deferred task
	_wifihal_imask(h); // Mask all IRQs
	// Return causes control to be transferred to _wifihal_dt(...)
}

void _wifihal_vbl(wifihal_t *h) {
	_wifihal_setphasemask(h);
}


OSErr wifihal_await(wifihal_t *h, wifiresult_t *result) {
	if (h == NULL || !_wifihal_isopen(h)) { return -1/*FIXME*/; }

	while (h->phase != WIFIHAL_PHASE_IDLE);
	if (result != NULL) { *result = h->result; }
	return noErr;
}

OSErr wifihal_cmd(wifihal_t *h, wificmd_t cmd) {
	if (h == NULL || !_wifihal_isopen(h)) { return -1/*FIXME*/; }

	wifihal_await(h, NULL); // Wait for previous command to finish

	h->cmd = cmd; // Store command
	h->phase = WIFIHAL_PHASE_TXDATA; // Set phase

	_wifihal_wrie(h); // Unmask only write request IRQ
	return noErr;
}

OSErr wifihal_open(wifihal_t *h, Ptr iobase) {
	if (h == NULL || iobase == NULL || _wifihal_isopen(h)) { return openErr; }

	*h = (wifihal_t){0};
	h->phase = WIFIHAL_PHASE_IDLE; // Go to idle phase
	h->iobase = iobase;

	_wifihal_espon(h); // Enable ESP32

	//TODO: Install card interrupt handler
	//TODO: Install VBL interrupt handler

	// Send GET_FWVERSION command so as to defer until ESP32 booted
	wificmd_t cmd = {
		.cmd = {
			.id = WIFICMD_GET_FWVERSION,
			.arg0 = 0,
			.arg1 = 0
		},
		.txdata = NULL,
		.txlength = 0,
		.rxdata = NULL
	};
	wifihal_cmd(h, cmd);
	return noErr;
}

void wifihal_close(wifihal_t *h) {
	if (h == NULL) { return; }
	wifihal_await(h, NULL); // Wait for previous command to finish
	h->iobase = NULL;
	_wifihal_espoff(h);
}
