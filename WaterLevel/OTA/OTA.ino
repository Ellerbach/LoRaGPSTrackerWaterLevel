
#include <LoRaWan.h>

#include <Seeed_vl53l0x.h>
Seeed_vl53l0x VL53L0X;

//set to true to send confirmed data up messages
bool confirmed=false;

#define PIN_SOIL_HUMID PIN_A2
#define PIN_WATER_LEVEL PIN_A0
#define PIN_BATTERY PIN_A4
#define PIN_CHARGING PIN_A5

//set initial datarate and physical information for the device
_data_rate_t dr=DR6;
_physical_type_t physicalType =EU868 ;

//internal variables
#define DATA_LENGTH 4
byte data[DATA_LENGTH];
char buffer[256];
int i=0;
int lastCall=0;


void setup(void)
{
  
    SerialUSB.begin(115200);
    //while(!SerialUSB);    
    lora.init(); 

    lora.setId(NULL, "5555ABCDEF123456", "5555000000001234");
    lora.setKey(NULL, NULL, "55551234567890ABCDEF1234567890AB");

    lora.setDeciveMode(LWOTAA);
    lora.setDataRate(dr, physicalType);
    
    lora.setChannel(0, 868.1);
    lora.setChannel(1, 868.3);
    lora.setChannel(2, 868.5);
    
    lora.setReceiceWindowFirst(0, 868.1);
    
    lora.setAdaptiveDataRate(false);

    lora.setDutyCycle(false);
    lora.setJoinDutyCycle(false);

    
    lora.setPower(14);
    
    while (!lora.setOTAAJoin(JOIN,20000))
      SerialUSB.println("Trying to join, failed, waiting a bit...");

    pinMode(PIN_SOIL_HUMID, INPUT);
    pinMode(PIN_WATER_LEVEL, INPUT);
    pinMode(PIN_BATTERY, INPUT);
    pinMode(PIN_CHARGING, INPUT);
   
   VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    Status=VL53L0X.VL53L0X_common_init();
    if(VL53L0X_ERROR_NONE!=Status)
    {
      SerialUSB.println("start vl53l0x mesurement failed!");
      VL53L0X.print_pal_error(Status);
      while(1);
    }

    VL53L0X.VL53L0X_high_accuracy_ranging_init();

    if(VL53L0X_ERROR_NONE!=Status)
    {
      SerialUSB.println("start vl53l0x mesurement failed!");
      VL53L0X.print_pal_error(Status);
      while(1);
    }
}

void loop(void)
{   
  if((millis()-lastCall)>5000){
    lastCall=millis();
    bool result = false;
    String packetString = "12";
    packetString=String(i);
    SerialUSB.println(packetString);
    
    int distance = MeasureDistance();
    int waterLevel = MeasureWaterLevel();
    int soilHumid = MeasureSoilHumid();
    int battery = MeasureBattery();
    int charging = MeasureCharging();
    SerialUSB.print("Distance: ");
    SerialUSB.print(distance);
    SerialUSB.print(" waterLevel: ");
    SerialUSB.print(waterLevel);
    SerialUSB.print(" soilHumid: ");
    SerialUSB.print(soilHumid);
    SerialUSB.print(" battery: ");
    SerialUSB.println(battery);
    SerialUSB.print(" charge: ");
    SerialUSB.println(charging);

    EncodeData(distance, waterLevel, soilHumid, battery, charging);
        
    if(confirmed)
        result = lora.transferPacketWithConfirmed(data, DATA_LENGTH);
      else
        result = lora.transferPacket(data, DATA_LENGTH);
    i++;
    
    if(result)
    {
        short length;
        short rssi;
        
        memset(buffer, 0, 256);
        length = lora.receivePacket(buffer, 256, &rssi);
        
        if(length)
        {
            SerialUSB.print("Length is: ");
            SerialUSB.println(length);
            SerialUSB.print("RSSI is: ");
            SerialUSB.println(rssi);
            SerialUSB.print("Data is: ");
            for(unsigned char i = 0; i < length; i ++)
            {
                SerialUSB.print( char(buffer[i]));

            }
            SerialUSB.println();
        }
    }
  }
}

int MeasureDistance()
{
  VL53L0X_RangingMeasurementData_t RangingMeasurementData;
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	memset(&RangingMeasurementData,0,sizeof(VL53L0X_RangingMeasurementData_t));
	Status=VL53L0X.PerformSingleRangingMeasurement(&RangingMeasurementData);
	if(VL53L0X_ERROR_NONE==Status)
	{
		if(RangingMeasurementData.RangeMilliMeter>=2000)
		{
			SerialUSB.println("out of range!!");
      return 0;
		}
		else
		{
			SerialUSB.print("Measured distance:");
			SerialUSB.print(RangingMeasurementData.RangeMilliMeter);
			SerialUSB.println(" mm");
		}
	}
	else
	{
		SerialUSB.print("mesurement failed !! Status code =");
		SerialUSB.println(Status);
    return 0;
	}
  return RangingMeasurementData.RangeMilliMeter;
}

int MeasureWaterLevel()
{
  return analogRead(PIN_WATER_LEVEL)/2;
}

int MeasureSoilHumid()
{
  return analogRead(PIN_SOIL_HUMID)/2;
}

int MeasureBattery()
{
  int bat = analogRead(PIN_BATTERY);
  //bat stops working at 2.5V so 70
  //3.7V is 105
  if (bat > 96)
    bat = 3;
  else if (bat > 87)
    bat = 2;
  else if (bat > 78)
    bat = 1;
  else 
    bat = 0;
  return bat;
}

int MeasureCharging()
{
  int ch = digitalRead(PIN_CHARGING);
  if (ch==LOW)
    return 1;
  return 0;
}

void EncodeData(int distance, int waterLevel, int soilHumid, int battery, int charging)
{

/*
                Min Max   Number of bits
Time to Flight	10	2000	11
Water Sensor    0   511   9
Soil Humid	    0	  511	  9
Battery Level	  0	  3	    2
Charging        0   1     1
Total			                32

	bits
TF	0->10
WS	11->19
SH	20->28
BA	29->30
CH	30->31
*/

    data[0] = distance & 0xFF;
    data[1] = (distance >> 8) & 0x07;
    data[1] = data[1] | ((waterLevel & 0x1F) << 3);
    data[2] = (waterLevel >> 3) & 0x0F;
    data[2] = data[2] | ((soilHumid & 0x0F) << 4);
    data[3] = (soilHumid >> 4) & 0x1F;
    data[3] = data[3] | ((battery & 0x03) << 5);
    data[3] = data[3] | ((charging & 0x01) << 7);
    SerialUSB.print("Data: ");
    PrintHex8(data, 4);
    SerialUSB.println();
}

void PrintHex8(byte *data, uint8_t length) // prints 8-bit data in hex with leading zeroes
{
       SerialUSB.print("0x"); 
       for (int i=0; i<length; i++) { 
         if (data[i]<0x10) {SerialUSB.print("0");} 
         SerialUSB.print(data[i],HEX); 
         SerialUSB.print(" "); 
       }
}