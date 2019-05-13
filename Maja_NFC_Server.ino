//Bluetooth libraries
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

//NFC libraries
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

//Bluetooth settings
BLEUUID serviceUUID("caccbe7e-7191-4f78-92f6-c25f67120eab");
BLEUUID    charUUID("6fd78f9f-fd43-460f-8cf2-c405c25c2b04");
BLECharacteristic* pCharacteristic;

//NFC settings
#define PN532_SCK  (14)
#define PN532_MOSI (13)
#define PN532_SS   (15)
#define PN532_MISO (12)

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
uint8_t nfcDataBuffer[32];
bool nfcDataRead=false;

//Structure to save that glucose value and a timestamp
struct glucoseData{
  uint16_t timestamp;
  uint16_t value;
};


//Data buffer settings
//Because a BLE Characteristic has a capacity of 600 bytes (-4 Bytes for the timestamp),
//the buffer should not exceed the size of 596 bytes
uint32_t bufferSize=596/sizeof(glucoseData);
glucoseData* buffer=new glucoseData[bufferSize];
uint32_t bufferPosition=0;
uint32_t bufferedElements=0;

//Adds the given glucoseData to the buffer
void addData(glucoseData data){
  //Writing into the buffer
  buffer[bufferPosition]=data;
  bufferPosition=(bufferPosition+1)%bufferSize;
  bufferedElements++;

  //Copying the buffered data into the BLE Characteristic
  uint32_t* bytes=new uint32_t[bufferedElements+1];
  bytes[0]=bufferedElements;

  int position=bufferPosition-1;  
  for(int i=0;i<bufferedElements;i++){
    uint32_t* ptr=(uint32_t*) &(buffer[position]);
    bytes[i+1]=*ptr;
    position--;
    if(position<0){
      position=bufferSize-1;
    }
  }

  uint8_t* msg=(uint8_t*) bytes;
  pCharacteristic->setValue(msg, bufferedElements*4+4);
  
  Serial.print("Data set. Size: ");
  Serial.println(pCharacteristic->getValue().size());
  pCharacteristic->notify();
}

//BLE Characteristic callback that empties the buffer, when the data has been read
class CustomCharacteristicsCallback: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic){
    bufferedElements=0;
    bufferPosition=0;
  }
  void onWrite(BLECharacteristic *pCharacteristic){}
};

//Algorithm to calculate the glucose value from the sensors raw temperature and glucose reading
uint16_t calculateGlucose(uint8_t* data){
  Serial.println("Calculating Glucose:");
  
  int rawGlucose = data[0] + ((data[1] & 63) << 8);
  int rawTemperature = data[3] + ((data[4] & 63) << 8);
  
  double slope = (0.000015623 * rawTemperature + 0.0017457);
  double offset = (-0.0002327 * rawTemperature - 19.47);
  
  int glucose = (rawGlucose * slope + offset);
  
  Serial.print("Raw Glucose: ");
  Serial.println(rawGlucose);
  Serial.print("Raw Temperature:");
  Serial.println(rawTemperature);
  Serial.print("Glucose:");
  Serial.println(glucose);
  
  return glucose;
}

//Reads the data from the Libre Sensor
void readNFC(){
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 }; 
  uint8_t uidLength;
  
  nfcDataRead = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (nfcDataRead) {
    //Checking the uid length that indicates different NFC versions
    if (uidLength == 4) {
      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
      nfcDataRead = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
      if (nfcDataRead) {
        nfcDataRead = nfc.mifareclassic_ReadDataBlock(4, nfcDataBuffer);
        if (nfcDataRead) {
          Serial.println("Reading Block 4:");
          nfc.PrintHexChar(nfcDataBuffer, 16);
          Serial.println("");
        }
        else {
          Serial.println("Unable to read Block 4!");
        }
      }
      else {
        Serial.println("Authentication to Block 4 failed!");
      }
    }
  }
}


void setup() {
  Serial.begin(9600);
  
  //NFC startup
  Serial.println("Starting NFC Reader!");
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
  }
  nfc.SAMConfig(); 
  Serial.println("NFC Reader Started!");

  //BLE startup  
  Serial.println("Starting BLE Server!");
  BLEDevice::init("MajaNFCServer");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(serviceUUID); 
  pCharacteristic = pService->createCharacteristic(charUUID, BLECharacteristic::PROPERTY_READ);
  pCharacteristic->setCallbacks(new CustomCharacteristicsCallback());
  pService->start();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
  Serial.println("BLE Server Started!");
}

void loop() {
  readNFC();
  if(nfcDataRead){
    glucoseData data;
    data.timestamp=millis()/1000;
    data.value=calculateGlucose(nfcDataBuffer);
    addData(data);
  }
  delay(1000);
}
