#include "config.h"

#ifdef ENC28J60
#include <UIPEthernet.h>
#else
#include <Ethernet.h>
#endif

// https://github.com/depuits/AButt
#include <AButt.h>
#include "PubSubClient.h"

#include <avr/wdt.h>

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

#ifdef CONFIG_IS_OPEN_ON_TRIGGER
AButt stateInput(CONFIG_PIN_STATE, false);
#else
AButt stateInput(CONFIG_PIN_STATE, true);
#endif

DHT dht(CONFIG_PIN_DHT, CONFIG_DHT_TYPE);

bool isOpen = false;

void doorOpen() {
	// the door is now open
#ifdef CONFIG_DEBUG
	Serial.print("Door open");
#endif
	mqttClient.publish(CONFIG_MQTT_TOPIC_STATE, CONFIG_MQTT_PAYLOAD_OPEN, true);
}
void doorClose() {
	// the door has closed
#ifdef CONFIG_DEBUG
	Serial.print("Door closed");
#endif
	mqttClient.publish(CONFIG_MQTT_TOPIC_STATE, CONFIG_MQTT_PAYLOAD_CLOSE, true);
}

void processMessage(char* message) {
	if (strcmp(message, CONFIG_MQTT_PAYLOAD_OPEN) == 0) {
		if (!isOpen) {
			digitalWrite(CONFIG_PIN_TOGGLE, LOW);
			delay(500);
			digitalWrite(CONFIG_PIN_TOGGLE, HIGH);
		}
	}
	else if (strcmp(message, CONFIG_MQTT_PAYLOAD_CLOSE) == 0) {
		digitalWrite(CONFIG_PIN_CLOSE, LOW);
		delay(500);
		digitalWrite(CONFIG_PIN_CLOSE, HIGH);
	}
	else if (strcmp(message, CONFIG_MQTT_PAYLOAD_TOGGLE) == 0) {
		digitalWrite(CONFIG_PIN_TOGGLE, LOW);
		delay(500);
		digitalWrite(CONFIG_PIN_TOGGLE, HIGH);
	}
	else if (strcmp(message, CONFIG_MQTT_PAYLOAD_OPENHALF) == 0) {
		if (!isOpen) {
			digitalWrite(CONFIG_PIN_TOGGLE50, LOW);
			delay(500);
			digitalWrite(CONFIG_PIN_TOGGLE50, HIGH);
		}
	}
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
#ifdef CONFIG_DEBUG
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
#endif

	char message[length + 1];
	for (int i = 0; i < length; i++) {
		message[i] = (char)payload[i];
	}
	message[length] = '\0';

#ifdef CONFIG_DEBUG
	Serial.println(message);
#endif

	processMessage(message);
}

void setup()
{
	wdt_disable(); //always good to disable it, if it was left 'on' or you need init time

	// setup serial communication
#ifdef CONFIG_DEBUG
	Serial.begin(9600);
	Serial.println("Setup");
#endif

	pinMode(CONFIG_PIN_STATE, INPUT_PULLUP);

	pinMode(CONFIG_PIN_TOGGLE, OUTPUT);
	digitalWrite(CONFIG_PIN_TOGGLE, HIGH);

	pinMode(CONFIG_PIN_CLOSE, OUTPUT);
	digitalWrite(CONFIG_PIN_CLOSE, HIGH);
	
	pinMode(CONFIG_PIN_TOGGLE50, OUTPUT);
	digitalWrite(CONFIG_PIN_TOGGLE50, HIGH);

	stateInput.onHold(doorOpen, doorClose);
	stateInput.setHoldDelay(500); // set the hold delay low so it's called quickly

#ifdef CONFIG_DEBUG
	Serial.println("setup mac");
#endif
	// setup ethernet communication using DHCP
	if(Ethernet.begin(mac) == 0)
	{
#ifdef CONFIG_DEBUG
		Serial.println("Ethernet configuration using DHCP failed");
#endif
		delay(5000);
		asm volatile ("jmp 0");
	}

#ifdef CONFIG_DEBUG
	Serial.println("Connecting to client");
#endif

	dht.begin();

	// setup mqtt client
	mqttClient.setServer(CONFIG_MQTT_HOST, CONFIG_MQTT_PORT);
	mqttClient.setCallback(mqttCallback);
}

void reconnect()
{
	// Loop until we're reconnected
	while (!mqttClient.connected())
	{
    Ethernet.maintain();
#ifdef CONFIG_DEBUG
		Serial.print("Attempting MQTT connection...");
#endif
		// Attempt to connect
		if (mqttClient.connect(CONFIG_MQTT_CLIENT_ID, CONFIG_MQTT_USER, CONFIG_MQTT_PASS, CONFIG_MQTT_WILL_TOPIC, CONFIG_MQTT_WILL_QOS, CONFIG_MQTT_WILL_RETAIN, CONFIG_MQTT_WILL_MSG))
		{
#ifdef CONFIG_DEBUG
			Serial.println("connected");
#endif
			mqttClient.subscribe(CONFIG_MQTT_TOPIC_SET);
		}
		else
		{
#ifdef CONFIG_DEBUG
			Serial.print("failed, rc=");
			Serial.print(mqttClient.state());
			Serial.println(" try again in 5 seconds");
#endif
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

void loop()
{
	Ethernet.maintain();
	if (!mqttClient.connected())
	{
		reconnect();
	}

	mqttClient.loop();

	stateInput.update();
	isOpen = stateInput.getState();


	//TODO add timer delay of 5 seconds
	// Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
	float h = dht.readHumidity();
	// Read temperature as Fahrenheit (isFahrenheit = true)
	float f = dht.readTemperature(CONFIG_DHT_FAHRENHEIT);



	unsigned long time = millis();
	unsigned long target = 1000l * 60l * 60l * 12l;
	if (time > target) {
		//never keep the device running for longer then 12 hours
		asm volatile ("jmp 0");
	}
}
