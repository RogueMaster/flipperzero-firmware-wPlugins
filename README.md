# Flipper Elements

**A compact, yet exhaustive periodic table and chemical database for Flipper Zero**

Explore essential chemical, physical, quantum, geochemical, and biological data for all 119 elements directly on your Flipper Zero.  
Designed for quick reference, study, laboratory notes, educational use, and science-focused hardware projects.

**Author:** [Siarhei Besarab](https://en.wikipedia.org/wiki/Siarhei_Besarab) (aka steanlab)

## Features

* **Complete Element Database (v2.0):** Contains a massive dataset for all 119 elements, stored entirely in flash memory (.rodata) with heavily optimized string formatting (Macros/Smart Text Wrapping) to ensure zero runtime RAM overhead and prevent stack overflows.

* **Element Identification & History:** Displays the element name, symbol, atomic number (Z), discoverer, block/group/period, category, atomic weight, and CAS Registry Number.

* **IUPAC-Compatible Atomic Masses:** For elements without a standard atomic weight, the mass number of the longest-lived isotope is shown in square brackets (e.g., 98 for technetium, 209 for polonium). Th, Pa, and U retain conventional atomic-weight values due to their characteristic terrestrial isotopic composition. 

* **Advanced Isotope Analytics:** Tracks total known isotopes, stable isotopes, natural isotopic abundances (showing all components $\ge$ 0.5%), and provides precise decay pathway metrics (Max Lifetime) detailing exact stages for heavy elements (Alpha, Beta+/-, SF, EC).

* **Comprehensive Atomic Sizing:** Includes atomic (ATM), covalent (COV), ionic (ION), metallic (MET), and Van der Waals (VDW) radii. Values are mapped directly in picometers (pm).

* **Thermo-Physical & Electrical Data:** Greatly expanded from basic properties. Includes density, melting/boiling points, thermal conductivity (W/(m·K)), specific heat capacity (J/(kg·K)), electrical resistivity (Ohm·m), electrical conductivity (S/m), and Mohs hardness.

* **Quantum & Chemical Properties:** Shows common oxidation states, expanded electronic configuration, electron affinity (kJ/mol), Pauling electronegativity, ionization energy (kJ/mol), and dipole polarizability (a.u.).

* **Geochemical & Cosmic Profiles:** Categorizes elements by their Goldschmidt classification (lithophile, siderophile, chalcophile, atmophile) and shows accurate concentration levels in the Earth's Crust (ppm), Oceans (mg/L), Atmosphere (ppm), and Cosmos (ppb).

* **Human Biology & Medicine Integration:** A dedicated bioscience section tracks the element's role in the human body (e.g., Trace, Major, Toxic), exact biological abundance by weight (%), and comprehensive descriptions of physiological functions, enzymes, or toxicity hazards.

* **Crystal & Magnetic Data:** 
  * Displays bulk magnetic classifications explicitly mapped (diamagn., paramagn., ferromagn., antiferromagn.) alongside Curie/Neel critical temperatures (Tc/Tn) in Kelvin.
  * Shows elementary crystal structures natively providing both the full structural name and its abbreviation (e.g., *face-centered cubic (fcc)*).

* **Smart UI & Quick Reference Tools:** 
  * Intuitive spatial grid (Mini-Map) layout that respects natural periodic gaps.
  * Deep scrollable element cards with quick horizontal element-jumping.
  * **Built-in Legend:** A rapid-access terminology guide for physical variables.

## Control Layout

**In the Main Grid (Periodic Table Layout):**
* **Up / Down / Left / Right** — Spatially navigate through periods, groups, and blocks.
* **Short Press OK** — Open the detailed comprehensive element property card.
* **Long Press OK** — Open the "About" screen (Author info, links).

**Inside the Element Detail Card:**
* **Up / Down** — Scroll through the detailed multi-line text (Atomic, Isotopes, Sizes, Physical, Quantum, Geo Props, Human Bio).
* **Left / Right** — Instantly jump to the previous (Z-1) or next (Z+1) element without returning to the main grid.
* **Short Press OK** — Open the **Property Guide/Legend** to decode specific scientific abbreviations and measurement units.

**Inside the About Screen:**
* **Left** — View Scientific References / Data sources.
* **Right** — View the Special Thanks & Credits screen.

## Data Fields Dictionary

The detailed element card is divided into smart categories including:

* **DISCOVERER & CAS:** Historical discovery attribution and chemical registry standard.
* **ATOMIC:** Element category, Block (s/p/d/f), Oxidation states, Group, Period, Crystal Structure.
* **ISOTOPES:** Total/Stable counters, Natural occurrence breakdown (%), Max half-life & decay mode.
* **SIZES (pm):** Atomic, Covalent, Ionic, Metallic, and Van der Waals radii.
* **PHYSICAL:** Mass (u), Density ($g/cm^3$), Melt/Boil points (K), Thermal Cond., Heat Capacity, Electrical Resistance/Conductance, Mohs hardness, Magnetism order, and Phase transition Temps.
* **QUANTUM:** Electron Affinity, Electronegativity, Ionization Energy, Dipolar Polarizability, Orbital configuration.
* **GEO PROPS:** Goldschmidt class, presence in Crust, Sea, Atmosphere, and Cosmic environments.
* **HUMAN BIO:** Human physiological role classification, percentage of human body weight, and specific biochemical functions / toxicity profiles.

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
