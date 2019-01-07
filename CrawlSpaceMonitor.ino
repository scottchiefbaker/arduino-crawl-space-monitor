#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include "include/misc.ino"
#include <dht11.h>
#include "include/dht11.ino"

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 1, 1, 160);

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
					strcat(body,"Content-Type: text/html\n");
					strcat(body,"Connection: close\nRefresh: 5\n");
					strcat(body,"\n");
					strcat(body,"<!DOCTYPE HTML>\n");
					strcat(body,"<html>\n");

					Serial.print("Serving: ");
					Serial.println(uri);

					/////////////////////////////////////////////////////

					if (strcmp(uri,"/foo.html") == 0) {
						strcat(body,"foo.html!!!");
					} else {
						//////////////////////////////////////////////////////
						// Analog
						//////////////////////////////////////////////////////

						// output the value of each analog input pin
						for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
							int sensorReading = analogRead(analogChannel);

							sprintf(tmp_str,"Analog %i = %d<br />\n",analogChannel,sensorReading);

							strcat(body,tmp_str);
						}

						//////////////////////////////////////////////////////
						// Digital
						//////////////////////////////////////////////////////

						for (int dpin = 2; dpin <= 10; dpin++) {
							int value = digitalRead(dpin);

							sprintf(tmp_str,"Digital %i = %d<br />\n",dpin,value);

							strcat(body,tmp_str);
						}

						//////////////////////////////////////////////////////
						// DS18BS20
						//////////////////////////////////////////////////////
						int    count = 8;
						char   sensor_id[count][25];
						float  sensor_value[count];
						byte   sensor_pin = 2;

						int found = get_ds_temp(sensor_pin, sensor_id, sensor_value);

						for (int i = 0; i < found; i++) {
							char float_str[8] = "";
							dtostrf(sensor_value[i],4,1,float_str);

							sprintf(tmp_str,"DS18BS20 %s = %s<br />\n",sensor_id[i],float_str);
							strcat(body,tmp_str);
						}

						//////////////////////////////////////////////////////
						// DHT11
						//////////////////////////////////////////////////////

						int pin = 28;
						int ok  = init_dht11(pin, &DHT11);

						if (ok) {
							// Get the humidity
							int humidity = get_dht11_humidty(DHT11);

							// Get the temperature as a string
							char str_tempf[7] = ""; // char array to store the float value as a string
							get_dht11_temp_string(DHT11, str_tempf);

							sprintf(tmp_str,"DHT11 %i Humidity = %i<br />\n",pin,humidity);
							strcat(body,tmp_str);
							sprintf(tmp_str,"DHT11 %i Temperature = %s<br />\n",pin,str_tempf);
						} else {
							sprintf(tmp_str,"DHT11 %i Error<br />\n",pin);
						}

						strcat(body,tmp_str);

						//////////////////////////////////////////////////////////
					}

					/////////////////////////////////////////////////////

					hits++;

					unsigned long int uptime_seconds = (millis() / 1000);

					sprintf(tmp_str,"Uptime = %li seconds<br />\n",uptime_seconds);
					strcat(body,tmp_str);
					sprintf(tmp_str,"Hits = %li<br />\n",hits);
					strcat(body,tmp_str);

					strcat(body,"</html>");

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
