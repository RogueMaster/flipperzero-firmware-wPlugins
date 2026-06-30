# Features

Adding Python support to the Flipper Zero platform was only possible by rigorously sorting unnecessary language features.
So here is a detailed list of all supported and unsupported Python language features.

## Supported

The following features are enabled and supported by the interpreter:

* Garbage collector is enabled.
* Finaliser calls in the garbage collector (e.g. `__del__`).
* The `__file__` constant.
* Import of external files from the SD card.
* Read and write files from and to the SD card.
* The `time` module.
* The `random` module.
* The `logging` module.
* The `io` module.
* The `float` data type.
* The `%` string formatting operator.
* The `.format` string formatting function.
* Support for [decorator](https://docs.python.org/3/glossary.html#term-decorator) functions.
* The `setattr` function.
* The `filter` function.
* The `reversed` function.
* The `min` and `max` function.
* Module-level `__init__` imports.
* Support for a [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop).

## Unsupported

The following features are disabled and _not_ supported by the interpreter:

* The `__doc__` constants.
* Source code line numbers in exceptions.
* The `cmath` module.
* The `complex` data type.
* Support for multiple inheritance.
* Module-level `__getattr__` support according to [PEP 562](https://peps.python.org/pep-0562/).
* Support for the descriptors `__get__`, `__set__` and  `__delete__`.
* Coroutines with `async` and `await` functions.
* The `:=` assign expression.
* Non-standard `.pend_throw()` method for generators.
* Support for `bytes.hex` and `bytes.fromhex`.
* Support for unicode characters.
* The string functions `.center`, `.count`, `.partition`, `.rpartition` and `.splitlines`.
* The `bytearray` data type.
* The `memoryview` data type.
* The `slice` object.
* The `frozenset` object.
* The `property` decorator.
* The `next` function with a second argument.
* All special methods for user classes (e.g. `__imul__`).
* The `enumerate` function.
* The `compile` function.
* Support for `eval`, `exec` and `execfile` functions.
* The `NotImplemented` special constant.
* The `input` function.
* The `pow` function with 3 integer arguments.
* The `help` function.
* The `micropython` module.
* The `array` module.
* The `collections` module.
* The `struct` module.
* The `gc` module.
* The `sys` module.
* The `select` module.
* The `json` module.

This list of unsupported features is not set in stone. 
If you miss support for one particular feature, feel free to [open an issue](https://github.com/ofabel/mp-flipper/issues) or make a pull request.
