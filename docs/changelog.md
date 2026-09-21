# Changelog

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
