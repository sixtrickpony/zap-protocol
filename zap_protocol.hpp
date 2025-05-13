#pragma once

namespace zap {

class BaseProtocol {
 public:
  BaseProtocol(::Stream *port) : port_(port) {}

  inline ::Stream *port() const { return port_; }

  //
  // Frame wrappers

  // Start a reply message on the specified stream ID
  void startMessage(uint8_t streamID) {
    port_->write(toHex(streamID));
    port_->write('>');
  }

  // Start a notification on the specified stream ID
  void startNotification(uint8_t streamID) {
    port_->write(toHex(streamID));
    port_->write('!');
  }

  // End the current frame with a newline
  void endFrame() {
    port_->write('\r');
    port_->write('\n');
  }

  //
  // Write Helpers
  //
  // These methods write to the serial port without any regard for
  // current output state, framing etc. It is the caller's responsibility
  // to ensure the integrity of the underlying protocol stream.

  // Write a single space character
  void writeSpace() { port_->write(' '); }

  // Write an integer, encoded as decimal
  void write(int x) { port_->print(x, DEC); }

  // Write a floating point value, encoded to the specified number of decimal
  // places
  void write(float x, int decimalPlaces = 4) { port_->print(x, decimalPlaces); }

  // Write a boolean value, encoded as "true" or "false"
  void write(bool x) { writeRaw(x ? STR_TRUE : STR_FALSE); }

  void writeQuotedString(const char *msg) {
    // TODO: support escaping
    port_->write('"');
    writeRaw(msg);
    port_->write('"');
  }

  void writeQuotedString(const __FlashStringHelper *msg) {
    // TODO: support escaping
    port_->write('"');
    writeRaw(msg);
    port_->write('"');
  }

  void writeQuotedString(const IndifferentString msg) {
    // TODO: support escaping
    port_->write('"');
    writeRaw(msg);
    port_->write('"');
  }

  void writeError(int id) {
    writeKey(STR_ERROR);
    writeRaw(id);
  }

  void writeError(const char *id) {
    writeKey(STR_ERROR);
    writeRaw(id);
  }

  void writeError(const __FlashStringHelper *id) {
    writeKey(STR_ERROR);
    writeRaw(id);
  }

  void writeError(const IndifferentString id) {
    writeKey(STR_ERROR);
    writeRaw(id);
  }

  void writeErrorCode(int code) {
    writeSpace();
    writeKey(STR_CODE);
    write(code);
  }

  void writeErrorMessage(const char *msg) {
    writeSpace();
    writeKey(STR_MESSAGE);
    writeQuotedString(msg);
  }

  void writeErrorMessage(const __FlashStringHelper *msg) {
    writeSpace();
    writeKey(STR_MESSAGE);
    writeQuotedString(msg);
  }

  void writeErrorMessage(const IndifferentString msg) {
    writeSpace();
    writeKey(STR_MESSAGE);
    writeQuotedString(msg);
  }

  void writeKey(int stringTableEntryIndex) {
    writeRaw(stringTableEntryIndex);
    port_->write(':');
  }

  void writeOK() { writeRaw(STR_OK); }

  void writeOK(int wait) {
    writeRawSpace(STR_OK);
    writeKey(STR_WAIT);
    port_->print(wait, DEC);
  }

  void writeBinaryBody(char *data, int len) {
    writeBinaryMarker();
    writeBinary(data, len);
  }

  void writeBinaryMarker() { port_->print('#'); }

  void writeBinary(char *data, int len) {
    while (len--) {
      port_->write(toHex(*data >> 4));
      port_->write(toHex((*data++) & 0xF));
    }
  }

  // The writeRaw*() family of methods are protocol-oblivious and
  // simply write raw data to the serial port.

  void writeRaw(const IndifferentString str) {
    if (str.isRAM()) {
      writeRaw((const char *)str);
    } else {
      writeRaw((const __FlashStringHelper *)str);
    }
  }

  // Write a string from the string table
  void writeRaw(int strTableIx) { writeRawP(strptr(strTableIx)); }
  void writeRaw(const char *message) { port_->print(message); }
  void writeRaw(const __FlashStringHelper *str) { writeRawP((const char *)str); }

  // Write a string from the string table, followed by a space
  void writeRawSpace(int strTableIx) {
    writeRawP(strptr(strTableIx));
    port_->write(' ');
  }

  void writeRawP(const char *str) {
    for (int i = 0;; i++) {
      const char b = pgm_read_byte_near(str + i);
      if (b == 0) break;
      port_->write(b);
    }
  }

  //
  // One-shots

  void sendError(uint8_t streamID, int id) {
    startMessage(streamID);
    writeError(id);
    endFrame();
  }

  void sendError(uint8_t streamID, const char *id) {
    startMessage(streamID);
    writeError(id);
    endFrame();
  }

  void sendError(uint8_t streamID, const __FlashStringHelper *id) {
    startMessage(streamID);
    writeError(id);
    endFrame();
  }

  void sendError(uint8_t streamID, const IndifferentString id) {
    startMessage(streamID);
    writeError(id);
    endFrame();
  }

 protected:
  ::Stream *port_;
};

template <uint8_t N = 14>
class Protocol : public BaseProtocol {
 public:
  Protocol(::Stream *port, char *rxBuffer, int rxBufferSize,
           const IndifferentString deviceInfo)
      : BaseProtocol(port),
        rxBuffer_(rxBuffer),
        rxBufferSize_(rxBufferSize),
        deviceInfo_(deviceInfo) {}

  Protocol(::Stream *port, char *rxBuffer, int rxBufferSize, const char *deviceInfo)
      : Protocol(port, rxBuffer, rxBufferSize, IndifferentString(deviceInfo)) {}

  Protocol(::Stream *port, char *rxBuffer, int rxBufferSize,
           const __FlashStringHelper *deviceInfo)
      : Protocol(port, rxBuffer, rxBufferSize, IndifferentString(deviceInfo)) {}

  void begin() {}

  //
  // Streams

  void setStreamHandler(int id, Stream *handler) {
    if (id < 1 || id > N) return;
    handler->setProtocol(this, id);
    streams_[id - 1] = handler;
  }

  //
  // Ident Pin

  void setIdentPin(uint8_t pin, uint8_t polarity = HIGH) {
    identPin_ = pin;
    identPolarity_ = polarity;
    pinMode(identPin_, OUTPUT);
    digitalWrite(identPin_, !polarity);
  }

  void identOn() {
    if (identPin_ == 0xFF) return;
    digitalWrite(identPin_, identPolarity_);
  }

  void identOff() {
    if (identPin_ == 0xFF) return;
    digitalWrite(identPin_, !identPolarity_);
  }

  //
  // Main Protocol Handler

  void tick() {
    // Serial read/dispatch

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

    // Periodic reports

    if (reportInterval_ > 0) {
      uint32_t now = millis();
      if (now >= nextReportAt_) {
        nextReportAt_ += reportInterval_;
        for (int i = 0; i < N; i++) {
          if (reportStreams_ & (1 << i) && streams_[i]->shouldReport()) {
            startNotification(i + 1);
            writeRaw(F("report "));
            streams_[i]->report();
            endFrame();
          }
        }
      }
    }
  }

 private:
  void dispatch() {
    if (rxWp_ < 2) {
      // Invalid frame - ignore it. There's no point sending an error
      // message if the client is giving us gibberish.
      return;
    }

    uint8_t streamID = decodeHexit(rxBuffer_[0]);
    if (streamID == INVALID_HEXIT) {
      // Protocol violation - nothing to do
      return;
    }

    // According to the protocol, rxBuffer_[1] should be '<',
    // but we'll just accept anything.

    // Check for a binary frame
    if (rxWp_ >= 3 && rxBuffer_[2] == '#') {
      if (streamID == 0) {
        // The control stream doesn't support binary frames
        // so we'll just ignore it.
        // TODO: send proper error message here? is there any point?
        return;
      }
      int len = decodeBinary();
      if (len < 0) {
        return;
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
  }

  void onControlStreamFrame(char *data, int len) {
    ArgParser p(data, len);
    Arg arg;
    int err = 0;

    startMessage(0);

    if (!p.scanWord(&arg)) {
      err = 1;
    } else if (streq(STR_REPORT, arg.S) == 0) {
      updateReporting(&p);
    } else if (streq(STR_HELLO, arg.S) == 0) {
      writeRaw(STR_HELLO);
      port_->print(' ');
      writeRaw(deviceInfo_);
    } else if (streq(STR_STREAMS, arg.S) == 0) {
      writeRaw(STR_STREAMS);
      port_->print(' ');
      bool first = true;
      for (int id = 1; id <= N; id++) {
        if (streams_[id - 1] == nullptr) continue;
        if (!first) writeSpace();
        first = false;
        if (id <= 9) {
          port_->write('0' + id);
        } else {
          port_->write('A' + id - 10);
        }
      }
    } else if (streq(STR_DESC, arg.S) == 0) {
      if (!p.scanInt(&arg)) {
        err = 2;
      } else {
        Stream *stream = lookupStreamByID(arg.I);
        if (stream == nullptr) {
          err = 4;
        } else {
          writeRawSpace(STR_DESC);
          port_->print(arg.I, HEX);
          writeSpace();
          stream->describe();
        }
      }
    } else if (streq(STR_IDENT, arg.S) == 0) {
      if (!p.scanBool(&arg)) {
        err = 2;
      } else if (arg.B) {
        identOn();
      } else {
        identOff();
      }
      writeOK();
    } else {
      err = 1;
    }

    if (err != 0) {
      port_->print("error:");
      port_->print(err, DEC);
    }

    endFrame();
  }

  void onStreamFrame(uint8_t streamID, uint8_t frameType, char *data, int len) {
    Stream *stream = lookupStreamByID(streamID);
    startMessage(streamID);
    if (stream == nullptr) {
      writeError(STR_ERR_INVALID_STREAM_ID);
    } else {
      int res = stream->handleMessage(frameType, data, len);
      if (res == 0) {
        writeOK();
      } else if (res > 0) {
        writeError(res);
      }
    }
    endFrame();
  }

  void updateReporting(ArgParser *p) {
    Arg arg;

    if (!p->scanBool(&arg)) {
      writeError(STR_ERR_INVALID_ARGUMENT);
      return;
    }

    if (!arg.B) {
      reportInterval_ = 0;
      writeOK();
      return;
    }

    if (!p->scanInt(&arg)) {
      writeError(STR_ERR_INVALID_ARGUMENT);
      return;
    }

    uint16_t interval = arg.I;

    uint16_t requestedStreams = 0;
    if (p->end()) {
      requestedStreams = 0x7FFF;
    } else {
      while (!p->end()) {
        if (!p->scanInt(&arg)) {
          writeError(STR_ERR_INVALID_ARGUMENT);
          return;
        }
        uint8_t streamIndex = arg.I - 1;
        if (streamIndex >= N || !streams_[streamIndex]) {
          writeError(STR_ERR_INVALID_STREAM_ID);
          return;
        }
        requestedStreams |= (1 << streamIndex);
      }
    }

    reportStreams_ = 0;
    for (int i = 0; i < N; i++) {
      if ((requestedStreams & (1 << i)) && streams_[i] && streams_[i]->canReport()) {
        reportStreams_ |= (1 << i);
      }
    }

    reportInterval_ = interval;
    nextReportAt_ = millis() + interval;

    writeOK();
  }

  Stream *lookupStreamByID(uint8_t streamID) {
    if (streamID < 1 || streamID > N) return nullptr;
    return streams_[streamID - 1];
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
      uint8_t low = decodeHexit(rxBuffer_[rp + 1]);
      if ((high | low) & 0x80) {
        return -1;
      }
      rxBuffer_[wp++] = (high << 4) | low;
    }
    return wp;
  }

  // Receive buffer and state
  char *rxBuffer_;       // Buffer
  int rxBufferSize_;     // Size of buffer in bytes
  uint8_t rxState_ = 0;  // Receive state
  int rxWp_ = 0;         // Write pointer

  // Report configuration
  uint16_t reportInterval_ = 0;  // interval (ms)
  uint32_t nextReportAt_ = 0;    // scheduled time of next report (referenced to millis())
  uint16_t reportStreams_ = 0;   // bitmask of streams that are reporting

  // Stream implementations
  // Index 0 is logical stream 1 since the control stream is implemented
  // by the Protocol class itself.
  Stream *streams_[N] = {0};

  // Ident
  uint8_t identPin_ = 0xFF;    // Pin for ident LED; set to 0xFF (disabled) by default
  bool identPolarity_ = true;  // Active polarity of ident pin

  // Device info
  IndifferentString deviceInfo_;
};
};  // namespace zap