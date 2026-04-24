package xyz.i12bp8.tagtinker.android.render

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Matrix
import android.graphics.Paint
import kotlin.math.max
import kotlin.math.min
import xyz.i12bp8.tagtinker.android.model.DitherMode
import xyz.i12bp8.tagtinker.android.model.EditorState
import xyz.i12bp8.tagtinker.android.model.FitMode

private val white = intArrayOf(255, 255, 255)
private val black = intArrayOf(0, 0, 0)
private val red = intArrayOf(218, 45, 53)
private val yellow = intArrayOf(226, 177, 32)

data class RenderResult(
    val bitmap: Bitmap,
    val bmpBytes: ByteArray,
)

object ImagePipeline {
    fun render(source: Bitmap, width: Int, height: Int, accentColor: String, editor: EditorState): RenderResult {
        val framed = drawSourceFrame(source, width, height, editor)
        val adjusted = adjustBitmap(framed, editor)
        val paletted = quantize(adjusted, accentColor, editor)
        return RenderResult(
            bitmap = paletted,
            bmpBytes = buildBmp(paletted, accentColor != "mono" && editor.useColor),
        )
    }

    private fun drawSourceFrame(source: Bitmap, width: Int, height: Int, editor: EditorState): Bitmap {
        val output = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(output)
        canvas.drawColor(Color.WHITE)

        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            isFilterBitmap = true
        }

        if (editor.fitMode == FitMode.Stretch) {
            canvas.drawBitmap(source, null, android.graphics.Rect(0, 0, width, height), paint)
            return output
        }

        val scaleFactor = when (editor.fitMode) {
            FitMode.Fill -> max(width / source.width.toFloat(), height / source.height.toFloat())
            else -> min(width / source.width.toFloat(), height / source.height.toFloat())
        } * editor.scale

        val drawWidth = max(1f, source.width * scaleFactor)
        val drawHeight = max(1f, source.height * scaleFactor)
        val left = (width - drawWidth) / 2f + editor.offsetX * width * 0.5f
        val top = (height - drawHeight) / 2f + editor.offsetY * height * 0.5f
        val matrix = Matrix().apply {
            postScale(drawWidth / source.width, drawHeight / source.height)
            postTranslate(left, top)
        }
        canvas.drawBitmap(source, matrix, paint)
        return output
    }

    private fun adjustBitmap(bitmap: Bitmap, editor: EditorState): Bitmap {
        val out = bitmap.copy(Bitmap.Config.ARGB_8888, true)
        val pixels = IntArray(out.width * out.height)
        out.getPixels(pixels, 0, out.width, 0, 0, out.width, out.height)
        val contrast = editor.contrast
        val brightness = editor.brightness
        val factor = (259f * (contrast + 255)) / (255 * (259 - contrast))
        for (i in pixels.indices) {
            val color = pixels[i]
            val r = clamp(factor * (Color.red(color) - 128) + 128 + brightness)
            val g = clamp(factor * (Color.green(color) - 128) + 128 + brightness)
            val b = clamp(factor * (Color.blue(color) - 128) + 128 + brightness)
            pixels[i] = Color.argb(255, r, g, b)
        }
        out.setPixels(pixels, 0, out.width, 0, 0, out.width, out.height)
        return out
    }

    private fun quantize(bitmap: Bitmap, accentColor: String, editor: EditorState): Bitmap {
        val width = bitmap.width
        val height = bitmap.height
        val src = IntArray(width * height)
        bitmap.getPixels(src, 0, width, 0, 0, width, height)
        val out = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val dst = IntArray(width * height)
        val palette = if (editor.useColor && accentColor != "mono") {
            arrayOf(white, black, if (accentColor == "yellow") yellow else red)
        } else {
            arrayOf(white, black)
        }

        if (editor.ditherMode == DitherMode.Threshold) {
            src.indices.forEach { index ->
                dst[index] = nearestColor(src[index], palette, editor.detail)
            }
        } else {
            val work = FloatArray(width * height * 3)
            src.indices.forEach { index ->
                work[index * 3] = Color.red(src[index]).toFloat()
                work[index * 3 + 1] = Color.green(src[index]).toFloat()
                work[index * 3 + 2] = Color.blue(src[index]).toFloat()
            }
            for (y in 0 until height) {
                for (x in 0 until width) {
                    val base = (y * width + x) * 3
                    val current = Color.rgb(
                        clamp(work[base]),
                        clamp(work[base + 1]),
                        clamp(work[base + 2]),
                    )
                    val next = nearestColor(current, palette, editor.detail)
                    dst[y * width + x] = next
                    val er = Color.red(current) - Color.red(next)
                    val eg = Color.green(current) - Color.green(next)
                    val eb = Color.blue(current) - Color.blue(next)
                    diffuse(work, width, height, x, y, er, eg, eb, editor.ditherMode)
                }
            }
        }

        out.setPixels(dst, 0, width, 0, 0, width, height)
        return out
    }

    private fun diffuse(
        work: FloatArray,
        width: Int,
        height: Int,
        x: Int,
        y: Int,
        er: Int,
        eg: Int,
        eb: Int,
        mode: DitherMode,
    ) {
        val points = if (mode == DitherMode.Atkinson) {
            listOf(
                intArrayOf(1, 0, 1, 8),
                intArrayOf(2, 0, 1, 8),
                intArrayOf(-1, 1, 1, 8),
                intArrayOf(0, 1, 1, 8),
                intArrayOf(1, 1, 1, 8),
                intArrayOf(0, 2, 1, 8),
            )
        } else {
            listOf(
                intArrayOf(1, 0, 7, 16),
                intArrayOf(-1, 1, 3, 16),
                intArrayOf(0, 1, 5, 16),
                intArrayOf(1, 1, 1, 16),
            )
        }

        points.forEach { point ->
            val nx = x + point[0]
            val ny = y + point[1]
            if (nx !in 0 until width || ny !in 0 until height) return@forEach
            val factor = point[2].toFloat() / point[3]
            val base = (ny * width + nx) * 3
            work[base] += er * factor
            work[base + 1] += eg * factor
            work[base + 2] += eb * factor
        }
    }

    private fun nearestColor(color: Int, palette: Array<IntArray>, detail: Int): Int {
        val threshold = 128 + (detail - 50) * 1.18f
        if (palette.size == 2) {
            val luma = Color.red(color) * 0.299f + Color.green(color) * 0.587f + Color.blue(color) * 0.114f
            return if (luma >= threshold) Color.WHITE else Color.BLACK
        }

        var best = palette.first()
        var bestDist = Float.MAX_VALUE
        palette.forEach { candidate ->
            val dr = Color.red(color) - candidate[0]
            val dg = Color.green(color) - candidate[1]
            val db = Color.blue(color) - candidate[2]
            val dist = dr * dr * 0.92f + dg * dg + db * db * 0.86f
            if (dist < bestDist) {
                bestDist = dist
                best = candidate
            }
        }
        return Color.rgb(best[0], best[1], best[2])
    }

    private fun buildBmp(bitmap: Bitmap, accent: Boolean): ByteArray {
        val width = bitmap.width
        val height = bitmap.height
        val rowStride = ((width + 31) / 32) * 4
        val colorStride = if (accent) rowStride else 0
        val pixelBytes = (rowStride + colorStride) * height
        val paletteBytes = if (accent) 12 else 8
        val headerSize = 14 + 40 + paletteBytes
        val fileSize = headerSize + pixelBytes
        val bytes = ByteArray(fileSize)

        writeAscii(bytes, 0, "BM")
        writeIntLE(bytes, 2, fileSize)
        writeIntLE(bytes, 10, headerSize)
        writeIntLE(bytes, 14, 40)
        writeIntLE(bytes, 18, width)
        writeIntLE(bytes, 22, height)
        writeShortLE(bytes, 26, 1)
        writeShortLE(bytes, 28, if (accent) 2 else 1)
        writeIntLE(bytes, 34, pixelBytes)

        var paletteOffset = 54
        writePalette(bytes, paletteOffset, Color.WHITE)
        paletteOffset += 4
        writePalette(bytes, paletteOffset, Color.BLACK)
        if (accent) {
            paletteOffset += 4
            var accentColor = Color.RED
            for (y in 0 until height) {
                for (x in 0 until width) {
                    val c = bitmap.getPixel(x, y)
                    if (c != Color.WHITE && c != Color.BLACK) accentColor = c
                }
            }
            writePalette(bytes, paletteOffset, accentColor)
        }

        val monoOffset = headerSize
        val accentOffset = headerSize + rowStride * height

        for (y in 0 until height) {
            val flippedY = height - 1 - y
            for (x in 0 until width) {
                val color = bitmap.getPixel(x, flippedY)
                val byteIndex = x / 8
                val bit = 7 - (x % 8)
                if (color == Color.BLACK) {
                    bytes[monoOffset + y * rowStride + byteIndex] =
                        (bytes[monoOffset + y * rowStride + byteIndex].toInt() or (1 shl bit)).toByte()
                } else if (accent && color != Color.WHITE) {
                    bytes[accentOffset + y * rowStride + byteIndex] =
                        (bytes[accentOffset + y * rowStride + byteIndex].toInt() or (1 shl bit)).toByte()
                }
            }
        }
        return bytes
    }

    private fun writeAscii(bytes: ByteArray, offset: Int, value: String) {
        value.encodeToByteArray().copyInto(bytes, offset)
    }

    private fun writeShortLE(bytes: ByteArray, offset: Int, value: Int) {
        bytes[offset] = (value and 0xff).toByte()
        bytes[offset + 1] = ((value shr 8) and 0xff).toByte()
    }

    private fun writeIntLE(bytes: ByteArray, offset: Int, value: Int) {
        bytes[offset] = (value and 0xff).toByte()
        bytes[offset + 1] = ((value shr 8) and 0xff).toByte()
        bytes[offset + 2] = ((value shr 16) and 0xff).toByte()
        bytes[offset + 3] = ((value shr 24) and 0xff).toByte()
    }

    private fun writePalette(bytes: ByteArray, offset: Int, color: Int) {
        bytes[offset] = Color.blue(color).toByte()
        bytes[offset + 1] = Color.green(color).toByte()
        bytes[offset + 2] = Color.red(color).toByte()
        bytes[offset + 3] = 0
    }

    private fun clamp(value: Float): Int = value.toInt().coerceIn(0, 255)
}
