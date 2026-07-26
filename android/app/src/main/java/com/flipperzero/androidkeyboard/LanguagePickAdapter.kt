package com.flipperzero.androidkeyboard

import android.view.ViewGroup
import android.widget.CheckBox
import androidx.recyclerview.widget.RecyclerView
import com.flipperzero.androidkeyboard.keyboard.LanguageInfo

class LanguagePickAdapter(
    private val matchedIds: Set<String>,
    private val enabledIds: MutableSet<String>,
    private val onEnabledChanged: () -> Unit,
) : RecyclerView.Adapter<LanguagePickAdapter.Holder>() {

    private var items: List<LanguageInfo> = emptyList()

    fun submit(list: List<LanguageInfo>) {
        items = list
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val box = CheckBox(parent.context).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            )
            textSize = 15f
            setPadding(
                0,
                (6 * resources.displayMetrics.density).toInt(),
                0,
                (6 * resources.displayMetrics.density).toInt(),
            )
        }
        return Holder(box)
    }

    override fun onBindViewHolder(holder: Holder, position: Int) {
        holder.bind(items[position])
    }

    override fun getItemCount(): Int = items.size

    inner class Holder(private val box: CheckBox) : RecyclerView.ViewHolder(box) {
        fun bind(info: LanguageInfo) {
            box.setOnCheckedChangeListener(null)
            box.text = buildString {
                append(info.title)
                append(" (")
                append(info.id)
                append(')')
                when {
                    info.isUserPack -> append(" ✎")
                    info.hasLabelPack -> append(" ■")
                    else -> append(" ○")
                }
                if (info.id in matchedIds) append(" ✓")
            }
            box.isChecked = info.id in enabledIds
            box.setOnCheckedChangeListener { _, checked ->
                if (checked) enabledIds += info.id else enabledIds -= info.id
                onEnabledChanged()
            }
        }
    }
}
