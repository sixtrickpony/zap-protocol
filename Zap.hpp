#pragma once

#include <Arduino.h>
#include "zap_arg_parser.hpp"
#include "zap_helpers.hpp"

#define ZAP_PARSE_ARGS(str, len) \
  ZAP_PARSE_ARGS_EX(args, arg, str, len)

#define ZAP_PARSE_ARGS_EX(parserVar, argVar, str, len) \
  zap::ArgParser parserVar(str, len); \
  zap::Arg argVar

namespace zap {

const uint8_t FRAME_TYPE_BINARY = 1;
const uint8_t FRAME_TYPE_TEXT   = 2;

class Protocol;

class Stream {
public:
  virtual void handleMessage(Protocol *p, uint8_t frameType, char *data, int len) = 0;
  virtual void describe(Protocol *p) = 0;
  virtual bool canReport() { return false; }
  virtual void report(Protocol *p) {}
};

class Protocol {
public:
  Protocol(HardwareSerial *port, char *rxBuffer, int rxBufferSize, char *txBuffer, int txBufferSize, const IndifferentString deviceInfo)
    : port_(port)
    , rxBuffer_(rxBuffer), rxBufferSize_(rxBufferSize)
    , txBuffer_(txBuffer), txBufferSize_(txBufferSize)
    , deviceInfo_(deviceInfo)
    , rxState_(0), rxWp_(0)
    , reportInterval_(0), nextReportAt_(0), reportStreams_(0) {
    for (int i = 0; i < 15; i++) {
      streams_[i] = nullptr;
    }
  }

  Protocol(HardwareSerial *port, char *rxBuffer, int rxBufferSize, char *txBuffer, int txBufferSize, const char *deviceInfo)
    : Protocol(port, rxBuffer, rxBufferSize, txBuffer, txBufferSize, IndifferentString(deviceInfo)) {}

  Protocol(HardwareSerial *port, char *rxBuffer, int rxBufferSize, char *txBuffer, int txBufferSize, const __FlashStringHelper *deviceInfo)
    : Protocol(port, rxBuffer, rxBufferSize, txBuffer, txBufferSize, IndifferentString(deviceInfo)) {}

  void init() {
    port_->begin(115200);
  }

  bool ready() const {
    return static_cast<bool>(port_);
  }

  inline HardwareSerial* port() const { return port_; }

  void setStreamHandler(int id, Stream *handler) {
    if (id < 1 || id > 15) return;
    streams_[id-1] = handler;
  }

  void tick() {
    while (port_->available()) {
      uint8_t ch = port_->read();
      switch (rxState_) {
        case 0:
          if (ch == '\r') {
            dispatch();
            rxWp_ = 0;
            rxState_ = 1;
          } else if (ch == '\n') {
            dispatch();
            rxWp_ = 0;
          } else {
            rxBuffer_[rxWp_++] = ch;
          }
          break;
        case 1:
          if (ch != '\n') {
            rxBuffer_[rxWp_++] = ch;
          }
          rxState_ = 0;
          break;
      }
    }

    if (reportInterval_ > 0) {
      uint32_t now = millis();
      if (now >= nextReportAt_) {
        nextReportAt_ += reportInterval_;
        for (int i = 0; i < 15; i++) {
          if (reportStreams_ & (1 << i)) {
            startNotification(i+1);
            writeRaw(F("report "));
            streams_[i]->report(this);
            endFrame();
          }
        }
      }
    }
  }

  void writeRaw(const IndifferentString str) {
    if (str.isRAM()) {
      writeRaw((const char*)str);
    } else {
      writeRaw((const __FlashStringHelper *)str);
    }
  }

  void writeRaw(const char *message) {
    Serial.print(message);
  }

  void writeRaw(const __FlashStringHelper *message) {
    const char *str = (const char*)message;
    for (int i = 0;; i++) {
      const char b = pgm_read_byte_near(str + i);
      if (b == 0) break;
      Serial.write(b);
    }
  }

  void writeOK() {

  }
  
  void writeError(int errorCode, const char *message = nullptr) {

  }

  void startMessage(uint8_t streamID) {
    port_->write('0' + streamID);
    port_->write('>');
  }

  void startBinaryMessage(uint8_t streamID) {
    port_->write('0' + streamID);
    port_->write('>');
    port_->write('#');
  }

  void startNotification(uint8_t streamID) {
    port_->write('0' + streamID);
    port_->write('!');
  }

  void startBinaryNotification(uint8_t streamID) {
    port_->write('0' + streamID);
    port_->write('!');
    port_->write('#');
  }

  void endFrame() {
    port_->write('\r');
    port_->write('\n');
  }

private:
  void dispatch() {
    if (rxWp_ < 2) {
      goto done;
    }

    uint8_t streamID = decodeHexit(rxBuffer_[0]);
    if (streamID == INVALID_HEXIT) {
      goto done;
    }

    // according to the protocol, rxBuffer_[1] should be '<', but we'll just accept anything.

    if (rxWp_ >= 3 && rxBuffer_[2] == '#') {
      if (streamID == 0) {
        // control stream doesn't support binary frames
        goto done;
      }
      int len = decodeBinary();
      if (len < 0) {
        goto done;
      }
      onStreamFrame(streamID, FRAME_TYPE_BINARY, rxBuffer_, len);
    } else {
      rxBuffer_[rxWp_] = 0;
      if (streamID == 0) {
        onControlStreamFrame(rxBuffer_ + 2, rxWp_ - 2);
      } else {
        onStreamFrame(streamID, FRAME_TYPE_TEXT, rxBuffer_ + 2, rxWp_ - 2);
      }
    }

  done:
    return;
  }

  void onControlStreamFrame(char *data, int len) {
    int err = 0;
    ArgParser p(data, len);
    Arg arg;

    startMessage(0);

    if (!p.scanWord(&arg)) {
      err = 1;
    } else if (strcmp("report", arg.S) == 0) {
      updateReporting(&p);
    } else if (strcmp("hello", arg.S) == 0) {
      port_->print("hello ");
      writeRaw(deviceInfo_);
    } else if (strcmp("streams", data) == 0) {
      port_->print("streams ");
      bool first = true;
      for (int id = 1; id <= 15; id++) {
        if (streams_[id-1] == nullptr) continue;
        if (!first) port_->write(' ');
        first = false;
        if (id <= 9) {
          port_->write('0' + id);  
        } else {
          port_->write('A' + id - 10);  
        }
      }
    } else if (strcmp("desc", data) == 0) {
      if (!p.scanInt(&arg)) {
        err = 2;
      } else {
        Stream *stream = lookupStreamByID(arg.I);
        if (stream == nullptr) {
          err = 4;
        } else {
          port_->print("desc ");
          port_->print(arg.I, HEX);
          port_->print(' ');
          stream->describe(this);
        }
      }
    } else {
      err = 1;
    }

    if (err != 0) {
      port_->print("error:");
      port_->print(err, DEC);
    }

    endFrame();
  }

  void updateReporting(ArgParser *p) {
    Arg arg;

    if (!p->scanBool(&arg)) {
      return;
    }

    if (!arg.B) {
      reportInterval_ = 0;
      port_->print("report off");
      return;
    }

    if (!p->scanInt(&arg)) {
      return;
    }

    uint16_t interval = arg.I;

    if (p->end()) {
      reportStreams_ = 0;
      for (int i = 0; i < 15; i++) {
        if (streams_[i] && streams_[i]->canReport()) {
          reportStreams_ |= (1 << i);
        }
      }
    } else {
      while (!p->end()) {
        if (!p->scanInt(&arg)) {
          return;
        }
        uint8_t streamIndex = arg.I - 1;
        if (streamIndex >= 15 || !streams_[streamIndex]) {
          return;
        }
        if (streams_[streamIndex]->canReport()) {
          reportStreams_ |= (1 << streamIndex);
        }
      }
    }

    reportInterval_ = interval;
    nextReportAt_ = millis() + interval;

    port_->print("report on");
  }

  void onStreamFrame(uint8_t streamID, uint8_t frameType, char *data, int len) {
    Stream *stream = lookupStreamByID(streamID);
    if (stream == nullptr) {
      // TODO: ???
      return;
    }
    startMessage(streamID);
    stream->handleMessage(this, frameType, data, len);
    endFrame();
  }

  Stream* lookupStreamByID(uint8_t streamID) {
    if (streamID < 1 || streamID > 15) return nullptr;
    return streams_[streamID-1];
  }

  // Attempt to decode binary data in the RX buffer.
  // Data is decoded in-place, writing begins at offset 0.
  // Returns the length of the decoded data, or < 0 on error.
  int decodeBinary() {
    // we need an even number of hex digits, but since there is a 3-byte header,
    // this equates to an odd number of bytes in the buffer.
    if (rxWp_ % 2 != 1) {
      return -1;
    }
    
    int wp = 0;
    for (int rp = 3; rp < rxWp_; rp += 2) {
      uint8_t high = decodeHexit(rxBuffer_[rp]);
      uint8_t low = decodeHexit(rxBuffer_[rp+1]);
      if ((high | low) & 0x80) {
        return -1;
      }
      rxBuffer_[wp++] = (high << 4) | low;
    }
    return wp;
  }

  HardwareSerial *port_;
  char *rxBuffer_, *txBuffer_;
  int rxBufferSize_, txBufferSize_;
  uint8_t rxState_;
  int rxWp_;
  uint16_t reportInterval_;
  uint32_t nextReportAt_;
  uint16_t reportStreams_;
  zap::Stream *streams_[15];

  IndifferentString deviceInfo_;
};

};
