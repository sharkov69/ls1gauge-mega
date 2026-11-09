#pragma once
#include <SdFat.h>

// Reopen for each bounded read so a growing file's metadata is refreshed.
// Caller owns the stable filename and supplies only the successfully synced size.
class CsvUploadSdSource {
  public:
	CsvUploadSdSource(SdFat &filesystem, const char *filename) : sd(filesystem), name(filename) {}
	static int read(void *context, uint32_t offset, uint8_t *dest, uint16_t length) {
		if (!context || !dest)
			return -1;
		CsvUploadSdSource &source = *static_cast<CsvUploadSdSource *>(context);
		if (!source.name)
			return -1;
		File file = source.sd.open(source.name, O_RDONLY);
		if (!file)
			return -1;
		const uint32_t size = file.size();
		int count = -1;
		if (offset <= size && length <= size - offset && file.seek(offset)) {
			count = file.read(dest, length);
			if (file.getError() || count != length)
				count = -1;
		}
		file.close();
		return count;
	}
  private:
	SdFat &sd;
	const char *name;
};
