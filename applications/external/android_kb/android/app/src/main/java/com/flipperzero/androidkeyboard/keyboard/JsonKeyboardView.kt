package com.flipperzero.androidkeyboard.keyboard

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.util.AttributeSet
import android.util.TypedValue
import android.view.Gravity
import android.view.MotionEvent
import android.widget.GridLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.graphics.toColorInt
import kotlin.math.abs

class JsonKeyboardView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : LinearLayout(context, attrs) {

    fun interface KeyListener {
        fun onKey(key: KeyboardKey, effectiveMods: Byte)
    }

    fun interface LayoutSwipeListener {
        /** +1 = swipe right (next), -1 = swipe left (previous). */
        fun onLayoutSwipe(direction: Int)
    }

    var keyListener: KeyListener? = null
    var layoutSwipeListener: LayoutSwipeListener? = null

    private var stickyMods: Byte = 0
    private val keyViews = mutableListOf<Pair<KeyboardKey, TextView>>()
    private val swipeThresholdPx: Float

    init {
        orientation = VERTICAL
        setBackgroundColor(BG)
        swipeThresholdPx = 48f * resources.displayMetrics.density
    }

    fun bindLayout(layout: KeyboardLayout) {
        removeAllViews()
        keyViews.clear()
        stickyMods = 0

        val density = resources.displayMetrics.density
        val margin = (4 * density).toInt()

        layout.rows.forEach { row ->
            val grid = GridLayout(context)
            val totalSpan = row.sumOf { it.span.toDouble() }.toFloat().coerceAtLeast(1f)
            val unitColumns = (totalSpan * 2).toInt().coerceAtLeast(row.size)
            grid.columnCount = unitColumns
            grid.setPadding(margin, margin / 2, margin, margin / 2)

            var column = 0
            row.forEach { key ->
                val units = (key.span * 2).toInt().coerceAtLeast(1)
                val button = TextView(context).apply {
                    text = key.label
                    gravity = Gravity.CENTER
                    setTextColor(Color.WHITE)
                    setBackgroundColor(keyBackground(key, false))
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, if (key.label.length > 2) 14f else 18f)
                    typeface = Typeface.DEFAULT_BOLD
                    setPadding(0, (12 * density).toInt(), 0, (12 * density).toInt())
                    attachKeyInteraction(key, this)
                }

                val params = GridLayout.LayoutParams().apply {
                    width = 0
                    height = LayoutParams.WRAP_CONTENT
                    columnSpec = GridLayout.spec(column, units, units.toFloat())
                    setMargins(margin, margin, margin, margin)
                }
                button.layoutParams = params
                grid.addView(button)
                keyViews += key to button
                column += units
            }
            addView(grid)
        }
        refreshStickyVisuals()
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun attachKeyInteraction(key: KeyboardKey, button: TextView) {
        if (key.isSpace) {
            var downX = 0f
            var tracking = false
            button.setOnTouchListener { _, event ->
                when (event.actionMasked) {
                    MotionEvent.ACTION_DOWN -> {
                        downX = event.x
                        tracking = true
                        true
                    }
                    MotionEvent.ACTION_UP -> {
                        if (!tracking) return@setOnTouchListener false
                        tracking = false
                        val dx = event.x - downX
                        if (abs(dx) >= swipeThresholdPx) {
                            layoutSwipeListener?.onLayoutSwipe(if (dx > 0) 1 else -1)
                        } else {
                            onKeyClicked(key)
                        }
                        true
                    }
                    MotionEvent.ACTION_CANCEL -> {
                        tracking = false
                        true
                    }
                    else -> true
                }
            }
        } else {
            button.setOnClickListener { onKeyClicked(key) }
        }
    }

    private fun onKeyClicked(key: KeyboardKey) {
        if (key.stickyMod) {
            stickyMods = (stickyMods.toInt() xor key.mods.toInt()).toByte()
            refreshStickyVisuals()
            return
        }

        val mods = (key.mods.toInt() or stickyMods.toInt()).toByte()
        keyListener?.onKey(key, mods)

        if (stickyMods.toInt() != 0 && key.hid.toInt() != 0) {
            stickyMods = 0
            refreshStickyVisuals()
        }
    }

    private fun refreshStickyVisuals() {
        keyViews.forEach { (key, view) ->
            val active = key.stickyMod && (stickyMods.toInt() and key.mods.toInt()) != 0
            view.setBackgroundColor(keyBackground(key, active))
        }
    }

    private fun keyBackground(key: KeyboardKey, stickyActive: Boolean): Int {
        return when {
            stickyActive -> ACCENT
            key.stickyMod || key.label in SPECIAL_LABELS || key.isSpace -> KEY_SPECIAL
            else -> KEY_NORMAL
        }
    }

    companion object {
        private val BG = "#121212".toColorInt()
        private val KEY_NORMAL = "#2A2A2A".toColorInt()
        private val KEY_SPECIAL = "#3A3A3A".toColorInt()
        private val ACCENT = "#2E7D32".toColorInt()
        private val SPECIAL_LABELS = setOf("⇥", "⌫", "␣", "↵", "⇧", "⌃", "⌥", "⌘", "esc", "☰")
    }
}
