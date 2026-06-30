import os
import lief
import pathlib

UFBT_PATH = pathlib.Path.home() / ".ufbt"

FAP_LOCATION_AFTER_BUILD = "dist/chief_cooker.fap"
FAP_LOCATION_ON_FLIPPER = "/ext/apps/Sub-GHz/chief_cooker.fap"
OBJCOPY_PATH = UFBT_PATH / "toolchain/current/bin/arm-none-eabi-objcopy"


def clearSections():
    binary = lief.parse(FAP_LOCATION_AFTER_BUILD)

    for i, sect in enumerate(binary.sections):
        if sect.name.find("_Z") >= 0:
            newSectName = "_s%d" % i
            cmd = '%s "%s" --rename-section %s=%s' % (
                OBJCOPY_PATH,
                FAP_LOCATION_AFTER_BUILD,
                sect.name,
                newSectName,
            )
            print("Renaming to %s from %s" % (newSectName, sect.name))
            os.system(cmd)


os.system("ufbt")
clearSections()
os.system(
    "%s/toolchain/current/python/python %s/current/scripts/runfap.py -p auto -s %s -t %s"
    % (UFBT_PATH, UFBT_PATH, FAP_LOCATION_AFTER_BUILD, FAP_LOCATION_ON_FLIPPER)
)
