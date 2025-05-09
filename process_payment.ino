#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 9
#define SS_PIN 10

MFRC522 rfid(SS_PIN, RST_PIN); // Create MFRC522 instance
MFRC522::MIFARE_Key key;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("--------------------------------");
  Serial.println("    Parking Fee Calculator");
  Serial.println("--------------------------------");
  Serial.println("Scan the RFID card to start...");
  Serial.println("--------------------------------");

  // Prepare the default key (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF)
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

void loop() {
  // Check for a new card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Verify the UID (assuming 5B ED 5F 3B from previous context)
  byte expectedUID[] = {0x23, 0x43, 0xB0, 0xFC};
  bool uidMatch = true;
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] != expectedUID[i]) {
      uidMatch = false;
      break;
    }
  }

  if (!uidMatch) {
    Serial.println("This card's UID does not match 23 43 B0 FC.");
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  // Authenticate for sector 1 (blocks 4-7) using key A
  byte blockAddr = 4;
  MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed: ");
    Serial.println(rfid.GetStatusCodeName(status));
    return;
  }

  // Read current license plate and cash amount
  byte buffer[18];
  byte size = 18;
  status = rfid.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Read failed: ");
    Serial.println(rfid.GetStatusCodeName(status));
    return;
  }
  String licensePlate = "";
  for (byte i = 0; i < 16; i++) {
    if (buffer[i] != ' ') licensePlate += (char)buffer[i];
  }
  blockAddr = 5;
  status = rfid.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Read failed: ");
    Serial.println(rfid.GetStatusCodeName(status));
    return;
  }
  String cashStr = "";
  for (byte i = 0; i < 16; i++) {
    if (buffer[i] != ' ') cashStr += (char)buffer[i];
  }
  long currentCash = cashStr.toInt();

  // Prompt user for parking hours
  Serial.println("--------------------------------");
  Serial.print("Enter the number of hours parked for license plate ");
  Serial.print(licensePlate);
  Serial.println(":");
  while (Serial.available() == 0) {} // Wait for input
  float hours = Serial.parseFloat();
  Serial.read(); // Clear the buffer
  Serial.println("--------------------------------");

  // Calculate charge
  float minutes = hours * 60;
  long charge = 0;
  if (minutes > 30) {
    charge = ((long)((minutes - 30 + 29) / 30)) * 100; // Round up to next 30-min period
  }
  long newCash = currentCash - charge;
  if (newCash < 0) {
    Serial.println("--------------------------------");
    Serial.println("Insufficient funds! Please add more cash.");
    Serial.println("--------------------------------");
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  // Prepare and write new cash amount
  String newCashStr = String(newCash);
  for (byte i = 0; i < 16; i++) {
    buffer[i] = (i < newCashStr.length()) ? newCashStr[i] : ' ';
  }
  blockAddr = 5;
  Serial.print("Updating cash amount to block 5...");
  status = rfid.MIFARE_Write(blockAddr, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Write failed: ");
    Serial.println(rfid.GetStatusCodeName(status));
    return;
  }
  Serial.println("Done");

  // Styled output
  Serial.println("--------------------------------");
  Serial.println("    Parking Fee Calculation");
  Serial.println("--------------------------------");
  Serial.print("License Plate: ");
  Serial.println(licensePlate);
  Serial.print("Parking Time: ");
  Serial.print(hours);
  Serial.println(" hours");
  Serial.print("Charge: ");
  Serial.print(charge);
  Serial.println(" units");
  Serial.print("Previous Balance: ");
  Serial.print(currentCash);
  Serial.println(" units");
  Serial.print("New Balance: ");
  Serial.print(newCash);
  Serial.println(" units");
  Serial.println("--------------------------------");

  // Halt PICC and stop encryption
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}
