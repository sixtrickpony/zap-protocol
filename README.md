# zap protocol

Zap is a text-based protocol allowing multiple independent _streams_ to be multiplexed
through a single serial link. Each stream on a Zap-enabled device supports 

## Example Session

A Zap device is identified by sending a `hello` message to the Control Stream.
It will reply with device information.

```
0<hello
0>hello name:"Example Device" vendor:"stp" id:"com.sixtrickpony.example" version:"1.1"
```

Next we inquire about the streams:

```
0<streams
0>streams 1
```

The response indicates there's a single stream with ID `1`. We send a `desc` request to
find out more about it:

```
0<desc 1
0>desc 1 name:adc class:sensor values:[adc] min:0 max:1023
```

Because the strem is a `sensor` we know it understands the `read` command to get the
current sensor value (note that we're sending this message to stream ID `1`)

```
1<read
1>read 500
```

We can enable periodic reports from this stream by sending a `report on` message to the
control stream, followed by the IDs of the streams for which we want to enable reporting.
This example will cause the device to send reports from stream `1` every 100ms.

```
0<report on 100 1
0>report on
1!report 490
1!report 495
1!report 520
```

Note that the reports have a message type of `!` - this indicates an "unsolicited" message;
that is, one that was not sent in direct response to a request.

The reports continue until disabled by a `report off` message:

```
0<report off
0>report off
```

## Standard Success/Error Responses

`ok` is the generic success response and never has any additional arguments:

```
>ok
```

Error responses begin with the positional argument `error` followed immediately by the
error ID, which should be a symbol. Errors may optionally include an integer `code` and
a string `message`.

```
>error $error-id
>error $error-id code:123
>error $error-id message:"invalid argument"
```








## Common Error IDs

TODO





## Protocol Description




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

---

# Protocol Format

Frames are newline-delimited ASCII and each adheres to the format:

```
<stream-id><frame-type-marker><binary-indicator?><body>\n
```

Wherein:

  - `stream-id` is a hex digit in the range `0-F`, indicating the source or destination stream
  - `frame-type-marker` is one of:
    - `<`: host to device request
    - `>`: device to host response
    - `!`  device to host notification
  - `binary-indicator`: `#`

If the binary indicator is present, `body` must be interpreted as hex-encoded binary.

Otherwise, `body` is parsed as a message in the form of an _argument list_.

## Argument Lists

Argument lists are space-separated lists of values; supported simple types are:

  - `Boolean`: `on`, `yes`, `true`, `off`, `no`, `false`
  - `Integer` (inc. hex-encoded): e.g. `123`, `-20`, `0`, `0xFF`
  - `Float`: e.g. `0.0`, `4.2`, `-123.5`
  - `String`, of which there are two flavours;
    - `Symbol`: matching the regex `[a-zA-Z_][a-zA-Z0-9_./?!-]*`, excluding otherwise
      reserved words (i.e. `Boolean` values)
    - `Quoted String`: any string inside double-qoutes e.g. `"hello there"`

Arguments may be **positional** or **named**. Where an argument list contains both,
all positional arguments must proceed named arguments. Named arguments follow the 
form `<name>:<space*><value>`, where `name` is any valid `Symbol`.

Argument lists may be nested by enclosing them in `[` and `]` braces; a nested list 
may appear anywhere where value is, and contents follow the same rules with regards
to positional and named arguments.

**Note:** the current Arduino reference implementation does not support parsing
nested argument lists. Indeed, it is recommended that consumption of nested lists
be avoided on the device/firmware side to reduce parsing complexity and memory
requirements. Producing nested lists on the device for consumption by the host
is of course OK, and fully supported by the Python client library.

### Example Argument Lists

```
# Single values
true
180
5.7
a_symbol
another-symbol
a.much.longer/symbol!
"this is a quoted string, it can contain spaces"

# Multiple values, positional
foo bar 1 2 3 a/symbol "this is a string"
1 2 3 [ this is a [ nested list ] ] ok? 

# Multiple values, named
min:100 max: 23

# Multiple values, positional + named
set config min:100 max:200 interval:350 enabled:true values: [1 2 3 ]
```

---

# Message Formats

## Control Stream

### `hello`

Request device info; response uses named arguments as key/value pairs. Common keys:

  - `name`: friendly device name (`string`)
  - `vendor`: vendor name (`string`)
  - `product`: product name (`string`)
  - `id`: product ID, reverse-domain encoded (`string`)
  - `serial`: serial number (`string`)
  - `version`: firmware version (`integer` or `string`, strings should be semver-compliant)
  - `revision`: hardware revision (`integer` or `string`, strings should be semver-compliant)

Example:

```
0<hello
0>hello name:"my device" id:"com.example.myDevice" version:"1.0.0"
```

### `streams`

Get a list of device's streams.

Responds with the stream IDs supported by the device (excluding the control stream), encoded as a positional argument list. There is no requirement that a device's stream IDs are contiguous.

Example:

```
0<streams
0>streams 1 2 4 6 A C
```

### `desc <stream-id>`

Request information about a single stream; response uses named arguments as key/value pairs. Common keys:

  - `name`: stream name; typically 
  - `class`: a stream's class is used 

### `report on <interval> <stream-ids>...`

Enabling reports causes notifications to be sent at the requested `interval` (in
milliseconds), one per specified stream. Stream reports begin with the `report` argument;
successive arguments are dictated by the stream's `class`.

If `stream-ids` are omitted, reporting will be enabled for all supported streams.

Enable reporting for all streams at 1s interval:

```
0<report on 1000
0>report on
... 1s passes ...
1!report 100
... 1s passes ...
1!report 200
```

Enable reporting for streams `2`, `6`, and `C`, at a 250ms interval:

```
0<report on 250 2 6 C
0>report on
... 250ms passes ...
2!report a b c
6!report 100
C!report false
... 250ms passes ...
2!report d e f
6!report 101
C!report true
```

### `report off`

Disable reporting.

```
0<report off
0>report off
```

## `sensor` class

### `desc` keys

  - `values`: positional list of value names
  - `min`: minimum value
  - `max`: maximum value
  - `units`: units

`min`, `max`, and `units` maybe be scalars (`integer` or `float`) or positional lists of the same. Scalars indicate that the same 

### `read`

Read the current sensor value, returning one value for each

Example (single value):

```
1<read
1>read 233
```

Example (multiple values):

```
1<read
1>read 100 false 3.2
```