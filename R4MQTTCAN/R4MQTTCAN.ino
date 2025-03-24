#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <Arduino_CAN.h>
#include <EEPROM.h>

#define MAX_SIZE 2048
#define TEST_MAX_SIZE 64
#define UPDATE_STORAGE_START_ADDRESS 0x0000
#define UPDATE_STORAGE_END_ADDRESS 0x07FF
#define UPDATE_SIZE_STORAGE_ADDRESS 0x1000
#define UPDATE_CHECKSUM_STORAGE_ADDRESS 0x1800
//#define UPDATE_CHECKSUM_STORAGE_ADDRESS 0x1801
#define CAN_MESSAGE_SIZE 8

//DEFAULT MESSAGE (EMPTY)
const uint8_t DEFAULT_CONTENTS = 255;

static uint32_t CAN_ID = 0x02;
static uint32_t CAN_ID_MESSAGE_SIZE = 0x00;
static uint32_t CAN_ID_CHECKSUM = 0x01;

const uint8_t VIN = 0x3131; //For 1st master
// const uint8_t VIN = 0x3132; //For 2nd master

const char ssid[] = "";    // your network SSID (name) 
const char pass[] = "";    // your network password
 //const char ssid[] = "";    // your network SSID (name) 
 //const char pass[] = "";

//Broker connection info
const char broker[] = "broker.emqx.io"; // host name
const int port = 1883;
const char topic_sub_commands[]  = "Test/commands";

const char topic_sub_VIN[] = "BMW/520d"; //For first master
//const char topic_sub_VIN[] = "AUDI/A4"; //For second master

const char topic_pub_status[]  = "Test/status";

//MACHINE STATES
typedef enum {
  STATE_INACTIVE,
  STATE_ACTIVE,
  STATE_PREPARE_TRANSFER,
  STATE_TRANSFER_READY,
  STATE_TRANSFER_DATA_INPROGRESS,
  STATE_FINISHED,
  STATE_ABORT_TRANSFER
} State;

//CAN message objects
//CanMsg const transfer_req_msg(CanStandardId(CAN_UPDATE_REQUEST_ID), 8, can_update_message_content);

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

//SETUP FUNCTIONS
void wifiConnection(const char ssid[], const char pass[]);
void brokerConnection(const char host[], const int port);
void subscribeTopic(const char topic[]);

//STATE MACHINE
void master_state_machine();

//LISTEN FOR MESSAGE
uint8_t listenForMessageOnTopic(const char* topic);

//PUBLISH MACHINE STATE ON TOPIC 2
void publish_state_message(State state);

//ACTIVATE CAN ON MASTER
void activate_CAN();

//ESTABLISHING CAN COMMUNICATION BETWEEN MASTER AND SLAVE
void establish_CAN_communication();

//READ MESSAGE FROM SPECIFIC TOPIC FOR UPDATE FILE AND SAVE TO EEPROM MEMORY
bool saveMessageToEeprom(const char* topic);

//PUBLISH MESSAGE FOR FILE STATE
void publish_complete_file_message(String message);

//BREAKS FILE INTO 8 BYTE MESSAGES AND SENDS OVER CAN
void sendCANmessageData();

//SEND SIZE OF MESSAGE
void send_message_size_CAN();

//CREATES CHECKSUM FOR UPDATE FILE AND SAVES TO EEPROM
void create_check_and_send_checksum();

const uint8_t LISTEN_CMD = 0x34;
const uint8_t PREPARE_TRANSFER_CMD = 0x35;
const uint8_t TRANSFER_DATA_CMD = 0x36;
const uint8_t FINISH_TRANSFER_CMD = 0x37; 
const uint8_t ABORT_TRANSFER_CMD = 0x38;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  if(!CAN.begin(CanBitRate::BR_250k))
  {
    Serial.println("Fiailed to activate CAN");
    for(;;){}
  }

     wifiConnection(ssid, pass);
     brokerConnection(broker, port);
     subscribeTopic(topic_sub_commands);
     subscribeTopic(topic_sub_VIN);


     //delay(3000);
    //activate_CAN();
    //establish_CAN_communication();
}

void loop()
{

  State current_state = STATE_INACTIVE;
while (current_state != STATE_FINISHED)
{
  uint8_t command = listenForMessageOnTopic(topic_sub_commands);
     switch (current_state)
     {
      case STATE_INACTIVE:
        publish_state_message("Inactive");
           if(command == VIN)
           {
            current_state = STATE_ACTIVE;
           } else {
            current_state = STATE_INACTIVE;
           }
           break;

      //Listening for commands active state
       case STATE_ACTIVE:
        publish_state_message("Active");
           if (command == PREPARE_TRANSFER_CMD)
           {
            current_state = STATE_PREPARE_TRANSFER;
            publish_state_message("Preparing transfer...");
           } 
           else if (command == ABORT_TRANSFER_CMD)
           {
            current_state = STATE_ABORT_TRANSFER;
            publish_state_message("Aborting...");
           } else {
            current_state = STATE_ACTIVE;
           }
           break;

      //Getting data ready for transfer process 
      //getting and saving data into master
      //TODO: check on topic assigned for data and take the file and save into eeprom
       case STATE_PREPARE_TRANSFER:
           if(command == ABORT_TRANSFER_CMD)
           {
            current_state = STATE_ABORT_TRANSFER;
            publish_state_message("Aborting...");

           } else {
            String file_write_message;
              //load file from mqtt topic

              if(saveMessageToEeprom(topic_sub_VIN) == true)
              { file_write_message = "Data received successfully!";
                current_state = STATE_TRANSFER_READY;

              } else {
                file_write_message = "ERROR data not received!";
                current_state = STATE_ABORT_TRANSFER;
              }
              publish_complete_file_message(file_write_message);
           }
           break;

      //orchestration step wait here for command to start sending update through CAN
       case STATE_TRANSFER_READY:
        publish_state_message("Data Transfer Ready (Waiting...)");
           if(command == TRANSFER_DATA_CMD)
           {
            current_state = STATE_TRANSFER_DATA_INPROGRESS;
            publish_state_message("Proceeding with transfer...");
           } 
           else if (command == ABORT_TRANSFER_CMD)
           {
            current_state = STATE_ABORT_TRANSFER;
            publish_state_message("Aborting...");
           } else {
           }
           break;
          
      //Data transfer process in progress
      //Sending saved data from master to target through CAN
       case STATE_TRANSFER_DATA_INPROGRESS:
        publish_state_message("Transfer In Progress...");
           //send data from eeprom to target using CAN
            send_message_size_CAN();
            create_check_and_send_checksum();
            sendCANmessageData();
            current_state = STATE_INACTIVE;
            publish_state_message("Transfer Finished");
           break;

        //Aborting transfer states
        case STATE_ABORT_TRANSFER:
         publish_state_message("Aborting...");
          current_state = STATE_INACTIVE;
           break;
     }
   }
}





















//listen function for retrieving command from MQTT topic
uint8_t listenForMessageOnTopic(const char* topic)
{
  delay(1000);

  int messageSize = mqttClient.parseMessage();

  if(messageSize)
  {
    String receivedTopic = mqttClient.messageTopic();
    if(receivedTopic.equals(topic))
    {
      uint8_t message_contents_u8 = mqttClient.read();
      return message_contents_u8;
    }
  }
  return DEFAULT_CONTENTS;
}


bool saveMessageToEeprom(const char* topic)
{
  bool message_empty = true;
  while(message_empty == true)
  {
    publish_state_message("Waiting for update file...");
    int messageSize = mqttClient.parseMessage();
    //Serial.print(messageSize);

  if(messageSize > 0)
  {
    String receivedTopic = mqttClient.messageTopic();

    if(receivedTopic.equals(topic))
    {
      EEPROM.write(UPDATE_SIZE_STORAGE_ADDRESS, messageSize & 0xFF);
      EEPROM.write(UPDATE_SIZE_STORAGE_ADDRESS + 1, (messageSize >> 8) & 0xFF);
      
      int eeprom_message_size = EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS) | (EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS + 1) << 8);
      char single_char;
      char full_message[messageSize];

      for (int i=0; i<messageSize; i++)
      {
        single_char = mqttClient.read();
        EEPROM.write(UPDATE_STORAGE_START_ADDRESS + i, single_char);
      }

      for (int i=0; i<messageSize; i++)
      {
        char message = EEPROM.read(UPDATE_STORAGE_START_ADDRESS + i);
        Serial.print((char)message);
      }

      Serial.println(messageSize);
      Serial.println(eeprom_message_size);

      if(messageSize == eeprom_message_size) 
      {
      return true;
      } else { 
      return false;
      }
    }
    message_empty = false;
  }
  }
}

//publish machine state on MQTT topic
void publish_state_message(String state)
{
  delay(2000);
  mqttClient.beginMessage(topic_pub_status);
  mqttClient.print(state);
  mqttClient.endMessage();
}

void publish_complete_file_message(String message){
  mqttClient.beginMessage(topic_pub_status);
  mqttClient.print(message);
  mqttClient.endMessage();
}


















































//Setup Functions

//Establish WIFI connection
void wifiConnection(const char ssid[], const char pass[])
{
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }
  Serial.print("Connected to network successfully\n");
}

//Establish connection to MQTT broker
void brokerConnection(const char host[], const int port)
{
  uint8_t failed_connection_counter_u8 = 0;

  Serial.print("attempting to establish connection with broker...' ");

  while(!mqttClient.connect(host, port) && failed_connection_counter_u8 != 5)
  {

    failed_connection_counter_u8++;
    Serial.print("MQTT connection fialed! Error code = ");
    Serial.println(mqttClient.connectError());
    Serial.print("\n retrying connection... Attempt: ");
    Serial.print(failed_connection_counter_u8);
    delay(3000);
  }
    if (failed_connection_counter_u8 >= 5)
    {
      Serial.print("Max number of attempts exceeded...");
      for (;;)
      {

      }
    } 
}

//Subscribe to specific MQTT topic
void subscribeTopic(const char topic[])
{
  Serial.print("Subscribing to topic...");
  Serial.print(topic);
  mqttClient.subscribe(topic);
  Serial.print("\nWaiting for messages on topic: ");
  Serial.println(topic);
}



void sendCANmessageData()
{
 int eeprom_message_size = EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS) | (EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS + 1) << 8);
 int number_of_messages = eeprom_message_size/CAN_MESSAGE_SIZE;
 int start_index = 0;
 int end_index = 8;

  for(int message_counter=1; message_counter<=number_of_messages; message_counter++)
  {
    uint8_t message[8];
    delay(1000);
    int index_counter = 0;

    int storage_index = UPDATE_STORAGE_START_ADDRESS + start_index;

    while(start_index < end_index)
    {
      uint8_t single_value = EEPROM.read(storage_index + index_counter);
      message[index_counter] = single_value;

      start_index++;
      index_counter++;
    }

    CanMsg msg(CanStandardId(CAN_ID), 8, message);

    if (int const rc = CAN.write(msg); rc < 0)
    {
      Serial.println("CAN.write(...) failed with error code");
      Serial.println(rc);
      for (;;) { }
    }

    for (int i = 0; i<8; i++)
    {
      Serial.print((char)message[i]);
    }

    start_index = end_index;
    end_index += 8;
  }
}

void send_message_size_CAN()
{
  int message_size = EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS) | (EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS + 1) << 8);

 uint8_t can_message_size[2];
 can_message_size[0] = message_size & 0xFF;
 can_message_size[1] = (message_size >> 8) & 0xFF;

 CanMsg size_msg(CanStandardId(CAN_ID_MESSAGE_SIZE), 2, can_message_size);
 CAN.write(size_msg);
 delay(1000);
}


void create_check_and_send_checksum()
{
  int message_size = EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS) | (EEPROM.read(UPDATE_SIZE_STORAGE_ADDRESS + 1) << 8);
  int checksum = 0;
  int checksum_value = 0;

  while(checksum_value != message_size)
  {
    checksum += EEPROM.read(UPDATE_STORAGE_START_ADDRESS+checksum_value);
    checksum_value += CAN_MESSAGE_SIZE;
  }
    EEPROM.write(UPDATE_CHECKSUM_STORAGE_ADDRESS, checksum & 0xFF);
    EEPROM.write(UPDATE_CHECKSUM_STORAGE_ADDRESS + 1, (checksum >> 8) & 0xFF);
    
    int checksum_to_check = EEPROM.read(UPDATE_CHECKSUM_STORAGE_ADDRESS) | (EEPROM.read(UPDATE_CHECKSUM_STORAGE_ADDRESS + 1) << 8);

    if(checksum_to_check == checksum)
    {
      int message_size_checksum = EEPROM.read(UPDATE_CHECKSUM_STORAGE_ADDRESS) | (EEPROM.read(UPDATE_CHECKSUM_STORAGE_ADDRESS + 1) << 8);

      uint8_t can_message_size[2];
      can_message_size[0] = message_size_checksum & 0xFF;
      can_message_size[1] = (message_size_checksum >> 8) & 0xFF;

      CanMsg checksum_msg(CanStandardId(CAN_ID_CHECKSUM), 2, can_message_size);
      CAN.write(checksum_msg);
    }
    delay(1000);
}