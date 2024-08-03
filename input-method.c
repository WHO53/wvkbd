/*
 * Copyright (C) 2022 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Guido Günther <agx@sigxcpu.org>
 * Author: Deepak <notwho53@gmail.com>
 */

#define G_LOG_DOMAIN "wvkbd-input-method"

#include "input-method.h"

#include "input-method-unstable-v2-client-protocol.h"

enum {
  PROP_0,
  PROP_MANAGER,
  PROP_SEAT,
  /* Input method state */
  PROP_ACTIVE,
  PROP_SURROUNDING_TEXT,
  PROP_TEXT_CHANGE_CAUSE,
  PROP_PURWBKBDE,
  PROP_HINT,
  PROP_LAST_PROP
};
static GParamSpec *props[PROP_LAST_PROP];

enum {
  DONE,
  PENDING_CHANGED,
  N_SIGNALS
};
static guint signals[N_SIGNALS];

static void wvkbd_im_state_free (WvkbdImState *state);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WvkbdImState, wvkbd_im_state_free);

/**
 * WvkbdInputMethod:
 *
 * A Wayland input method handler. This wraps the
 * zwp_input_method_v2 protocol easing things like
 * double buffering state.
 *
 * The properties reflect applied state which is only updated
 * when the input method receives the `done` event form the
 * comwvkbditor.
 */
struct _WvkbdInputMethod {
  GObject  parent;

  gpointer manager;
  struct wl_seat *seat;
  struct zwp_input_method_v2 *input_method;

  WvkbdImState *pending;
  WvkbdImState *submitted;

  guint       serial;
};
G_DEFINE_TYPE (WvkbdInputMethod, wvkbd_input_method, G_TYPE_OBJECT)


static void
wvkbd_im_state_free (WvkbdImState *state)
{
  g_clear_pointer (&state->surrounding_text, g_free);
  g_free (state);
}


static WvkbdImState *
wvkbd_im_state_dup (WvkbdImState *state)
{
  WvkbdImState *new = g_memdup (state, sizeof (WvkbdImState));

  new->surrounding_text = g_strdup (state->surrounding_text);

  return new;
}


static void
handle_activate (void                       *data,
                 struct zwp_input_method_v2 *zwp_input_method_v2)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (data);

  g_debug ("%s", __func__);

  if (self->pending->active == TRUE)
    return;

  self->pending->active = TRUE;
  g_clear_pointer (&self->pending->surrounding_text, g_free);
  self->pending->text_change_cause = WBKBD_INPUT_METHOD_TEXT_CHANGE_CAUSE_IM;
  self->pending->purwvkbde = WBKBD_INPUT_METHOD_PURWBKBDE_NORMAL;
  self->pending->hint = WBKBD_INPUT_METHOD_HINT_NONE;

  g_signal_emit (self, signals[PENDING_CHANGED], 0, self->pending);
}


static void
handle_deactivate (void                       *data,
                   struct zwp_input_method_v2 *zwp_input_method_v2)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (data);

  g_debug ("%s", __func__);
  if (self->pending->active == FALSE)
    return;

  self->pending->active = FALSE;
  g_signal_emit (self, signals[PENDING_CHANGED], 0, self->pending);
}


static void
handle_surrounding_text (void                       *data,
                         struct zwp_input_method_v2 *zwp_input_method_v2,
                         const char                 *text,
                         uint32_t                    cursor,
                         uint32_t                    anchor)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (data);

  g_debug ("%s: '%s', cursor %d, anchor: %d", __func__, text, cursor, anchor);
  if (g_strcmp0 (self->pending->surrounding_text, text) == 0 &&
      self->pending->cursor == cursor &&
      self->pending->anchor == anchor)
    return;

  g_free (self->pending->surrounding_text);
  self->pending->surrounding_text = g_strdup (text);
  self->pending->cursor = cursor;
  self->pending->anchor = anchor;
  g_signal_emit (self, signals[PENDING_CHANGED], 0, self->pending);
}


static void
handle_text_change_cause (void                       *data,
                          struct zwp_input_method_v2 *zwp_input_method_v2,
                          uint32_t                    cause)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (data);

  g_debug ("%s: cause: %u", __func__, cause);

  if (self->pending->text_change_cause == cause)
    return;

  self->pending->text_change_cause = cause;
  g_signal_emit (self, signals[PENDING_CHANGED], 0, self->pending);
}


static void
handle_content_type (void                       *data,
                     struct zwp_input_method_v2 *zwp_input_method_v2,
                     uint32_t                    hint,
                     uint32_t                    purwvkbde)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (data);

  g_debug ("%s, hint: %d, purwvkbde: %d", __func__, hint, purwvkbde);

  if (self->pending->hint == hint && self->pending->purwvkbde == purwvkbde)
    return;

  self->pending->hint = hint;
  self->pending->purwvkbde = purwvkbde;
  g_signal_emit (self, signals[PENDING_CHANGED], 0, self->pending);
}


static void
handle_done (void                       *data,
             struct zwp_input_method_v2 *zwp_input_method_v2)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (data);
  g_autoptr (WvkbdImState) current = self->submitted;

  g_debug ("%s", __func__);

  self->serial++;
  g_object_freeze_notify (G_OBJECT (self));

  self->submitted = wvkbd_im_state_dup (self->pending);

  if (current->active != self->submitted->active)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);

  if (g_strcmp0 (current->surrounding_text, self->submitted->surrounding_text) ||
      current->cursor != self->submitted->cursor ||
      current->anchor != self->submitted->anchor)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SURROUNDING_TEXT]);

  if (current->text_change_cause != self->submitted->text_change_cause)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TEXT_CHANGE_CAUSE]);

  if (current->purwvkbde != self->submitted->purwvkbde)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PURWBKBDE]);

  if (current->hint != self->submitted->hint)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HINT]);

  g_signal_emit (self, signals[DONE], 0);

  g_object_thaw_notify (G_OBJECT (self));
}


static void
handle_unavailable (void                       *data,
                    struct zwp_input_method_v2 *zwp_input_method_v2)
{
  g_warning ("Input method unavailable");
}


static const struct zwp_input_method_v2_listener input_method_listener = {
  .activate = handle_activate,
  .deactivate = handle_deactivate,
  .surrounding_text = handle_surrounding_text,
  .text_change_cause = handle_text_change_cause,
  .content_type = handle_content_type,
  .done = handle_done,
  .unavailable = handle_unavailable,
};


static void
wvkbd_input_method_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (object);

  switch (property_id) {
  case PROP_SEAT:
    self->seat = g_value_get_pointer (value);
    break;
  case PROP_MANAGER:
    self->manager = g_value_get_pointer (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
wvkbd_input_method_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD (object);

  switch (property_id) {
  case PROP_ACTIVE:
    g_value_set_boolean (value, self->submitted->active);
    break;
  case PROP_SURROUNDING_TEXT:
    g_value_set_string (value, self->submitted->surrounding_text);
    break;
  case PROP_TEXT_CHANGE_CAUSE:
    g_value_set_enum (value, self->submitted->text_change_cause);
    break;
  case PROP_PURWBKBDE:
    g_value_set_enum (value, self->submitted->purwvkbde);
    break;
  case PROP_HINT:
    g_value_set_enum (value, self->submitted->hint);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}


static void
wvkbd_input_method_constructed (GObject *object)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD(object);

  g_assert (self->seat);
  g_assert (self->manager);
  g_assert (self->seat && self->manager);

  self->input_method = zwp_input_method_manager_v2_get_input_method (self->manager,
                                                                       self->seat);
  zwp_input_method_v2_add_listener (self->input_method, &input_method_listener, self);

  G_OBJECT_CLASS (wvkbd_input_method_parent_class)->constructed (object);
}

static void
wvkbd_input_method_finalize (GObject *object)
{
  WvkbdInputMethod *self = WBKBD_INPUT_METHOD(object);

  g_clear_pointer (&self->submitted, wvkbd_im_state_free);
  g_clear_pointer (&self->pending, wvkbd_im_state_free);
  g_clear_pointer (&self->input_method, zwp_input_method_v2_destroy);

  G_OBJECT_CLASS (wvkbd_input_method_parent_class)->finalize (object);
}


static void
wvkbd_input_method_class_init (WvkbdInputMethodClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = wvkbd_input_method_constructed;
  object_class->finalize = wvkbd_input_method_finalize;
  object_class->set_property = wvkbd_input_method_set_property;
  object_class->get_property = wvkbd_input_method_get_property;

  /**
   * WvkbdInputMethod:manager:
   *
   * A zwp_input_method_v2_manager.
   */
  props[PROP_MANAGER] =
    g_param_spec_pointer ("manager", "", "",
                          G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  /**
   * WvkbdInputMethod:seat:
   *
   * A wl_seat.
   */
  props[PROP_SEAT] =
    g_param_spec_pointer ("seat", "", "",
                          G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  /**
   * WvkbdInputMethod:active:
   *
   * Whether the input method is active. See activate/deactive in
   * input-method-unstable-v2.xml.
   */
  props[PROP_ACTIVE] =
    g_param_spec_boolean ("active", "", "",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS);
  /**
   * WvkbdInputMethod:surrounding_text:
   *
   * The applied surrounding_text.
   */
  props[PROP_SURROUNDING_TEXT] =
    g_param_spec_string ("surrounding-text", "", "",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);
  /**
   * WvkbdInputMethod:text-change-cause:
   *
   * The applied text change cause.
   */
  props[PROP_TEXT_CHANGE_CAUSE] =
    g_param_spec_enum ("text-change-cause", "", "",
                       WBKBD_TYPE_INPUT_METHOD_TEXT_CHANGE_CAUSE,
                       WBKBD_INPUT_METHOD_TEXT_CHANGE_CAUSE_IM,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);
  /**
   * WvkbdInputMethod:purwvkbde:
   *
   * The applied input purwvkbde.
   */
  props[PROP_PURWBKBDE] =
    g_param_spec_enum ("purwvkbde", "", "",
                       WBKBD_TYPE_INPUT_METHOD_PURWBKBDE,
                       WBKBD_INPUT_METHOD_PURWBKBDE_NORMAL,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);
  /**
   * WvkbdInputMethod:hint:
   *
   * The applied input hint.
   */
  props[PROP_HINT] =
    g_param_spec_enum ("hint", "", "",
                       WBKBD_TYPE_INPUT_METHOD_HINT,
                       WBKBD_INPUT_METHOD_HINT_NONE,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

  /**
   * WvkbdInputMethod::done:
   *
   * The done signal is sent when the state changes sent by the comwvkbditor
   * should be applied. The `active`, `surrounding-text`, `text-change-cause`,
   * `purwvkbde` and `hint` properties are then guaranteed to have the values
   * sent by the comwvkbditor.
   */
  signals[DONE] =
    g_signal_new ("done",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
  /**
   * WvkbdInputMethod::pending-changed:
   * @im: The input method
   * @pending_state: The new pending state
   *
   * The pending state changed. Tracking pending state changes is only
   * useful for debugging as only `applied` state matters for the OSK.
   */
  signals[PENDING_CHANGED] =
    g_signal_new ("pending-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
}


static void
wvkbd_input_method_init (WvkbdInputMethod *self)
{
  self->pending = g_new0 (WvkbdImState, 1);
  self->submitted = g_new0 (WvkbdImState, 1);
}


WvkbdInputMethod *
wvkbd_input_method_new (gpointer manager, gpointer seat)
{
  g_assert (seat && manager);
  return g_object_new (WBKBD_TYPE_INPUT_METHOD,
                       "manager", manager,
                       "seat", seat,
                       NULL);
}

gboolean
wvkbd_input_method_get_active (WvkbdInputMethod *self)
{
  g_return_val_if_fail (WBKBD_IS_INPUT_METHOD (self), FALSE);

  return self->submitted->active;
}

WvkbdInputMethodTextChangeCause
wvkbd_input_method_get_text_change_cause (WvkbdInputMethod *self)
{
  g_return_val_if_fail (WBKBD_IS_INPUT_METHOD (self),
                        WBKBD_INPUT_METHOD_TEXT_CHANGE_CAUSE_IM);

  return self->submitted->text_change_cause;
}

WvkbdInputMethodPurwvkbde
wvkbd_input_method_get_purwvkbde (WvkbdInputMethod *self)
{
  g_return_val_if_fail (WBKBD_IS_INPUT_METHOD (self), WBKBD_INPUT_METHOD_PURWBKBDE_NORMAL);

  return self->submitted->purwvkbde;
}

WvkbdInputMethodHint
wvkbd_input_method_get_hint (WvkbdInputMethod *self)
{
  g_return_val_if_fail (WBKBD_IS_INPUT_METHOD (self), WBKBD_INPUT_METHOD_HINT_NONE);

  return self->submitted->hint;
}

const char *
wvkbd_input_method_get_surrounding_text (WvkbdInputMethod *self, guint *anchor, guint *cursor)
{
  g_return_val_if_fail (WBKBD_IS_INPUT_METHOD (self), NULL);

  if (anchor)
    *anchor = self->submitted->anchor;

  if (cursor)
    *cursor = self->submitted->cursor;

  return self->submitted->surrounding_text;
}

guint
wvkbd_input_method_get_serial (WvkbdInputMethod *self)
{
  g_return_val_if_fail (WBKBD_IS_INPUT_METHOD (self), 0);

  return self->serial;
}

/**
 * wvkbd_input_method_send_string:
 * @self: The input method
 * @string: The text to send
 * @commit: Whether to invoke `commit` request as well
 *
 * This sends the given text via a `commit_string` request.
 */
void
wvkbd_input_method_send_string (WvkbdInputMethod *self, const char *string, gboolean commit)
{
  zwp_input_method_v2_commit_string (self->input_method, string);
  if (commit)
    wvkbd_input_method_commit (self);
}

/**
 * wvkbd_input_method_send_preedit:
 * @self: The input method
 * @preedit: The preedit to send
 * @cstart: The start of the cursor
 * @cend: The end of the cursor
 * @commit: Whether to invoke `commit` request as well
 *
 * This sends the given text via a `set_preedit_string` request.
 */
void
wvkbd_input_method_send_preedit (WvkbdInputMethod *self, const char *preedit,
                               guint cstart, guint cend, gboolean commit)
{
  zwp_input_method_v2_set_preedit_string (self->input_method, preedit, cstart, cend);
  if (commit)
    wvkbd_input_method_commit (self);
}

/**
 * wvkbd_input_method_delete_surrounding_text:
 * @self: The input method
 * @before_length: Number of bytes before cursor to delete
 * @after_length: Number of bytes after cursor to delete
 * @commit: Whether to invoke `commit` request as well
 *
 * This deletes text around the cursor using the `delete_surrounding_text` request.
 */
void
wvkbd_input_method_delete_surrounding_text (WvkbdInputMethod *self,
                                          guint before_length,
                                          guint after_length,
                                          gboolean commit)
{
  zwp_input_method_v2_delete_surrounding_text (self->input_method, before_length, after_length);
  if (commit)
    wvkbd_input_method_commit (self);
}

/**
 * wvkbd_input_method_commit:
 * @self: The input method
 *
 * Sends a `commit` request to the comwvkbditor so that any pending
 * `commit_string`, `set_preedit_string` and `delete_surrounding_text`.
 * changes get applied.
 */
void
wvkbd_input_method_commit (WvkbdInputMethod *self)
{
  zwp_input_method_v2_commit (self->input_method, self->serial);
}
