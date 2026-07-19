package com.flipperzero.androidkeyboard.touchpad

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import androidx.core.graphics.toColorInt
import com.flipperzero.androidkeyboard.InputSink
import com.flipperzero.androidkeyboard.hid.MouseButtons
import kotlin.math.abs
import kotlin.math.roundToInt

/**
 * Relative touchpad surface.
 * - 1 finger drag → mouse move
 * - 1 finger tap → left click
 * - 2 finger vertical drag → scroll
 * - 2 finger tap → right click
 *
 * Transport is injected via [inputSink] (same pattern as [com.flipperzero.androidkeyboard.keyboard.JsonKeyboardView] listeners).
 */
class TouchpadView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : View(context, attrs) {

    fun interface Listener {
        fun onReadyRequired()
    }

    var listener: Listener? = null
    var inputSink: InputSink? = null

    private val density = resources.displayMetrics.density
    private val moveScale = 1.35f
    private val scrollScale = 0.08f
    private val tapSlopPx = 12f * density
    private val tapMaxMs = 280L

    private var activePointerId = MotionEvent.INVALID_POINTER_ID
    private var lastX = 0f
    private var lastY = 0f
    private var downX = 0f
    private var downY = 0f
    private var downAt = 0L
    private var moved = false
    private var twoFinger = false
    private var lastScrollY = 0f
    private var scrollAcc = 0f

    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = BG }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = BORDER
        style = Paint.Style.STROKE
        strokeWidth = 2f * density
    }
    private val hintPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = HINT
        textAlign = Paint.Align.CENTER
        textSize = 15f * density
        typeface = Typeface.DEFAULT
    }

    override fun onDraw(canvas: Canvas) {
        val pad = 8f * density
        canvas.drawRoundRect(pad, pad, width - pad, height - pad, 16f * density, 16f * density, bgPaint)
        canvas.drawRoundRect(pad, pad, width - pad, height - pad, 16f * density, 16f * density, borderPaint)
        canvas.drawText("Touchpad", width / 2f, height / 2f - 10f * density, hintPaint)
        canvas.drawText(
            "tap · drag · 2-finger scroll/right",
            width / 2f,
            height / 2f + 16f * density,
            hintPaint,
        )
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                activePointerId = event.getPointerId(0)
                lastX = event.x
                lastY = event.y
                downX = event.x
                downY = event.y
                downAt = System.currentTimeMillis()
                moved = false
                twoFinger = false
                scrollAcc = 0f
                lastScrollY = event.y
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                twoFinger = true
                moved = true
                if (event.pointerCount >= 2) {
                    lastScrollY = (event.getY(0) + event.getY(1)) / 2f
                    scrollAcc = 0f
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount >= 2) {
                    twoFinger = true
                    val midY = (event.getY(0) + event.getY(1)) / 2f
                    val dy = lastScrollY - midY
                    lastScrollY = midY
                    scrollAcc += dy * scrollScale
                    val steps = scrollAcc.toInt()
                    if (steps != 0) {
                        scrollAcc -= steps
                        if (!sendScroll(steps)) return true
                        moved = true
                    }
                } else {
                    val index = event.findPointerIndex(activePointerId)
                    if (index < 0) return true
                    val x = event.getX(index)
                    val y = event.getY(index)
                    val dx = ((x - lastX) * moveScale).roundToInt()
                    val dy = ((y - lastY) * moveScale).roundToInt()
                    lastX = x
                    lastY = y
                    if (abs(x - downX) > tapSlopPx || abs(y - downY) > tapSlopPx) {
                        moved = true
                    }
                    if (dx != 0 || dy != 0) {
                        if (!sendMove(dx, dy)) return true
                    }
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                val elapsed = System.currentTimeMillis() - downAt
                val isTap = !moved && elapsed <= tapMaxMs &&
                    abs(event.x - downX) <= tapSlopPx &&
                    abs(event.y - downY) <= tapSlopPx
                if (isTap) {
                    val button = if (twoFinger) MouseButtons.RIGHT else MouseButtons.LEFT
                    if (!sendClick(button)) return true
                }
                activePointerId = MotionEvent.INVALID_POINTER_ID
                twoFinger = false
            }
            MotionEvent.ACTION_POINTER_UP -> {
                if (event.pointerCount <= 2) {
                    val remaining = if (event.actionIndex == 0) 1 else 0
                    if (remaining < event.pointerCount) {
                        activePointerId = event.getPointerId(remaining)
                        lastX = event.getX(remaining)
                        lastY = event.getY(remaining)
                    }
                }
            }
        }
        return true
    }

    private fun sendMove(dx: Int, dy: Int): Boolean {
        val sink = inputSink
        if (sink == null || !sink.sendMouseMove(dx, dy)) {
            listener?.onReadyRequired()
            return false
        }
        return true
    }

    private fun sendScroll(delta: Int): Boolean {
        val sink = inputSink
        if (sink == null || !sink.sendMouseScroll(delta)) {
            listener?.onReadyRequired()
            return false
        }
        return true
    }

    private fun sendClick(button: Byte): Boolean {
        val sink = inputSink
        if (sink == null || !sink.sendMouseButton(true, button)) {
            listener?.onReadyRequired()
            return false
        }
        postDelayed({ sink.sendMouseButton(false, button) }, 40L)
        return true
    }

    companion object {
        private val BG = "#1A1A1A".toColorInt()
        private val BORDER = "#3A3A3A".toColorInt()
        private val HINT = "#888888".toColorInt()
    }
}
