# Modes

If a device has mutually exclusive modes, subclass `zap::ModeSelector` to implement
a stream that can coordinate between switching between them:

```c++
// These are the modes that the device can be in
char *modeNames[2] = {
  "mode-1",
  "mode-2"
};

// Sublcass mode selector to implement custom mode selection logic
class ModeSelector : public zap::ModeSelector {
public:
  ModeSelector() : zap::ModeSelector(modeNames, 2) {}

protected:
  // Mode change logic goes here. Will only be called if currentMode != newMode.
  //
  // On success, return 0.
  //
  // If the mode change succeeds but will take some time to take effect, return
  // the number of milliseconds the client should wait before attempting further
  // operations. In this case, the response to the client will be:
  //
  //   1>ok wait:1000
  //
  // If an error occurs, return -1. setMode() is responsible for sending an
  // appropriate error message.
  int setMode(uint8_t currentMode, uint8_t newMode) {
    return 0;
  }
};

ModeSelector modeSelector;

void setup() {
  // ...
  // ...
  
  // Register the mode selector
  // Any stream ID is allowed, it is not required to be stream ID 1
  protocol.setStreamHandler(1, &modeSelector);
  
  // ...
  // ...
}
```

At startup `ModeSelector` assumes that the device is in the first available mode.