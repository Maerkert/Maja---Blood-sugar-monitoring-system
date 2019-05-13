//BLuetooth libraries
#include "BLEDevice.h"
#include "BLECharacteristic.h"

//LED configuration
const int ledR1 = 15;
const int ledChannelR1 = 0; 
const int ledG1 = 2;
const int ledChannelG1 = 1; 
const int ledB1 = 4;
const int ledChannelB1 = 2; 

const int ledR2 = 21;
const int ledChannelR2 = 3; 
const int ledG2 = 22;
const int ledChannelG2 = 4; 
const int ledB2 = 23;
const int ledChannelB2 = 5; 

const int ledR3 = 13;
const int ledChannelR3 = 6; 
const int ledG3 = 12;
const int ledChannelG3 = 7; 
const int ledB3 = 14;
const int ledChannelB3 = 8; 

const int freq = 5000; 
const int resolution = 8;

const int buttonInputPin = 33;
const int pipser=5;


//Bluetooth settings
BLEUUID serviceUUID("caccbe7e-7191-4f78-92f6-c25f67120eab");
BLEUUID    charUUID("6fd78f9f-fd43-460f-8cf2-c405c25c2b04");

static BLEScan* pBLEScan;
static BLEAddress* pAddress;
static BLEClient*  pClient;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static boolean doConnect = false;
static boolean connected = false;

//glucoseData structure
struct glucoseData{
  uint16_t timestamp;
  uint16_t value;
};

//Data buffer settings
int bufferSize=300;
glucoseData* buffer=new glucoseData[bufferSize];
int bufferPosition=0;
int bufferedElements=0;
static bool newData=false; //Static for the acces from another Thread

//Reads the glucoseData from the BLE Server and stores it into the buffer for later use
void readDataIntoBuffer(){
  newData=false;
  std::string s=pRemoteCharacteristic->readValue();
  uint32_t* ptr=(uint32_t*) s.c_str();
  int numData=*ptr;
    
  for(int i=0;i<numData ;i++){
    glucoseData data;
    uint32_t* dataptr=(uint32_t*) &data;
    *dataptr=(uint32_t) ptr[i+1];
      
    buffer[bufferPosition]=data;
    bufferPosition=(bufferPosition+1)%bufferSize;
    if(bufferedElements<bufferSize){
      bufferedElements++;
    }

    Serial.println("Data read: ");
    Serial.print("Timestamp: ");
    Serial.println(data.timestamp);
    Serial.print("Value: ");
    Serial.println(data.value);
    Serial.println("");
  }
}

//Returns the latest glucoseData
glucoseData getLatestData(){
  if(bufferedElements>0){
    return buffer[bufferPosition-1]; 
  }
  glucoseData data;
  data.timestamp=0;
  data.value=0;
  return data;
}

//BLE Callback that gets triggered when new NFC data has been read
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.println("Notify received!");
    newData=true;
}

//BLE Callback that checks if a scanned device is the Libre Sensor
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    const char* serverName="MajaNFCServer";
    if (!strcmp(advertisedDevice.getName().c_str(), serverName)) {
      Serial.println("Found our device!"); 
      advertisedDevice.getScan()->stop();
      pAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  }
};

//Sets the color of the leds
void setColor(byte r, byte g, byte b){
  ledcWrite(ledChannelR1, r);
  ledcWrite(ledChannelR2, r);
  ledcWrite(ledChannelR3, r);

  ledcWrite(ledChannelG1, g);
  ledcWrite(ledChannelG2, g);
  ledcWrite(ledChannelG3, g);

  ledcWrite(ledChannelB1, b);
  ledcWrite(ledChannelB2, b);
  ledcWrite(ledChannelB3, b);
}

//Sets the color depending on the given glucose value
void setColorByGlucose(int glucose){
//Mittlerer Wert = 105
//Hoher Wert = 140
//Niedriger Wert = 70
  
  float colorValue = fabs(105-glucose)/35.0f;
  if(colorValue>1){
    colorValue=1;
  }

  byte red = (byte) (255.0f * colorValue);
  Serial.print("RED: ");
  Serial.println(red);
  
  byte green = (byte) (255.0f * (1.0f-colorValue));
   Serial.print("GREEN: ");
  Serial.println(green);

  setColor(red, green, 0);
}


//Manages the BLE connection
bool isConnected(){
  if(connected){
    if(pClient->isConnected()){
      return true;
    }
    else{
      Serial.println("BLE Connection Lost!"); 
      pClient->disconnect();
      setColor(0, 0, 255);
      connected=false;
    }
  }
  if(!doConnect){
    if(pBLEScan==nullptr){
      pBLEScan = BLEDevice::getScan(); 
      pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
      pBLEScan->setActiveScan(true);
    }
    pBLEScan->start(30);   
  }
  
  if(doConnect){
    doConnect=false;
    
    Serial.print("Connecting to: ");
    Serial.println(pAddress->toString().c_str());

    if(pClient==nullptr){
      pClient  = BLEDevice::createClient(); 
    }
    pClient->connect(*pAddress);
    Serial.println(" - Connected to server");

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service");
      return false;
    }
    Serial.println(" - Found our service");

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic");
      return false;
    }
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println(" - Found our characteristic");

    connected=true;   
    newData=true; 
    setColor(255, 255, 255);
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(9600);
  
  //BLE Startup
  Serial.println("Starting BLE Client");
  BLEDevice::init("MajaClient");
  Serial.println("BLE Client Started");

  //Pin setups
  ledcSetup(ledChannelR1, freq, resolution);
  ledcAttachPin(ledR1, ledChannelR1);
  ledcSetup(ledChannelG1, freq, resolution);
  ledcAttachPin(ledG1, ledChannelG1);
  ledcSetup(ledChannelB1, freq, resolution);
  ledcAttachPin(ledB1, ledChannelB1);

  ledcSetup(ledChannelR2, freq, resolution);
  ledcAttachPin(ledR2, ledChannelR2);
  ledcSetup(ledChannelG2, freq, resolution);
  ledcAttachPin(ledG2, ledChannelG2);
  ledcSetup(ledChannelB2, freq, resolution);
  ledcAttachPin(ledB2, ledChannelB2);

  ledcSetup(ledChannelR3, freq, resolution);
  ledcAttachPin(ledR3, ledChannelR3);
  ledcSetup(ledChannelG3, freq, resolution);
  ledcAttachPin(ledG3, ledChannelG3);
  ledcSetup(ledChannelB3, freq, resolution);
  ledcAttachPin(ledB3, ledChannelB3);

  pinMode(buttonInputPin, INPUT);
  
  setColor(0, 0, 255);
}


const int checkCycle=20;
int counter=0;


void loop() {
  if(counter == checkCycle){
    counter=0;
    if(isConnected()){
      if(newData){
        readDataIntoBuffer(); 
       if(bufferedElements>0){
          setColorByGlucose(getLatestData().value);
        }
      }
    }
  }

  if(digitalRead(buttonInputPin) == HIGH){
    setColor(0, 0, 0);
  }
  
  counter++;
  delay(50); 
}
