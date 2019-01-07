#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <dht11.h>

int   get_ds_temp(byte pin, char sensor_id[][25], float* sensor_value);
int   init_dht11(int pin, dht11 *obj);
float get_dht11_humidty(dht11 obj);
char  *get_dht11_temp_string(dht11 obj, char *ret);
char  *eos(char str[]);

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 1, 1, 160);

dht11 DHT11; // Glotal DHT11 object

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup() {
	// Open serial communications and wait for port to open:
	Serial.begin(57600);

	// start the Ethernet connection and the server:
	Ethernet.begin(mac, ip);
	server.begin();
	Serial.print("server is at ");
	Serial.println(Ethernet.localIP());

	//for (int dpin = 2; dpin <= 10; dpin++) {
	//    pinMode(dpin, INPUT);
	//}
}

unsigned long hits = 1;
void loop() {
	// listen for incoming clients
	EthernetClient client = server.available();
	if (client) {
		char body[1024]  = "";
		char line[200]   = "";
		char uri[30]     = "";
		char tmp_str[40] = "";

		Serial.println("new client");
		// an http request ends with a blank line
		boolean currentLineIsBlank = true;
		while (client.connected()) {
			if (client.available()) {
				char c = client.read();

				// Append this char to the existing line
				int line_end = strlen(line);
				line[line_end] = c;
				line[line_end + 1] = '\0';

				//Serial.write(c);
				// if you've gotten to the end of the line (received a newline
				// character) and the line is blank, the http request has ended,
				// so you can send a reply
				if (c == '\n' && currentLineIsBlank) {
					strcat(body,"HTTP/1.1 200 OK\n");
					strcat(body,"Content-Type: application/json\n");
					strcat(body,"Connection: close\nRefresh: 5\n");
					strcat(body,"\n");
					strcat(body,"{\n");

					Serial.print("Serving: ");
					Serial.println(uri);

					char *quick = strstr(uri,"quick");

					/////////////////////////////////////////////////////

					if (strcmp(uri,"/foo.html") == 0) {
						strcat(body,"foo.html!!!");
					} else {
						//////////////////////////////////////////////////////
						// Analog
						//////////////////////////////////////////////////////

						strcat(body,"\t\"analog\": {\n");

						int usableAnalogPins[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
						int numPins            = (sizeof(usableAnalogPins) / sizeof(usableAnalogPins[0]));

						// output the value of each analog input pin
						for (int i = 0; i < numPins; i++) {
							int aPin          = usableAnalogPins[i];
							int sensorReading = analogRead(aPin);

							sprintf(tmp_str,"\t\t\"%i\": %d",aPin,sensorReading);

							if (i < numPins - 1) {
								strcat(tmp_str,",\n");
							} else {
								strcat(tmp_str,"\n");
							}

							strcat(body,tmp_str);
						}

						strcat(body,"\t},\n");

						//////////////////////////////////////////////////////
						// Digital
						//////////////////////////////////////////////////////

						strcat(body,"\t\"digital\": {\n");

						int usableDigitalPins[] = { 3,4,5,6,7,8,9,11,12,13,14,15 }; // 0 and 1 are serial, 2 is DS18B20, 10 is Ethernet
						numPins                 = (sizeof(usableDigitalPins) / sizeof(usableDigitalPins[0]));

						for (int i = 0; i < numPins; i++) {
							int dpin  = usableDigitalPins[i];
							int value = digitalRead(dpin);

							sprintf(tmp_str,"\t\t\"%i\": %d",dpin,value);

							if (i < numPins - 1) {
								strcat(tmp_str,",\n");
							} else {
								strcat(tmp_str,"\n");
							}

							strcat(body,tmp_str);
						}

						strcat(body,"\t},\n");

						//////////////////////////////////////////////////////
						// DS18BS20
						//////////////////////////////////////////////////////
						int    count       = 8;
						int    ds18b20_pin = 2;
						char   sensor_id[count][25];
						float  sensor_value[count];

						if (!quick) {
							int found = get_ds_temp(ds18b20_pin, sensor_id, sensor_value);

							strcat(body,"\t\"DS18B20\": {\n");

							for (int i = 0; i < found; i++) {
								char float_str[8] = "";
								dtostrf(sensor_value[i],4,1,float_str);

								sprintf(tmp_str,"\t\t\"%s\": %s",sensor_id[i],float_str);

								if (i < found - 1) {
									strcat(tmp_str,",\n");
								} else {
									strcat(tmp_str,"\n");
								}

								strcat(body,tmp_str);
							}
							strcat(body,"\t},\n");
						}

						//////////////////////////////////////////////////////
						// DHT11
						//////////////////////////////////////////////////////

						int dht11_pin = 28;

						if (!quick) {
							int ok  = init_dht11(dht11_pin, &DHT11);

							strcat(body,"\t\"DHT11\": {\n");

							if (ok) {
								// Get the humidity
								int humidity = get_dht11_humidty(DHT11);

								// Get the temperature as a string
								char str_tempf[7] = ""; // char array to store the float value as a string
								get_dht11_temp_string(DHT11, str_tempf);

								sprintf(tmp_str,"\t\t\"%i\": {\n\t\t\t\"humidity\": %i,\n\t\t\t\"temperature\": %s\n\t\t}\n",dht11_pin,humidity,str_tempf);
							} else {
								sprintf(tmp_str,"\t\t\"%i\": \"error\"\n",dht11_pin);
							}

							strcat(body,tmp_str);

							strcat(body,"\t},\n");
						}

						//////////////////////////////////////////////////////////
					}

					/////////////////////////////////////////////////////

					hits++;

					unsigned long int uptime_seconds = (millis() / 1000);

					sprintf(tmp_str,"\t\"uptime\": %li,\n",uptime_seconds);
					strcat(body,tmp_str);
					sprintf(tmp_str,"\t\"hits\": %li\n",hits);
					strcat(body,tmp_str);

					strcat(body,"}\n");

					client.print(body);
					break;
				}

				if (c == '\n') {
					// you're starting a new line
					currentLineIsBlank = true;

					// Check if the line is the GET line, if it is store the URI
					if ((strncmp("GET ",line, 4)) == 0) {
						char* tmp_str;

						tmp_str = strtok(line," "); // find the first space
						tmp_str = strtok(NULL," "); // find the second space

						strcpy(uri,tmp_str);
					}

					//line[0] = '\0';
					strcpy(line,"");
				} else if (c != '\r') {
					// you've gotten a character on the current line
					currentLineIsBlank = false;
				}
			}
		}

		// give the web browser time to receive the data
		delay(1);
		// close the connection:
		client.stop();
		Serial.println("client disconnected");
	}
}

///////////////////////////////////////////////////////////////////////////////////////

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

// Return a point to the end of the string (good for appending with sprintf)
// https://stackoverflow.com/questions/2674312/how-to-append-strings-using-sprintf
char *eos(char str[]) {
	return (str) + strlen(str);
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

////////////////////////////////////////////////////////////////////
// DHT11 stuff
////////////////////////////////////////////////////////////////////

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
