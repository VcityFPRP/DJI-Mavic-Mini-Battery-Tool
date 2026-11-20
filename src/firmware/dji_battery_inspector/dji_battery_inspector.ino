#include <Wire.h>

#define BQ_ADDR 0x0B

bool internalPullups = true;

void applyPullups() {
  if (internalPullups) {
    pinMode(A4, INPUT_PULLUP);
    pinMode(A5, INPUT_PULLUP);
    digitalWrite(A4, HIGH);
    digitalWrite(A5, HIGH);
  } else {
    pinMode(A4, INPUT);
    pinMode(A5, INPUT);
    digitalWrite(A4, LOW);
    digitalWrite(A5, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Wire.begin();
  Wire.setClock(50000); // 50 kHz: reliable for SMBus + clock stretching
  Wire.setWireTimeout(30000, true);
  applyPullups();

  Serial.println(F("\n=================================================="));
  Serial.println(F("  DJI Mavic Mini 1 (WM160) Battery Inspector v2   "));
  Serial.println(F("  BMS: TI bq9003 / BQ40z307 (SMBus 0x0B)          "));
  Serial.println(F("=================================================="));
  printHelp();
}

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F("  s - Scan I2C bus (look for 0x0B)"));
  Serial.println(F("  d - Full Diagnostic Report (Voltages, Cycles, PF, OpStatus)"));
  Serial.println(F("  u - Unseal + Full Access (DJI keys 0xCCDF7EE0 / 0xE0BCBF17)"));
  Serial.println(F("  c - Clear standard PF (0x0029)"));
  Serial.println(F("  z - Clear Error 30074 latch at 0x4A60 (22 11 -> 00 00)"));
  Serial.println(F("  l - LifetimeDataReset (0x0028)"));
  Serial.println(F("  r - DeviceReset (0x0041)"));
  Serial.println(F("  k - SealDevice (0x0030)"));
  Serial.println(F("  f - Dump full Data Flash (0x4000 - 0x5FFF)"));
  Serial.println(F("  b <hex> - Read Data Flash at address (e.g. 'b 4060' or 'b 4A60')"));
  Serial.println(F("  w <addr_hex> <b0> [b1..] - Write bytes to Data Flash (e.g. 'w 4A60 00 00')"));
  Serial.println(F("  p - Toggle Arduino internal 5V pull-ups ON/OFF\n"));
}

bool readWord(uint8_t addr, uint8_t cmd, uint16_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(cmd);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t count = Wire.requestFrom((uint8_t)addr, (uint8_t)2);
  if (count < 2) return false;
  uint8_t l = Wire.read();
  uint8_t h = Wire.read();
  val = l | ((uint16_t)h << 8);
  return true;
}

bool writeWord(uint8_t addr, uint8_t cmd, uint16_t val) {
  Wire.beginTransmission(addr);
  Wire.write(cmd);
  Wire.write((uint8_t)(val & 0xFF));
  Wire.write((uint8_t)((val >> 8) & 0xFF));
  return (Wire.endTransmission() == 0);
}

bool readBlock(uint8_t addr, uint8_t cmd, uint8_t *buf, uint8_t maxLen, uint8_t &outLen) {
  Wire.beginTransmission(addr);
  Wire.write(cmd);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t req = (maxLen + 1 > 32) ? 32 : (maxLen + 1);
  uint8_t count = Wire.requestFrom((uint8_t)addr, req);
  if (count == 0) return false;
  uint8_t len = Wire.read();
  if (len > maxLen) len = maxLen;
  if (len > count - 1) len = count - 1;
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.available() ? Wire.read() : 0;
  }
  while (Wire.available()) Wire.read();
  outLen = len;
  return true;
}

// SMBus Block Write to 0x44: [0x44, count, data...]
bool writeBlock44(const uint8_t *payload, uint8_t len) {
  if (len > 30) return false;
  Wire.beginTransmission(BQ_ADDR);
  Wire.write(0x44);
  Wire.write(len);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(payload[i]);
  }
  return (Wire.endTransmission() == 0);
}

// Read MAC subcommand or DataFlash address via 0x44
// Returns payload AFTER the 2-byte address/subcmd echo
bool readMAC44(uint16_t subcmd, uint8_t *buf, uint8_t maxDataLen, uint8_t &outDataLen) {
  uint8_t cmdBytes[2] = { (uint8_t)(subcmd & 0xFF), (uint8_t)((subcmd >> 8) & 0xFF) };
  if (!writeBlock44(cmdBytes, 2)) return false;
  delay(8);

  Wire.beginTransmission(BQ_ADDR);
  Wire.write(0x44);
  if (Wire.endTransmission(false) != 0) return false;

  // AVR Wire buffer max is 32 bytes: 1 (blockLen) + 2 (echo) + up to 29 data bytes
  uint8_t req = (maxDataLen + 3 > 32) ? 32 : (maxDataLen + 3);
  uint8_t count = Wire.requestFrom((uint8_t)BQ_ADDR, req);
  if (count < 3) {
    while (Wire.available()) Wire.read();
    return false;
  }
  uint8_t blkLen = Wire.read();
  Wire.read(); // echo lo
  Wire.read(); // echo hi
  uint8_t availData = count - 3;
  uint8_t dataLen = (blkLen >= 2) ? (blkLen - 2) : 0;
  if (dataLen > availData) dataLen = availData;
  if (dataLen > maxDataLen) dataLen = maxDataLen;

  for (uint8_t i = 0; i < dataLen; i++) {
    buf[i] = Wire.available() ? Wire.read() : 0;
  }
  while (Wire.available()) Wire.read();
  outDataLen = dataLen;
  return true;
}

void printHexByte(uint8_t b) {
  if (b < 16) Serial.print('0');
  Serial.print(b, HEX);
}

void scanBus() {
  Serial.println(F("\nScanning I2C / SMBus (0x01 - 0x7F)..."));
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  [FOUND] 0x"));
      printHexByte(addr);
      if (addr == BQ_ADDR) {
        Serial.print(F(" <-- DJI Battery BMS (bq9003 / BQ40z307)"));
      }
      Serial.println();
      found++;
    }
  }
  if (found == 0) {
    Serial.println(F("  [ERROR] No I2C devices found!"));
    Serial.println(F("  Check:"));
    Serial.println(F("   1. Wake BMS with 8.0-8.4V (<=0.5A) on B+ (Pin 3/4) & GND (Pin 2/5)"));
    Serial.println(F("   2. Wiring: Pin 1=SCL->A5, Pin 6=SDA->A4, Pin 2/5=GND->GND"));
    Serial.println(F("   3. 4.7k pull-ups to 3.3V recommended (toggle internal 5V pullups with 'p')"));
  }
}

void printDiagnosticReport() {
  Serial.println(F("\n--- DJI Mavic Mini 1 Diagnostic Report ---"));
  uint16_t vPack = 0;
  if (!readWord(BQ_ADDR, 0x09, vPack)) {
    Serial.println(F("No response from 0x0B. Run 's' to scan."));
    return;
  }

  uint16_t current = 0, tempRaw = 0, soc = 0, asoc = 0, remCap = 0, fullCap = 0;
  uint16_t cycleCount = 0, designCap = 0, designVolt = 0, specInfo = 0, bStatus = 0;
  uint16_t cell1 = 0, cell2 = 0, mfgDate = 0, serNum = 0;

  readWord(BQ_ADDR, 0x0A, current);
  readWord(BQ_ADDR, 0x08, tempRaw);
  readWord(BQ_ADDR, 0x0D, soc);
  readWord(BQ_ADDR, 0x0E, asoc);
  readWord(BQ_ADDR, 0x0F, remCap);
  readWord(BQ_ADDR, 0x10, fullCap);
  readWord(BQ_ADDR, 0x17, cycleCount);
  readWord(BQ_ADDR, 0x18, designCap);
  readWord(BQ_ADDR, 0x19, designVolt);
  readWord(BQ_ADDR, 0x1A, specInfo);
  readWord(BQ_ADDR, 0x1B, mfgDate);
  readWord(BQ_ADDR, 0x1C, serNum);
  readWord(BQ_ADDR, 0x16, bStatus);
  readWord(BQ_ADDR, 0x3F, cell1);
  readWord(BQ_ADDR, 0x3E, cell2);

  char strBuf[33];
  uint8_t strLen = 0;

  if (readBlock(BQ_ADDR, 0x20, (uint8_t*)strBuf, 31, strLen)) {
    strBuf[strLen] = '\0';
    Serial.print(F("Manufacturer:  ")); Serial.println(strBuf);
  }
  if (readBlock(BQ_ADDR, 0x21, (uint8_t*)strBuf, 31, strLen)) {
    strBuf[strLen] = '\0';
    Serial.print(F("Device Name:   ")); Serial.println(strBuf);
  }
  if (readBlock(BQ_ADDR, 0x22, (uint8_t*)strBuf, 31, strLen)) {
    strBuf[strLen] = '\0';
    Serial.print(F("Chemistry:     ")); Serial.println(strBuf);
  }
  if (readBlock(BQ_ADDR, 0xD8, (uint8_t*)strBuf, 31, strLen)) {
    strBuf[strLen] = '\0';
    Serial.print(F("DJI S/N (0xD8):")); Serial.println(strBuf);
  }

  Serial.println(F("------------------------------------------"));
  Serial.print(F("Pack Voltage:  ")); Serial.print(vPack); Serial.println(F(" mV"));
  Serial.print(F("Cell 1 (0x3F): ")); Serial.print(cell1); Serial.print(F(" mV | Cell 2 (0x3E): "));
  Serial.print(cell2); Serial.println(F(" mV"));
  Serial.print(F("Cell Delta:    ")); Serial.print(abs((int16_t)cell1 - (int16_t)cell2)); Serial.println(F(" mV"));
  Serial.print(F("Temperature:   ")); Serial.print(((float)tempRaw * 0.1f) - 273.15f, 1); Serial.println(F(" C"));
  Serial.print(F("Current:       ")); Serial.print((int16_t)current); Serial.println(F(" mA"));
  Serial.print(F("RSOC / ASOC:   ")); Serial.print(soc); Serial.print(F("% / ")); Serial.print(asoc); Serial.println(F("%"));
  Serial.print(F("Capacity:      ")); Serial.print(remCap); Serial.print(F(" / ")); Serial.print(fullCap);
  Serial.print(F(" mAh (Design: ")); Serial.print(designCap); Serial.println(F(" mAh)"));
  Serial.print(F("Cycle Count:   ")); Serial.println(cycleCount);
  Serial.print(F("SpecInfo:      0x")); Serial.println(specInfo, HEX);
  Serial.print(F("BatteryStatus: 0x")); Serial.println(bStatus, HEX);

  uint8_t blk[29];
  uint8_t blkLen = 0;

  if (readMAC44(0x0002, blk, 16, blkLen)) {
    Serial.print(F("FW Ver (0002): "));
    for (uint8_t i = 0; i < blkLen; i++) { printHexByte(blk[i]); Serial.print(' '); }
    Serial.println();
  }

  if (readMAC44(0x0054, blk, 8, blkLen) && blkLen >= 4) {
    uint32_t op = (uint32_t)blk[0] | ((uint32_t)blk[1] << 8) | ((uint32_t)blk[2] << 16) | ((uint32_t)blk[3] << 24);
    uint8_t sec = (op >> 8) & 0x03;
    Serial.print(F("OperationStat: 0x")); Serial.print(op, HEX);
    Serial.print(F(" [SEC=")); Serial.print(sec);
    if (sec == 3) Serial.print(F(" SEALED"));
    else if (sec == 2) Serial.print(F(" UNSEALED"));
    else if (sec == 1) Serial.print(F(" FULL ACCESS"));
    Serial.print(F("]"));
    if (op & (1UL << 14)) Serial.print(F(" [PF]"));
    if (op & (1UL << 13)) Serial.print(F(" [XDSG]"));
    if (op & (1UL << 12)) Serial.print(F(" [XCHG]"));
    if (op & (1UL << 2))  Serial.print(F(" [FUSE]"));
    if (op & (1UL << 1))  Serial.print(F(" [CHG]"));
    if (op & (1UL << 0))  Serial.print(F(" [DSG]"));
    Serial.println();
  }

  if (readMAC44(0x0053, blk, 8, blkLen) && blkLen >= 4) {
    uint32_t pf = (uint32_t)blk[0] | ((uint32_t)blk[1] << 8) | ((uint32_t)blk[2] << 16) | ((uint32_t)blk[3] << 24);
    Serial.print(F("PFStatus:      0x")); Serial.println(pf, HEX);
  }

  if (readMAC44(0x0051, blk, 8, blkLen) && blkLen >= 4) {
    uint32_t ss = (uint32_t)blk[0] | ((uint32_t)blk[1] << 8) | ((uint32_t)blk[2] << 16) | ((uint32_t)blk[3] << 24);
    Serial.print(F("SafetyStatus:  0x")); Serial.println(ss, HEX);
  }
  Serial.println(F("------------------------------------------\n"));
}

void unsealAndFullAccess() {
  Serial.println(F("\nWaiting 4.5s silent bus before sending DJI Unseal key..."));
  delay(4500);

  // Unseal: 0xCCDF7EE0 -> 0x7EE0 then 0xCCDF
  Serial.println(F("Sending Unseal (0x7EE0 -> 0xCCDF)..."));
  writeWord(BQ_ADDR, 0x00, 0x7EE0);
  delay(50);
  writeWord(BQ_ADDR, 0x00, 0xCCDF);
  delay(500);

  // Full Access: 0xE0BCBF17 -> 0xBF17 then 0xE0BC
  Serial.println(F("Sending Full Access (0xBF17 -> 0xE0BC)..."));
  writeWord(BQ_ADDR, 0x00, 0xBF17);
  delay(50);
  writeWord(BQ_ADDR, 0x00, 0xE0BC);
  delay(500);

  uint8_t blk[8];
  uint8_t blkLen = 0;
  if (readMAC44(0x0054, blk, 8, blkLen) && blkLen >= 4) {
    uint32_t op = (uint32_t)blk[0] | ((uint32_t)blk[1] << 8) | ((uint32_t)blk[2] << 16) | ((uint32_t)blk[3] << 24);
    uint8_t sec = (op >> 8) & 0x03;
    Serial.print(F("New SEC state: ")); Serial.print(sec);
    if (sec == 1) Serial.println(F(" (FULL ACCESS - SUCCESS!)"));
    else if (sec == 2) Serial.println(F(" (UNSEALED - FAS rejected)"));
    else Serial.println(F(" (STILL SEALED!)"));
  } else {
    Serial.println(F("Failed to read OperationStatus after unseal."));
  }
}

void readFlashAt(uint16_t addr, uint8_t len) {
  uint8_t blk[29];
  uint8_t outLen = 0;
  if (!readMAC44(addr, blk, len, outLen)) {
    Serial.print(F("ERR_READ_0x")); Serial.println(addr, HEX);
    return;
  }
  Serial.print(F("0x"));
  if (addr < 0x1000) Serial.print('0');
  Serial.print(addr, HEX);
  Serial.print(F(": "));
  for (uint8_t i = 0; i < outLen; i++) {
    printHexByte(blk[i]);
    Serial.print(' ');
  }
  Serial.println();
}

void dumpFullFlash() {
  Serial.println(F("\nSTART_FLASH_DUMP (0x4000 - 0x5FFF)"));
  for (uint16_t addr = 0x4000; addr < 0x6000; addr += 16) {
    readFlashAt(addr, 16);
    delay(5);
  }
  Serial.println(F("END_FLASH_DUMP"));
}

void clear30074() {
  uint8_t blk[16];
  uint8_t outLen = 0;
  if (!readMAC44(0x4A60, blk, 8, outLen) || outLen < 2) {
    Serial.println(F("Failed to read 0x4A60 (need Full Access SEC=1)."));
    return;
  }
  Serial.print(F("0x4A60 before: "));
  printHexByte(blk[0]); Serial.print(' '); printHexByte(blk[1]); Serial.println();

  if (blk[0] == 0x22 && blk[1] == 0x11) {
    uint8_t payload[4] = { 0x60, 0x4A, 0x00, 0x00 };
    if (writeBlock44(payload, 4)) {
      delay(50);
      Serial.println(F("Wrote 00 00 to 0x4A60. Verifying..."));
      readFlashAt(0x4A60, 8);
    } else {
      Serial.println(F("Write to 0x4A60 failed!"));
    }
  } else {
    Serial.println(F("0x4A60 does not contain 22 11 - skipping write for safety."));
  }
}

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;
  char cmd = tolower(line.charAt(0));

  if (cmd == 's') scanBus();
  else if (cmd == 'd') printDiagnosticReport();
  else if (cmd == 'u') unsealAndFullAccess();
  else if (cmd == 'c') {
    Serial.println(F("Sending PermanentFailDataReset (0x0029)..."));
    Serial.println(writeWord(BQ_ADDR, 0x00, 0x0029) ? F("ACK OK") : F("NACK"));
  } else if (cmd == 'z') {
    clear30074();
  } else if (cmd == 'l') {
    Serial.println(F("Sending LifetimeDataReset (0x0028)..."));
    Serial.println(writeWord(BQ_ADDR, 0x00, 0x0028) ? F("ACK OK") : F("NACK"));
  } else if (cmd == 'r') {
    Serial.println(F("Sending DeviceReset (0x0041)..."));
    Serial.println(writeWord(BQ_ADDR, 0x00, 0x0041) ? F("ACK OK") : F("NACK"));
  } else if (cmd == 'k') {
    Serial.println(F("Sending SealDevice (0x0030)..."));
    Serial.println(writeWord(BQ_ADDR, 0x00, 0x0030) ? F("ACK OK") : F("NACK"));
  } else if (cmd == 'f') {
    dumpFullFlash();
  } else if (cmd == 'p') {
    internalPullups = !internalPullups;
    applyPullups();
    Serial.print(F("Internal 5V pull-ups: "));
    Serial.println(internalPullups ? F("ON") : F("OFF"));
  } else if (cmd == 'b') {
    int sp = line.indexOf(' ');
    if (sp > 0) {
      uint16_t addr = (uint16_t)strtol(line.substring(sp + 1).c_str(), NULL, 16);
      readFlashAt(addr, 16);
    }
  } else if (cmd == 'w') {
    // Parse: w <addr_hex> <b0> <b1> ...
    int sp = line.indexOf(' ');
    if (sp > 0) {
      String rest = line.substring(sp + 1);
      rest.trim();
      int sp2 = rest.indexOf(' ');
      if (sp2 > 0) {
        uint16_t addr = (uint16_t)strtol(rest.substring(0, sp2).c_str(), NULL, 16);
        uint8_t payload[30];
        payload[0] = (uint8_t)(addr & 0xFF);
        payload[1] = (uint8_t)((addr >> 8) & 0xFF);
        uint8_t idx = 2;
        String bytesStr = rest.substring(sp2 + 1);
        bytesStr.trim();
        while (bytesStr.length() > 0 && idx < 30) {
          int nextSp = bytesStr.indexOf(' ');
          String tok = (nextSp > 0) ? bytesStr.substring(0, nextSp) : bytesStr;
          payload[idx++] = (uint8_t)strtol(tok.c_str(), NULL, 16);
          if (nextSp < 0) break;
          bytesStr = bytesStr.substring(nextSp + 1);
          bytesStr.trim();
        }
        if (idx > 2) {
          Serial.print(F("Writing ")); Serial.print(idx - 2); Serial.print(F(" bytes to 0x"));
          Serial.println(addr, HEX);
          Serial.println(writeBlock44(payload, idx) ? F("Write ACK OK") : F("Write NACK!"));
        }
      }
    }
  } else if (cmd == 'h' || cmd == '?') {
    printHelp();
  }
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleLine(line);
  }
}
