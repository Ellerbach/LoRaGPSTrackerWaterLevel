using Newtonsoft.Json;
using System;

namespace EncoderDecoder
{
    class Program
    {
        static byte[] data = new byte[4];
        static int distance = 0;
        static int waterLevel = 0;
        static int soilHumid = 0;
        static int battery = 0;
        static int charging = 0;

        static void Main(string[] args)
        {
            // set everythign to 0, encode and decode
            EncodeDecode(distance, waterLevel, soilHumid, battery, charging);
            // Now check the full value individually
            distance = 2000;
            EncodeDecode(distance, waterLevel, soilHumid, battery, charging);
            distance = 0; waterLevel = 511;
            EncodeDecode(distance, waterLevel, soilHumid, battery, charging);
            waterLevel = 0; soilHumid = 511;
            EncodeDecode(distance, waterLevel, soilHumid, battery, charging);
            soilHumid = 0; battery = 3;
            EncodeDecode(distance, waterLevel, soilHumid, battery, charging);
            battery = 0; charging = 1;
            EncodeDecode(distance, waterLevel, soilHumid, battery, charging);


            Console.ReadKey();
        }

        static void EncodeDecode(int distance, int waterLevel, int soilHumid, int battery, int charging)
        {
            Console.WriteLine($"Height: {distance} WaterLevel: {waterLevel} SoilHumidity: {soilHumid} Battery: {battery} Charging: {charging}");
            EncodeData(distance, waterLevel, soilHumid, battery, charging);
            Console.WriteLine(DecoderWaterSensors(data, 1));
        }

        static string DecoderWaterSensors(byte[] payload, uint fport)
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
            if (payload.Length != 4)
                return null;

            WaterSensors waterSensors = new WaterSensors();
            waterSensors.WaterLevel = payload[0] + ((payload[1] & 0x07) << 8);
            waterSensors.WaterPresence = ((payload[1] >> 3) & 0x1F) + ((payload[2] & 0x0F) << 3);
            waterSensors.SoilHumidity = ((payload[2] >> 4) & 0x0F) + ((payload[3] & 0x1F) << 4);
            waterSensors.Battery = (payload[3] >> 5) & 0x03;
            if ((byte)(payload[3] & 0x80) == 0x80)
                waterSensors.Charging = true;
            else
                waterSensors.Charging = false;

            return JsonConvert.SerializeObject(waterSensors);
        }

        public class WaterSensors
        {
            public int WaterLevel { get; set; }
            public int WaterPresence { get; set; }
            public int SoilHumidity { get; set; }
            public int Battery { get; set; }
            public bool Charging { get; set; }
        }


        static void EncodeData(int distance, int waterLevel, int soilHumid, int battery, int charging)
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

            data[0] = (byte)(distance & 0xFF);            
            data[1] = (byte)((distance >> 8) & 0x07);            
            data[1] = (byte)(data[1] | ((waterLevel & 0x1F) << 3));            
            data[2] = (byte)((waterLevel >> 3) & 0x0F);            
            data[2] = (byte)(data[2] | ((soilHumid & 0x0F) << 4));            
            data[3] = (byte)((soilHumid >> 4) & 0x1F);            
            data[3] = (byte)(data[3] | ((battery & 0x03) << 5));            
            data[3] = (byte)(data[3] | ((charging & 0x01) << 7));            
            Console.WriteLine($"{BitConverter.ToString(data)}");
        }
    }
}
