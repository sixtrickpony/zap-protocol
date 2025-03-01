#include "ssp.hpp"
#include <Wire.h>

class LightSensor : public ssp::Stream {
public:
  LightSensor() : valid_(false) {}

  setValue(uint16_t v) { valid_ = true; v_ = v; }

  bool canReport() { return true; }

  void report(ssp::Protocol *p) {
    p->port()->print(v_, DEC);
  }

protected:
  void handleMessage(ssp::Protocol *p, uint8_t frameType, char *data, int len) {
    int err = 0;
    ssp::ArgParser args(data, len);
    ssp::Arg arg;

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

  void describe(ssp::Protocol *p) {
    p->writeRaw(F("name:lightLevel class:sensor value:[v] min:0 max:1024"));
  }

private:
  bool valid_;
  uint16_t v_;
};

#define RX_BUFFER_SIZE 128
#define TX_BUFFER_SIZE 128

char rx_buffer[RX_BUFFER_SIZE];
char tx_buffer[TX_BUFFER_SIZE];

ssp::Protocol protocol(&Serial, rx_buffer, RX_BUFFER_SIZE, tx_buffer, TX_BUFFER_SIZE);
LightSensor sensor;

void hardFault() {
  while (1) {}
}

void setup() {
  protocol.setDeviceInfo(F("vendor:\"Curious Chip\" product:\"Light Sensor\" id:\"com.curiouschip.lightSensor\""));
  protocol.setStreamHandler(1, &sensor);
  protocol.init();
  
  while (!protocol.ready());
}

void loop() {
  sensor.setValue(analogRead(1));
  protocol.tick();
}
