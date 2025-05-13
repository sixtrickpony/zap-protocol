#include "Zap.hpp"

class AnalogSensor : public zap::ScalarSensorStream<uint16_t> {
 public:
  AnalogSensor(uint8_t pin) : pin_(pin) {}
  void tick() { setValue(analogRead(pin_)); }
  void describe() { proto->writeRaw(F("name:analogSensor class:sensor value:[x] min:0 max:1023")); }
  void report() { proto->port()->print(value(), DEC); }
private:
  uint8_t pin_;
};

// Instantiate protocol components
const char deviceInfo[] PROGMEM =
    "vendor:\"Test\" product:\"Digital Sensor\" id:\"com.example.digitalSensor\"";

// Template argument is number of slots to reserve for user streams
zap::Protocol<4,48> protocol(&Serial, (__FlashStringHelper *)deviceInfo);

AnalogSensor sensor1(1);
AnalogSensor sensor2(2);


#define SELECT_PIN 8

char *modeNames[2] = {
  "mode-1",
  "mode-2"
};

class ModeSelector : public zap::ModeSelector {
public:
  ModeSelector() : zap::ModeSelector(modeNames, 2) {}

protected:
  int setMode(uint8_t currentMode, uint8_t newMode) {
    return 0;
  }
};

ModeSelector modeSelector;
zap::DeviceSelector deviceSelector(SELECT_PIN, INPUT_PULLUP);

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  pinMode(1, INPUT);

  // Register the digital IO as stream ID and initialise the protocol
  protocol.setStreamHandler(1, &modeSelector);
  protocol.setStreamHandler(2, &deviceSelector);
  protocol.setStreamHandler(3, &sensor1);
  protocol.setStreamHandler(4, &sensor2);
  protocol.setIdentPin(LED_BUILTIN);
  protocol.begin();
}

void loop() {
  // Every tick round the loop we update the sensor state and tick the protocol
  // to handle comms and periodic reporting.
  deviceSelector.tick();
  sensor1.tick();
  sensor2.tick();
  protocol.tick();
}
