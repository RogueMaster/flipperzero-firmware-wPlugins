# Changelog

## 1.8 - Brand UI Overhaul & Interactive Menus

* Implemented an exclusive interactive About-carousel GUI model featuring rigid graphical lower sub-menus and native vertical tracking bounding overlays. 
* Integrated a sub-view dedicated to displaying verifiable scientific reference materials directly within the hardware limits (e.g., CIAAW, NUBASE, Slater, Shannon frameworks).
* Added dynamic branding via pure memory-optimized XBM assets inclusion, redesigning both application startup/logo screens and bounded exit layout prompts effectively.
* Replaced the monolithic native Flipper OS text-box within standard Information tabs by a structurally independent viewport architecture relying entirely on detached UI-thread custom callbacks for v_about. 
* Intelligent Unit-Appending logic: Deep visual readout strings optimizations dropping appended raw values conditionally. Removes "ghost parameters" effectively (dynamically converting malformed outputs like "N/A W/(m.K)" securely down to precise standalone "N/A" boundaries everywhere).

## 1.7 - Thermo-Physical & Quantum Datasets Expansion 

* Added Extended Element Data Structures: Added exact values for Metallic Radius (pm) rendering safely into atomic parameters view.
* Added Advanced Quantum Properties: Integrated polarizability metrics tracked directly in atomic units (a.u.).
* Added Advanced Thermo-Physical Parameters: Accurately mapping detailed thermal conductivity (W/(m.K)) and specific heat states (with precise compound/phase states labels).
* Expanded Memory Buffers: Increased native layout textual tracking buffers structurally (1536 to 2048 chars) protecting memory from stack overflows triggered by massive dataset augmentations.
* Re-scaled Internal Vertical Limits: Expanded deepest maximum screen text scrolling boundary offsets dynamically (590 tracking depth threshold limits) correctly adapting natively wrapped scrollbar interactions. 
* Main interface 'About' references bumped to align seamlessly with the new release state (v1.7).

## 1.6 - Precision UX Update & Expanded Parsing

* Added Seamless In-Card Traversal: You can now use Left/Right D-Pad buttons to switch instantly between elements directly inside the details sheet without reverting to the main grid.
* Dedicated view labels integrated for explicitly split structural outputs: NAT (>=1%), MAX LIFE, and Stbl counts.
* Changed: Filtered Isotope render thresholds: dynamically truncates naturally occurring elements (NAT) below the 1% mark to avoid visual overflow.
* Changed: Expanded buffer bounds and virtual text offsets limit capabilities effectively parsing massive NuBase datasets natively without crashing vertical reading frames. 
* Changed: Restructured Multi-line geometries layout: thoroughly preserves full string boundaries strictly ensuring textual bounds of complex crystalline formats naturally adapt over sequential lines instead of truncation gaps.

## 1.5 - Isotope Overhaul

* Added Isotope summary statistics: total known, stable, and naturally occurring (NAT).
* Added Display of the top 3 most common natural isotopes, sorted by descending percentage (e.g., 197Au: 100%).
* Added MAX_LIFE parameter tracker for the heaviest isotopes, rendering values in ms up to Exa-Years.
* Added Specific physical transition path parameters: SF, EC, Alpha, Beta+/-.
* Changed Display values dynamically condensed to fit the 128x64 screen constraints (e.g., 99.9%, My/Gy format suffix limits).
* ChangedComplete two-line render restructuring to accommodate extra element stats without overlapping.
* ChangedPre-processed specific bounds values strictly matching the NUBASE bounds directly inside .rodata for optimized heap consumption.

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
