#include <Ethernet.h>
#include <DHT.h>        // AdaFruit: 'DHT sensor library'
#include "MemoryFree.h" // https://github.com/maniacbug/MemoryFree

// ds18b20 requires 'DallasTemperature' lib
#include "OneWire.h"
#include "DallasTemperature.h"

#include <ArduinoJson.h>

// Skip pins #0 and #1 cuz they're Serial RX/TX
// Ethernet shield pins: https://arduino.stackexchange.com/questions/33019/arduino-ethernet-shield-on-arduino-mega-pin-usage
// Pins #10, #50, #51, #52, and #53 are used for Ethernet on the Mega.
// Pin #4 is used by the SD card
uint8_t dpins[]        = { 2,3,4,5,6,7,8,9,14,15,16,17,18 };
uint8_t apins[]        = { 0,1,2,3,4,5 };
uint8_t dht11_pins[]   = { 28 };
uint8_t ds18b20_pins[] = { 22, 23, 24, 25, 29 };
uint32_t query_start   = 0;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xCC };
IPAddress  ip(192, 168, 5, 99);

EthernetServer server(80);

///////////////////////////////////////////////////////////////////////

uint32_t hits = 0;

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
				String resp = build_response(header);

				/*
				*/
				String remote_ip     = ip_2_string(client.remoteIP());
				int16_t content_size = resp.length();
				String url           = extract_url(header);

				char buf[64] = "";
				snprintf(buf, 64, "Sent %i bytes to %s for %s\r\n", content_size, remote_ip.c_str(), url.c_str());
				Serial.print(buf);

				// send the response HTTP data to the client
				client.print(resp);

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

JsonDocument process_analog() {
	JsonDocument doc;

	for (int pin : apins) {
		int pval = analogRead(pin);
		doc[(String)pin] = pval;
	}

	return doc;
}

JsonDocument process_digital() {
	JsonDocument doc;

	for (int pin : dpins) {
		pinMode(pin, INPUT);

		int pval = digitalRead(pin);
		doc[(String)pin] = pval;
	}

	return doc;
}

JsonDocument process_ds18b20() {
	static JsonDocument doc;

	static unsigned long last_hit = 0;
	const bool too_soon           = abs(millis() - last_hit) < 2000;

	if (last_hit && too_soon) {
		// We don't clear the object and instead use the previous value
		doc["cached"] = true;
	} else {
		doc.clear();
	}

	// Loop over the ds18b210 pins
	for (int pin : ds18b20_pins) {
		// Don't bother processing
		if (too_soon) {
			break;
		}

		int   count = 8;            // Number of sensors to look for
		char  sensor_id[count][17]; // HEX string of the sensor ID
		float sensor_value[count];  // Temperatur in F

		int found = get_ds_temp(pin, sensor_id, sensor_value);

		// Loop over the number of sensors we found
		if (found == 0) {
			doc[(String)pin]["id"] = "";
		} else {
			for (int i = 0; i < found; i++) {
				doc[(String)pin]["id"]    = sensor_id[i];
				doc[(String)pin]["tempF"] = sensor_value[i];
			}
		}
	}

	// Store when the last hit was
	if (!too_soon) {
		last_hit = millis();
	}

	return doc;
}

JsonDocument process_extra(JsonDocument doc, String header) {
	if (is_simple_request(header)) {
		doc["simple"] = true;
	} else {
		doc["simple"] = false;
	}

	String url = extract_url(header);
	int16_t query_time_ms = millis() - query_start;

	doc["hits"]        = hits;
	doc["uptime"]      = millis() / 1000;
	doc["qps"]         = hits / (millis() / 1000.0);
	doc["query_ms"]    = query_time_ms;
	doc["free_memory"] = freeMemory();
	doc["url"]         = url;

	return doc;
}

JsonDocument process_dht11() {
	static JsonDocument doc;

	static unsigned long last_hit = 0;
	const bool too_soon           = abs(millis() - last_hit) < 5000;

	if (last_hit && too_soon) {
		// We don't clear the object and instead use the previous value
		doc["cached"] = true;
	} else {
		doc.clear();
	}

	for (int pin : dht11_pins) {
		// Don't bother processing
		if (too_soon) {
			break;
		}

		DHT dht(pin, DHT22);
		dht.begin();

		float humid = dht.readHumidity();
		float tempF = dht.readTemperature(true);

		// Invalid data back from sensor
		if (isnan(humid) || isnan(tempF)) {
			doc[(String)pin]["temp"]  = -1;
			doc[(String)pin]["humid"] = -1;
		} else {
			doc[(String)pin]["temp"]  = tempF;
			doc[(String)pin]["humid"] = humid;
		}
	}

	if (!too_soon) {
		last_hit = millis();
	}

	return doc;
}

char *eos(char str[]) {
	return (str) + strlen(str);
}

String extract_url(String headers) {
	int16_t url_start = headers.indexOf("GET ");
	int16_t url_end   = headers.indexOf(" ", url_start + 5);

	String url = "";
	if (url_start >= 0) {
		url = headers.substring(url_start + 4, url_end);
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

String build_response(String header) {
	JsonDocument doc;

	bool simple = is_simple_request(header);

	////////////////////////////////////////////////

	//Serial.print("Sending respond packet");

	String resp_headers = "";

	// HTTP Header
	resp_headers += "HTTP/1.1 200 OK\r\n";
	resp_headers += "Content-Type: application/json\r\n";
	resp_headers += "Connection: close\r\n";
	resp_headers += "\r\n";

	//////////////////////////////////////////////

	doc["analog"]  = process_analog();
	doc["digital"] = process_digital();

	//////////////////////////////////////////////

	if (!simple) {
		doc["dht22"]   = process_dht11();
		doc["ds18b20"] = process_ds18b20();

		// Various stats
		doc = process_extra(doc, header);
	}

	String json_str = "";
	serializeJson(doc, json_str);

	String ret = resp_headers + json_str;

	return ret;
}

int get_ds_temp(byte pin, char sensor_id[][17], float* sensor_value) {
	OneWire ds(pin);

	byte data[9];
	byte addr[8];
	uint8_t num_found = 0;

	while (ds.search(addr)) {
		if (OneWire::crc8(addr, 7) != addr[7]) {
			Serial.println("CRC is not valid!");
			return -1000;
		}

		if (addr[0] != 0x10 && addr[0] != 0x28) {
			Serial.print("Device is not recognized");
			return -1001;
		}

		ds.reset();
		ds.select(addr);
		ds.write(0x44,1); // Start conversion, with parasite power on at the end

		ds.reset();
		ds.select(addr);
		ds.write(0xBE); // Read Scratchpad

		for (byte i = 0; i < 9; i++) { // Read 9 bytes
			data[i] = ds.read();
		}

		float tempRead = ((data[1] << 8) | data[0]);    // Using two's compliment
		float tempF    = ((tempRead / 16) * 1.8f) + 32; // Convert C to F

		char addr_str[17] = "";
		// Four byte hex address
		snprintf(addr_str, sizeof(addr_str), "%02x%02x%02x%02x", addr[0],addr[1],addr[2],addr[3]);

		// Full eight byte hex address
		//snprintf(addr_str,sizeof(addr_str),"%02x%02x%02x%02x%02x%02x%02x%02x",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5],addr[6],addr[7]);

		strcpy(sensor_id[num_found], addr_str);
		sensor_value[num_found] = tempF;

		num_found++;
	}

	ds.reset_search();

	return num_found;
}

String ip_2_string(const IPAddress& ipAddress) {
	String ret = String(ipAddress[0]) + String(".") + String(ipAddress[1]) + String(".") + String(ipAddress[2]) + String(".") + String(ipAddress[3]);

	return ret;
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
