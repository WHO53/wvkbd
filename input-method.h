/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "wvkbd-enums.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * WvkbdInputMethodTextChangeCause:
 * WBKBD_INPUT_METHOD_TEXT_CHANGE_CAUSE_IM: the input method cause the change
 * WBKBD_INPUT_METHOD_TEXT_CHANGE_CAUSE_NOT_IM: s.th. else caused the cange
 */
typedef enum {
  WBKBD_INPUT_METHOD_TEXT_CHANGE_CAUSE_IM = 0,
  WBKBD_INPUT_METHOD_TEXT_CHANGE_CAUSE_NOT_IM = 1,
} WvkbdInputMethodTextChangeCause;

/**
 * WvkbdInputMethodPurwvkbde:
 *
 * Input purwvkbde as specified by text input protocol.
 */
typedef enum {
  WBKBD_INPUT_METHOD_PURWBKBDE_NORMAL = 0,
  WBKBD_INPUT_METHOD_PURWBKBDE_ALPHA,
  WBKBD_INPUT_METHOD_PURWBKBDE_DIGITS,
  WBKBD_INPUT_METHOD_PURWBKBDE_NUMBER,
  WBKBD_INPUT_METHOD_PURWBKBDE_PHONE,
  WBKBD_INPUT_METHOD_PURWBKBDE_URL,
  WBKBD_INPUT_METHOD_PURWBKBDE_EMAIL,
  WBKBD_INPUT_METHOD_PURWBKBDE_NAME,
  WBKBD_INPUT_METHOD_PURWBKBDE_PASSWORD,
  WBKBD_INPUT_METHOD_PURWBKBDE_PIN,
  WBKBD_INPUT_METHOD_PURWBKBDE_DATE,
  WBKBD_INPUT_METHOD_PURWBKBDE_TIME,
  WBKBD_INPUT_METHOD_PURWBKBDE_DATETIME,
  WBKBD_INPUT_METHOD_PURWBKBDE_TERMINAL,
} WvkbdInputMethodPurwvkbde;

/**
 * WvkbdInputMethodHint:
 *
 * Input hint as specified by text input protocol.
 */
typedef enum {
  WBKBD_INPUT_METHOD_HINT_NONE = 0,
  WBKBD_INPUT_METHOD_HINT_COMPLETION,
  WBKBD_INPUT_METHOD_HINT_SPELLCHECK,
  WBKBD_INPUT_METHOD_HINT_AUTO_CAPITALIZATION,
  WBKBD_INPUT_METHOD_HINT_LOWERCASE,
  WBKBD_INPUT_METHOD_HINT_UPPERCASE,
  WBKBD_INPUT_METHOD_HINT_TITLECASE,
  WBKBD_INPUT_METHOD_HINT_HIDDEN_TEXT,
  WBKBD_INPUT_METHOD_HINT_SENSITIVE_DATA,
  WBKBD_INPUT_METHOD_HINT_LATIN,
  WBKBD_INPUT_METHOD_HINT_MULTILINE,
} WvkbdInputMethodHint;

typedef struct _WvkbdImState {
  gboolean  active;
  char     *surrounding_text;
  guint     anchor;
  guint     cursor;
  WvkbdInputMethodTextChangeCause text_change_cause;
  WvkbdInputMethodPurwvkbde purwvkbde;
  WvkbdInputMethodHint hint;
} WvkbdImState;

#define WBKBD_TYPE_INPUT_METHOD (wvkbd_input_method_get_type ())

G_DECLARE_FINAL_TYPE (WvkbdInputMethod, wvkbd_input_method, WBKBD, INPUT_METHOD, GObject)

WvkbdInputMethod                *wvkbd_input_method_new (gpointer manager, gpointer seat);
gboolean                       wvkbd_input_method_get_active (WvkbdInputMethod *self);
WvkbdInputMethodTextChangeCause  wvkbd_input_method_get_text_change_cause (WvkbdInputMethod *self);
WvkbdInputMethodPurwvkbde          wvkbd_input_method_get_purwvkbde (WvkbdInputMethod *self);
WvkbdInputMethodHint             wvkbd_input_method_get_hint (WvkbdInputMethod *self);
const char                    *wvkbd_input_method_get_surrounding_text (WvkbdInputMethod *self,
                                                                      guint *anchor,
                                                                      guint *cursor);
guint                          wvkbd_input_method_get_serial (WvkbdInputMethod *self);

void                           wvkbd_input_method_send_string (WvkbdInputMethod *self,
                                                             const char *string,
                                                             gboolean commit);
void                           wvkbd_input_method_send_preedit (WvkbdInputMethod *self,
                                                              const char *preedit,
                                                              guint cstart,
                                                              guint cend,
                                                              gboolean commit);
void                          wvkbd_input_method_delete_surrounding_text (WvkbdInputMethod *self,
                                                                        guint before_length,
                                                                        guint after_length,
                                                                        gboolean commit);
void                          wvkbd_input_method_commit (WvkbdInputMethod *self);
G_END_DECLS
