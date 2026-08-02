# Callsign Generator

The callsign generator exists to teach your ear the sound of common amateur-radio prefixes without also teaching it a tiny address book. With ten calls in the box, hearing `YO3` can quickly become a cue to answer `YO3GND`. That is association masquerading as copy.

It instead assembles plausible four- to six-character training calls from weighted prefix patterns for 20 common entities, then generates a suitable digit and suffix. They are not a directory of licensed stations, and one may resemble a real call by chance. The point is to keep the shape of real callsigns while leaving the rest uncertain enough that you must actually listen.

I am rather proud of how much callsign geography fits into this small package. It does not merely bolt a random digit and suffix onto something vaguely prefix-shaped. In Romania, for example, private operators receive `YO2` through `YO9`; `YO0` and `YO1` are special-call territory but comparatively rare on air, while the familiar `YP`, `YQ`, and `YR` special-call ranges belong in the practice mix. The generator knows the difference.

Equivalent rules for prefixes, digits, lengths, and weighting are encoded for all 20 entities. This is not a complete callsign authority; real allocations contain more corners than sensibly fit here. It does put prefixes into something resembling their natural habitat, giving the practice a fair chance of helping when a call arrives on air.
