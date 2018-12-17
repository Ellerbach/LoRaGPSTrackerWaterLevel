        private static string DecoderSearchAndRescueGpsSensor(byte[] payload, uint fport)
        {
            dynamic decoded = new JObject();


            if (payload.Length == 2)    /* receiving ONLY ONE delta */
            {
                var lat_val = (float)payload[0] - 127; //positive or negative?
                var lng_val = (float)payload[1] - 127;

                decoded.lat = lat_val / 10000;
                decoded.lng = lng_val / 10000;
            }
            else if (payload.Length == 9)     /* receiving full coordinates */
            {

                byte[] bytes_lat = { payload[3], payload[2], payload[1], payload[0] };
                byte[] bytes_lng = { payload[7], payload[6], payload[5], payload[4] };

                //default lora mote
                var lat_val = BitConverter.ToInt32(bytes_lat, 0);
                var lng_val = BitConverter.ToInt32(bytes_lng, 0);

                decoded.lat = (float)lat_val / Math.Pow(2, 23) * 90;
                decoded.lng = (float)lng_val / Math.Pow(2, 23) * 180;  
                decoded.pres = payload[8] * 2;              
            }
            else
            {
                return $"{{\"error\": \" length of '{payload.Length}' bytes is not supported, try 8 bytes for a complete GPS coordinate or 2 for a delta }}";
            }

            return decoded.ToString(Newtonsoft.Json.Formatting.None);

        }
