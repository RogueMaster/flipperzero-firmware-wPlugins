/**
 * @file SpritesB.h
 * \brief
 * A class for drawing animated sprites from image and mask bitmaps.
 * Optimized for small code size.
 */

#ifndef SpritesB_h
#define SpritesB_h

#include "Arduboy2.h"
#include "include/SpritesCommon.h"

/** \brief
 * A class for drawing animated sprites from image and mask bitmaps.
 * Optimized for small code size.
 *
 * \details
 * The functions in this class are identical to the `Sprites` class. The only
 * difference is that the functions in this class are optimized for smaller
 * code size rather than execution speed.
 *
 * See the `Sprites` class documentation for details on the use of the
 * functions in this class.
 *
 * Even if the speed is acceptable when using `SpritesB`, you should still try
 * using `Sprites`. In some cases `Sprites` will produce less code than
 * `SpritesB`, notably when only one of the functions is used.
 *
 * You can easily switch between using the `Sprites` class or the `SpritesB`
 * class by using one or the other to create an object instance:
 *
 * \code{.cpp}
 * Sprites sprites;  // Use this to optimize for execution speed
 * SpritesB sprites; // Use this to (likely) optimize for code size
 * \endcode
 *
 * \see Sprites
 */
class SpritesB
{
  public:
    /** \brief
     * Draw a sprite using a separate image and mask array.
     *
     * \param x,y The coordinates of the top left pixel location.
     * \param bitmap A pointer to the array containing the image frames.
     * \param mask A pointer to the array containing the mask frames.
     * \param frame The frame number of the image to draw.
     * \param mask_frame The frame number for the mask to use (can be different
     * from the image frame number).
     *
     * \see Sprites::drawExternalMask()
     */
    static void drawExternalMask(int16_t x, int16_t y, const uint8_t *bitmap,
                                 const uint8_t *mask, uint8_t frame, uint8_t mask_frame);

    /** \brief
     * Draw a sprite using an array containing both image and mask values.
     *
     * \param x,y The coordinates of the top left pixel location.
     * \param bitmap A pointer to the array containing the image/mask frames.
     * \param frame The frame number of the image to draw.
     *
     * \see Sprites::drawPlusMask()
     */
    static void drawPlusMask(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);

    /** \brief
     * Draw a sprite by replacing the existing content completely.
     *
     * \param x,y The coordinates of the top left pixel location.
     * \param bitmap A pointer to the array containing the image frames.
     * \param frame The frame number of the image to draw.
     *
     * \see Sprites::drawOverwrite()
     */
    static void drawOverwrite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);

    /** \brief
     * "Erase" a sprite.
     *
     * \param x,y The coordinates of the top left pixel location.
     * \param bitmap A pointer to the array containing the image frames.
     * \param frame The frame number of the image to erase.
     *
     * \see Sprites::drawErase()
     */
    static void drawErase(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);

    /** \brief
     * Draw a sprite using only the bits set to 1.
     *
     * \param x,y The coordinates of the top left pixel location.
     * \param bitmap A pointer to the array containing the image frames.
     * \param frame The frame number of the image to draw.
     *
     * \see Sprites::drawSelfMasked()
     */
    static void drawSelfMasked(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);

    // Master function. Needs to be abstracted into separate function for
    // every render type.
    // (Not officially part of the API)
    static void draw(int16_t x, int16_t y,
                     const uint8_t *bitmap, uint8_t frame,
                     const uint8_t *mask, uint8_t sprite_frame,
                     uint8_t drawMode);

    // (Not officially part of the API)
    static void drawBitmap(int16_t x, int16_t y,
                           const uint8_t *bitmap, const uint8_t *mask,
                           uint8_t w, uint8_t h, uint8_t draw_mode);
};

#endif
