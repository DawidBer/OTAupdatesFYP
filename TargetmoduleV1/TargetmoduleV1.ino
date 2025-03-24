
#include <EEPROM.h>
#include <Arduino_FreeRTOS.h>
#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include <Arduino_CAN.h>

ArduinoLEDMatrix matrix;
TaskHandle_t led_display_task, receive_update_task; 

bool switch_active = false;
bool emergency_state = false;

bool perform_switch = false;

#define PARITION_SIZES 2048 //2Kb

#define FLAG_SECTION_START 0x1804

//#define ACTIVE_START 0x0000
int ACTIVE_START;
//#define ACTIVE_END 0x07FF

//#define INACTIVE_START 0x0800
int INACTIVE_START;
//#define INACTIVE_END 0x0FFF

//#define BACKUP_START 0x1000
int BACKUP_START;
//#define BACKUP_END 0x1FFF

#define MESSAGE_SIZE_UPPER 0x1800
#define MESSAGE_SIZE_LOWER 0x1801

#define CHECKSUM_SIZE_UPPER 0x1802
#define CHECKSUM_SIZE_LOWER 0x1803

#define DATA_SIZE_OFFSET 1

#define CAN_MESSAGE_SIZE 8 

bool checksum_validated = false;
bool checksum_validated_new = true;
bool perform_switch_emergency = false;

void setup() {
  //EEPROM.write(FLAG_SECTION_START, 0);
  setPartitions();

 Serial.begin(115200);
 matrix.begin();

 //BEGIN CAN
 if(!CAN.begin(CanBitRate::BR_250k))
 {
  Serial.print("Failed to start CAN");
  for(;;){}
 }

 //FIRST TASK LED PANEL
 auto const led_display = xTaskCreate
 (
  display_message,
  static_cast<const char*>("LED panel display"),
  512/4,
  nullptr,
  1,
  &led_display_task
 );

if(led_display != pdPASS)
{
  Serial.print("Failed to create led display task");
  return;
}

  //SECOND TASK RECIEVE UPDATE
  auto const update_data = xTaskCreate
  (
    check_for_update,
    static_cast<const char*>("listen and write CAN message"),
    512/4,
    nullptr,
    2,
    &receive_update_task
  );

  if(update_data != pdPASS)
{
  Serial.print("Failed to create receive update task");
  return;
}

 vTaskStartScheduler(); //begin tasks
 for(;;);
 }

void loop()
{}

//TASK 1
void display_message(void *pvParameters)
{
  for(;;)
  {
    read_and_display_message_on_panel();
  }
}

void read_and_display_message_on_panel()
{
  //EEPROM read size and message etc..
int message_size = (EEPROM.read(MESSAGE_SIZE_UPPER) << 8) | EEPROM.read(MESSAGE_SIZE_LOWER);
  // Serial.print(message_size);
  char full_message[message_size];

  //display message on panel
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(80);
  matrix.textFont(Font_4x6);
  matrix.beginText(10,1,0xFFFFFF);

   for(int i=0; i<43; i++)
   { 
     char message = EEPROM.read(ACTIVE_START + i); //get the value after the size address
     full_message[i] = message;                                       //add values after size address to char array
   }
  matrix.println(full_message);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

//TASK2
void check_for_update(void *pvParameters)
{
  for(;;)
  {
    monitor_CAN_messages();
    vTaskDelay(configTICK_RATE_HZ);
  }
}

void monitor_CAN_messages()
{
  static int memory_index_counter = 0;
  if(CAN.available())
  {
    CanMsg msg = CAN.read();
    if(msg.id == 0)
    {   memory_index_counter = 0;
        EEPROM.write(MESSAGE_SIZE_UPPER, msg.data[1]);
        delay(5);
        EEPROM.write(MESSAGE_SIZE_LOWER, msg.data[0]);
        delay(5);

      int message_size = (EEPROM.read(MESSAGE_SIZE_UPPER) << 8) | EEPROM.read(MESSAGE_SIZE_LOWER);

      Serial.println(message_size);
    }

    else if (msg.id == 1)
    {
      EEPROM.write(CHECKSUM_SIZE_UPPER, msg.data[1]);
      delay(5);
      EEPROM.write(CHECKSUM_SIZE_LOWER, msg.data[0]);
      delay(5);

      int checksum = (EEPROM.read(CHECKSUM_SIZE_UPPER) << 8) | EEPROM.read(CHECKSUM_SIZE_LOWER);

      Serial.println(checksum);
      checksum_validated_new = false;
      perform_switch = true;

    } else {

      for(int i=0; i<msg.data_length; i++)
      {
        EEPROM.write(INACTIVE_START + memory_index_counter, (char)msg.data[i]);
        delay(5);
        char storedval = (EEPROM.read(INACTIVE_START + memory_index_counter));
        Serial.print(storedval);
        memory_index_counter++;
      }
    }

  } else {
    if(checksum_validated == false)
    {
      if(checksumValidationActiveCode() == true)
      {
        //set new eeprom address for led panel after next restart
        checksum_validated = true;
        Serial.println(EEPROM.read(FLAG_SECTION_START));
      }
    }
     else if(checksum_validated_new == false)
      {
        if(checksumValidationNewCode() == true)
        {
          copyActiveIntoBackup();
          setFlagNextBoot();
          checksum_validated_new = true;
        }
      } else {}
  }

}

bool checksumValidationActiveCode()
{
  int saved_checksum = (EEPROM.read(CHECKSUM_SIZE_UPPER) << 8) | EEPROM.read(CHECKSUM_SIZE_LOWER);
  int message_size = (EEPROM.read(MESSAGE_SIZE_UPPER) << 8) | EEPROM.read(MESSAGE_SIZE_LOWER);

  int checksum = 0;
  int checksum_value = 0;

  while(checksum_value != message_size)
  {
    checksum += EEPROM.read(ACTIVE_START + checksum_value);
    checksum_value += CAN_MESSAGE_SIZE;
  }

  Serial.println(checksum);
  Serial.println(saved_checksum);
  Serial.println(EEPROM.read(FLAG_SECTION_START));

    if(saved_checksum == checksum)
    {
      return true;
    } else {
      perform_switch_emergency = true;
      return false;
    }
}

bool checksumValidationNewCode()
{
  int saved_checksum = (EEPROM.read(CHECKSUM_SIZE_UPPER) << 8) | EEPROM.read(CHECKSUM_SIZE_LOWER);
  int message_size = (EEPROM.read(MESSAGE_SIZE_UPPER) << 8) | EEPROM.read(MESSAGE_SIZE_LOWER);

  int checksum = 0;
  int checksum_value = 0;

  while(checksum_value != message_size)
  {
    checksum += EEPROM.read(INACTIVE_START + checksum_value);
    checksum_value += CAN_MESSAGE_SIZE;
  }

  Serial.println(checksum);
  Serial.println(saved_checksum);

    if(saved_checksum == checksum)
    {
      return true;
    } else {
      return false;
    }
}

void setPartitions()
{
  uint8_t partition_state = EEPROM.read(FLAG_SECTION_START);
  if(partition_state == 1)
  {
    //regular switch state no error booting swapping active and inactive
    INACTIVE_START = 0x0000;
    ACTIVE_START = 0x0800;
    BACKUP_START = 0x1000;
  } 
  else if(partition_state == 2)
  {
    //error case switch booting from backup (last safe code state)
    BACKUP_START = 0x0000;
    INACTIVE_START = 0x0800;
    ACTIVE_START = 0x1000;

  } else {
    //default state 
    ACTIVE_START = 0x0000;
    INACTIVE_START = 0x0800;
    BACKUP_START = 0x1000;
  }
}

void setFlagNextBoot()
{
  int current_flag_value = EEPROM.read(FLAG_SECTION_START);
  if((current_flag_value == 0 || current_flag_value == 2) && perform_switch == true && perform_switch_emergency != true)
  {
    EEPROM.write(FLAG_SECTION_START, 1);
    Serial.println("1 switching partition next boot active and inactive");
  } 
  else if(current_flag_value == 1 && perform_switch == true && perform_switch_emergency != true)
  {
    EEPROM.write(FLAG_SECTION_START, 0);
    Serial.println("2 switching partition next boot inactive and active");
  }
  else if(perform_switch_emergency == true)
  {
    EEPROM.write(FLAG_SECTION_START, 2);
    Serial.println("3 switching partition active and backup error state");
  }
  else {}
}

void copyActiveIntoBackup()
{
  int message_size = (EEPROM.read(MESSAGE_SIZE_UPPER) << 8) | EEPROM.read(MESSAGE_SIZE_LOWER);

  for(int i=0; i<message_size; i++)
  {
    char value_to_write = EEPROM.read(ACTIVE_START+ i);
    EEPROM.write(BACKUP_START+i, value_to_write);
  }
}