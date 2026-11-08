#pragma once
// Provisioning stays on the SD card, outside the sketch and Git.
uint8_t csvDeviceSecret[32] = {};
bool csvProvisioned = false, csvTlsQualified = false;
// USB provisioning uses eight-byte chunks; existing installation keys are never overwritten.
bool csvConfigChunk(uint8_t offset, const uint8_t *bytes) {
	if (offset > 32 || offset % 8 || SD.exists("P59.CFG")) return false;
	if (offset == 0 && SD.exists("P59.TMP") && !SD.remove("P59.TMP")) return false;
	File file = SD.open("P59.TMP", O_RDWR | O_CREAT);
	if (!file) return false;
	const bool valid = file.size() == offset && file.seek(offset) &&
	    file.write(bytes, 8) == 8 && file.sync() && !file.getError();
	file.close();
	return valid;
}
bool csvConfigCommit() {
	if (SD.exists("P59.CFG")) return false;
	uint8_t bytes[40] = {};
	File file = SD.open("P59.TMP", O_RDONLY);
	if (!file) return false;
	bool valid = file.size() == sizeof(bytes) && file.read(bytes, sizeof(bytes)) == sizeof(bytes) && !file.getError();
	file.close();
	uint8_t key = 0, space = 0;
	for (uint8_t i = 0; i < 32; ++i) key |= bytes[i];
	for (uint8_t i = 32; i < 40; ++i) space |= bytes[i];
	valid = valid && key && space;
	memset(bytes, 0, sizeof(bytes));
	return valid && SD.rename("P59.TMP", "P59.CFG");
}
void csvLoadProvisioning() {
	csvProvisioned = csvTlsQualified = false;
	memset(csvDeviceSecret, 0, sizeof(csvDeviceSecret));
	uint8_t config[40];
	File file = SD.open("P59.CFG", O_RDONLY);
	if (!file) return;
	const bool valid = file.size() == sizeof(config) &&
	    file.read(config, sizeof(config)) == sizeof(config) && !file.getError();
	file.close();
	if (!valid) return;
	uint8_t keyBits = 0, namespaceBits = 0;
	for (uint8_t i = 0; i < 32; ++i) keyBits |= config[i];
	for (uint8_t i = 32; i < 40; ++i) namespaceBits |= config[i];
	if (!keyBits || !namespaceBits) return;
	memcpy(csvDeviceSecret, config, 32);
	csvQueueInit(config + 32);
	csvProvisioned = true;
	// Qualification is installation-specific, not a compiler flag asserting TLS works.
	// Create only after the actual modem rejects wrong-host/expired/untrusted certificates.
	uint8_t qualification[12];
	file = SD.open("P59.TLS", O_RDONLY);
	if (file) {
		csvTlsQualified = file.size() == sizeof(qualification) &&
		    file.read(qualification, sizeof(qualification)) == sizeof(qualification) &&
		    !file.getError() && memcmp(qualification, "TLS1", 4) == 0 &&
		    memcmp(qualification + 4, config + 32, 8) == 0;
		file.close();
	}
	memset(config, 0, sizeof(config));
}
// Operator records completed CA, hostname and expiry rejection tests for this modem.
bool csvSaveTlsQualification() {
	if (!csvProvisioned || SD.exists("P59.TLS")) return false;
	uint8_t marker[12] = {'T', 'L', 'S', '1'};
	File file = SD.open("P59.CFG", O_RDONLY);
	if (!file) return false;
	bool valid = file.seek(32) && file.read(marker + 4, 8) == 8 && !file.getError();
	file.close();
	if (!valid) return false;
	if (SD.exists("P59.QLF") && !SD.remove("P59.QLF")) return false;
	file = SD.open("P59.QLF", O_RDWR | O_CREAT);
	if (!file) return false;
	valid = file.write(marker, sizeof(marker)) == sizeof(marker) && file.sync() && !file.getError();
	file.close();
	return valid && SD.rename("P59.QLF", "P59.TLS");
}
