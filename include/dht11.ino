dht11 DHT11;

float get_dht11_humidty(dht11 obj) {
  return (int)obj.humidity;
}

float get_dht11_temp(dht11 obj) {
  return c_to_f(obj.temperature);
}

char *get_dht11_temp_string(dht11 obj, char *ret) {
	float temperature = c_to_f(obj.temperature);

	// Init ret to be 100% sure
	strcpy(ret,"");

	// Convert the float to a left aligned string
	dtostrf(temperature, -6, 2, ret);

	// Remove any trailing whitespace
	for (byte i = 0; i < strlen(ret); i++) {
		if (ret[i] == ' ') {
			ret[i] = 0;
		}
	}

	return ret;
}

int init_dht11(int pin, dht11 *obj) {
	int check = obj->read(pin); // Has to run before you get temp

	if (check == 0) {
		return 1;
	} else {
		return 0;
	}
}
