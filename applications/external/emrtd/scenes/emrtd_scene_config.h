/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The scene list.
 *
 * Each entry expands into an enum member and into the three handler pointers
 * the scene manager dispatches through. The order is the order of the enum and
 * nothing else depends on it.
 *
 * The flow:
 *
 *     Start ─┬─ Read ──── Read ─┬─ ReadSuccess ── Result ─┬─ Holder
 *            │                  │                         ├─ Document
 *            │                  └─ ReadError              ├─ Security
 *            ├─ Document ─┬─ DocNumberInput               ├─ Keys
 *            │            ├─ DateInput (birth, expiry)    ├─ FileList ── FileDetail
 *            │            ├─ CanInput                     └─ Photo
 *            │            └─ ForgetConfirm ── ForgetDone
 *            ├─ Options ── DataGroups
 *            ├─ Saved ──── SavedDetail
 *            ├─ Donate
 *            └─ About
 */

ADD_SCENE(emrtd, start, Start)
ADD_SCENE(emrtd, document, Document)
ADD_SCENE(emrtd, doc_number_input, DocNumberInput)
ADD_SCENE(emrtd, date_input, DateInput)
ADD_SCENE(emrtd, can_input, CanInput)
ADD_SCENE(emrtd, forget_confirm, ForgetConfirm)
ADD_SCENE(emrtd, options, Options)
ADD_SCENE(emrtd, data_groups, DataGroups)
ADD_SCENE(emrtd, read, Read)
ADD_SCENE(emrtd, read_success, ReadSuccess)
ADD_SCENE(emrtd, read_error, ReadError)
ADD_SCENE(emrtd, result, Result)
ADD_SCENE(emrtd, result_holder, ResultHolder)
ADD_SCENE(emrtd, result_document, ResultDocument)
ADD_SCENE(emrtd, result_security, ResultSecurity)
ADD_SCENE(emrtd, result_keys, ResultKeys)
ADD_SCENE(emrtd, result_file_list, ResultFileList)
ADD_SCENE(emrtd, result_file_detail, ResultFileDetail)
ADD_SCENE(emrtd, result_photo, ResultPhoto)
ADD_SCENE(emrtd, saved, Saved)
ADD_SCENE(emrtd, saved_detail, SavedDetail)
ADD_SCENE(emrtd, donate, Donate)
ADD_SCENE(emrtd, about, About)
