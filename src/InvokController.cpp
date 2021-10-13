/*
  Class definition for InvokController.h library.
  Created by Thoby L. Noorhalim
  26 August 2021
*/

#include <InvokController.h>

// ------------------------------ Constructor ------------------------------
Controller::Controller(){
  this->connectionType = "websocket"; // Default to websocket
}

Controller::Controller(std::string connectionType){
  this->connectionType = connectionType; 
}

void Controller::begin(){
  if(this->connectionType == "websocket"){
    // Begin Wifi connection routine

    // Set device as station mode
    WiFi.mode(WIFI_STA);

    #ifdef ESP32
      // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
      WiFi.setHostname(this->hostname.c_str());
    #else 
      WiFi.hostname(this->hostname.c_str());
    #endif

    Serial.printf("\nConnecting to %s\n", this->SSID.c_str());
    WiFi.disconnect();
    WiFi.begin(this->SSID.c_str(), this->password.c_str());
    while ( WiFi.status() != WL_CONNECTED ) {
      delay(1000);
      Serial.print(".");
    }
    getLocalIP();
    Serial.println("");
    Serial.print("RRSI: ");
    Serial.println(WiFi.RSSI());

    // Set Websocket server
    this->websocket = WebSocketsServer(this->websocketPort);

    // Begin websocket routine
    this->websocket.begin();
    this->websocket.onEvent(std::bind(&Controller::onWebSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  }
}

void Controller::loop(){
  if(this->connectionType == "websocket"){
    this->websocket.loop();
  }
}

// ------------------------------ Setters ------------------------------

void Controller::setSSID(std::string SSID){
  this->SSID = SSID;
}

void Controller::setSSIDPassword(std::string password){
  this->password = password;
}

void Controller::setWifiHostname(std::string hostname){
  this->hostname = hostname;
}

void Controller::setWebsocketPort(int port){
  this->websocketPort = port;
}

void Controller::setMessage(std::string data){
  this->message = data;
}

void Controller::setDataArrived(bool state){
  this->dataArrived = state;
}

void Controller::setIncomingCommand(std::string command){
  this->incomingCommand = command;
}

// ------------------------------ Getters ------------------------------

IPAddress Controller::getLocalIP(){
  this->localIP = WiFi.localIP();
  return WiFi.localIP();
}

bool Controller::isConnected(){
  return this->_isConnected;
}

string Controller::getMessage(){
  return this->message;
}

bool Controller::isDataArrived(){
  return this->dataArrived;
}

void Controller::print(std::string toPrint){
  if(_isConnected){
    printString = "monitor," + toPrint;
    this->websocket.sendTXT(connectedIndex, printString.c_str());
  }
}

string Controller::getIncomingCommand(){
  std::string buffer = incomingCommand;
  // setIncomingCommand("");
  return buffer;
}

void Controller::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("Client [%u] Disconnected!\n", num);
      this->_isConnected = false;
      break;

    // New client has connected
    case WStype_CONNECTED:
      if(!_isConnected){
        this->connectedIndex = num;
        IPAddress ip = this->websocket.remoteIP(num);
        Serial.printf("Client [%u] Connection from ", num);
        Serial.println(ip.toString());
        this->_isConnected = true;

        // Respond once when connection established
        // this->websocket.sendTXT(num, "hello");
      } else {
        Serial.printf("Connection refused, already connected to client\n");
      }
      break;

    // Message Received
    case WStype_TEXT:
      // Cast payload to string
      this->rawData = std::string(reinterpret_cast<char*>(const_cast<uint8_t*>(payload)));
      this->message = this->rawData;
      this->dataArrived = true;
      
      #ifdef DEBUG
        Serial.printf("Message data is %s \n", this->message.c_str());
      #endif

      this->parsedDataVector.reserve(10);
      this->parsedDataVector = parsecpp(this->message, ",");
      this->command = parsedDataVector[0];

      if(command.compare("cms") == 0){
        this->response = "sms," + parsedDataVector[1];
        this->websocket.sendTXT(num, response.c_str());
        this->response.clear();
        #ifdef DEBUG
          onMessageCallback(num, parsedDataVector[1]);
        #endif
      } else if (command.compare("joystick") == 0){
        // Update Joystick Data
        joystick.updateData(parsedDataVector);
      } else if (command.compare("cpk") == 0 ){
        // Update Color Data
        colorPicker.updateData(parsedDataVector);
      } else if (command.compare("bar") == 0 ){
        // Update Button Array Data
        buttonArray.updateData(parsedDataVector);
      } else if (command.compare("slider") == 0){
        // Update Slider Data
        slider.updateData(parsedDataVector);
      } else if (command.compare("serial") == 0){
        // Update Incoming Command
        if(parsedDataVector[1].compare("initrequest") == 0){
          String initResponse = "Connected to Server " + this->localIP.toString();
          this->print(initResponse.c_str());
        } else {
          setIncomingCommand(rawData.substr(parsedDataVector[0].length()+1));
        }
        
      }
      
      break;

    // For everything else: do nothing
    case WStype_PING:
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}

void Controller::onMessageCallback(uint8_t num, std::string message){
  Serial.printf("Channel [%d], Message is %s, echoed to client.\n", num, message.c_str());
}

void Controller::printIP(){
  Serial.print("Connected IP Address ");
  Serial.println(this->localIP);
}

void Controller::setAuthorisation(std::string user, std::string pass){
  this->websocket.setAuthorization(user.c_str(), pass.c_str());
}

std::vector<std::string> Controller::parsecpp(std::string data, std::string delim){
  std::vector<std::string> myVector{};
  
  myVector.reserve(NUM_OF_DATA);
  int pos = 0;

  while((pos = data.find(delim)) != std::string::npos){
    myVector.push_back(data.substr(0, pos));
    data.erase(0, pos + delim.length());
  }
  // Push last substring to vector
  myVector.push_back(data.substr(0));
  return myVector;
}

