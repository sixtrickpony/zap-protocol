# zap protocol

## Protocol

Frames are newline-delimited ASCII.

There are three types of frame:

  - host to device message
  - device to host message
  - device to host notification

Each frame belongs to a stream, identified by a single hexadecimal digit; stream 0 is the "control stream", and exists on all devices.

Frame format:

```
<stream-id><frame-type-marker><binary-indicator?><body>\n
```

Wherein:

  - `stream-id`: `0-F`
  - `frame-type-marker`: one of `<` (host to device message), `>` (device to host message), or `!` (device to host notification)
  - `binary-indicator`: `#`

If the binary indicator `#` is present the frame payload is interpreted as hex-encoded ASCII.

## Body Format

Frame bodies are lists of arguments, each argument being either positional or named. Positional arguments must proceed named arguments.

Named arguments follow the form: `<name>:<space?><value>`

Values can be:

  - Boolean: `on`, `yes`, `true`, `off`, `no`, `false`
  - Integer
  - Float
  - String: either a bare word matching the regex `[a-zA-Z_][a-zA-Z0-9_-]*` (that isn't an otherwise reserved word), or a double-quoted string.
  - A nested argument list, enclosed in `[` and `]` braces; the contents follow the same rules for positional and named arguments.

## Control Stream

### `hello`

Replies with device info using named arguments as key/value pairs. Common keys:

  - `name`: friendly device name
  - `vendor`: vendor name
  - `product`: product name
  - `id`: product ID (reverse-domain encoded)
  - `serial`: serial number (string)
  - `version`: firmware version; can be integer or semver string
  - `revision`: hardware revision; can be integer or semver string

### `streams`

Get a list of device's streams.

Replies with the stream IDs supported by the device, encoded as a positional argument list. There is no requirement that a device's stream IDs are contiguous.

### `desc <stream-id>`

Describe a single stream.

Replies with stream info using named arguments as key/value pairs. Common keys:

  - `name`
  - `class`: `sensor` | `motor`
  - `min`, `max`: meaning is determined by `class`:
    - `sensor` - min/max value that sensor can report
    - `motor` - min/max speed
  - `units`

### `report`

Various subcommands for controlling automated reporting of values.

  - `report on <interval> <stream-ids>...`
  - `report off`

When reports are enabled, notifications will be sent at the requested `interval` (in milliseconds), one per stream. Stream reports begin with the `report` argument; successive arguments are dictated by the stream's `class`.

If `stream-ids` are omitted for the initial `report on` command, reporting will be enabled for all supported streams.

Example:

```
0< report on 1000 1 2 3
0> report on
... 1s passes ...
1! report 25
2! report 30
3! report 123
... 1s passes ...
1! report 28
2! report 32
3! report 222
```

## Stream Commands

### `sensor`

#### `read`

#### `configure`

Depends on specific device.

### `motor`

#### `f <speed>`

Forward.

#### `r <speed>`

Reverse.

#### `stop`

Stop.