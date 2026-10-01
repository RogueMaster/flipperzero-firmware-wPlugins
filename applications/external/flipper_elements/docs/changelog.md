# Changelog

## 2.0 - Bio/Geo Expansion & Smart Engine UI
* Human Biochemistry Data: Added physiological classifications, exact human body abundance percentages, and dedicated biological function/toxicity descriptions for all 119 elements.
* Geochemical Profiles: Added Goldschmidt classifications (lithophile, siderophile, chalcophile, atmophile) along with precise occurrence concentrations mapped across the Earth's Crust, Oceans, Atmosphere, and Cosmic environments.
* Property Guide Subsystem: Integrated a brand-new, instantly accessible Legend tab mapped to the 'OK' button inside the element card, rapidly decoding physical variables, crystal structure abbreviations, and quantum terms.

## 1.9 - Electro-Magnetic Data & Phase Transitions
* Van der Waals Radii: Added VDW metrics to the element's Sizes panel.
* Magnetic Phase Transitions: Added tracking for Curie (Tc) and Néel (Tn) transition temperatures.
* Electro-Magnetic Properties: Added precise measurements for Electrical Resistivity (Ohm.m) and Conductivity (S/m).
* Smart Constants: The engine now dynamically appends standard (293-298 K) baseline labels exclusively for elements possessing valid resistivity/conductivity values.
* References Update: The "Alvarez" dataset has been integrated into the Sources menu.

## 1.8 - Brand UI Overhaul & Interactive Menus
* Interactive About Carousel: Replaced the standard native text box with a custom multi-page UI architecture. Users can now switch instantly between 'About', 'References', and 'Thanks' panels using the D-Pad.
* Scientific Sources Tab: Added a dedicated sub-view exclusively for listing reference materials (e.g., CIAAW, NUBASE, Slater, Shannon).
* XBM Graphics Engine: Completely redesigned application startup/exit sequences integrating native, memory-optimized LAB-66 logo graphic assets.
* Smart Unit Filter: The rendering engine now cleverly strips suffix variables (such as W/(m.K) or a.u.) whenever an element parameter is N/A, avoiding messy "ghost unit" rendering.

## 1.7 - Thermo-Physical & Quantum Properties Update 
* Metallic Radii: Added exact sizes for Metallic Radius measured in picometers (pm).
* Quantum Polarizability: Integrated static polarizability metrics tracked in atomic units (a.u.).
* Thermo-Physical Framework: Display detailed thermal conductivity (W/(m.K)) alongside specific heat states, matching compound/phase tags respectively.
* Expanded Text Buffers: Boosted element text reading memory boundaries from 1536 up to 2048 chars to efficiently process new macro-datasets without crashing device stack layouts.
* Extended Viewport Limits: Augmented detail views bottom offsets vertically (tracking limit to 780 px) to scale seamlessly with vast structural readouts.

## 1.6 - Precision UX Update & Data Parsing 
* Seamless Card Traversal: Overhauled GUI mapping enabling users to instantaneously leap onto neighboring left/right atomic elements inside detailed menus via the D-Pad without returning to the main grid.
* Advanced Multi-line Formats: Structured crystal shapes logic perfectly wraps sequential read arrays, bypassing long phrases truncations and preventing display bugs.
* Isotope Filter Enhancements: Added logic to isolate the most common naturally occurring elements by filtering out traces below the 1% threshold to reduce screen clutter.
* UI Category Grouping: Added dedicated visual split dividers cleanly grouping NAT (>=1%), MAX LIFE, and stable (Stbl) isotope data rows.

## 1.5 - Extreme Isotope Statistics Tracker 
* Heavy Analysis Dashboard: Global Element NuBase outputs summarize Total known, Stable, and Natural (NAT) stats natively within the UI parameters.
* Naturally Occurring Yield Trimming: Highlights up to the top 3 natural isotopes, accurately sorted by percentage distribution (e.g., 197Au: 100%).
* Extreme Lifespan Tracker (MAX LIFE)**: Automatically assigns the longest-living unstable isotope alongside accurate decay transition modes (SF, Alpha, EC, Beta+/-). 
* Display Formatting Output**: Heavily optimized visualization natively converts large numbers onto My/Gy/Py formats directly sparing minimal horizontal resolution capabilities.
* Microcontroller RAM Optimizations: Pre-processed dataset parameter limits match standard bounds injected flawlessly right into Flipper .rodata avoiding live processing operations inside standard loops.

## 1.4 - Mohs Scale & Earth Properties Update
* **Added Mohs Hardness**: Integrated approximate scratch hardness values on the Mohs scale.
* **Added Abundance Data**: Display element abundance in the Earth's crust (in ppm).
* **Added Isotopes Count**: Integrated data covering the number of all known/experimentally observed isotopes for each element.
* **UI/UX Tweaks**: Crystal structure acronyms (e.g., *fcc*, *bcc*, *hcp*) are now fully expanded (e.g., *face-centred cubic*) for better readability.

## 1.3 - Physical Data & IUPAC Standards Update
* **Added Density Data**: Complete volumetric density properties for 119 elements.
* **Added CAS Registry Numbers**: Integrated standardized CAS numbers into the element's database.
* **Added Crystal Structures**: Elemental crystal structures at approximately ambient pressure.
* **CIAAW/IUPAC Standards Applied**: Elements with no stable isotopes now properly display the mass number of their longest-lived isotope inside square brackets (e.g. 98 for Technetium, 209 for Polonium, etc.).

## 1.2 - Magnetism Update
* **Added Magnetic Properties**: Integrated bulk magnetic character of elements near room temperature (Diamagnetic, Paramagnetic, Ferromagnetic, etc.). 
* **UI Structure**: A new MAGN: row has been appended under the === PHYSICAL === properties category.

## 1.1 - Atomic Radii & Oxidation Update
* **Added Oxidation States**: Added Greenwood & Earnshaw's common oxidation states level-0.
* **Added Atomic & Covalent Radii**: Values measured in picometers (pm).
* **Added Ionic Radii**: Expanded data featuring the specific dominant ion formulas alongside their sizes in pm (e.g., ION: 76.0 (Li+)).
* **UI Refinement**: Atomic number Z successfully detached and relocated directly to the top-left corner of the details screen for classic aesthetic navigation.

## 1.0 - Initial Release
* **Smart Mini-Map**: Navigate precisely through rows and periods.
* **Detailed Insights**: Displays Atomic Weight, Category, Boiling & Melting points, and Quantum configuration.
* **Navigation UI**: Intuitive linear Z-based (Atomic number) chronological traversal that dynamically bypasses structural gaps.
* **Ultimate Performance**: All 119 elements are packed entirely inside the flash memory (.rodata). Zero RAM overhead
