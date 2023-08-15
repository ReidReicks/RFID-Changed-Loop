#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         9           // Configurable, see typical pin layout above
#define SS_PIN          10          // Configurable, see typical pin layout above
#define BUTTON1         5
#define BUTTON2         4
#define BUTTON3         3
#define LED1            8
#define LED2            7
#define LED3            6
#define LEDerror        2

byte BUTTON[] = {BUTTON1, BUTTON2, BUTTON3};
byte LED[] = {LED1,LED2,LED3};
int curState[] = {0,0,0};
int LEDState[] = {0,0,0};
int lastState[] = {0,0,0};
// data of the three cards we use at FBN
int num_cards = 3;
byte cards[3][16] = {{0x00, 0x01, 0x53, 0x44, // SD-CLR , Clear move
                      0x00, 0x00, 0x00, 0x99, 
                      0x00, 0x05, 0x00, 0x00,
                      0x00, 0x02, 0x00, 0x99,},

                      {0x00, 0x01, 0x53, 0x44, // SD , Slow
                      0x00, 0x00, 0x00, 0x01,
                      0x00, 0x05, 0x00, 0x00,
                      0x00, 0x02, 0x00, 0x99,},
                      
                     {0x00, 0x01, 0x46, 0x55, // FU , Stop
                      0x00, 0x00, 0x00, 0x01, 
                      0x00, 0x05, 0x00, 0x00,
                      0x00, 0x02, 0x00, 0x99,}};

//static unsigned long ledCameOn = 0;
byte sector         = 1;
byte blockAddr      = 4;
byte trailerBlock   = 7;
byte buffer[18];
byte size = sizeof(buffer);

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

void setup() {
  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card
  for (byte i = 0; i < 6; i++) { // key for factory set cards
      key.keyByte[i] = 0xFF;
  }
  for (int i = 0; i < num_cards; i++){
    pinMode(BUTTON[i], INPUT);
    pinMode(LED[i], OUTPUT);
    curState[i] = digitalRead(BUTTON[i]);
  }  
  for (int i = 0; i < 2; i++){
    for (int i = 0; i < num_cards; i++){
      digitalWrite(LED[i], HIGH);
    }
    delay(250);
    for (int i = 0; i < num_cards; i++){
      digitalWrite(LED[i], LOW);
    }
    delay(250);
  }
}

void loop() {
  for (int i=0;i<num_cards;i++){

  while (digitalRead(BUTTON[i]) == HIGH){
    
    byte size = sizeof(buffer);
    status = (MFRC522::StatusCode) mfrc522.PICC_WakeupA(buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        blinkLED(1);
    }
// Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
    if ( ! mfrc522.PICC_IsNewCardPresent()){ 
      digitalWrite(LED[i], LOW);
      return;
    }
      if(! mfrc522.PICC_ReadCardSerial()){
      return;
      }
      rfid_write_button();
        
    // Check for compatibility
      MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
      if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
      &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
      &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
      blinkLED(2);
      return;
     }
      rfid_read();
    }
  }
     mfrc522.PICC_HaltA(); // Halt PICC
      mfrc522.PCD_StopCrypto1(); // Stop encryption on PCD
}

void rfid_write_button(){
  for (int i = 0; i < num_cards; i++){

    lastState[i] = curState[i];
    curState[i] = digitalRead(BUTTON[i]);
    
    if (lastState[i] == LOW && curState[i] == HIGH){
      if (LEDState[i] == LOW){
        verify(i);
      }
    }
  }
}

void rfid_read(){
  
  size = sizeof(buffer);
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
      blinkLED(3);
      return;
  }
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
      blinkLED(5);
  }

  for (int i = 0; i < num_cards; i++){
    if (buffer[2] == cards[i][2] && buffer[7] == cards[i][7]){
      changeLED(i);

    }
  }
}
  
void verify(byte cardnumber){
  // Load card data to write
  byte dataBlock[16];
  for (byte i = 0; i < 16; i++) {
    dataBlock[i] = cards[cardnumber][i];
  }

  size = sizeof(buffer);
  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    blinkLED(6);
    return;
  }
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, dataBlock, 16);
  if (status != MFRC522::STATUS_OK) {
    blinkLED(7);
  }
  // checking all bytes match the card wrote
  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    blinkLED(5);
  }
  byte count = 0;
  for (byte i = 0; i < 16; i++) {
      if (buffer[i] == dataBlock[i])
          count++;
  }
  if (count != 16) {
      blinkLED(4);
      return;
  }
  else {
    delay(500);
    changeLED(cardnumber);
  }
}

void blinkLED(int blinks){
  for (int i = 0; i < blinks; i++){
    digitalWrite(LEDerror, HIGH);
    delay(500);
    digitalWrite(LEDerror, LOW);
    delay(500);
  }
}

void changeLED(int number){
  for (int i = 0; i < num_cards; i++){
    LEDState[i] = LOW;
  }
  LEDState[number] = HIGH;
  for (int i = 0; i < num_cards; i++){
    digitalWrite(LED[i], LEDState[i]);
  }
  
}
