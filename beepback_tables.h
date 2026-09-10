/* Generated tables - do not edit by hand. */
#pragma once

/* Unit outlines for the four polygon shapes, apex up.
   Generated, so the firmware needs no trig at draw time. */
static const float BB_TRI_X[3] = {0.0000f, 0.9699f, -0.9699f};
static const float BB_TRI_Y[3] = {-1.0000f, 0.6800f, 0.6800f};
static const float BB_SQR_X[4] = {0.7425f, 0.7425f, -0.7425f, -0.7425f};
static const float BB_SQR_Y[4] = {-0.7425f, 0.7425f, 0.7425f, -0.7425f};
static const float BB_PENT_X[5] = {0.0000f, 0.9986f, 0.6172f, -0.6172f, -0.9986f};
static const float BB_PENT_Y[5] = {-1.0500f, -0.3245f, 0.8495f, 0.8495f, -0.3245f};
static const float BB_STAR_X[10] = {0.0000f, 0.3042f, 1.0937f, 0.4922f, 0.6760f, 0.0000f, -0.6760f, -0.4922f, -1.0937f, -0.3042f};
static const float BB_STAR_Y[10] = {-1.1500f, -0.4187f, -0.3554f, 0.1599f, 0.9304f, 0.5175f, 0.9304f, 0.1599f, -0.3554f, -0.4187f};

/* sin over one full turn, 64 steps plus the wrap point */
static const float BB_SIN[65] = {
    0.00000f, 0.09802f, 0.19509f, 0.29028f, 0.38268f, 0.47140f, 0.55557f, 0.63439f,
    0.70711f, 0.77301f, 0.83147f, 0.88192f, 0.92388f, 0.95694f, 0.98079f, 0.99518f,
    1.00000f, 0.99518f, 0.98079f, 0.95694f, 0.92388f, 0.88192f, 0.83147f, 0.77301f,
    0.70711f, 0.63439f, 0.55557f, 0.47140f, 0.38268f, 0.29028f, 0.19509f, 0.09802f,
    0.00000f, -0.09802f, -0.19509f, -0.29028f, -0.38268f, -0.47140f, -0.55557f, -0.63439f,
    -0.70711f, -0.77301f, -0.83147f, -0.88192f, -0.92388f, -0.95694f, -0.98079f, -0.99518f,
    -1.00000f, -0.99518f, -0.98079f, -0.95694f, -0.92388f, -0.88192f, -0.83147f, -0.77301f,
    -0.70711f, -0.63439f, -0.55557f, -0.47140f, -0.38268f, -0.29028f, -0.19509f, -0.09802f,
    -0.00000f
};
