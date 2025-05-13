#include "Zap.hpp"

//
// Analog Sensors

class AnalogSensor : public zap::ScalarSensorStream<uint16_t> {
 public:
  AnalogSensor(uint8_t pin) : pin_(pin) {}
  void tick() { setValue(analogRead(pin_)); }
  void describe() { proto->writeRaw(F("name:analogSensor class:sensor value:[x] min:0 max:1023")); }
  void report() { proto->port()->print(value(), DEC); }
private:
  uint8_t pin_;
};

AnalogSensor sensor1(1);
AnalogSensor sensor2(2);

//
// Ident

zap::Ident ident(LED_BUILTIN, HIGH);

//
// Mode Selection

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

//
// Device Selection

#define SELECT_PIN 8
zap::DeviceSelector deviceSelector(SELECT_PIN, INPUT_PULLUP);

// Instantiate protocol components
const char deviceInfo[] PROGMEM =
    "vendor:\"Test\" product:\"Digital Sensor\" id:\"com.example.digitalSensor\"";

// Template argument is number of slots to reserve for user streams
zap::Protocol<5,48> protocol(&Serial, (__FlashStringHelper *)deviceInfo);

//
//

void setup() {
  // Init serial port
  Serial.begin(115200);
  while (!Serial) {}

  // Register streams
  protocol.setStreamHandler(1, &ident);
  protocol.setStreamHandler(2, &modeSelector);
  protocol.setStreamHandler(3, &deviceSelector);
  protocol.setStreamHandler(4, &sensor1);
  protocol.setStreamHandler(5, &sensor2);

  // Start the protocol
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
