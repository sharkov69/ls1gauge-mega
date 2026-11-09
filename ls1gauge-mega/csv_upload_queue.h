#pragma once
#include <EEPROM.h>
#include <SdFat.h>
#include <stdint.h>
#include <string.h>

// EEPROM 64..95 is reserved for this allocator; logger/settings use 0..18.
// Commit the allocation before publishing it on SD, so a torn SD write wastes
// an ID rather than letting another physical file reuse an acknowledged ID.
static uint32_t csvQueueCrc(const uint8_t *p, uint8_t n) {
	uint32_t crc = 0xffffffffUL;
	while (n--) {
		crc ^= *p++;
		for (uint8_t b = 0; b < 8; ++b)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
	}
	return ~crc;
}
static bool csvQueueCounterRead(uint8_t slot, uint64_t &value) {
	uint8_t bytes[12];
	const int address = 64 + slot * 16;
	if (EEPROM.read(address + 15) != 0xa5) return false;
	for (uint8_t i = 0; i < 12; ++i) bytes[i] = EEPROM.read(address + i);
	uint32_t crc;
	memcpy(&value, bytes, 8); memcpy(&crc, bytes + 8, 4);
	return value && csvQueueCrc(bytes, 8) == crc;
}
static bool csvQueueAllocate(uint8_t *id, const uint8_t *nameSpace) {
	uint64_t values[2] = {0, 0};
	bool valid[2] = {csvQueueCounterRead(0, values[0]), csvQueueCounterRead(1, values[1])};
	for (uint8_t i=0;i<2;++i)
		if (!valid[i] && EEPROM.read(79+i*16)==0xa5) return false;
	if (!valid[0] && !valid[1]) {
		// An interrupted first allocation has never published an ID. Its commit
		// marker remains erased/invalid; later corruption must fail closed.
		if (EEPROM.read(79) == 0xa5 || EEPROM.read(95) == 0xa5) return false;
	}
	const uint8_t latest = valid[1] && (!valid[0] || values[1] > values[0]) ? 1 : 0;
	uint64_t next = (valid[latest] ? values[latest] : 0) + 1;
	if (!next) return false;
	const uint8_t target = valid[latest] ? latest ^ 1 : 0;
	const int address = 64 + target * 16;
	uint8_t bytes[12]; memcpy(bytes, &next, 8);
	uint32_t crc = csvQueueCrc(bytes, 8); memcpy(bytes + 8, &crc, 4);
	EEPROM.update(address + 15, 0);
	for (uint8_t i = 0; i < 12; ++i) EEPROM.update(address + i, bytes[i]);
	EEPROM.update(address + 15, 0xa5);
	uint64_t check = 0;
	if (!csvQueueCounterRead(target, check) || check != next) return false;
	memcpy(id, nameSpace, 8); memcpy(id + 8, &next, 8);
	return true;
}

bool csvQueueReady = false;
char csvQueueName[13] = {};
uint8_t csvQueueId[16] = {};
static uint8_t csvQueueNamespace[8];
static File csvQueueDirectory;
static bool csvQueueEnabled = false;
static uint32_t csvQueueScanAt = 0, csvQueueSelectedSize = 0;

static bool csvQueueSidecar(const char *name, char *sidecar) {
	if (strlen(name) != 12 || (name[0] != 'l' && name[0] != 'L') || name[1] != '-' || name[8] != '.') return false;
	for (uint8_t i = 2; i < 8; ++i) if (name[i] < '0' || name[i] > '9') return false;
	uint8_t revision;
	char ext[4]; for (uint8_t i=0;i<3;++i) ext[i]=name[9+i] | 32; ext[3]=0;
	if (!strcmp(ext, "csv")) revision = 0;
	else if (!strcmp(ext, "bak")) revision = 1;
	else if (ext[0]=='b' && ext[1]>='0' && ext[1]<='9' && ext[2]>='0' && ext[2]<='9' && (ext[1]!='0' || ext[2]!='0'))
		revision = 1 + (ext[1]-'0')*10 + ext[2]-'0';
	else return false;
	memcpy(sidecar, name, 13);
	const char hex[]="0123456789abcdef";
	sidecar[9]='u'; sidecar[10]=hex[revision>>4]; sidecar[11]=hex[revision&15];
	return true;
}

// Two CRC-protected slots retain the last durable ID and size if power fails
// during an update. Newer allocation wins; for one ID the larger size wins.
static bool csvQueueIdentity(const char *name, uint32_t size, uint8_t *id) {
	char path[13]; if (!csvQueueSidecar(name,path)) return false;
	const bool exists = SD.exists(path);
	File metadata = SD.open(path, O_RDWR | O_CREAT);
	if (!metadata) return false;
	uint8_t records[2][24] = {};
	bool valid[2] = {false,false};
	uint32_t sizes[2] = {0,0}; uint64_t counters[2] = {0,0};
	for (uint8_t i=0;i<2;++i) {
		if (!metadata.seek(i*24) || metadata.read(records[i],24)!=24) continue;
		uint32_t crc; memcpy(&crc,records[i]+20,4);
		memcpy(&sizes[i],records[i]+16,4); memcpy(&counters[i],records[i]+8,8);
		valid[i] = counters[i] && !memcmp(records[i],csvQueueNamespace,8) && csvQueueCrc(records[i],20)==crc;
	}
	if (metadata.getError()) { metadata.close(); return false; }
	uint8_t latest = valid[1] && (!valid[0] || counters[1]>counters[0] || (counters[1]==counters[0] && sizes[1]>sizes[0])) ? 1 : 0;
	if (exists && !valid[0] && !valid[1]) { metadata.close(); return false; }
	const bool fresh = !valid[latest] || size < sizes[latest];
	if (fresh) {
		if (!csvQueueAllocate(id,csvQueueNamespace)) { metadata.close(); return false; }
	} else memcpy(id,records[latest],16);
	if (fresh || size > sizes[latest]) {
		uint8_t record[24]; memcpy(record,id,16); memcpy(record+16,&size,4);
		uint32_t crc=csvQueueCrc(record,20); memcpy(record+20,&crc,4);
		const uint8_t target=valid[latest] ? latest^1 : 0;
		if (!metadata.seek(target*24) || metadata.write(record,24)!=24 || !metadata.sync()) { metadata.close(); return false; }
	}
	metadata.close(); return true;
}

void csvQueueInit(const uint8_t *nameSpace) {
	csvQueueDirectory.close(); csvQueueReady=false; csvQueueScanAt=0;
	uint8_t nonzero=0; for(uint8_t i=0;i<8;++i) nonzero|=nameSpace[i];
	csvQueueEnabled=nonzero!=0; memcpy(csvQueueNamespace,nameSpace,8);
}
void csvQueueRelease() {
	if (csvQueueReady && !sdUploadFaultLatched) {
		const uint32_t size=!strcmp(csvQueueName,sdUploadName) ? sdUploadSyncedSize : csvQueueSelectedSize;
		uint8_t id[16]; csvQueueIdentity(csvQueueName,size,id);
	}
	csvQueueReady=false;
}
uint32_t csvQueueLimit() {
	if (!csvQueueReady || sdUploadFaultLatched) return 0;
	if (!strcmp(csvQueueName,sdUploadName)) return sdUploadFailed ? 0 : sdUploadSyncedSize;
	return csvQueueSelectedSize;
}
void csvQueueTick() {
	if (!csvQueueEnabled || csvQueueReady || sdUploadFaultLatched) return;
	if (!csvQueueDirectory) {
		if ((int32_t)(millis()-csvQueueScanAt)<0) return;
		csvQueueDirectory=SD.open("/",O_RDONLY);
		if (!csvQueueDirectory) {csvQueueScanAt=millis()+5000; return;}
	}
	File entry;
	if (!entry.openNext(&csvQueueDirectory,O_RDONLY)) {
		csvQueueDirectory.close(); csvQueueScanAt=millis()+30000; return;
	}
	char name[13]={}, sidecar[13];
	entry.getName(name,sizeof(name));
	const bool candidate=!entry.isDir() && csvQueueSidecar(name,sidecar);
	const uint32_t size=entry.size(); const bool error=entry.getError()!=0; entry.close();
	if (!candidate || error || !size || size>=0xffffffffUL) return;
	if (!strcmp(name,sdUploadName) && (sdUploadFailed || !sdUploadSyncedSize)) return;
	// Persist only the committed boundary, never the logger's buffered tail.
	const uint32_t limit=!strcmp(name,sdUploadName) ? sdUploadSyncedSize : size;
	if (!csvQueueIdentity(name,limit,csvQueueId)) return;
	memcpy(csvQueueName,name,13); csvQueueSelectedSize=limit; csvQueueReady=true;
}
bool csvQueueNewFile(const char *name) {
	char sidecar[13]; if (!csvQueueSidecar(name,sidecar)) return false;
	if (csvQueueReady && !strcmp(csvQueueName,name)) csvQueueReady=false;
	return !SD.exists(sidecar) || SD.remove(sidecar);
}
bool csvQueueRename(const char *oldName,const char *newName) {
	char oldSidecar[13],newSidecar[13];
	if (!csvQueueSidecar(oldName,oldSidecar) || !csvQueueSidecar(newName,newSidecar)) return false;
	if (SD.exists(oldSidecar) && (SD.exists(newSidecar) || !SD.rename(oldSidecar,newSidecar))) return false;
	if (csvQueueReady && !strcmp(csvQueueName,oldName)) memcpy(csvQueueName,newName,13);
	return true;
}
