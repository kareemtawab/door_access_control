/*
   --------------------------------------------------------------------------------------------------------------------
   Example sketch/program showing An Arduino Door Access Control featuring RFID, EEPROM, doorstrike
   --------------------------------------------------------------------------------------------------------------------
   This is a MFRC522 library example; for further details and other examples see: https://github.com/miguelbalboa/rfid

   This example showing a complete Door Access Control System

  Simple Work Flow (not limited to) :
                                     +---------+
  +----------------------------------->READ TAGS+^------------------------------------------+
  |                              +--------------------+                                     |
  |                              |                    |                                     |
  |                              |                    |                                     |
  |                         +----v-----+        +-----v----+                                |
  |                         |ADMIN TAG|        |OTHER TAGS|                                |
  |                         +--+-------+        ++-------------+                            |
  |                            |                 |             |                            |
  |                            |                 |             |                            |
  |                      +-----v---+        +----v----+   +----v------+                     |
  |         +------------+READ TAGS+---+    |KNOWN TAG|   |UNKNOWN TAG|                     |
  |         |            +-+-------+   |    +-----------+ +------------------+              |
  |         |              |           |                |                    |              |
  |    +----v-----+   +----v----+   +--v--------+     +-v----------+  +------v----+         |
  |    |ADMIN TAG|   |KNOWN TAG|   |UNKNOWN TAG|     |GRANT ACCESS|  |DENY ACCESS|         |
  |    +----------+   +---+-----+   +-----+-----+     +-----+------+  +-----+-----+         |
  |                       |               |                 |               |               |
  |       +----+     +----v------+     +--v---+             |               +--------------->
  +-------+EXIT|     |DELETE FROM|     |ADD TO|             |                               |
        +----+     |  EEPROM   |     |EEPROM|             |                               |
                   +-----------+     +------+             +-------------------------------+


   Use a ADMIN Card which is act as Programmer then you can able to choose card holders who will granted access or not

 * **Easy User Interface**

   Just one RFID tag needed whether Delete or Add Tags. You can choose to use Leds for output or Serial LCD module to inform users.

 * **Stores Information on EEPROM**

   Information stored on non volatile Arduino's EEPROM memory to preserve Users' tag and ADMIN Card. No Information lost
   if power lost. EEPROM has unlimited Read cycle but roughly 100,000 limited Write cycle.

 * **Security**
   To keep it simple we are going to use Tag's Unique IDs. It's simple and not hacker proof.

   @license Released into the public domain.

   Typical pin layout used:
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
   SPI SS      SDA(SS)      10            53        D10        10               10
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
*/

#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define doorstrike 5     // Set doorstrike Pin
#define wipebutton 8     // Button pin for WipeMode
#define buzzer 2     // Button pin for WipeMode

boolean match = false;          // initialize card match to false
boolean programMode = false;  // initialize programming mode to false
boolean replaceADMIN = false;

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
char readuidhexformat[10] = {0};
char adminuidhexformat[10] = {0};
byte ADMINCard[4];   // Stores ADMIN card's ID read from EEPROM
byte upbar[8] = {
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
};
// Create MFRC522 instance.
#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display

///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  lcd.backlight();
  lcd.init();
  lcd.createChar(0, upbar);

  Serial.begin(115200);  // Initialize serial communications with PC

  pinMode(wipebutton, INPUT_PULLUP);   // Enable pin's pull up resistor
  pinMode(doorstrike, OUTPUT);
  digitalWrite(doorstrike, LOW);       // Make sure door is locked
  pinMode(buzzer, OUTPUT);

  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware

  //If you set Antenna Gain to Max it will increase reading distance
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  ShowReaderDetails();  // Show details of PCD - MFRC522 Card Reader details

  drawdefaultscreen();

  // Check if ADMIN card defined, if not let user choose a ADMIN card
  // This also useful to just redefine the ADMIN Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No ADMIN Card Defined"));
    Serial.println(F("Scan A PICC to Define as ADMIN Card"));
    do {
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined ADMIN Card.
    lcd.clear();
    lcd.setCursor(1, 1);
    lcd.print("ADMIN TAG DEFINED!");
    lcd.setCursor(0, 3);
    lcd.print("TAG UID: ");
    lcd.print(readuidhexformat);
    Serial.println(F("ADMIN Card Defined"));
    delay(5000);
  }
  drawdefaultscreen();
  Serial.println(F("-------------------"));
  Serial.println(F("ADMIN Card's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {          // Read ADMIN Card's UID from EEPROM
    ADMINCard[i] = EEPROM.read(2 + i);    // Write it to ADMINCard
  }
  sprintf(adminuidhexformat, "%02X:%02X:%02X:%02X", ADMINCard[0], ADMINCard[1], ADMINCard[2], ADMINCard[3]);
  Serial.println(adminuidhexformat);
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting PICCs to be scanned"));

}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
  do {
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
    // When device is in use if wipe button pressed for 3 seconds initialize ADMIN Card wiping
    if (digitalRead(wipebutton) == LOW) { // Check if button is pressed
      digitalWrite(buzzer, HIGH);
      delay(10);
      digitalWrite(buzzer, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WIPE BUTTON PRESSED");
      lcd.setCursor(0, 1);
      lcd.print("HOLD FOR 3 SECONDS");
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("You have 3 seconds to Cancel"));
      bool buttonState = monitorwipebuttonutton(3000); // Give user enough time to cancel operation
      if (buttonState == true && digitalRead(wipebutton) == LOW) {    // If button still be pressed, wipe EEPROM
        digitalWrite(buzzer, HIGH);
        for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
          if (EEPROM.read(x) == 0) {              //If EEPROM address 0
            // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
          }
          else {
            EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
          }
        }
        digitalWrite(buzzer, HIGH);
        delay(10);
        digitalWrite(buzzer, LOW);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ALL TAGS ERASED!");
        lcd.setCursor(0, 3);
        lcd.print("POWER CYCLE NEEDED!");
        Serial.println(F("All Cards Erased from device"));
        Serial.println(F("Power CyCle Needed!"));
        digitalWrite(buzzer, LOW);
        while (1);
      }
      digitalWrite(buzzer, HIGH);
      delay(10);
      digitalWrite(buzzer, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WIPE BUTTON RELEASED");
      lcd.setCursor(0, 1);
      lcd.print("WIPE CANCELLED!");
      Serial.println(F("ADMIN Card Erase Cancelled"));
      delay(2500);
      drawdefaultscreen();
    }
  }
  while (!successRead);   //the program will not go further while you are not getting a successful read
  if (programMode) {
    if ( isADMIN(readCard) ) { //When in program mode check First If ADMIN card scanned again to exit program mode
      digitalWrite(buzzer, HIGH);
      delay(10);
      digitalWrite(buzzer, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ADMIN TAG DETECTED");
      lcd.setCursor(0, 2);
      lcd.print("EXITING");
      lcd.setCursor(0, 3);
      lcd.print("PROGRAM MODE ...");
      Serial.println(F("ADMIN Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      delay(2500);
      drawdefaultscreen();
      return;
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        digitalWrite(buzzer, HIGH);
        delay(10);
        digitalWrite(buzzer, LOW);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TAG UID: ");
        lcd.print(readuidhexformat);
        lcd.setCursor(0, 1);
        lcd.print("OLD TAG REMOVED");
        lcd.setCursor(0, 3);
        lcd.print("PROGRAM MODE ACTIVE");
        delay(2500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SWIPE ANOTHER TAG");
        lcd.setCursor(0, 1);
        lcd.print("OR SWIPE ADMIN TAG");
        lcd.setCursor(0, 3);
        lcd.print("PROGRAM MODE ACTIVE");
        Serial.println(F("I know this PICC, removing..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
      else {                    // If scanned card is not known add it
        digitalWrite(buzzer, HIGH);
        delay(10);
        digitalWrite(buzzer, LOW);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TAG UID: ");
        lcd.print(readuidhexformat);
        lcd.setCursor(0, 1);
        lcd.print("NEW TAG ADDED");
        lcd.setCursor(0, 3);
        lcd.print("PROGRAM MODE ACTIVE");
        delay(2500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SWIPE ANOTHER TAG");
        lcd.setCursor(0, 1);
        lcd.print("OR SWIPE ADMIN TAG");
        lcd.setCursor(0, 3);
        lcd.print("PROGRAM MODE ACTIVE");
        Serial.println(F("I do not know this PICC, adding..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
    }
  }
  else {
    if ( isADMIN(readCard)) {    // If scanned card's ID matches ADMIN Card's ID - enter program mode
      programMode = true;
      digitalWrite(buzzer, HIGH);
      delay(10);
      digitalWrite(buzzer, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ADMIN TAG DETECTED");
      lcd.setCursor(0, 1);
      lcd.print("SCAN NEW/OLD TAGS");
      lcd.setCursor(0, 3);
      lcd.print("PROGRAM MODE ACTIVE");
      Serial.println(F("Hello ADMIN - Entered Program Mode"));
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      Serial.print(F("I have "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      Serial.println(F("Scan ADMIN Card again to Exit Program Mode"));
      Serial.println(F("-----------------------------"));
    }
    else {
      if ( findID(readCard) || readCard[0] == 8) { // If not, see if the card is in the EEPROM
        lcd.clear();

        if (readCard[0] == 8) {
          lcd.setCursor(3, 0);
          lcd.print("ACCESS GRANTED");
          lcd.setCursor(0, 1);
          for (int i = 1; i <= 20; i++) {
            lcd.write(byte(0));
          }
          lcd.setCursor(0, 2);
          lcd.print("PHONE DEVICE UID:");
          lcd.setCursor(0, 3);
          lcd.print(readuidhexformat);
        }
        else {
          lcd.setCursor(3, 0);
          lcd.print("ACCESS GRANTED");
          lcd.setCursor(0, 1);
          for (int i = 1; i <= 20; i++) {
            lcd.write(byte(0));
          }
          lcd.setCursor(0, 2);
          lcd.print("TAG UID:");
          lcd.setCursor(0, 3);
          lcd.print(readuidhexformat);
        }
        Serial.println(F("Welcome, You shall pass"));
        granted(5000);         // Open the door lock for 5000 ms
        delay(2500);
        drawdefaultscreen();
      }
      else {      // If not, show that the ID was not valid
        lcd.clear();
        lcd.setCursor(3, 0);
        lcd.print("ACCESS DENIED");
        lcd.setCursor(0, 1);
        for (int i = 1; i <= 20; i++) {
          lcd.write(byte(0));
        }
        lcd.setCursor(0, 2);
        lcd.print("TAG UID:");
        lcd.setCursor(0, 3);
        lcd.print(readuidhexformat);
        Serial.println(F("You shall not pass"));
        denied();
        delay(2500);
        drawdefaultscreen();
      }
    }
  }
}

//////////////////////////////////////// Default Screen ///////////////////////////////////
void drawdefaultscreen() {
  digitalWrite(buzzer, HIGH);
  delay(10);
  digitalWrite(buzzer, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("13.56MHz RFID ACCESS");
  lcd.setCursor(3, 1);
  lcd.print("CONTROL SYSTEM");
  if (EEPROM.read(1) != 143) {
    lcd.setCursor(0, 2);
    lcd.print("NO ADMIN TAG SET!");
    lcd.setCursor(0, 3);
    lcd.print("SWIPE NEW ADMIN TAG");
  }
  else {
    for ( uint8_t i = 0; i < 4; i++ ) {          // Read ADMIN Card's UID from EEPROM
      ADMINCard[i] = EEPROM.read(2 + i);    // Write it to ADMINCard
      Serial.print(ADMINCard[i], HEX);
    }
    lcd.setCursor(0, 3);
    lcd.print("ADMIN UID: ");
    lcd.print(ADMINCard[0], HEX);
    lcd.print(ADMINCard[1], HEX);
    lcd.print(ADMINCard[2], HEX);
    lcd.print(ADMINCard[3], HEX);
  }

}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted ( uint16_t setDelay) {
  digitalWrite(doorstrike, HIGH);
  lcd.noBacklight();
  digitalWrite(buzzer, HIGH);
  delay(30);
  lcd.backlight();
  delay(470);
  digitalWrite(buzzer, LOW);
  delay(setDelay - 500);      // Hold door lock open for given seconds
  digitalWrite(doorstrike, LOW);
  digitalWrite(buzzer, HIGH);
  delay(500);
  digitalWrite(buzzer, LOW);
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(doorstrike, LOW);   // Relock door
  lcd.noBacklight();
  digitalWrite(buzzer, HIGH);
  delay(30);
  lcd.backlight();
  digitalWrite(buzzer, LOW);
  delay(100);
  lcd.noBacklight();
  digitalWrite(buzzer, HIGH);
  delay(30);
  lcd.backlight();
  digitalWrite(buzzer, LOW);
  delay(100);
  lcd.noBacklight();
  digitalWrite(buzzer, HIGH);
  delay(30);
  lcd.backlight();
  digitalWrite(buzzer, LOW);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  char uid [50];
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
  }
  sprintf(readuidhexformat, "%02X:%02X:%02X:%02X", readCard[0], readCard[1], readCard[2], readCard[3]);
  Serial.println(readuidhexformat);
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    while (true); // do not go further
  }
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}

////////////////////// Check readCard IF is ADMINCard   ///////////////////////////////////
// Check to see if the ID passed is the ADMIN programing card
boolean isADMIN( byte test[] ) {
  if ( checkTwo( test, ADMINCard ) )
    return true;
  else
    return false;
}

bool monitorwipebuttonutton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(wipebutton) != LOW)
        return false;
    }
  }
  return true;
}
