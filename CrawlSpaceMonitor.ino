#include <Ethernet.h>
#include <DHT.h>        // AdaFruit: 'DHT sensor library'
#include "MemoryFree.h" // https://github.com/maniacbug/MemoryFree

// ds18b20 requires 'DallasTemperature' lib
#include "OneWire.h"
#include "DallasTemperature.h"

// Skip pins #0 and #1 cuz they're Serial RX/TX
// Ethernet shield pins: https://arduino.stackexchange.com/questions/33019/arduino-ethernet-shield-on-arduino-mega-pin-usage
// Pins #10, #50, #51, #52, and #53 are used for Ethernet on the Mega.
// Pin #4 is used by the SD card
uint8_t dpins[]        = { 2,3,4,5,6,7,8,9,14,15,16,17,18 };
uint8_t apins[]        = { 0,1,2,3,4,5 };
uint8_t dht11_pins[]   = { 28 };
uint8_t ds18b20_pins[] = { 22, 23, 24, 25, 29 };

byte mac[]  = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xCC };
IPAddress   ip(192, 168, 5, 99);

uint32_t query_start = 0;

///////////////////////////////////////////////////////////////////////

EthernetServer server(80);
uint32_t hits            = 0;
uint8_t alast_val        = apins[sizeof(apins)/sizeof(apins[0]) - 1];
uint8_t dlast_val        = dpins[sizeof(dpins)/sizeof(dpins[0]) - 1];
uint8_t dht11_last_val   = dht11_pins[sizeof(dht11_pins)/sizeof(dht11_pins[0]) - 1];
uint8_t ds18b20_last_val = ds18b20_pins[sizeof(ds18b20_pins)/sizeof(ds18b20_pins[0]) - 1];

void setup() {
	Serial.begin(115200);

	// Start the Ethernet connection and the server:
	Ethernet.begin(mac, ip);

	// Check for Ethernet hardware present
	if (Ethernet.hardwareStatus() == EthernetNoHardware) {
		Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
		while (true) {
			delay(1); // do nothing, no point running without Ethernet hardware
		}
	}

	if (Ethernet.linkStatus() == LinkOFF) {
		Serial.println("Ethernet cable is not connected.");
	}

	// Set all the digital pin to input
	for (int pin : dpins) {
		pinMode(pin, INPUT_PULLUP);
	}

	// start the WebServer
	server.begin();
	Serial.print("WebServer is at ");
	Serial.println(Ethernet.localIP());
}

void loop() {
	// listen for incoming clients
	EthernetClient client = server.available();

	char html[1024] = "";

	if (client) {
		// Serial.println("new client");
		query_start = millis();

		// We pre-reserve 256 bytes to avoid fragmentation
		String header = "";
		header.reserve(256);

		// An http request ends with a blank line
		bool currentLineIsBlank = true;
		while (client.connected() && client.available()) {
			char c = client.read();

			// Read char by char HTTP request
			if (header.length() < 256) {
				// Store characters to string
				header += c;
			}

			// if you've gotten to the end of the line (received a newline
			// character) and the line is blank, the http request has ended,
			// so you can send a reply
			if (c == '\n' && currentLineIsBlank) {
				//Serial.print(header.c_str());

				hits++;

				///////////////////////////////////////////////
				// We've got enough info we can start sending
				// a response now.
				///////////////////////////////////////////////
				build_response(html, header);

				String remote_ip     = ip_2_string(client.remoteIP());
				int16_t content_size = strlen(html);
				String url           = extract_url(header);

				char buf[64] = "";
				snprintf(buf, 64, "Sent %i bytes to %s for %s\r\n", content_size, remote_ip.c_str(), url.c_str());
				Serial.print(buf);

				// send the HTML to the client
				client.print(html);

				//////////////////////////////////////////////////////////////
				//////////////////////////////////////////////////////////////

				break;
			}

			if (c == '\n') {
				// you're starting a new line
				currentLineIsBlank = true;
			} else if (c != '\r') {
				// you've gotten a character on the current line
				currentLineIsBlank = false;
			}
		}

		// give the web browser time to receive the data
		delay(1);
		// close the connection:
		client.stop();
		//Serial.println("client disconnected");
	}
}

char *process_analog(char str[]) {
	sprintf(eos(str),"\"analog\": {");

	for (int pin : apins) {
		int pval = analogRead(pin);

		sprintf(eos(str), "\"%d\":%d", pin, pval);

		if (pin != alast_val) {
			sprintf(eos(str), ",");
		}
	}

	sprintf(eos(str), "},\n"); // Close analog

	return str;
}

char *process_digital(char str[]) {
	sprintf(eos(str), "\"digital\": {");
	for (int pin : dpins) {
		pinMode(pin, INPUT);
		int pval = digitalRead(pin);

		sprintf(eos(str), "\"%d\":%d", pin, pval);

		if (pin != dlast_val) {
			sprintf(eos(str), ",");
		}
	}

	sprintf(eos(str), "},\n"); // Close digital

	return str;
}

char *process_ds18b20(char str[]) {
	//sprintf(eos(str), "\"ds18b20\": { \"Not implemented yet\": true },\n");
	//return str;

	sprintf(eos(str), "\"ds18b20\": {");

	for (int pin : ds18b20_pins) {
		int   count = 8;           // Number of sensors to look for
		char  sensor_id[count][25]; // HEX string of the sensor ID
		float sensor_value[count]; // Temperatur in F

		int found = get_ds_temp(pin, sensor_id, sensor_value);

		char buf[6] = "";
		dtostrf(sensor_value[0], 4,1, buf);

		if (found > 0) {
			//sprintf(eos(str), "\"%d\": {\"id\": \"%s\", \"tempF\": %s, \"found\": %i}", pin, sensor_id[0], buf, found);
			sprintf(eos(str), "\"%d\": {\"id\": \"%s\", \"tempF\": %s}", pin, sensor_id[0], buf);
		} else {
			sprintf(eos(str), "\"%d\": {}", pin);
		}

		if (pin != ds18b20_last_val) {
			sprintf(eos(str), ",");
		}
	}

	sprintf(eos(str), "},\n"); // Close section

	return str;
}


char *process_extra(char str[], String header) {
	if (is_simple_request(header)) {
		sprintf(eos(str), "\"simple\": true,\n");
	} else {
		sprintf(eos(str), "\"simple\": false,\n");
	}

	String url = extract_url(header);

	int16_t query_time_ms = millis() - query_start;

	sprintf(eos(str), "\"hits\": %lu,\n", hits);
	sprintf(eos(str), "\"uptime\": %lu,\n", millis() / 1000);
	sprintf(eos(str), "\"qps\": %d,\n", hits / (millis() / 1000));
	sprintf(eos(str), "\"query_ms\": %d,\n", query_time_ms);
	sprintf(eos(str), "\"free_memory\": %d,\n", freeMemory());
	sprintf(eos(str), "\"url\": \"%s\"\n", url.c_str());

	return str;
}

char *process_dht11(char str[]) {
	static long last_hit = 0;
	bool too_soon        = (millis() - last_hit < 2000);

	sprintf(eos(str), "\"dht22\": {");

	for (int pin : dht11_pins) {
		// Too soon, must wait about two seconds between hits
		if (too_soon) {
			sprintf(eos(str), "\"%d\": [\"too soon\"]", pin);

			if (pin != dht11_last_val) {
				sprintf(eos(str), ",");
			}

			continue;
		}

		DHT dht(pin, DHT22);
		dht.begin();

		float humid = dht.readHumidity();
		float tempF = dht.readTemperature(true);

		char temp_buf[6] = "";
		dtostrf(tempF, 4,1, temp_buf);
		char humid_buf[6] = "";
		dtostrf(humid, 4,1, humid_buf);

		// Make sure we get actual data back from the sensor
		if (isnan(humid) || isnan(tempF)) {
			sprintf(eos(str), "\"%d\": {\"temp\": -1, \"humid\": -1}", pin);
		} else {
			sprintf(eos(str), "\"%d\": {\"temp\": %s, \"humid\": %s}", pin, temp_buf, humid_buf);
		}

		if (pin != dht11_last_val) {
			sprintf(eos(str), ",");
		}
	}

	sprintf(eos(str), "},\n"); // Close section

	// Store when the last hit was
	if (!too_soon) {
		last_hit = millis();
	}

	return str;
}

char *eos(char str[]) {
	return (str) + strlen(str);
}

String extract_url(String headers) {
	int16_t url_start = headers.indexOf("GET ");
	int16_t url_end   = headers.indexOf(" ", url_start + 5);

	String url = "";
	if (url_start >= 0) {
		url_start += 3;
		url = headers.substring(url_start + 1, url_end);
	}

	return url;
}

bool is_simple_request(String header) {
	bool simple = false;

	// If the request contains "?simple"
	if (header.indexOf("?simple") > 0) {
		simple = true;
	}

	return simple;
}

void build_response(char* html, String header) {
	bool simple = is_simple_request(header);

	////////////////////////////////////////////////

	//Serial.print("Sending respond packet");

	// HTTP Header
	sprintf(eos(html), "HTTP/1.1 200 OK\r\n");
	sprintf(eos(html), "Content-Type: application/json\r\n");
	sprintf(eos(html), "Connection: close\r\n");
	sprintf(eos(html), "\r\n");

	// Start the JSON block
	sprintf(eos(html), "{\n");

	//////////////////////////////////////////////

	process_analog(html);
	process_digital(html);

	//////////////////////////////////////////////

	if (!simple) {
		process_dht11(html);
		process_ds18b20(html);
	}

	// Footer information
	process_extra(html, header);

	// Close the whole JSON
	sprintf(eos(html), "}\n");
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
		float tempF    = ((tempRead / 16) * 1.8f) + 32;

		char addr_str[25] = "";
		addr_to_str(addr, addr_str);

		/*
		char t[50] = "";
		sprintf(t,"Sensor #%i = %s\n",found,addr_str);
		Serial.print(t);
		*/

		strcpy(sensor_id[found - 1], addr_str);
		sensor_value[found - 1] = tempF;
	}

	ds.reset_search();

	return found;
}

String ip_2_string(const IPAddress& ipAddress) {
  String ret = String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;

  return ret;
}

int addr_to_str(byte addr[8], char* addr_str) {
	// Sensors have an 8 byte address, we can save memory if we only use the first 4
	int id_len = 4; // Either 4 or 8

	// Should addresses include the ":" separator
	byte include_colon = 0;

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

/*

OLD version

<html><head><title>Arduino</title></head><body>
<h1>Arduino Ethernet Shield</h1>

<div>Digital #0: 1</div>
<div>Digital #1: 1</div>
<div>Digital #2: 1</div>
<div>Digital #3: 1</div>
<div>Digital #4: 0</div>
<div>Digital #5: 0</div>
<div>Digital #6: 1</div>
<div>Digital #7: 1</div>
<div>Digital #8: 1</div>
<div>Digital #9: 1</div>
<div>Digital #14: 0</div>
<div>Digital #15: 0</div>
<div>Digital #16: 0</div>
<div>Digital #17: 0</div>
<div>Digital #18: 0</div>
<div>Analog #0: 256</div>
<div>Analog #1: 991</div>
<div>Analog #2: 544</div>
<div>Analog #3: 675</div>
<div>Analog #4: 583</div>
<div>Analog #5: 522</div>
<div>Analog #6: 520</div>
<div>Analog #7: 455</div>
<div>Analog #8: 416</div>
<div>Analog #9: 412</div>
<div>Analog #10: 431</div>
<div>Analog #11: 392</div>
<div>Analog #12: 378</div>
<div>Analog #13: 364</div>
<div>Analog #14: 360</div>
<div>Analog #15: 392</div>
<div>DHT11 #28:  57.20/72</div>
<div>DS18D20 #245:  57.65</div>
<div>DS18D20 #196:  88.59</div>
<div>DS18D20 #102: 185.00</div>
<div>Hits: 6461772</div>
<div>Uptime: 2545030 seconds</div>
<div>FreeMemory: 6925 bytes</div>
*/
