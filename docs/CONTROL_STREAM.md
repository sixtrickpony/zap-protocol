# Control Stream

## Device Identification

If the target device supports device identification (e.g. via LED), it can be activated using the `ident` command:

```
0<ident on
0>ok
0<ident off
0>ok
```

## Device Selection

When a project uses multiple devices of the same type — for example, two controllers in a 2-player game — the `select`
command can be used to put devices into a mode where they notify the host when interacted with (e.g. a when button is
pressed). This feature can be used to implement a configuration screen before the main program starts.

```
0<select on
0>ok
0<select off
0>ok
```

While selection mode is active, the device will emit a `select` notifications on the control stream each time the
device is selected (e.g. when the button is pressed):

```
0!select
```