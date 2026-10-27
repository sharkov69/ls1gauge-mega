
#define MAX_BMP_FILES   12
#define NAME_BUF_LEN    14
char bmpFiles[MAX_BMP_FILES][NAME_BUF_LEN];
uint8_t bmpCount = 0;

void loadBMP() {
  bmpCount = 0;
  File root;
  File entry;
  if (!root.open("/")) {
    DBGL("Nie mogę otworzyć katalogu głównego.");
    return;
  }

  while (bmpCount < MAX_BMP_FILES) {
    entry.close();  // MUSI być przed openNext
    if (!entry.openNext(&root, O_RDONLY)) break;

    if (!entry.isDir()) {
      char name[NAME_BUF_LEN];
      entry.getName(name, sizeof(name));

      // Sprawdź czy kończy się na ".bmp" (ignorując wielkość liter)
      size_t len = strlen(name);
      if (len >= 4) {
        char* ext = name + len - 4;
        if ((tolower(ext[0]) == '.') &&
            (tolower(ext[1]) == 'b') &&
            (tolower(ext[2]) == 'm') &&
            (tolower(ext[3]) == 'p')) {
          
          // Zapisz ścieżkę z '/' na początku
          bmpFiles[bmpCount][0] = '/';
          strncpy(bmpFiles[bmpCount] + 1, name, NAME_BUF_LEN - 2);
          bmpFiles[bmpCount][NAME_BUF_LEN - 1] = '\0';

          DBG("Znaleziono BMP: ");
          DBGL(bmpFiles[bmpCount]);

          bmpCount++;
        }
      }
    }
  }

  entry.close();
  root.close();
}


uint16_t read16(File &f) {
  uint16_t result;
  result  = f.read();
  result |= (f.read() << 8);
  return result;
}

/*uint32_t read32(File &f) {
  uint32_t result;
  result  = f.read();
  result |= (f.read() << 8);
  result |= (f.read() << 16);
  result |= (f.read() << 24);
  return result;
}*/

uint32_t read32(File &f) {
  uint32_t result = (uint8_t)f.read();
  result |= (uint32_t)(uint8_t)f.read() << 8;
  result |= (uint32_t)(uint8_t)f.read() << 16;
  result |= (uint32_t)(uint8_t)f.read() << 24;
  return result;
}
File bmpFile;
uint8_t bmpW, bmpH, bmpRow = 0, y = 0;
uint32_t bmpImgOff, bmpRowSize;
bool bmpFlip, bmpDrawing = false;
char currentBMP[14];

void startDrawBMP(const char* filename) {
  DBGL("BMP Start");
  bmpFile = SD.open(filename);
  if (!bmpFile) { bmpDrawing = false; return; }
  if (read16(bmpFile) != 0x4D42) { bmpFile.close(); bmpDrawing = false; return; } 
  (void)read32(bmpFile);
  (void)read32(bmpFile);
  bmpImgOff = read32(bmpFile);
  if (read32(bmpFile) != 40) { bmpFile.close(); bmpDrawing = false; return; }
  bmpW = read32(bmpFile);
  bmpH = read32(bmpFile);
  if (read16(bmpFile) != 1) { bmpFile.close(); bmpDrawing = false; return; }
  if (read16(bmpFile) != 24) { bmpFile.close(); bmpDrawing = false; return; }
  if (read32(bmpFile) != 0) { bmpFile.close(); bmpDrawing = false; return; }
  if (bmpW > 240) { bmpFile.close(); bmpDrawing = false; return; }
  bmpH = abs(bmpH);
  bmpRowSize = (bmpW * 3 + 3) & ~3;
  bmpRow = 0;
  bmpFile.seek(bmpImgOff);
  tft.fillScreen(GC9A01A_BLACK);
  bmpDrawing = true;
}

void continueDrawBMP() {
  if (!bmpDrawing) return;

  int rowsThisLoop = 20; // <-- ile wierszy naraz
  while (rowsThisLoop-- && bmpRow < bmpH) {
    static uint8_t sdbuffer[240*3 + 3];
    bmpFile.read(sdbuffer, bmpRowSize);
    tft.startWrite();
    tft.setAddrWindow(0, bmpH - 1 - bmpRow, bmpW, 1);
    
    for (int col = 0; col < bmpW; col++) {
      int i = col * 3;
      tft.SPI_WRITE16(tft.color565(sdbuffer[i + 2], sdbuffer[i + 1], sdbuffer[i]));
      //tft.pushColor(tft.color565(sdbuffer[i + 2], sdbuffer[i + 1], sdbuffer[i]));
    }
    tft.endWrite();
    bmpRow++;
  }
  if (bmpRow >= bmpH) {
    bmpFile.close();
    bmpDrawing = false;
    DBGL("BMP Stop");
  }
}

void drawBMP(const char* filename) {
  strcpy(currentBMP, filename);
  startDrawBMP(filename);
}

void nextBMP() {
  static uint8_t idx = 0;
  if (bmpCount == 0) return;
  idx = (idx + 1) % bmpCount;
  drawBMP(bmpFiles[idx]);
}