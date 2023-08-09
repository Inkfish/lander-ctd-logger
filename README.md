# CTD Logger for the Inkfish Open Oceans Landers

This repository holds firmware for the [SparkFun OpenLog][] specifically tailored to logging measurements from the [Sea-Bird SBE 49 FastCAT CTD][SBE 49] on the Inkfish Open Oceans landers.

  [SBE 49]: https://www.seabird.com/sbe-49-fastcat-ctd/product?id=60762467704
  [SparkFun OpenLog]: https://www.sparkfun.com/products/13712

The SBE 49 is capable of sampling at 16 Hz, but the lander controller functions best when it receives CTD data at 1 Hz. This firmware logs at the higher rate, then downsamples the data and passes it to the controller at the lower rate.


## Wiring Diagram

The only components needed are the [SparkFun OpenLog][] and a [MAX3232 transceiver breakout][MAX3232]. 5V input is supplied by the Lander Control Board's buck converter.

<img src="./docs/CTD%20logger.svg" width="100%" />

(Note that some components are represented as SIP ICs for purposes of illustration. The leftmost pin of the MAX3232 transceiver above is `T1OUT`.)

In summary, the circuit interposes the communication between the CTD and Lander Control Board:

    +-------+          +---------------+         +-----------+
    | CTD   |          | MAX3232       |         | OpenLog   |
    |       |          |               |         |           |
    |   TX -------------> R1IN  R1OUT ------------> RX       |
    |       |  RS-232  |               |   TTL   |           |
    |       |    +------- T1OUT  T1IN <------------ TX       |
    |       |    |     |               |         |           |
    +-------+    |     +---------------+         +-----------+
                 |
                 |     +-------------------------------------+
                 |     | LCB                                 |
                 |     |                                     |
                 +------> CTD_RX                             |
                       |                                     |
                       +-------------------------------------+


**Note:** Consider the need to attach the programmer to the OpenLog. See Programming section below.


  [MAX3232]: https://www.sparkfun.com/products/11189


## Compiling

This project uses [PlatformIO][]. You can build the firmware and program the OpenLog using the PlatformIO Core CLI tool.

  [PlatformIO]: https://platformio.org/

First, clone the repository with submodules:

    git submodule update --init

Patch `SerialPort.h` to disable `BUFFERED_TX` and `ENABLE_RX_ERROR_CHECKING`:

    sed -i.bak \
      -e 's/\(#define BUFFERED_TX\) 1/\1 0/' \
      -e 's/\(#define ENABLE_RX_ERROR_CHECKING\) 1/\1 0/' \
      Libraries/SerialPort/SerialPort/SerialPort.h

Compile the project using PlatformIO to check for errors:

    pio run


## Programming

The OpenLog can be programmed using a 4-wire TTL UART to USB adapter. Review the [OpenLog Hookup Guide][hookup] for details on wiring. Importantly, the OpenLog `DTR` pin (inexplicably labeled `GRN`) must be connected to the programmer's `DTR` or `RTS` output.

  [hookup]: https://learn.sparkfun.com/tutorials/openlog-hookup-guide#hardware-hookup

    pio run -t upload


### Programming with a RS-232 cable

A USB to RS-232 cable (DB-9 connector) can also be used for programming. The `RX`/`TX` pinout on the DB-9 connector must be confirmed due to the various permutations.

  | DB-9              | MAX3232           | OpenLog |
  | ----------------- | ----------------- | ------- |
  | `TX` (pin 2 or 3) | `R1IN` → `R1OUT`  | `RX`    |
  | `RX` (pin 2 or 3) | `T1OUT` ← `T1IN`  | `TX`    |
  | `DTR` (pin 4)     | `R2IN`  → `R2OUT` | `GRN`   |
  | `GND` (pin 5)     | `GND`             | `GND`   |


### Programming with a Bus Pirate

The [Bus Pirate][] can be used as a programmer. Attach the pins as follows:

  | Bus Pirate | OpenLog |
  | ---------- | ------- |
  | `MOSI`     | `RX`    |
  | `MISO`     | `TX`    |
  | `CLK`      | `GRN`   | 
  | `GND`      | `GND`   |

Place the Bus Pirate into the transparent UART bridge mode with flow control signaling:

    # Enter UART mode, use baud 115200 for programming
    HiZ>m
    2. UART
    (1)>2
    ...

    # Enable power output on the pins
    UART>W
    POWER SUPPLIES ON
    Clutch engaged!!!

    # Start the transparent bridge
    UART>(3)
    UART bridge
    Reset to exit
    Are you sure? y

Disconnect the terminal session and proceed with programming the board.

  [Bus Pirate]: http://dangerousprototypes.com/docs/Bus_Pirate


## Configuration

By default, the OpenLog expects to receive and transmit data at 9600 baud, the same rate as the Lander Control Board. If this needs to be changed, a different baud rate can be written to the file `config.txt` at the root of the microSD card.


## Testing

A quick and dirty test script can be used to send fake samples to the logger from a PC. The script counts the number of downsampled replies that match expected values.

    python3 test/simctd.py --sal --sv /dev/tty.usbserial

The `--sal` and `--sv` flags simulate the `OUTPUTSAL` (salinity) and `OUTPUTSV` (sound velocity) options on the SBE 49.
