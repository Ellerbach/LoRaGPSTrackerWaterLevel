// Use this define if you want to send a real LoRaWan message
// That is the default
#define USE_LORA
// Use this define if you want to always send the full coordinates
#define ALWAYS_FULL
// Use this one if you are in Europe
#define LORA_EU

#include <LoRaWan.h>
#include <TinyGPS.h>

TinyGPS gps;

char data[51];
char buffer[256];
float flat, flon;

float prevLat = -1;
float prevLon = -1;

float distanceThreshold = .005;       // minimal distance to move before sending data
int sendFullCoordinatesInterval = 10; // force sending the full coordinates every XX loops
int currentLoop = 0;

#ifdef LORA_EU
_data_rate_t dr=DR6;
_physical_type_t physicalType = EU868;
#else
_data_rate_t dr=DR0; 
_physical_type_t physicalType = US915HYBRID;
#endif

float DistanceTo(double lat1, double lon1, double lat2, double lon2, char unit = 'K')
{
    const float Pi = 3.14159;

    double rlat1 = Pi * lat1 / 180;
    double rlat2 = Pi * lat2 / 180;
    double theta = lon1 - lon2;
    double rtheta = Pi * theta / 180;
    double dist =
        sin(rlat1) * sin(rlat2) + cos(rlat1) * cos(rlat2) * cos(rtheta);
    dist = acos(dist);
    dist = dist * 180 / Pi;
    dist = dist * 60 * 1.1515;

    switch (unit)
    {
    case 'K': //Kilometers -> default
        return dist * 1.609344;
    case 'N': //Nautical Miles
        return dist * 0.8684;
    case 'M': //Miles
        return dist;
    }

    return dist;
}

void setup(void)
{
    SerialUSB.begin(115200);
    //while (!SerialUSB);

    Serial.begin(9600); // open the GPS

#ifdef USE_LORA

    lora.init();
    //lora.setDeviceReset();
    lora.setId(NULL, "5555ABCDEF123456", "5555000000001234");
    lora.setKey(NULL, NULL, "55551234567890ABCDEF1234567890AB");

    lora.setDeciveMode(LWOTAA);
    lora.setDataRate(dr, physicalType);

#ifdef LORA_EU 
    lora.setChannel(0, 868.1);
    lora.setChannel(1, 868.3);
    lora.setChannel(2, 868.5);
    
    lora.setReceiceWindowFirst(0, 868.1);
#else
    // You can leave the default one for US
#endif
    lora.setAdaptiveDataRate(false);

    lora.setDutyCycle(false);
    lora.setJoinDutyCycle(false);

    lora.setPower(14);


    while (!lora.setOTAAJoin(JOIN, 20000))
        ;
#endif
}

void loop(void)
{
    SerialUSB.println("In loop, getting GPS data ...");

    // For one second we parse GPS data and report some key values
    bool newData = false;
    for (unsigned long start = millis(); millis() - start < 1000;)
    {
        while (Serial.available())
        {
            char c = Serial.read();
            //SerialUSB.write(c); // uncomment this line if you want to see the GPS data flowing
            if (gps.encode(c)) // Did a new valid sentence come in?
            {
                newData = true;
            }
        }
    }

    if (newData)
    {
        SerialUSB.println("Got a position!");
        unsigned long age;
        float flat, flon;

        gps.f_get_position(&flat, &flon, &age);
        SerialUSB.print("flat:");
        SerialUSB.print(flat, 6);
        SerialUSB.print(" flon:");
        SerialUSB.println(flon, 6);

        bool shouldSendFullCoordinates = false;
        bool shouldSendDelta = false;

        // calc distance
        float dist;
        if (prevLat != -1 && prevLon != -1)
        {
            dist = DistanceTo(flat, flon, prevLat, prevLon, 'K');
            SerialUSB.print("Calculated distance: ");
            SerialUSB.println(dist, 5);

#ifndef ALWAYS_FULL
            if (dist > distanceThreshold || prevLat == -1) // larger than 5 meters
            {
                SerialUSB.println("Distance is larger than 5 meters.");
                if (abs(flat - prevLat) > 0.000127 || abs(flon - prevLon) > 0.000127)
                {
                    SerialUSB.print("Coordinates delta > 0.000127, so send full data.");
                    shouldSendFullCoordinates = true;
                }
                else
                {
                    SerialUSB.print("Coordinates delta is < 0.000127, so send delta.");
                    shouldSendDelta = true;
                }
            }
            else
            {
                SerialUSB.print("Distance is too small, do nothing.");
                // do nothing, distance is too small
            }
#else 
            shouldSendFullCoordinates = true;
            shouldSendDelta = false;
#endif

        }
        else
        {
            SerialUSB.print("Initial coordinates, so send in full.");
            shouldSendFullCoordinates = true;
        }

        if(currentLoop % sendFullCoordinatesInterval == 0)
        {
            shouldSendFullCoordinates = true;
            shouldSendDelta = false;
        }

        if (shouldSendFullCoordinates)
        {
            SerialUSB.print("Sending full coordinates.");
            byte bytes[9];

            int ilat = flat * pow(2, 23) / 90;

            bytes[0] = (byte)((ilat >> 24) & 0xff);
            bytes[1] = (byte)((ilat >> 16) & 0xff);
            bytes[2] = (byte)((ilat >> 8) & 0xff);
            bytes[3] = (byte)(ilat & 0xff);

            int ilon = flon * pow(2, 23) / 180;

            bytes[4] = (byte)((ilon >> 24) & 0xff);
            bytes[5] = (byte)((ilon >> 16) & 0xff);
            bytes[6] = (byte)((ilon >> 8) & 0xff);
            bytes[7] = (byte)(ilon & 0xff);

            unsigned long precision = gps.hdop();
            if (precision == 0xFFFFFFFF)
                bytes[8] = 0;
            else
            {
                precision = precision / 2;
                if (precision > 255)
                    precision = 255;
                bytes[8] = (byte)precision;
            }
            SerialUSB.print("bytes ");
            for (int i = 0; i < 8; i++)
            {
                SerialUSB.print(bytes[i]);
                SerialUSB.print("-");
            }
            SerialUSB.println();

#ifdef USE_LORA
            bool result = lora.transferPacket(bytes, 9);

            if (result)
            {
                short length;
                short rssi;

                memset(buffer, 0, 256);
                length = lora.receivePacket(buffer, 256, &rssi);

                if (length)
                {
                    SerialUSB.print("Length is: ");
                    SerialUSB.println(length);
                    SerialUSB.print("RSSI is: ");
                    SerialUSB.println(rssi);
                    SerialUSB.print("Data is: ");
                    for (unsigned char i = 0; i < length; i++)
                    {

                        SerialUSB.print(char(buffer[i]));
                    }
                    SerialUSB.println();
                }
            }
#endif
            prevLat = flat;
            prevLon = flon;
        }
        if (shouldSendDelta)
        {
            SerialUSB.print("Sending delta.");

            float fLatDelta = (flat - prevLat) * pow(10, 5); // = 17
            float fLonDelta = (flon - prevLon) * pow(10, 5); // = -5
            SerialUSB.print("fLatDelta:");
            SerialUSB.print(fLatDelta, 6);
            SerialUSB.print(" fLonDelta:");
            SerialUSB.println(fLonDelta, 6);

            int iLatDelta = fLatDelta + 127; // = 144
            int iLonDelta = fLonDelta + 127; // = 122
            SerialUSB.print("iLatDelta:");
            SerialUSB.print(iLatDelta, 6);
            SerialUSB.print(" iLonDelta:");
            SerialUSB.println(iLonDelta, 6);

            byte deltaBytes[2];
            deltaBytes[0] = iLatDelta;
            deltaBytes[1] = iLonDelta;

            SerialUSB.println("delta bytes: ");
            SerialUSB.println(deltaBytes[0]);
            SerialUSB.println(deltaBytes[1]);

#ifdef USE_LORA
            bool result = lora.transferPacket(deltaBytes, 2);

            if (result)
            {
                short length;
                short rssi;

                memset(buffer, 0, 256);
                length = lora.receivePacket(buffer, 256, &rssi);

                if (length)
                {
                    SerialUSB.print("Length is: ");
                    SerialUSB.println(length);
                    SerialUSB.print("RSSI is: ");
                    SerialUSB.println(rssi);
                    SerialUSB.print("Data is: ");
                    for (unsigned char i = 0; i < length; i++)
                    {

                        SerialUSB.print(char(buffer[i]));
                    }
                    SerialUSB.println();
                }
            }
#endif
            prevLat = flat;
            prevLon = flon;
        }
    }
    else
    {

        SerialUSB.println("No pos");
    }

    delay(10000);
    currentLoop++;
}
