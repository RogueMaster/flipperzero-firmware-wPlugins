
# Flipper Elements

**A compact periodic table of the elements for Flipper Zero.**

Explore essential chemical and physical data for all 119 elements directly on your Flipper Zero.  
Designed for quick reference, study, laboratory notes, educational use, and science-focused hardware projects.

**Author:** [Siarhei Besarab](https://en.wikipedia.org/wiki/Siarhei_Besarab) (aka steanlab)

## Features

* **Complete Element Database:** Contains data for all 119 elements, stored entirely in flash memory (`.rodata`) with no runtime RAM overhead.

* **Element Identification:** Displays the element name, symbol, atomic number `[Z]`, category, atomic weight, and CAS Registry Number.

* **IUPAC-Compatible Atomic Masses:** For elements without a standard atomic weight, the mass number of the longest-lived isotope is shown in square brackets, for example `[98]` for technetium and `[209]` for polonium. Th, Pa, and U retain conventional atomic-weight values due to their characteristic terrestrial isotopic composition. 

* **Chemical Data:** Shows common oxidation states, electronic/quantum configuration, and the total number of known or experimentally observed isotopes.

* **Atomic-Scale Data:** Includes atomic radii, covalent radii, and dominant ionic radii. Radius values are displayed in picometers (`pm`), together with the relevant ion formula where applicable.

* **Physical Data:** Displays density, melting point, boiling point, approximate Mohs hardness, magnetic behaviour near room temperature, crystal structure, and abundance in the Earth’s crust (`ppm`).

* **Magnetic Properties:** Includes bulk magnetic classifications such as diamagnetic, paramagnetic, ferromagnetic, ferrimagnetic, and antiferromagnetic where applicable.

* **Crystal Structures:** Shows elemental crystal structures at approximately ambient pressure. Common abbreviations are expanded for readability, for example *face-centred cubic*, *body-centred cubic*, and *hexagonal close-packed*.

* **Smart Mini-Map:** Navigate precisely through groups, periods, and the overall layout of the periodic table.

* **Navigation UI:** Supports intuitive linear traversal through the elements by atomic number, while preserving spatial navigation across the periodic-table layout.

* **Optimized for Flipper Zero:** The complete element dataset is compiled into flash memory (`.rodata`), avoiding dynamic allocation and runtime RAM overhead.

## Control Layout

* `Up` / `Down` — Move vertically through the periodic-table layout. Navigation dynamically bypasses structural gaps.
* `Left` / `Right` — Browse elements sequentially by atomic number `[Z]`.
* `Short Press OK` — Open the detailed element property card.
* `Long Press OK` — Open the application information screen with author, version, and credits.

## Data Fields

The detailed element card may include:

* Atomic number `[Z]`, name, symbol, category, atomic weight, and CAS Registry Number
* Common oxidation states
* Electron / quantum configuration
* Atomic, covalent, and ionic radii (`pm`)
* Density, melting point, boiling point, and Mohs hardness
* Magnetic behaviour near room temperature
* Crystal structure at approximately ambient pressure
* Abundance in the Earth’s crust (`ppm`)
* Number of known or experimentally observed isotopes

## Contacts

* **LAB-66:** [t.me/lab66](https://t.me/lab66)
* **LinkedIn:** [@steanlab](https://www.linkedin.com/in/steanlab)
* **Mastodon:** [@lab66](https://mastodon.social/@lab66)

## Support Development

If this database tool is useful for your academic, educational, laboratory, or hardware projects, you can support its development:

* [PayPal](https://paypal.me/steanlab)
* [Revolut](https://revolut.me/steanlab)
* [GitHub Sponsors](https://github.com/sponsors/steanlab)
* [Donorbox](https://donorbox.org/donations-for-lab-66)
* Crypto Bitcoin (BTC): bc1qe5dmykh247nnycz8tjlfp3te67kftsmc8uxz5k, Ethereum (ETH): 0x3Aa313FA17444db70536A0ec5493F3aaA49C9CBf, Z-CASH (ZEC): t1LrYwjGn84gG4fWRs3ajaHZdaK2VWi5gFk

*Thank you for supporting independent science and open-source tools.*
