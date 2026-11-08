#pragma once
#include <stdint.h>
#include <string.h>

// Preliminary protocol core. The transport must verify TLS before calling prepare/acknowledge.
class CsvUpload {
  public:
	static constexpr uint16_t HEADER_SIZE = 58, CHUNK_SIZE = 1024, ACK_SIZE = 24;
	typedef int (*Reader)(void *, uint32_t, uint8_t *, uint16_t);
	enum Status : uint8_t { UNCONFIGURED, READY, TLS_REQUIRED, WAITING, AT_END, READ_ERROR,
	                        BAD_RESPONSE, REMOTE_AHEAD };
	bool begin(const uint8_t *secret, const uint8_t *fileId) {
		memset(packet, 0, sizeof(packet));
		committed = limit = expectedEnd = 0;
		configured = pending = false;
		query = true;
		state = UNCONFIGURED;
		if (!secret || !fileId || !any(secret, 32) || !any(fileId, 16))
			return false;
		memcpy(packet, "P59C", 4);
		memcpy(packet + 4, secret, 32);
		memcpy(packet + 36, fileId, 16);
		configured = true;
		state = READY;
		return true;
	}
	uint16_t prepare(bool verifiedTls, uint32_t syncedSize, Reader reader, void *context) {
		if (!configured || pending)
			return 0;
		if (!verifiedTls) {
			state = TLS_REQUIRED;
			return 0;
		}
		if (syncedSize == UINT32_MAX || committed > syncedSize) {
			state = REMOTE_AHEAD;
			return 0;
		}
		limit = syncedSize;
		uint16_t length = 0;
		if (!query) {
			if (committed == limit) {
				state = AT_END;
				return 0;
			}
			const uint32_t remaining = limit - committed;
			length = remaining > CHUNK_SIZE ? CHUNK_SIZE : (uint16_t)remaining;
			if (!reader || reader(context, committed, packet + HEADER_SIZE, length) != length) {
				state = READ_ERROR;
				return 0;
			}
		}
		put32(packet + 52, query ? UINT32_MAX : committed);
		packet[56] = (uint8_t)length;
		packet[57] = (uint8_t)(length >> 8);
		expectedEnd = committed + length;
		pending = true;
		state = WAITING;
		return HEADER_SIZE + length;
	}
	bool acknowledge(bool verifiedTls, uint16_t httpStatus, const uint8_t *body, uint16_t length) {
		if (!pending)
			return false;
		if (!verifiedTls || httpStatus != 200 || !body || length != ACK_SIZE ||
		    memcmp(body, "P59A", 4) != 0 || memcmp(body + 4, packet + 36, 16) != 0) {
			retry();
			state = BAD_RESPONSE;
			return false;
		}
		const uint32_t next = get32(body + 20);
		if (next > limit || (!query && next < expectedEnd)) {
			retry();
			state = next > limit ? REMOTE_AHEAD : BAD_RESPONSE;
			return false;
		}
		committed = next;
		pending = query = false;
		state = READY;
		return true;
	}
	// A timeout is uncertain: ask the server rather than assuming the POST failed.
	void retry() { pending = false; query = true; state = configured ? READY : UNCONFIGURED; }
	const uint8_t *data() const { return packet; }
	uint32_t offset() const { return committed; }
	Status status() const { return state; }

  private:
	uint8_t packet[HEADER_SIZE + CHUNK_SIZE] = {};
	uint32_t committed = 0, limit = 0, expectedEnd = 0;
	bool configured = false, pending = false, query = true;
	Status state = UNCONFIGURED;
	static bool any(const uint8_t *p, uint8_t n) {
		while (n--) if (*p++) return true;
		return false;
	}
	static void put32(uint8_t *p, uint32_t value) {
		for (uint8_t i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (i * 8));
	}
	static uint32_t get32(const uint8_t *p) {
		return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	}
};
