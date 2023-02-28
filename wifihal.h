#ifndef WIFIHAL_H
#define WIFIHAL_H

#include <stdint.h>

typedef char* Ptr;
typedef int OSErr;
typedef int size_t;
#define NULL (0)
#define TickCount() (0)
#define noErr (0)
#define openErr (-23)
#define paramErr (-50)

#define IRQDISABLE() {}; //FIXME
#define IRQENABLE() {}; //FIXME

typedef struct wificmd_s {
	uint8_t id;
	uint8_t arg0;
	uint16_t arg1;
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

typedef struct wifiresponse_s {
	uint8_t code;
	uint8_t value;
	size_t length;
} wifiresponse_t;

typedef enum wifitransferphase_e {
	WIFIHAL_PHASE_IDLE = 0,
	WIFIHAL_PHASE_TXDATA = 1,
	WIFIHAL_PHASE_TXCMD = 2,
	WIFIHAL_PHASE_RXRESULT = 3,
	WIFIHAL_PHASE_RXDATA = 4
} wifitransferphase_t;

typedef struct wificmdentry_s {
	wificmd_t cmd;
	Ptr txdata;
	size_t txlength;
	Ptr rxdata;
	size_t rxlength;
	int done;
	wifitransferphase_t phase;
	wifiresponse_t result;
	struct wificmdentry_s *next;
} wificmdentry_t;

typedef struct wifihal_s {
	Ptr iobase;
	wificmdentry_t *cmd;
	wificmdentry_t *last;
} wifihal_t;

// Turns on ESP32 and gets ready to do wifi commands
OSErr wifihal_open(wifihal_t *h, Ptr iobase);

// Sends a command to WiFi card
OSErr wifihal_cmd(wifihal_t *h, wificmdentry_t *cmd);

// Waits for a command to complete
OSErr wifihal_await(wificmdentry_t *h);

// Turns off ESP32
void wifihal_close(wifihal_t *h);

#endif
