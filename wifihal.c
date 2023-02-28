#include "wifihal.h"
#include "wifireg.h"

#define WIFIHAL_DT_TIMESLICE	(8)
#define WIFIHAL_DT_MAXTRIES		(50)

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

	IRQDISABLE(); // Critical section!

	if (h->cmd == NULL) { _wifireg_imask(h); }

	switch (h->cmd->phase) {
	case WIFIHAL_PHASE_IDLE:
	case WIFIHAL_PHASE_TXDATA:
	case WIFIHAL_PHASE_TXCMD:
	_wifireg_wrie(h); // Unmask only write request IRQ
	break;

	case WIFIHAL_PHASE_RXRESULT:
	case WIFIHAL_PHASE_RXDATA:
	_wifireg_rdie(h); // Unmask only read request IRQ
	break;
	}
	
	IRQENABLE();
}

// Deferred task function... accomplishes data transfer
void _wifihal_dt(wifihal_t *h) {
	// Compute when our timeslice ends
	uint32_t end = TickCount() + WIFIHAL_DT_TIMESLICE;
	int timeout1 = 0, timeout2 = 0; // These get set when timeout occurs

	while (h->cmd != NULL) { // While there are commands to be sent...
		switch (h->cmd->phase) {
			case WIFIHAL_PHASE_IDLE:
			h->cmd->phase++;

			case WIFIHAL_PHASE_TXDATA:
			while (h->cmd->txlength > 3) { 
				_WIFIHAL_DT_WAIT(_wifireg_wrreq(h)); // Wait until write ready
				_wifireg_tx4(h, h->cmd->txdata); // Send 4 bytes
				h->cmd->txdata += 4; // Increase tx pointer by 4
				h->cmd->txlength -= 4; // Decrease tx length by 4
			}
			_WIFIHAL_DT_WAIT(_wifireg_wrreq(h)); // Wait until write ready
			switch (h->cmd->txlength) { // Send remaining 1/2/3 bytes
				case 1: _wifireg_tx1(h, h->cmd->txdata); break;
				case 2: _wifireg_tx2(h, h->cmd->txdata); break;
				case 3: _wifireg_tx4(h, h->cmd->txdata); break;
				default: break;
			}
			h->cmd->txdata += h->cmd->txlength; // Increase pointer past end
			h->cmd->txlength = 0; // Zero rmaining tx length
			h->cmd->phase++;

			case WIFIHAL_PHASE_TXCMD:
			_WIFIHAL_DT_WAIT(_wifireg_wrreq(h)); // Wait until write ready
			_wifireg_txcmd(h, h->cmd->cmd); // Send command
			h->cmd->phase++;

			case WIFIHAL_PHASE_RXRESULT:
			_WIFIHAL_DT_WAIT(_wifireg_rdreq(h)); // Wait until read ready
			// Receive result and store in h->cmd->result
			uint32_t result;
			_wifireg_rx4(h, (Ptr)&result);
			h->cmd->result.code =   (result >> 24) & 0xFF;
			h->cmd->result.value =  (result >> 16) & 0xFF;
			h->cmd->result.length = (result >> 00) & 0xFFFF;
			// Store result length in h->cmd->rxlength
			h->cmd->rxlength = h->cmd->result.length;
			h->cmd->phase++;

			case WIFIHAL_PHASE_RXDATA:
			while (h->cmd->rxlength > 3) { 
				_WIFIHAL_DT_WAIT(_wifireg_rdreq(h)); // Wait until read ready
				_wifireg_rx4(h, h->cmd->rxdata); // Receive 4 bytes
				h->cmd->rxdata += 4; // Increase rx pointer by 4
				h->cmd->rxlength -= 4; // Decrease rx length by 4
			}
			if (h->cmd->rxlength > 0) {
				_WIFIHAL_DT_WAIT(_wifireg_rdreq(h)); // Wait until read ready
				switch (h->cmd->rxlength) { // Receive remaining 1/2/3 bytes
					case 1: _wifireg_rx1(h, h->cmd->rxdata); break;
					case 2: _wifireg_rx2(h, h->cmd->rxdata); break;
					case 3: _wifireg_rx4(h, h->cmd->rxdata); break;
					default: break;
				}
				h->cmd->rxdata += h->cmd->rxlength; // Increase pointer past end
				h->cmd->rxlength = 0; // Zero remaining rx length
			}

			// Set commmand done
			h->cmd->phase = WIFIHAL_PHASE_IDLE;
			h->cmd->done = 1;

			h->cmd = h->cmd->next; // Advance to next position in linked list
			// If no more commands set last to null
			if (h->cmd == NULL) { h->last = NULL; }
		}
	}

	// If we timed out, disable interrupts until next vblank
	if (timeout1 || timeout2) { _wifireg_imask(h); }
	// Otherwise set the interrupt mask correctly depending on current phase
	else { _wifihal_setphasemask(h); }

	return;
}

// WiFi card data transfer interrupt handler
void _wifihal_isr(wifihal_t *h) {
	//TODO: Install deferred task
	_wifireg_imask(h); // Mask all IRQs
	// Return causes control to be transferred to _wifihal_dt(...)
}

// Vertical blanking interrupt handler
void _wifihal_vbl(wifihal_t *h) {
	_wifihal_setphasemask(h);
}

// Waits for a command to complete
OSErr wifihal_await(wificmdentry_t *cmd) {
	if (cmd == NULL) { return paramErr; }
	while (!cmd->done);
	return noErr;
}

// Sends a command to WiFi card
OSErr wifihal_cmd(wifihal_t *h, wificmdentry_t *cmd) {
	if (h == NULL || !_wifihal_isopen(h)) { return paramErr; }

	cmd->done = 0; // Set not done
	cmd->phase = WIFIHAL_PHASE_IDLE; // Initial phase
	cmd->result = (wifiresponse_t){0}; // Zero resu;t
	cmd->next = NULL; // Null next pointer

	IRQDISABLE(); // Critical section!
	if (h->cmd == NULL) { // If no current command...
		h->cmd = cmd; // Set current command to incoming one
		_wifireg_wrie(h); // Unmask only write request IRQ
	}
	else { h->last->next = cmd; } // Otherwise add to end of linked list
	h->last = cmd; // Update end pointer
	IRQENABLE();

	return noErr;
}

// Turns on ESP32 and gets ready to do wifi commands
OSErr wifihal_open(wifihal_t *h, Ptr iobase) {
	if (h == NULL || iobase == NULL || _wifihal_isopen(h)) { return openErr; }

	*h = (wifihal_t){0}; // Zero the wifihal entry
	h->cmd = NULL;
	h->iobase = iobase; // Save iobase pointer

	_wifireg_espon(h); // Enable ESP32

	//TODO: Install card interrupt handler
	//TODO: Install VBL interrupt handler

	// Send GET_FWVERSION command so as to defer until ESP32 booted
	wificmdentry_t cmd = {
		.cmd = {
			.id = WIFICMD_GET_FWVERSION,
			.arg0 = 0,
			.arg1 = 0
		},
		.txdata = NULL,
		.rxdata = NULL
	};
	wifihal_cmd(h, &cmd);

	return noErr;
}

// Turns off ESP32
void wifihal_close(wifihal_t *h) {
	if (h == NULL) { return; }
	while (h->cmd != NULL); // Wait for previous command to finish
	h->iobase = NULL;
	_wifireg_espoff(h);
}
