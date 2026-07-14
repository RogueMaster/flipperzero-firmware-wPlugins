## 1.8

* Update to latest SDK version.

## 1.7

* Replaced speaker note constants with an attribute delegator function to save space.
* Update to latest SDK version.

## 1.6

* Added extra functions for the **random** module.

## 1.5

* Added **io** module for basic file system operations.
* Added **logging** module to allow level based log messages.
* Rework of the **print** function: output redirection, based on script invocation.
* Added UART support: connect, read and write.
* Fixed the line feed handling in the REPL.

## 1.4

* Added interactive Python shell (aka REPL) as a CLI command.
* Allow passing a path to a script to execute as a CLI argument.
* Updated MicroPython to version 1.23.0.

## 1.3

* Added simple ADC support: read value and voltage.
* Added simple PWM support: start, stop, check status.
* Added infrared support: receive and transmit a signal, check status.
* Added success indicator to GPIO init function.
* Reset used GPIO pins upon script termination.
* Improved GPIO related functions to prevent user errors.
* Published Python package on PyPI for code completion support.

## 1.2

* Added simple GPIO support: initialize, read, write, interrupts.
* Added constants for musical note frequencies from C0 up to B8.
* Some minor fixes in the dialog functions.

## 1.1

* Display splash screen upon application start.
* API documentation available on GitHub pages.

## 1.0

* Initial stable release.
