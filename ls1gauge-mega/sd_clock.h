#pragma once
#include <stdint.h>

// RTC resolves whole seconds. Anchor once, then preserve measured sample spacing.
// Absolute subsecond phase is unknown (<1 s), not millis() modulo a RTC second.
struct SdSampleClock {
	uint32_t seconds = 0, lastMs = 0;
	uint16_t fraction = 0;
	bool ready = false;
	void update(uint32_t rtcSeconds, uint32_t nowMs) {
		if (ready) {
			const uint32_t elapsed = nowMs - lastMs;
			seconds += elapsed / 1000;
			fraction += elapsed % 1000;
			if (fraction >= 1000) {
				++seconds;
				fraction -= 1000;
			}
		}
		// Follow clock corrections; tolerate quantization and small oscillator drift.
		const int32_t correction = (int32_t)(rtcSeconds - seconds);
		if (!ready || correction > 1 || correction < -1) {
			seconds = rtcSeconds;
			fraction = 0;
			ready = true;
		}
		lastMs = nowMs;
	}
};
