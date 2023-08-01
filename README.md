# CTD Logger for the Inkfish Open Oceans Landers

This repository holds firmware for the [SparkFun OpenLog][] specifically tailored to logging measurements from the [Sea-Bird SBE 49 FastCAT CTD][SBE 49] on the Inkfish Open Oceans landers.

  [SBE 49]: https://www.seabird.com/sbe-49-fastcat-ctd/product?id=60762467704
  [SparkFun OpenLog]: https://www.sparkfun.com/products/13712

The SBE 49 is capable of sampling at 16 Hz, but the lander controller functions best when it receives CTD data at 1 Hz. This firmware logs at the higher rate, then downsamples the data and passes it to the controller at the lower rate.


## Wiring Diagram

The only components needed are the [SparkFun OpenLog][] and a [MAX3232 transceiver breakout][MAX3232]. 5V input is supplied by the Lander Control Board's buck converter.

<img src="./docs/CTD%20logger.svg" width="100%" />

(Note that some components, such as the CTD, are represented as SIP ICs for purposes of illustration.)

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


  [MAX3232]: https://www.sparkfun.com/products/11189


## Compiling

This project uses [PlatformIO][]. You can build the firmware and program the OpenLog using the PlatformIO Core CLI tool:

    git submodule update --init
    pio run

  [PlatformIO]: https://platformio.org/
