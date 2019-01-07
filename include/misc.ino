int add(int a, int b) {
	return a + b;
}

int addr_to_str(byte addr[8], char* addr_str) {
  // Sensors have an 8 byte address, we can save memory if we only use the first 4
  int id_len = 4; // Either 4 or 8

  // Should addresses include the ":" separator
  byte include_colon = 1;

  // Init the return string
  strcpy(addr_str,"");

  for (int i = 0; i < id_len; i++) {
    char tmp_segment[5] = "";
    sprintf(tmp_segment,"%02x", addr[i]);

    /*
    char t[40] = "";
    sprintf(t,"byte #%i is %i = %s\n",i,addr[i],tmp_segment);
    Serial.print(t);
    */

    // If we're including colons, and it's NOT the last octet
    if ((include_colon) && i < (id_len - 1)) {
      strcat(tmp_segment,":");
    }

    // Append this segment to the return string
    strcat(addr_str,tmp_segment);
  }

  return id_len;
}

float c_to_f(float tempC) {
  float tempF = (tempC * 9 / 5) + 32;

  return tempF;
}

int get_ds_temp(byte pin, char sensor_id[][25], float* sensor_value) {
  OneWire ds(pin);

  byte data[12];
  byte addr[8];
  byte found = 0;

  while (ds.search(addr)) {

    if (OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return -1000;
    }

    if (addr[0] != 0x10 && addr[0] != 0x28) {
      Serial.print("Device is not recognized");
      return -1001;
    }

    found++;

    ds.reset();
    ds.select(addr);
    ds.write(0x44,1); // start conversion, with parasite power on at the end

    ds.reset();
    ds.select(addr);
    ds.write(0xBE); // Read Scratchpad

    for (byte i = 0; i < 9; i++) { // we need 9 bytes
      data[i] = ds.read();
    }

    byte MSB = data[1];
    byte LSB = data[0];

    float tempRead = ((MSB << 8) | LSB); //using two's compliment
    float TemperatureSum = tempRead / 16;

    char addr_str[25] = "";
    addr_to_str(addr, addr_str);

    /*
    char t[50] = "";
    sprintf(t,"Sensor #%i = %s\n",found,addr_str);
    Serial.print(t);
    */

    //sensor_id[found - 1] = addr_str;
    strcpy(sensor_id[found - 1], addr_str);
    sensor_value[found - 1] = c_to_f(TemperatureSum);
  }

  ds.reset_search();

  return found;
}

