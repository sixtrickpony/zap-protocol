#pragma once

namespace zap {

class Stream {
 public:
  virtual void describe() = 0;

  // Handle an incoming message.
  //
  // Before invocation, the caller will start a reply frame by writing
  // "{$streamID}>" to the serial port. After invocation, the frame
  // is terminated with a newline.
  //
  // handleMessage() uses the return value to control what additional
  // reply is written by the caller.
  //
  // Return values:
  //
  // 0   - operation succeeded, caller will write "ok" response
  // >0  - error; return value is assumed to be index into string table
  //       representing the error code
  // -1  - handleMessage() has written a response; call should take no
  //       further action (save for ending the frame with a newline).
  //
  virtual int handleMessage(uint8_t frameType, char *data, int len) = 0;

  // Returns true if this stream is capable of emitting periodic reports
  virtual bool canReport() { return false; }

  // Returns true if this stream should send a report right now.
  // This method can be overridden, for example, to mute reports
  // if the sensor is disabled, or to only send reports if the
  // underlying value has changed.
  virtual bool shouldReport() { return true; }

  virtual void report() {}

  void setProtocol(BaseProtocol *p, uint8_t id) {
    proto = p;
    streamID = id;
  }

 protected:
  BaseProtocol *proto;
  uint8_t streamID;
};

class ModeSelector : public Stream {
 public:
  ModeSelector(char **modeNames, uint8_t modeCount)
      : names_(modeNames), count_(modeCount) {}

  uint8_t activeMode() { return active_; }

  void describe() {
    proto->writeRaw(F("class:modeSelect modes:["));
    for (int i = 0; i < count_; i++) {
      if (i > 0) proto->writeSpace();
      proto->writeRaw(names_[i]);
    }
    proto->port()->write(']');
  }

  int handleMessage(uint8_t frameType, char *data, int len) {
    ZAP_PARSE_ARGS(data, len);

    if (!args.scanWord(&arg)) {
      return STR_ERR_INVALID_ARG;
    } else if (!streq(STR_MODE, arg.S)) {
      return STR_ERR_UNKNOWN_COMMAND;
    }

    // Query current mode
    if (args.end()) {
      proto->writeRawSpace(STR_MODE);
      proto->writeRaw(names_[active_]);
      return -1;
    }

    if (!args.scanWord(&arg)) {
      return STR_ERR_INVALID_ARG;
    }

    uint8_t requestedMode = findModeByName(arg.S);
    if (requestedMode == 0xFF) {
      return STR_ERR_UNKNOWN_ENTITY;
    }

    if (requestedMode == active_) {
      // FIXME: this isn't strictly correct; if the previous mode change
      // resulted in a wait state, and this mode change was received before
      // the wait time has elapsed, we should really return the remaining
      // amount of wait time.
      return 0;
    }

    int res = setMode(active_, requestedMode);
    if (res < 0) {
      return -1;
    }

    active_ = requestedMode;
    proto->writeOK(res);
    return -1;
  }

 protected:
  // Override this methods to implement mode switching.
  // setMode() will only be invoked when currentMode != newMode
  //
  // Return values:
  // 0 => mode change OK
  // >0 => mode change OK, return value is milliseconds client should wait
  //       before invoking any further operations.
  // <0 => error; setMode() is responsible for writing error
  virtual int setMode(uint8_t currentMode, uint8_t newMode) = 0;

 private:
  uint8_t findModeByName(const char *name) {
    for (int i = 0; i < count_; i++) {
      if (strcmp(name, names_[i]) == 0) {
        return i;
      }
    }
    return 0xFF;
  }

  char **names_;
  uint8_t count_;
  uint8_t active_ = 0;
};

class Ident : public Stream {
 public:
  Ident(uint8_t pin, bool polarity = HIGH) : pin_(pin), polarity_(polarity) {
    pinMode(pin_, OUTPUT);
  }

  void describe() { proto->writeRaw(F("class:ident")); }

  int handleMessage(uint8_t frameType, char *data, int len) {
    ZAP_PARSE_ARGS(data, len);

    if (args.scanBool(&arg)) {
      setEnabled(arg.B);
      return 0;
    } else {
      return STR_ERR_INVALID_ARG;
    }
  }

  bool setEnabled(bool isEnabled) { digitalWrite(pin_, isEnabled == polarity_); }

 private:
  uint8_t pin_;    // pin used for ident
  bool polarity_;  // active polarity of ident pin
};
//
// DeviceSelector is a simple implementation of the deviceSelect class,
// allowing the device to expose a method for the user to physically
// select/disambiguate the device by toggling the value of a pin (e.g.
// button press). Use this in scenarios where a setup requires more than
// one device of a single type and it is necessary to determine the logical
// identity of each device (e.g. for assigning controllers in 2-player game)
//
// The deviceSelect class is very simple, responding to only boolean commands
// to enable/disable selection, and reporting a "select" notification when
// the device is selected.

class DeviceSelector : public Stream {
 public:
  DeviceSelector(uint8_t pin, int mode = INPUT, bool polarity = LOW)
      : pin_(pin), polarity_(polarity) {
    pinMode(pin_, mode);
  }

  void tick() {
    if (!enabled_) return;

    bool currentState = digitalRead(pin_) == polarity_;
    if (currentState != active_) {
      if (currentState) {
        proto->startNotification(streamID);
        proto->writeRaw(F("select"));
        proto->endFrame();
      }
      active_ = currentState;
    }
  }

  void describe() { proto->writeRaw(F("class:deviceSelect")); }

  int handleMessage(uint8_t frameType, char *data, int len) {
    ZAP_PARSE_ARGS(data, len);

    if (args.scanBool(&arg)) {
      setEnabled(arg.B);
      return 0;
    } else {
      return STR_ERR_INVALID_ARG;
    }
  }

 private:
  void setEnabled(bool isEnabled) {
    enabled_ = isEnabled;
    active_ = false;
  }

  bool enabled_ = false;  // is selection enabled?
  bool active_ = false;   // was select pin active on last tick?
  uint8_t pin_;           // pin used for detecting selection
  bool polarity_;         // active polarity of select pin
};

template <typename T>
class ScalarSensorStream : public Stream {
 public:
  ScalarSensorStream() : enabled_(false), valid_(false), value_(T{}) {}

  inline bool enabled() { return enabled_; }
  inline bool valid() { return valid_; }
  inline T value() { return value_; }

  void setValue(T v) {
    if (enabled_) {
      value_ = v;
      valid_ = true;
    }
  }

  void invalidate() { valid_ = false; }

  void enable() {
    if (!enabled_) {
      setEnabled(true);
      enabled_ = true;
    }
  }

  void disable() {
    if (enabled_) {
      setEnabled(false);
      enabled_ = false;
      valid_ = false;
    }
  }

  bool canReport() { return true; }
  bool shouldReport() { return valid_; }

  int handleMessage(uint8_t frameType, char *data, int len) {
    ZAP_PARSE_ARGS(data, len);

    if (!args.scanWord(&arg)) {
      return STR_ERR_INVALID_ARG;
    }

    if (streq(STR_READ, arg.S) == 0) {
      if (!valid_) {
        return STR_ERR_NO_VALUE;
      } else {
        proto->writeRawSpace(STR_READ);
        report();
      }
      return 0;
    }

    if (streq(STR_ENABLE, arg.S) == 0) {
      if (args.end()) {
        proto->writeRawSpace(STR_ENABLE);
        proto->write(enabled_);
        return -1;
      } else if (args.scanBool(&arg)) {
        if (arg.B) {
          enable();
        } else {
          disable();
        }
        return 0;
      } else {
        return STR_ERR_INVALID_ARG;
      }
    }

    if (streq(STR_SET, arg.S) == 0) {
      beginConfig();
      bool aborted = false;
      while (!args.end()) {
        if (!args.next(&arg)) {
          aborted = true;
          break;
        } else if (arg.key != nullptr) {
          setConfig(arg);
        }
      }
      return commitConfig(aborted) ? 0 : -1;
    }

    return STR_ERR_UNKNOWN_COMMAND;
  }

  // Override this method to implement custom enable/disable logic, e.g.
  // set pin modes, kick off timers, activate external peripherals etc.
  //
  // The underlying engine ensures that this method is only called when
  // the enabled state has actually changed.
  virtual void setEnabled(bool enabled) {}

  // Begin a configuration transaction; called when the message
  // handler encounters as "set" command.
  virtual void beginConfig() {}

  // Within a configuration transaction this method is called once
  // per key/value argument. The implementation can choose to apply
  // valid changes immediately, or buffer them until the corrresponding
  // call to commitConfig().
  virtual void setConfig(Arg arg) {}

  // Called at the end of a configuration transaction.
  // If the implementation buffers config changes before final validation/
  // application, this is the time to apply them.
  //
  // The `aborted` parameter will be set to true if argument parsing failed
  // during the transaction.
  //
  // On success, returns true, and the caller is responsible for sending
  // the "ok" response.
  //
  // On failure, returns false, and it is this method's responsibility to
  // write the appropriate error code to the stream.
  virtual bool commitConfig(bool aborted) {}

 private:
  bool enabled_;
  bool valid_;
  T value_;
};
};  // namespace zap
