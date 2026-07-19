#include "intro.h"

const char* const introParagraphs[] = {
    "They called it the MagnetoDynamic Induction Bomb: the MDIB.",

    "As modern warfare centered on missile silos buried deep underground,\n"
    "a new weapon was devised to destroy them by generating a vast oscillating magnetic field.",

    "The pulse induced immense currents in buried steel, pipelines, and machinery,\n"
    "turning the target's own infrastructure against it.",

    "But the first full-scale test coupled with the conductive core of the Earth itself, and destabilised the planet's magnetic field.",

    "Then came the solar wind.\n\n",
    "Satellites, power grids and communications failed.\n\n"
    "Repeated solar storms destroyed what remained and made recovery impossible.",

    "The machines did not all fail in the storms.\n\n"
    "The civilisation required to maintain them did.",

    "Survivors abandoned the old world's fragile electronics, turning to machines of iron, oil, and steam.",

    "Scrambling for solutions, a new generation of machine was developed to reclaim civilisation from chaos:",

    "The WaYfaring Reclamation Machine:\n\n"
    "The WYRM.",

    "Relying solely on hydraulic controls and primitive diesel engines,\n"
    "the WYRMs' long articulated bodies could crawl through ravaged terrain that ordinary vehicles could not.",

    "Their replaceable plates allowed damaged sections to be repaired with whatever materials remained available.",

    "Their hydraulic breaching stroke could drive the machine's armoured body through concrete and wreckage with immense force.\n\n"
    "Governments began to mass-produce them in the thousands.",

    "But reclamation never came.",

    "As civilisation disintegrated, the WYRMs were abandoned, stolen, and scattered into private hands.",

    "They quickly became powerful weapons of war, turning their breaching strokes against one another.\n\n"
    "Ownership was a near-guarantee of safety in a terminally hostile world.",

    "You heard rumours of one of the last undiscovered depots of government-issue WYRMs.",

    "And you got there first.",

    "Now, one of the great machines of the old world is yours.",

    "After climbing inside, you inspect the user manual:\n"
    "it explains that you can choose any of the 3 outermost plates to attack with, selecting them with the machine's inbuilt dpad.",
    "And that every plate has two main attributes:\n\nIntegrity (I):\nhow much damage it can withstand\n\nPower (P):\nhow much damage it can deal",

    "But there's no time for reading right now...\n\n",
    "A rival managed to track you down using a WYRM of their own.",

    "You prepare to attack!"
    ""
};

size_t GetIntroParagraphCount(void) {
    return sizeof(introParagraphs) / sizeof(introParagraphs[0]);
}
