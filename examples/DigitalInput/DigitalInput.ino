#include "Zap.hpp"

// Each sensor in a system is represented by a subclass of zap::Stream
class DigitalSensor : public zap::Stream {
public:
  DigitalSensor() : value_(0) {}

  // Set the stream's current value from an external source
  // More advanced implementations could implement logic directly in the
  // stream subclass.
  setValue(bool v) { value_ = v; }

  // Indicate that this stream is capable of periodic reporting
  bool canReport() { return true; }

  // Write the sensor's current value to the output
  // This method is called by the protocol driver when performing periodic
  // reporting, and additionally in response to read requests.
  void report(zap::Protocol *p) { p->port()->print(value_ ? 1 : 0, DEC); }

protected:
  // Handle an incoming message
  void handleMessage(zap::Protocol *p, uint8_t frameType, char *data, int len) {
    int err = 0;

    ZAP_PARSE_ARGS(data, len);

    if (!args.scanWord(&arg)) {
      err = 1;
      goto error;
    }

    if (strcmp("read", arg.S) == 0) {
      p->writeRaw(F("read "));
      report(p);
    } else {
      err = 2;
      goto error;
    }

    return;

  error:
    p->writeError(err);
  }

  // Stream description, sent in response to a "desc" command
  void describe(zap::Protocol *p) {
    p->writeRaw(F("name:digitalSensor class:sensor value:[v] min:0 max:1"));
  }

private:
  uint8_t value_;
};

// Protocol buffers for RX/TX

#define RX_BUFFER_SIZE 128
#define TX_BUFFER_SIZE 128

char rx_buffer[RX_BUFFER_SIZE];
char tx_buffer[TX_BUFFER_SIZE];

// Instantiate protocol components
const char deviceInfo[] PROGMEM = "vendor:\"Test\" product:\"Digital Sensor\" id:\"com.example.digitalSensor\"";
zap::Protocol protocol(
  &Serial,
  rx_buffer, RX_BUFFER_SIZE,
  tx_buffer, TX_BUFFER_SIZE,
  (__FlashStringHelper *)deviceInfo
);

DigitalSensor sensor;

void setup() {
  // Register the digital IO as stream ID and initialise the protocol
  protocol.setStreamHandler(1, &sensor);
  protocol.init();
  while (!protocol.ready());

  pinMode(1, INPUT);
}

void loop() {
  // Every tick round the loop we update the sensor state and tick the protocol
  // to handle comms and periodic reporting.
  sensor.setValue(digitalRead(1));
  protocol.tick();
}
