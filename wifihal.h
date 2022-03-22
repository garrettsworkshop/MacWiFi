#ifndef WIFIHAL_H
#define WIFIHAL_H

#include <stdint.h>

typedef char* Ptr;
typedef int OSErr;
typedef int size_t;
#define NULL (0)
#define TickCount() (0)
#define noErr (0)
#define openErr (-1)

typedef struct wificmdword_s {
	uint8_t id;
	uint8_t arg0;
	uint16_t arg1;
} wificmdword_t;

typedef struct wificmd_s {
	wificmdword_t cmd;
	Ptr txdata;
	size_t txlength;
	Ptr rxdata;
} wificmd_t;

#define WIFICMD_CANCEL			(0x0A)
#define WIFICMD_GET_APLIST		(0x09)
#define WIFICMD_CONNECT			(0x08)
#define WIFICMD_GET_MAC			(0x07)
#define WIFICMD_GET_TXCOUNT		(0x06)
#define WIFICMD_TX				(0x05)
#define WIFICMD_GET_RXCOUNT		(0x04)
#define WIFICMD_RX				(0x03)
#define WIFICMD_SET_RXIRQ		(0x02)
#define WIFICMD_GET_FWVERSION	(0x01)
#define WIFICMD_ECHO			(0x00)

typedef struct wifiresult_s {
	uint8_t code;
	uint8_t value;
	size_t length;
} wifiresult_t;

typedef enum wificmdphase_e {
	WIFIHAL_PHASE_IDLE = 0,
	WIFIHAL_PHASE_TXDATA = 1,
	WIFIHAL_PHASE_TXCMD = 2,
	WIFIHAL_PHASE_RXRESULT = 3,
	WIFIHAL_PHASE_RXDATA = 4
} wificmdphase_t;

typedef struct wifihal_s {
	Ptr iobase;
	wificmdphase_t phase;
	wificmd_t cmd;
	wifiresult_t result;
} wifihal_t;

#define WIFIHAL_ROMBASE			(0x80000)
#define WIFIHAL_REG_RESPONSE	(0x00008)
#define WIFIHAL_REG_PAYLOAD		(0x00004)
#define WIFIHAL_REG_CMD			(0x00000)

OSErr wifihal_open(wifihal_t *h, Ptr iobase);

OSErr wifihal_cmd(wifihal_t *h, wificmd_t cmd);

OSErr wifihal_await(wifihal_t *h, wifiresult_t *result);

void wifihal_close(wifihal_t *h);

#endif
