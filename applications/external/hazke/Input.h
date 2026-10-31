#pragma once

// Flight controls for one frame, filled from the Flipper's buttons.
struct InputState {
    bool pitchUp, pitchDown;
    bool rollLeft, rollRight;
    bool yawLeft, yawRight; // no button; autolock drives yaw
    bool fire;
};
