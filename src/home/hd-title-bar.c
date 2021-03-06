/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/* This class handles drawing of things in the title bar right at the top
 * of the screen - currently just top-left buttons (which have to animate)
 */

#include "tidy/tidy-sub-texture.h"

#include "hd-title-bar.h"
#include "hd-clutter-cache.h"
#include "mb/hd-app.h"
#include "mb/hd-comp-mgr.h"
#include "mb/hd-decor.h"
#include "mb/hd-theme.h"
#include "hd-render-manager.h"
#include "hd-gtk-utils.h"
#include "hd-gtk-style.h"
#include "hd-transition.h"
#include "hd-util.h"
#include "hd-task-navigator.h"

#include <matchbox/theme-engines/mb-wm-theme-png.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>
#include <glib/gi18n.h>
#include <math.h>

enum
{
  BTN_BG_ATTACHED = 0,
  BTN_BG_LEFT_END,
  BTN_BG_RIGHT_END,
  BTN_BG_LEFT_PRESSED,
  BTN_BG_LEFT_ATTACHED_PRESSED,
  BTN_BG_RIGHT_PRESSED,
  BTN_SEPARATOR_LEFT,
  BTN_SEPARATOR_STATUS,
  BTN_SEPARATOR_RIGHT,
  BTN_SWITCHER,
  BTN_SWITCHER_HIGHLIGHT,
  BTN_SWITCHER_PRESSED,
  BTN_LAUNCHER,
  BTN_LAUNCHER_PRESSED,
  BTN_BACK,
  BTN_BACK_PRESSED,
  BTN_CLOSE,
  BTN_CLOSE_PRESSED,
  BTN_MENU,
  BTN_DONE,
  BTN_MENU_INDICATOR,
  BTN_COUNT
};

static const char *const BTN_FILENAMES[BTN_COUNT] = {
    HD_THEME_IMG_LEFT_ATTACHED,
    HD_THEME_IMG_LEFT_END,
    HD_THEME_IMG_RIGHT_END,
    HD_THEME_IMG_LEFT_PRESSED,
    HD_THEME_IMG_LEFT_ATTACHED_PRESSED,
    HD_THEME_IMG_RIGHT_PRESSED,
    HD_THEME_IMG_SEPARATOR,
    HD_THEME_IMG_SEPARATOR,
    HD_THEME_IMG_SEPARATOR,
    HD_THEME_IMG_TASK_SWITCHER,
    HD_THEME_IMG_TASK_SWITCHER_HIGHLIGHT,
    HD_THEME_IMG_TASK_SWITCHER_PRESSED,
    HD_THEME_IMG_TASK_LAUNCHER,
    HD_THEME_IMG_TASK_LAUNCHER_PRESSED,
    HD_THEME_IMG_BACK,
    HD_THEME_IMG_BACK_PRESSED,
    HD_THEME_IMG_CLOSE,
    HD_THEME_IMG_CLOSE_PRESSED,
    HD_THEME_IMG_EDIT_ICON,
    NULL, // BTN_DONE
    HD_THEME_IMG_MENU_INDICATOR,
};

static const char *const BTN_LABELS[BTN_COUNT * 2] = {
    NULL, NULL,  // BTN_BG_ATTACHED = 0,
    NULL, NULL,  // BTN_BG_LEFT_END,
    NULL, NULL,  // BTN_BG_RIGHT_END,
    NULL, NULL,  // BTN_BG_LEFT_PRESSED,
    NULL, NULL,  // BTN_BG_LEFT_ATTACHED_PRESSED,
    NULL, NULL,  // BTN_BG_RIGHT_PRESSED,
    NULL, NULL,  // BTN_SEPARATOR_LEFT,
    NULL, NULL,  // BTN_SEPARATOR_STATUS,
    NULL, NULL,  // BTN_SEPARATOR_RIGHT,
    NULL, NULL,  // BTN_SWITCHER,
    NULL, NULL,  // BTN_SWITCHER_HIGHLIGHT,
    NULL, NULL,  // BTN_SWITCHER_PRESSED,
    NULL, NULL,  // BTN_LAUNCHER,
    NULL, NULL,  // BTN_LAUNCHER_PRESSED,
    NULL, NULL,  // BTN_BACK,
    NULL, NULL,  // BTN_BACK_PRESSED,
    NULL, NULL,  // BTN_CLOSE,
    NULL, NULL,  // BTN_CLOSE_PRESSED,
    NULL, NULL,  // BTN_MENU
    "hildon-libs", "wdgt_bd_done", // BTN_DONE
    NULL, NULL,  // BTN_MENU_INDICATOR
};

enum {
  BTN_FLAG_ALIGN_RIGHT = 1,
  BTN_FLAG_ALIGN_LEFT = 2,

  /* We try and set sizes for what we can, because if we get images
     * we couldn't load, they won't show properly otherwise */
  BTN_FLAG_SET_SIZE = 4,

  /* should this be a member of the 'foreground' group, so it can
   * be pulled out above the blur if needed */
  BTN_FLAG_FOREGROUND = 8,

  /* Should the button be centred in the area allocated for it
   * (or aligned to the edge). This is used for button images,
   * vs. the button backgrounds which must be pulled right to
   * the edge. */
  BTN_FLAG_CENTRE = 16,
};

static const gboolean BTN_FLAGS[BTN_COUNT] = {
   BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND, // BTN_BG_ATTACHED = 0,
   BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND, // BTN_BG_LEFT_END,
   BTN_FLAG_ALIGN_RIGHT | BTN_FLAG_SET_SIZE,  // BTN_BG_RIGHT_END,
   BTN_FLAG_ALIGN_LEFT | BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND, // BTN_BG_LEFT_PRESSED,
   BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND, // BTN_BG_LEFT_ATTACHED_PRESSED,
   BTN_FLAG_ALIGN_RIGHT | BTN_FLAG_SET_SIZE,  // BTN_BG_RIGHT_PRESSED,
   BTN_FLAG_FOREGROUND, // BTN_SEPARATOR_LEFT,
   0, // BTN_SEPARATOR_STATUS,
   0, // BTN_SEPARATOR_RIGHT,
   BTN_FLAG_ALIGN_LEFT | BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND | BTN_FLAG_CENTRE, // BTN_SWITCHER,
   BTN_FLAG_ALIGN_LEFT | BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND | BTN_FLAG_CENTRE, // BTN_SWITCHER_HIGHLIGHT,
   BTN_FLAG_ALIGN_LEFT | BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND | BTN_FLAG_CENTRE, // BTN_SWITCHER_PRESSED,
   BTN_FLAG_ALIGN_LEFT | BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND | BTN_FLAG_CENTRE, // BTN_LAUNCHER,
   BTN_FLAG_ALIGN_LEFT | BTN_FLAG_SET_SIZE | BTN_FLAG_FOREGROUND | BTN_FLAG_CENTRE, // BTN_LAUNCHER_PRESSED,
   BTN_FLAG_ALIGN_RIGHT | BTN_FLAG_SET_SIZE | BTN_FLAG_CENTRE,  // BTN_BACK,
   BTN_FLAG_ALIGN_RIGHT | BTN_FLAG_SET_SIZE | BTN_FLAG_CENTRE,  // BTN_BACK_PRESSED,
   BTN_FLAG_ALIGN_RIGHT | BTN_FLAG_SET_SIZE | BTN_FLAG_CENTRE,  // BTN_CLOSE,
   BTN_FLAG_ALIGN_RIGHT | BTN_FLAG_SET_SIZE | BTN_FLAG_CENTRE,  // BTN_CLOSE_PRESSED,
   0, // BTN_MENU,
   BTN_FLAG_ALIGN_RIGHT, // BTN_DONE,
   0, // BTN_MENU_INDICATOR,
};

struct _HdTitleBarPrivate
{
  /* All the images we need for buttons */
  ClutterActor          *buttons[BTN_COUNT];
  /* Container to hold 'foreground' items that might need to be
   * shown above the blur (if the dialog titles are blurred) */
  ClutterGroup          *foreground;

  /* Stretched image for the title background */
  ClutterActor          *title_bg;
  ClutterText           *title;
  /* The title to be used when in HDRM_STATE_LOADING */
  gchar                 *loading_title;
  /* Pulsing animation for switcher */
  ClutterTimeline       *switcher_timeline;
  /* progress indicator */
  ClutterTimeline       *progress_timeline;
  ClutterActor          *progress_texture;

  /* The current state set by set_state = next state we will go to once the
   * idle callback is called */
  HdTitleBarVisEnum      state;
  /* The actual state we are in right at this second */
  HdTitleBarVisEnum      current_state;
  /* Do we show the menu indicator? This isn't included in state because
   * it could be unwillingly cleared by calls to set_state */
  gboolean               has_menu_indicator;
  /* GSource id of the delayed hd_title_bar_update_idle() callback. */
  guint                  update_title_bar;
};

/* HdHomeThemeButtonBack, MBWMDecorButtonClose */

/* ------------------------------------------------------------------------- */

G_DEFINE_TYPE (HdTitleBar, hd_title_bar, CLUTTER_TYPE_GROUP);
#define HD_TITLE_BAR_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_TITLE_BAR, HdTitleBarPrivate))

static void
hd_title_bar_stage_allocation_changed (HdTitleBar *bar, GParamSpec *unused,
                                       ClutterActor *stage);
static void
hd_title_bar_add_left_signals(HdTitleBar *bar, ClutterActor *actor);
static void
hd_title_bar_add_right_signals(HdTitleBar *bar, ClutterActor *actor);
static void
on_switcher_timeline_new_frame(ClutterTimeline *timeline,
                               guint msecs, HdTitleBar *bar);
static void
hd_title_bar_set_full_width(HdTitleBar *bar, gboolean full_size);
static void hd_title_bar_set_button_positions(HdTitleBar *bar);
static void hd_title_bar_set_title (HdTitleBar *bar,
                                    const char *title,
                                    gboolean has_markup,
                                    gboolean waiting);
/* ------------------------------------------------------------------------- */

/* One pulse is breathe in or breathe out.  The animation takes two
 * cycles (2 times two pulses) and we leave the button breathe held. */
#define HD_TITLE_BAR_SWITCHER_PULSE_DURATION 1000
#define HD_TITLE_BAR_SWITCHER_PULSE_NPULSES  5

/* margin to left of the app title */
#define HD_TITLE_BAR_TITLE_MARGIN 24
#define HD_TITLE_BAR_TEXT_MARGIN 24
#define HD_TITLE_BAR_TITLE_MARGIN_SMALL 8

#define HD_TITLE_BAR_TITLE_FONT "SystemFont"

enum
{
  CLICKED_TOP_LEFT,
  PRESS_TOP_LEFT,
  LEAVE_TOP_LEFT,
  CLICKED_TOP_RIGHT,
  PRESS_TOP_RIGHT,
  LEAVE_TOP_RIGHT,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0, };

/* ------------------------------------------------------------------------- */

static void
hd_title_bar_init (HdTitleBar *bar)
{
  ClutterActor *actor = CLUTTER_ACTOR(bar);
  ClutterColor title_color;
  ClutterGeometry title_bg_size = {0,0,112,56}; /* size of theme image */
  gchar *font_name;
  HdTitleBarPrivate *priv = bar->priv = HD_TITLE_BAR_GET_PRIVATE(bar);
  gint i;

  priv->state = HDTB_VIS_NONE;

  /* Explicitly enable maemo-specific visibility detection to cut down
   * spurious paints */
#ifdef UPSTREAM_DISABLED
  clutter_actor_set_visibility_detect(actor, TRUE);
#endif
  clutter_actor_set_position(actor, 0, 0);
  clutter_actor_set_size(actor,
         hd_comp_mgr_get_current_screen_width (), HD_COMP_MGR_TOP_MARGIN);
  clutter_actor_set_name(CLUTTER_ACTOR(actor), "HdTitleBar");

  hd_gtk_style_resolve_logical_color(&title_color, "TitleTextColor");
  font_name = hd_gtk_style_resolve_logical_font(HD_TITLE_BAR_TITLE_FONT);

  priv->foreground = CLUTTER_GROUP(clutter_group_new());
  /* Explicitly enable maemo-specific visibility detection to cut down
   * spurious paints */
#ifdef UPSTREAM_DISABLED
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->foreground), TRUE);
#endif
  clutter_actor_set_name(CLUTTER_ACTOR(priv->foreground),
      "HdTitleBar::foreground");
  clutter_actor_add_child(CLUTTER_ACTOR(bar), CLUTTER_ACTOR(priv->foreground));

  /* Title background */
  priv->title_bg = hd_clutter_cache_get_sub_texture(
      HD_THEME_IMG_TITLE_BAR, TRUE, &title_bg_size);
  if (TIDY_IS_SUB_TEXTURE(priv->title_bg))
    tidy_sub_texture_set_tiled(TIDY_SUB_TEXTURE(priv->title_bg), TRUE);
  clutter_actor_add_child(CLUTTER_ACTOR(bar), CLUTTER_ACTOR(priv->title_bg));

  hd_title_bar_add_left_signals(bar, priv->title_bg);
  clutter_actor_set_reactive (priv->title_bg, FALSE);

  /* Load every button we need */
  for (i=0;i<BTN_COUNT;i++)
    {
      if (BTN_FILENAMES[i])
        {
          priv->buttons[i] = hd_clutter_cache_get_texture(BTN_FILENAMES[i], TRUE);
        }
      else
        {
          ClutterActor *label;
          gfloat w, h;

          label = clutter_text_new_full(
                      font_name,
                      dgettext (BTN_LABELS[2 * i], BTN_LABELS[2 * i + 1]),
                      &title_color);
          clutter_text_set_use_markup(CLUTTER_TEXT(label), TRUE);

          priv->buttons[i] = clutter_group_new();
          clutter_actor_add_child (CLUTTER_ACTOR(priv->buttons[i]), label);

          clutter_actor_get_size(CLUTTER_ACTOR(label), &w, &h);
          w += (2 * HD_TITLE_BAR_TEXT_MARGIN);
          clutter_actor_set_size(CLUTTER_ACTOR(priv->buttons[i]),
                                 w,
                                 HD_COMP_MGR_TOP_MARGIN);
          clutter_actor_set_position (label, HD_TITLE_BAR_TEXT_MARGIN,
                                      (HD_COMP_MGR_TOP_MARGIN - h) / 2);
        }

      /* Explicitly enable maemo-specific visibility detection to cut down
       * spurious paints */
#ifdef UPSTREAM_DISABLED
      clutter_actor_set_visibility_detect(
          CLUTTER_ACTOR(priv->buttons[i]), TRUE);
#endif
      /* The position of left-aligned buttons is (0, 0) by default,
       * and right aligned ones will be placed on the initial
       * stage_allocation_changed(). */
      if (BTN_FLAGS[i] & BTN_FLAG_FOREGROUND)
        {
          clutter_actor_add_child(CLUTTER_ACTOR(priv->foreground),
                                  priv->buttons[i]);
        }
      else
        clutter_actor_add_child(CLUTTER_ACTOR(bar), priv->buttons[i]);
      clutter_actor_hide(priv->buttons[i]);

      if (BTN_FLAGS[i] & BTN_FLAG_SET_SIZE)
        {
          if (!(BTN_FLAGS[i] & BTN_FLAG_ALIGN_RIGHT))
            clutter_actor_set_size(priv->buttons[i],
              HD_COMP_MGR_TOP_LEFT_BTN_WIDTH,
              HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT);
          else
            clutter_actor_set_size(priv->buttons[i],
              HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH,
              HD_COMP_MGR_TOP_RIGHT_BTN_HEIGHT);
        }
    }

  /* TODO: setup BTN_SWITCHER_HIGHLIGHT for adding here... */

  hd_title_bar_add_left_signals(bar, priv->buttons[BTN_SWITCHER]);
  hd_title_bar_add_left_signals(bar, priv->buttons[BTN_LAUNCHER]);
  hd_title_bar_add_right_signals(bar, priv->buttons[BTN_BACK]);

  hd_title_bar_add_left_signals(bar, priv->buttons[BTN_MENU]);
  hd_title_bar_add_right_signals(bar, priv->buttons[BTN_DONE]);

  /* Create the title */
  priv->title = CLUTTER_TEXT(clutter_text_new());
  /* Explicitly enable maemo-specific visibility detection to cut down
   * spurious paints */
#ifdef UPSTREAM_DISABLED
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->title), TRUE);
#endif
  clutter_text_set_color(priv->title, &title_color);
  /* do not call clutter_text_set_use_markup() until we know whether
   * or not the text has markup */
  clutter_actor_add_child(CLUTTER_ACTOR(bar), CLUTTER_ACTOR(priv->title));
  clutter_actor_hide(CLUTTER_ACTOR(priv->title));

  /* Make sure the 'foreground' is in the right place */
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->foreground));

  /* Create timeline animation */
  priv->switcher_timeline =
    clutter_timeline_new(HD_TITLE_BAR_SWITCHER_PULSE_DURATION
                                      * HD_TITLE_BAR_SWITCHER_PULSE_NPULSES);

  g_signal_connect (priv->switcher_timeline, "new-frame",
                        G_CALLBACK (on_switcher_timeline_new_frame), bar);

  /* Create progress indicator */
  {
    ClutterGeometry progress_geo =
        {0, 0, HD_THEME_IMG_PROGRESS_SIZE, HD_THEME_IMG_PROGRESS_SIZE};
    priv->progress_texture = hd_clutter_cache_get_sub_texture(
                                                    HD_THEME_IMG_PROGRESS,
                                                    TRUE,
                                                    &progress_geo);
    clutter_actor_add_child(CLUTTER_ACTOR(bar), priv->progress_texture);
#ifdef UPSTREAM_DISABLED
    clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->progress_texture),
                                        TRUE);
#endif
    clutter_actor_set_size(priv->progress_texture,
                HD_THEME_IMG_PROGRESS_SIZE, HD_THEME_IMG_PROGRESS_SIZE);
    clutter_actor_hide(priv->progress_texture);
    /* Create the timeline for animation */
    priv->progress_timeline = g_object_ref(
        clutter_timeline_new(1000 * HD_THEME_IMG_PROGRESS_FRAMES /
                             HD_THEME_IMG_PROGRESS_FPS));
    clutter_timeline_set_repeat_count(priv->progress_timeline, -1);
    g_signal_connect (priv->progress_timeline, "new-frame",
                      G_CALLBACK (on_decor_progress_timeline_new_frame),
                      priv->progress_texture);
  }

  g_signal_connect_swapped(clutter_stage_get_default(), "notify::allocation",
                           G_CALLBACK(hd_title_bar_stage_allocation_changed),
                           bar);

  g_free(font_name);
}

/*
 * Returns TRUE iff the status area is visible.
 * There are several ways the status area can be hidden,
 * and this function attempts to keep track of all of
 * them in the same place.
 */
static gboolean
status_area_is_visible (void)
{
  ClutterActor *status_area = hd_render_manager_get_status_area();
  MBWindowManagerClient *sa_client = hd_render_manager_get_status_area_client();
  return status_area && sa_client &&
    clutter_actor_is_visible (status_area) &&
    sa_client->frame_geometry.x >= 0 &&
    sa_client->frame_geometry.y >= 0;
  /*
   * It can also be invisible by being behind something, but in
   * that case clutter_actor_is_visible should be returning
   * false anyway.
   */
}

gint
hd_title_bar_get_button_width(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv = bar->priv;

  if (priv->current_state & HDTB_VIS_SMALL_BUTTONS)
    return HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH_SMALL;
  else
    /* TODO someday, in some cases this should return the TOP_RIGHT_BIN_WIDTH */
    return HD_COMP_MGR_TOP_LEFT_BTN_WIDTH;
}

static void
hd_title_bar_dispose (GObject *obj)
{
  HdTitleBarPrivate *priv = HD_TITLE_BAR(obj)->priv;
  gint i;

  if (priv->loading_title)
    {
      g_free(priv->loading_title);
      priv->loading_title = 0;
    }
  if (priv->progress_timeline)
    clutter_timeline_stop(priv->progress_timeline);
  for (i=0;i<BTN_COUNT;i++)
    if (priv->buttons[i])
      {
        clutter_actor_destroy(priv->buttons[i]);
        priv->buttons[i] = 0;
      }
  if (priv->progress_texture)
    {
      clutter_actor_destroy(priv->progress_texture);
      priv->progress_texture = 0;
    }
  if (priv->foreground)
    {
      clutter_actor_destroy(CLUTTER_ACTOR(priv->foreground));
      priv->foreground = 0;
    }
  g_object_unref(priv->progress_timeline);
  /* TODO: unref others - or do we care as we are a singleton? */

  G_OBJECT_CLASS (hd_title_bar_parent_class)->dispose (obj);
}

static void
hd_title_bar_class_init (HdTitleBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdTitleBarPrivate));

  gobject_class->dispose = hd_title_bar_dispose;

  signals[CLICKED_TOP_LEFT] =
      g_signal_new ("clicked-top-left",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
  signals[PRESS_TOP_LEFT] =
        g_signal_new ("press-top-left",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
  signals[LEAVE_TOP_LEFT] =
        g_signal_new ("leave-top-left",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
  signals[CLICKED_TOP_RIGHT] =
      g_signal_new ("clicked-top-right",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
  signals[PRESS_TOP_RIGHT] =
        g_signal_new ("press-top-right",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
  signals[LEAVE_TOP_RIGHT] =
        g_signal_new ("leave-top-right",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static void hd_title_bar_set_state_real(HdTitleBar *bar,
                                        HdTitleBarVisEnum button)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  priv->state = button;

  /* The state may have changed to use/not use small
   * buttons - in check case we must update positions */
  hd_title_bar_set_button_positions(bar);

  /* if a button isn't in the top-left or right, we want to make
   * sure we don't display the pressed state */
  if (!(button & HDTB_VIS_BTN_LEFT_MASK))
    {
      clutter_actor_hide(priv->buttons[BTN_BG_LEFT_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_BG_LEFT_ATTACHED_PRESSED]);
    }
  if (!(button & HDTB_VIS_BTN_RIGHT_MASK))
    clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_PRESSED]);


  if (button & HDTB_VIS_BTN_LAUNCHER)
    {
      clutter_actor_show(priv->buttons[BTN_LAUNCHER]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_LAUNCHER]);
      clutter_actor_hide(priv->buttons[BTN_LAUNCHER_PRESSED]);
    }

  if (button & HDTB_VIS_BTN_SWITCHER)
    {
      clutter_actor_show(priv->buttons[BTN_SWITCHER]);
      clutter_actor_show(priv->buttons[BTN_SWITCHER_HIGHLIGHT]);
      if (!(button & HDTB_VIS_BTN_SWITCHER_HIGHLIGHT))
        /* set_switcher_pulse() doesn't want it to be highlighted. */
        clutter_actor_set_opacity(priv->buttons[BTN_SWITCHER_HIGHLIGHT], 0);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_SWITCHER]);
      clutter_actor_hide(priv->buttons[BTN_SWITCHER_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_SWITCHER_HIGHLIGHT]);
    }

  if (button & HDTB_VIS_BTN_BACK)
    {
      clutter_actor_show(priv->buttons[BTN_BACK]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_BACK]);
      clutter_actor_hide(priv->buttons[BTN_BACK_PRESSED]);
    }

  if (button & HDTB_VIS_BTN_CLOSE)
    {
      clutter_actor_show(priv->buttons[BTN_CLOSE]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_CLOSE]);
      clutter_actor_hide(priv->buttons[BTN_CLOSE_PRESSED]);
    }

  if (button & HDTB_VIS_BTN_MENU)
    {
      clutter_actor_show(priv->buttons[BTN_MENU]);
      clutter_actor_show(priv->buttons[BTN_DONE]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_MENU]);
      clutter_actor_hide(priv->buttons[BTN_DONE]);
    }

  hd_title_bar_set_full_width(bar, button & HDTB_VIS_FULL_WIDTH);

  if (button & HDTB_VIS_FOREGROUND)
    {
      ClutterActor *front = CLUTTER_ACTOR(hd_render_manager_get_front_group());
      if (clutter_actor_get_parent(CLUTTER_ACTOR(priv->foreground)) != front)
        {
          clutter_actor_reparent(CLUTTER_ACTOR(priv->foreground),
                                 CLUTTER_ACTOR(front));
          clutter_actor_raise_top(CLUTTER_ACTOR(priv->foreground));
        }
    }
  else
    {
      if (clutter_actor_get_parent(CLUTTER_ACTOR(priv->foreground)) !=
          CLUTTER_ACTOR(bar))
        {
          clutter_actor_reparent(CLUTTER_ACTOR(priv->foreground),
                                 CLUTTER_ACTOR(bar));
          clutter_actor_raise_top(CLUTTER_ACTOR(priv->foreground));
          // we must raise status area above this, or it gets dimmed by it
          if (hd_render_manager_get_status_area())
            clutter_actor_raise_top(hd_render_manager_get_status_area());
        }
    }
}

#ifndef G_DEBUG_DISABLE
# define PRINT_STATE(state, this)       \
  if (((state) & this) == this)         \
    g_debug("  " G_STRINGIFY(this))
void hd_title_bar_print_state(HdTitleBar *bar, gboolean current)
{
  HdTitleBarVisEnum state;

  if (current)
    {
      state = bar->priv->current_state;
      g_debug("HdTitleBar::current_state:");
    }
  else
    {
      state = bar->priv->state;
      g_debug("HdTitleBar::state:");
    }

  PRINT_STATE(state, HDTB_VIS_BTN_LAUNCHER);
  PRINT_STATE(state, HDTB_VIS_BTN_SWITCHER);
  PRINT_STATE(state, HDTB_VIS_BTN_MENU);
  PRINT_STATE(state, HDTB_VIS_BTN_BACK);
  PRINT_STATE(state, HDTB_VIS_BTN_CLOSE);
  PRINT_STATE(state, HDTB_VIS_BTN_DONE);
  PRINT_STATE(state, HDTB_VIS_FULL_WIDTH);
  PRINT_STATE(state, HDTB_VIS_BTN_SWITCHER_HIGHLIGHT);
  PRINT_STATE(state, HDTB_VIS_FOREGROUND);
  PRINT_STATE(state, HDTB_VIS_SMALL_BUTTONS);
  PRINT_STATE(state, HDTB_VIS_BTN_LEFT_MASK);
  PRINT_STATE(state, HDTB_VIS_BTN_RIGHT_MASK);
}
# undef PRINT_STATE
#endif

void hd_title_bar_set_state(HdTitleBar *bar,
                            HdTitleBarVisEnum button)
{
  if (!HD_IS_TITLE_BAR(bar))
    return;

  bar->priv->state = button;
  hd_title_bar_update(bar);
}

HdTitleBarVisEnum hd_title_bar_get_state(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return 0;
  priv = bar->priv;

  return priv->state;
}

void
hd_title_bar_left_pressed(HdTitleBar *bar, gboolean pressed)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (pressed)
    {
      if (clutter_actor_is_visible(priv->buttons[BTN_BG_ATTACHED]))
        {
          clutter_actor_hide(priv->buttons[BTN_BG_LEFT_PRESSED]);
          clutter_actor_show(priv->buttons[BTN_BG_LEFT_ATTACHED_PRESSED]);
        }
      else
        {
          clutter_actor_show(priv->buttons[BTN_BG_LEFT_PRESSED]);
          clutter_actor_hide(priv->buttons[BTN_BG_LEFT_ATTACHED_PRESSED]);
        }
      if (priv->state & HDTB_VIS_BTN_LAUNCHER)
        clutter_actor_show(priv->buttons[BTN_LAUNCHER_PRESSED]);
      if (priv->state & HDTB_VIS_BTN_SWITCHER)
        clutter_actor_show(priv->buttons[BTN_SWITCHER_PRESSED]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_BG_LEFT_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_BG_LEFT_ATTACHED_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_LAUNCHER_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_SWITCHER_PRESSED]);
    }
}

static void
hd_title_bar_set_full_width(HdTitleBar *bar, gboolean full_size)
{
  HdTitleBarPrivate *priv;
  ClutterActor *status_area;

  if (!HD_IS_TITLE_BAR(bar))
    return;

  priv = bar->priv;

  status_area = hd_render_manager_get_status_area();
  HDRMStateEnum hd_render_state = hd_render_manager_get_state();

  hd_title_bar_set_button_positions(bar);

  if (full_size)
    {
      /* If we want the full-size opaque background */
      clutter_actor_hide(priv->buttons[BTN_BG_LEFT_END]);
      clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_END]);
      clutter_actor_hide(priv->buttons[BTN_BG_ATTACHED]);
      /* set up background image */
      clutter_actor_show(priv->title_bg);
      clutter_actor_set_width(priv->title_bg,
          hd_comp_mgr_get_current_screen_width ());

      /* In portrait desktop edit mode move the BTN_MENU image slightly off-screen.
       * Then the width of the BTN_MENU will be almost equal to
       * HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH_SMALL. */
      if (hd_render_state == HDRM_STATE_HOME_EDIT_PORTRAIT)
        {
          clutter_actor_set_x(priv->buttons[BTN_MENU], (HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH_SMALL -
                              HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH) / 2);
        }
      else
        {
          clutter_actor_set_x(priv->buttons[BTN_MENU], 0);
        }

      /* set up separator positions */
      if (priv->state & HDTB_VIS_BTN_LEFT_MASK)
        {
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_LEFT]);
          if (clutter_actor_is_visible (priv->buttons[BTN_MENU]))
            {
              /* In portrait mode apply the offset. Thanks to that the width of the BTN_MENU
               * will be equal to HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH_SMALL. */
              if (hd_render_state == HDRM_STATE_HOME_EDIT_PORTRAIT)
                {
                  int offset = HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH -
                               HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH_SMALL;

                  clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_LEFT],
                                      clutter_actor_get_width(priv->buttons[BTN_MENU]) -
                                      clutter_actor_get_width(priv->buttons[BTN_SEPARATOR_LEFT]) -
                                      offset + 2);
                }
              else
                {
                  clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_LEFT],
                                      clutter_actor_get_width(priv->buttons[BTN_MENU]) -
                                      clutter_actor_get_width(priv->buttons[BTN_SEPARATOR_LEFT]) + 2);
                }
            }
          else
            clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_LEFT],
                                hd_title_bar_get_button_width(bar) - 
                                clutter_actor_get_width(priv->buttons[BTN_SEPARATOR_LEFT]));
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_LEFT]);

      if (status_area_is_visible())
        {
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_STATUS]);

	  if (priv->state & HDTB_VIS_BTN_LEFT_MASK)
            clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_STATUS],
                hd_title_bar_get_button_width(bar) +
                clutter_actor_get_width(status_area));
          else
            clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_STATUS],
                clutter_actor_get_width(status_area));
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_STATUS]);

      if (priv->state & HDTB_VIS_BTN_RIGHT_MASK)
        {
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_RIGHT]);
          if (clutter_actor_is_visible (priv->buttons[BTN_DONE]))
            clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_RIGHT],
                                hd_comp_mgr_get_current_screen_width () -
                                clutter_actor_get_width (priv->buttons[BTN_DONE]));
          else
            clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_RIGHT],
                                hd_comp_mgr_get_current_screen_width ()
                                - hd_title_bar_get_button_width(bar));
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_RIGHT]);
    }
  else
    { /* !full_size */
      clutter_actor_hide(priv->title_bg);
      clutter_actor_hide(priv->progress_texture);
      clutter_timeline_stop(priv->progress_timeline);
      clutter_actor_hide(priv->buttons[BTN_MENU_INDICATOR]);
    }
  /* If we want the slightly translucent 'tab-style' background */
  if ((!full_size) || (priv->state & HDTB_VIS_FOREGROUND))
    {
      gint left_width = 0;
      if (priv->state & HDTB_VIS_BTN_LEFT_MASK)
        left_width = hd_title_bar_get_button_width(bar);

      // we don't show the status area in foreground mode
      if (status_area_is_visible() && !(priv->state & HDTB_VIS_FOREGROUND))
        {
          left_width += clutter_actor_get_width(status_area);
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_LEFT]);
          clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_LEFT],
              hd_title_bar_get_button_width(bar));
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_LEFT]);

      clutter_actor_hide(priv->buttons[BTN_SEPARATOR_STATUS]);
      clutter_actor_hide(priv->buttons[BTN_SEPARATOR_RIGHT]);

      /* move the rounded actor to the furthest right we want it
       * (edge of status area or button) */
      clutter_actor_show(priv->buttons[BTN_BG_LEFT_END]);
      clutter_actor_set_x(priv->buttons[BTN_BG_LEFT_END],
          left_width-HD_COMP_MGR_TOP_LEFT_BTN_WIDTH);
      /* Use the 'attached' actor to fill in the gap on the left */
      if (left_width>HD_COMP_MGR_TOP_LEFT_BTN_WIDTH)
        {
          clutter_actor_show(priv->buttons[BTN_BG_ATTACHED]);
          clutter_actor_set_width(priv->buttons[BTN_BG_ATTACHED],
              left_width-HD_COMP_MGR_TOP_LEFT_BTN_WIDTH);
        }
      else
        {
          clutter_actor_hide(priv->buttons[BTN_BG_ATTACHED]);
        }
      /* just put the right actor up if we need it... */
      if (priv->state & HDTB_VIS_BTN_RIGHT_MASK)
        clutter_actor_show(priv->buttons[BTN_BG_RIGHT_END]);
      else
        clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_END]);
    }

  /* Set the size of this group, so visibility detection on it
   * will work */
  clutter_actor_set_size(CLUTTER_ACTOR(bar),
                         hd_comp_mgr_get_current_screen_width(),
                         HD_COMP_MGR_TOP_MARGIN);
}

void
hd_title_bar_right_pressed(HdTitleBar *bar, gboolean pressed)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (pressed)
    {
      clutter_actor_show(priv->buttons[BTN_BG_RIGHT_PRESSED]);
      if (priv->state & HDTB_VIS_BTN_BACK)
        clutter_actor_show(priv->buttons[BTN_BACK_PRESSED]);
      if (priv->state & HDTB_VIS_BTN_CLOSE)
        clutter_actor_show(priv->buttons[BTN_CLOSE_PRESSED]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_BACK_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_CLOSE_PRESSED]);
    }
}

static void
hd_title_bar_set_for_edit_mode(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv;
  HdTitleBarVisEnum state = HDTB_VIS_NONE;
  const gchar *title;

  if (!HD_IS_TITLE_BAR(bar))
    return;

  priv = bar->priv;

  priv->has_menu_indicator = TRUE;

  state |= HDTB_VIS_BTN_MENU;
  state |= HDTB_VIS_BTN_DONE;
  state |= HDTB_VIS_FULL_WIDTH;

  if (priv->state & HDTB_VIS_SMALL_BUTTONS)
    state |= HDTB_VIS_SMALL_BUTTONS;

  hd_title_bar_set_state_real(bar, state);

  title = dgettext ("maemo-af-desktop", "home_ti_desktop_menu");
  hd_title_bar_set_title (bar, title, FALSE, FALSE);
}

static void
hd_title_bar_set_for_loading(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv;
  HdTitleBarVisEnum state = hd_title_bar_get_state (bar);

  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  state |= HDTB_VIS_FULL_WIDTH;
  state &= ~HDTB_VIS_FOREGROUND;

  hd_title_bar_set_state_real(bar, state);
  hd_title_bar_set_title (bar, priv->loading_title, FALSE, TRUE);
}

static void
hd_title_bar_set_window(HdTitleBar *bar, MBWindowManagerClient *client)
{
  MBWMClientType    c_type;
  MBWMDecor         *decor = 0;
  MBWMXmlClient     *c;
  MBWMXmlDecor      *d;
  HdTitleBarVisEnum state;
  gboolean pressed = FALSE;
  MBWMList *l;
  HdTitleBarPrivate *priv;
  gboolean is_waiting = FALSE;

  if (!HD_IS_TITLE_BAR(bar))
    return;

  priv = bar->priv;

  priv->has_menu_indicator = FALSE;
  if (client != NULL)
    {
      c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

      if (MB_WM_CLIENT_CLIENT_TYPE (client) != MBWMClientTypeApp)
        {
          g_critical("%s: should only be called on MBWMClientTypeApp",
		     __FUNCTION__);
          return;
        }

      is_waiting = hd_decor_window_is_waiting(client->wmref,
                                              client->window->xwindow);
      priv->has_menu_indicator = hd_decor_window_has_menu_indicator(
                                              client->wmref,
                                              client->window->xwindow);

      for (l = client->decor; l; l = l->next)
        {
          MBWMDecor *d = l->data;
          if (d->type == MBWMDecorTypeNorth)
            decor = d;
        }
    }

  if (!decor)
    {
      HdTitleBarVisEnum state;
      /* No north decor found, or no client */
      /* we have nothing, make sure we're back to normal */
      hd_title_bar_set_title (bar, NULL, FALSE, FALSE);
      if (hd_render_manager_get_state() &
	      (HDRM_STATE_HOME_EDIT | HDRM_STATE_HOME_EDIT_DLG | 
		    HDRM_STATE_HOME_EDIT_PORTRAIT | HDRM_STATE_HOME_EDIT_DLG_PORTRAIT))
        /* in Home edit mode we can have back button */
	      state = hd_title_bar_get_state(bar) & ~HDTB_VIS_FULL_WIDTH;
      else
	      state = hd_title_bar_get_state(bar) & ~(HDTB_VIS_FULL_WIDTH |
			            HDTB_VIS_BTN_CLOSE | HDTB_VIS_BTN_BACK);
      hd_title_bar_set_state_real(bar, state);
      return;
    }

  if (!((c = mb_wm_xml_client_find_by_type
                    (client->wmref->theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type))))
  return;

  /* add the title */
  const char* title = mb_wm_client_get_name (client);
  if (d->show_title && title && strlen(title))
    hd_title_bar_set_title (bar, title,
                            client->window->name_has_markup, is_waiting);
  else
    hd_title_bar_set_title (bar, NULL, FALSE, is_waiting);

  /* Go through all buttons and set the ones visible that are required */
  state = hd_title_bar_get_state(bar) & (~HDTB_VIS_BTN_RIGHT_MASK);

  for (l = MB_WM_DECOR(decor)->buttons;l;l = l->next)
    {
      MBWMDecorButton * button = l->data;
      if (button->type == MBWMDecorButtonClose)
        state |= HDTB_VIS_BTN_CLOSE;
      if ((HdThemeButtonType)button->type == HdHomeThemeButtonBack)
        state |= HDTB_VIS_BTN_BACK;

      pressed |= button->state != MBWMDecorButtonStateInactive;
    }
  /* Also set us to be full width */
  state |= HDTB_VIS_FULL_WIDTH;

  hd_title_bar_set_state_real(bar, state);
  hd_title_bar_right_pressed(bar, pressed);
}

/* Get the position at the end of the title where the progress indicator
 * and app menu indicator should be... */
static gint hd_title_bar_get_end_of_title(HdTitleBar *bar,
                                          int width,
                                          gboolean allow_title_overlap) {
  HdTitleBarPrivate *priv = bar->priv;
  gint x = 0;
  gint max_x = hd_comp_mgr_get_current_screen_width () -
              (width + hd_title_bar_get_button_width(bar));
  PangoRectangle logical_rect = { 0, };
  PangoLayout *layout;

  layout = clutter_text_get_layout (priv->title);
  pango_layout_get_extents (layout, NULL, &logical_rect);

  x = clutter_actor_get_x(CLUTTER_ACTOR(priv->title)) +
      (int)pango_units_to_double(logical_rect.width) +
      HD_TITLE_BAR_PROGRESS_MARGIN;

  if (x > max_x)
    {
      if (allow_title_overlap)
        x = max_x;
      else
        x = -width;
    }
  return x;
}

static void hd_title_bar_set_title (HdTitleBar *bar,
                                    const char *title,
                                    gboolean has_markup,
                                    gboolean waiting)
{
  HdTitleBarPrivate *priv;

  if (!HD_IS_TITLE_BAR(bar))
    return;

  priv = bar->priv;

  if (title)
    {
      ClutterActor *status_area;
      gchar *font_name;
      gint h, w;
      int x_start = 0;
      int x_end = hd_comp_mgr_get_current_screen_width ()
                  - hd_title_bar_get_button_width(bar)
                  - (waiting ? HD_THEME_IMG_PROGRESS_SIZE : 0);

      int title_margin = (priv->state & HDTB_VIS_SMALL_BUTTONS) ?
                          HD_TITLE_BAR_TITLE_MARGIN_SMALL : HD_TITLE_BAR_TITLE_MARGIN;

      if (priv->state & HDTB_VIS_BTN_LEFT_MASK)
        x_start += hd_title_bar_get_button_width(bar);

      status_area = hd_render_manager_get_status_area();
      if (status_area_is_visible())
        x_start += clutter_actor_get_width(status_area);

      font_name = hd_gtk_style_resolve_logical_font(HD_TITLE_BAR_TITLE_FONT);
      clutter_text_set_font_name(priv->title, font_name);
      g_free(font_name);
      clutter_text_set_text(priv->title, title);

      clutter_text_set_use_markup(priv->title, has_markup);

      h = clutter_actor_get_height(CLUTTER_ACTOR(priv->title));
      w = x_end - (x_start + title_margin);

      clutter_actor_set_width(CLUTTER_ACTOR(priv->title), w);
      clutter_actor_set_position(CLUTTER_ACTOR(priv->title),
                                 x_start+title_margin,
                                 (HD_COMP_MGR_TOP_MARGIN-h)/2);
      clutter_text_set_ellipsize(priv->title, PANGO_ELLIPSIZE_END);
      clutter_actor_show(CLUTTER_ACTOR(priv->title));
    }
  else
    clutter_actor_hide(CLUTTER_ACTOR(priv->title));

  if (waiting)
    {
      clutter_actor_show(priv->progress_texture);
      clutter_timeline_start(priv->progress_timeline);
      clutter_actor_set_position(priv->progress_texture,
                hd_title_bar_get_end_of_title(bar,
                                              HD_THEME_IMG_PROGRESS_SIZE,
                                              TRUE/*allow title overlap*/),
                (HD_COMP_MGR_TOP_MARGIN - HD_THEME_IMG_PROGRESS_SIZE)/2);
    }
  else
    {
      clutter_actor_hide(priv->progress_texture);
      clutter_timeline_stop(priv->progress_timeline);
    }

  /* Only show the indicator if we are asked to, and we're not
   * showing the waiting indicator. Also don't show it if we have
   * no title, as we're probably just waiting in a loading screen. */
  if (priv->has_menu_indicator && title && !waiting)
     {
       gint x = hd_title_bar_get_end_of_title(bar,
                                              HD_THEME_IMG_MENU_INDICATOR_SIZE,
                                              FALSE/* don't allow title overlap */);
       /* x<0 is used to signify that there was overlap with the title so we
        * should not display it. */
       if (x >= 0)
         {
           clutter_actor_show(priv->buttons[BTN_MENU_INDICATOR]);
           clutter_actor_set_position(priv->buttons[BTN_MENU_INDICATOR],
               x,
               (HD_COMP_MGR_TOP_MARGIN - HD_THEME_IMG_MENU_INDICATOR_SIZE)/2);
         }
       else
         clutter_actor_hide(priv->buttons[BTN_MENU_INDICATOR]);
     }
   else
     clutter_actor_hide(priv->buttons[BTN_MENU_INDICATOR]);
}

void hd_title_bar_set_loading_title   (HdTitleBar *bar,
                                       const char *title)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (priv->loading_title)
    g_free(priv->loading_title);
  priv->loading_title = g_strdup(title);

  hd_title_bar_update(bar);
}

static gboolean
hd_title_bar_update_idle(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv = bar->priv;
  HdTitleBarVisEnum old_state;

  old_state = priv->current_state;
  priv->current_state = priv->state;

  if (STATE_IS_EDIT_MODE(hd_render_manager_get_state()))
    {
      hd_title_bar_set_for_edit_mode(bar);
      clutter_actor_set_reactive (bar->priv->title_bg, TRUE);
    }
  else if (STATE_IS_LOADING(hd_render_manager_get_state()))
    {
      hd_title_bar_set_for_loading (bar);
      clutter_actor_set_reactive (bar->priv->title_bg, FALSE);
    }
  else
    {
      MBWindowManagerClient *c;

      if (STATE_IS_APP (hd_render_manager_get_state ()))
        {
          extern MBWindowManager *hd_mb_wm;
          HdCompMgr *cmgr = HD_COMP_MGR(hd_mb_wm->comp_mgr);
          HdCompMgrClient *cclient = hd_comp_mgr_get_current_client(cmgr);
          c = cclient ? MB_WM_COMP_MGR_CLIENT(cclient)->wm_client : NULL;
        }
      else
        c = NULL;

      hd_title_bar_set_window(bar, c);
      clutter_actor_set_reactive(bar->priv->title_bg, FALSE);
    }

  /* If anything that might change the position of the status area to change
   * has been modified, re-place titlebar elements now */
  if ((priv->current_state & (HDTB_VIS_SMALL_BUTTONS|HDTB_VIS_BTN_LEFT_MASK)) !=
      (old_state           & (HDTB_VIS_SMALL_BUTTONS|HDTB_VIS_BTN_LEFT_MASK)))
    hd_render_manager_place_titlebar_elements();

  /* Unfortunately, as we have updated title visibility of buttons and so forth,
   * we must now update the input viewport */
  hd_render_manager_set_input_viewport();

  /* This is only for this idle callback, so don't leave it dangling */
  priv->update_title_bar = 0;
  return FALSE;
}

void
hd_title_bar_update(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv = bar->priv;

  if (!priv->update_title_bar)
    /* Add idle callback. This MUST be higher priority than Clutter timelines
     * (D+30) or we won't set our title bar up correctly until after any running
     * transitions have stopped. */
    priv->update_title_bar = g_idle_add_full(G_PRIORITY_DEFAULT+20,
                                  (GSourceFunc)hd_title_bar_update_idle,
                                  bar, NULL);
}

void
hd_title_bar_update_now(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv = bar->priv;
  HdTitleBarVisEnum prev_button_size;

  if (priv->update_title_bar)
    {
      g_source_remove(priv->update_title_bar);
      priv->update_title_bar = 0;
    }

  /* We're going to portrait but we're not quite there yet,
   * so the title bar must look landscape but the title must
   * be the new portrait application's. */
  prev_button_size = priv->state & HDTB_VIS_SMALL_BUTTONS;
  if (hd_transition_is_rotating())
    {
      if (STATE_IS_PORTRAIT(hd_render_manager_get_previous_state()))
        priv->state |= HDTB_VIS_SMALL_BUTTONS;
      else
        priv->state &= ~HDTB_VIS_SMALL_BUTTONS;
    }
  hd_title_bar_update_idle(bar);
  priv->state = (priv->state & ~HDTB_VIS_SMALL_BUTTONS) | prev_button_size;
}

/* Is the given decor one we should consider for a title bar? */
gboolean
hd_title_bar_is_title_bar_decor(HdTitleBar *bar, MBWMDecor *decor)
{
  return (decor->type == MBWMDecorTypeNorth) &&
         decor->parent_client &&
         (MB_WM_CLIENT_CLIENT_TYPE(decor->parent_client) == MBWMClientTypeApp);
}

ClutterGroup *
hd_title_bar_get_foreground_group(HdTitleBar *bar)
{
  if (!HD_IS_TITLE_BAR(bar))
    return 0;

  return bar->priv->foreground;
}

void
hd_title_bar_set_switcher_pulse(HdTitleBar *bar, gboolean pulse)
{
  HdTitleBarPrivate *priv;

  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (!pulse)
    { /* Stop animation and unhilight the tasks button. */
      clutter_timeline_stop(priv->switcher_timeline);
      clutter_actor_set_opacity(priv->buttons[BTN_SWITCHER_HIGHLIGHT], 0);
      priv->state &= ~HDTB_VIS_BTN_SWITCHER_HIGHLIGHT;
    }
  else if (!clutter_timeline_is_playing(priv->switcher_timeline))
    { /* Be sure not to start overlapping animations. */
      if (priv->state & HDTB_VIS_BTN_SWITCHER_HIGHLIGHT)
        /* Continue the previous animation and skip the first
         * breathe-in pulse. */
        clutter_timeline_advance(priv->switcher_timeline,
            clutter_timeline_get_progress(priv->switcher_timeline));
      else
        /* Make sure set_state() leaves is highlighted. */
        priv->state |= HDTB_VIS_BTN_SWITCHER_HIGHLIGHT;

      clutter_timeline_start(priv->switcher_timeline);
    }
}


static void hd_title_bar_set_button_positions(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv = bar->priv;
  /* In HDTB_VIS_SMALL_BUTTONS, the area allocated for
   * buttons is smaller than their images - so we must
   * move the images slightly off-screen. */
  gint offset = HD_COMP_MGR_TOP_LEFT_BTN_WIDTH -
                hd_title_bar_get_button_width(bar);
  gint i, wscr;

  wscr = hd_comp_mgr_get_current_screen_width ();

  for (i = 0; i < BTN_COUNT; i++) {
    int button_offset = offset;
    /* If the button is supposed to be centred in the area,
     * do that. */
    if (BTN_FLAGS[i] & BTN_FLAG_CENTRE)
      button_offset /= 2;

    if (BTN_FLAGS[i] & BTN_FLAG_ALIGN_LEFT)
      clutter_actor_set_x(priv->buttons[i],
                          -button_offset);

    if (BTN_FLAGS[i] & BTN_FLAG_ALIGN_RIGHT)
      clutter_actor_set_x(priv->buttons[i],
          button_offset + wscr - clutter_actor_get_width(priv->buttons[i]));
  }

	/* Fixes menu button graphics glitch in portrait mode */
  clutter_actor_set_x(priv->buttons[BTN_BG_LEFT_ATTACHED_PRESSED],
    -offset);
  clutter_actor_set_x(priv->buttons[BTN_BG_ATTACHED],
    0);
  clutter_actor_set_x(priv->buttons[BTN_DONE],
    hd_comp_mgr_get_current_screen_width () - clutter_actor_get_width(priv->buttons[BTN_DONE]));
}

/* ------------------------------------------------------------------------- */

extern gboolean hd_dbus_display_is_off;

static void
on_switcher_timeline_new_frame(ClutterTimeline *timeline,
                               guint msecs, HdTitleBar *bar)
{
  HdTitleBarPrivate *priv;
  float amt;
  gint opacity;

  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (hd_dbus_display_is_off)
    {
      /* skip the animation */
      clutter_actor_set_opacity(priv->buttons[BTN_SWITCHER_HIGHLIGHT], 255);
      return;
    }

  /* Only get this to fire a redraw if it is visible... fixes bug 113278.
     (and now only update the actual area using
      hd_util_partial_redraw_if_possible...) */
#ifdef UPSTREAM_DISABLED
  clutter_actor_set_allow_redraw(CLUTTER_ACTOR(bar), FALSE);
#endif
  amt = ((float)msecs / (float)clutter_timeline_get_duration(timeline))
              * HD_TITLE_BAR_SWITCHER_PULSE_NPULSES / 2;
  if (priv->state & HDTB_VIS_BTN_SWITCHER)
    {
      opacity = (gint)((1-cos(amt*2*3.141592))*127);
      clutter_actor_set_opacity(priv->buttons[BTN_SWITCHER_HIGHLIGHT], opacity);
    }

  hd_util_partial_redraw_if_possible(priv->buttons[BTN_SWITCHER_HIGHLIGHT], 0);
#ifdef UPSTREAM_DISABLED
  clutter_actor_set_allow_redraw(CLUTTER_ACTOR(bar), TRUE);
#endif
}

/* Realign all right-aligned buttons when the screen size changes. */
static void
hd_title_bar_stage_allocation_changed (HdTitleBar *bar, GParamSpec *unused,
                                       ClutterActor *stage)
{
  hd_title_bar_set_button_positions(bar);
}

static void
hd_title_bar_top_left_clicked (HdTitleBar *bar)
{
  g_signal_emit (bar, signals[CLICKED_TOP_LEFT], 0);
}

static void
hd_title_bar_top_left_press (HdTitleBar *bar)
{
  if (!STATE_IN_EDIT_MODE(hd_render_manager_get_state()))
    hd_title_bar_left_pressed(bar, TRUE);

  g_signal_emit (bar, signals[PRESS_TOP_LEFT], 0);
}

static void
hd_title_bar_top_left_leave (HdTitleBar *bar)
{
  hd_title_bar_left_pressed(bar, FALSE);

  g_signal_emit (bar, signals[LEAVE_TOP_LEFT], 0);
}

static void
hd_title_bar_top_right_clicked (HdTitleBar *bar)
{
  /* For if we ever add the back button back into launcher */
  if (STATE_IS_LAUNCHER (hd_render_manager_get_state()))
    hd_launcher_back_button_clicked();

  g_signal_emit (bar, signals[CLICKED_TOP_RIGHT], 0);
}

static void
hd_title_bar_top_right_press (HdTitleBar *bar)
{
  hd_title_bar_right_pressed (bar, TRUE);

  if ((hd_render_manager_get_state () == HDRM_STATE_HOME_EDIT) ||
      (hd_render_manager_get_state () == HDRM_STATE_HOME_EDIT_PORTRAIT)
     )
    {
      if (hd_home_is_portrait_capable ())
        hd_render_manager_set_state (HDRM_STATE_HOME_PORTRAIT);
      else
        hd_render_manager_set_state (HDRM_STATE_HOME);
	}

  g_signal_emit (bar, signals[PRESS_TOP_RIGHT], 0);
}

static void
hd_title_bar_top_right_leave (HdTitleBar *bar)
{
  hd_title_bar_right_pressed(bar, FALSE);

  g_signal_emit (bar, signals[LEAVE_TOP_RIGHT], 0);
}

static gboolean
hd_title_bar_top_left_touch (ClutterActor *actor, ClutterEvent *event,
                             HdTitleBar *bar)
{
  switch (event->type)
    {
      case CLUTTER_TOUCH_BEGIN:
          hd_title_bar_top_left_press(bar);
          break;
      case CLUTTER_TOUCH_END:
          hd_title_bar_top_left_clicked(bar);
          break;
      default:
          break;
  }

  return CLUTTER_EVENT_PROPAGATE;
}

static gboolean
hd_title_bar_top_right_touch (ClutterActor *actor, ClutterEvent *event,
                              HdTitleBar *bar)
{
  switch (event->type)
    {
      case CLUTTER_TOUCH_BEGIN:
          hd_title_bar_top_right_press(bar);
          break;
      case CLUTTER_TOUCH_END:
          hd_title_bar_top_right_clicked(bar);
          break;
      default:
          break;
    }

  return CLUTTER_EVENT_PROPAGATE;
}

static void
hd_title_bar_add_left_signals(HdTitleBar *bar, ClutterActor *actor)
{
  clutter_actor_set_reactive(actor, TRUE);
  g_signal_connect_swapped (actor, "button-release-event",
                            G_CALLBACK (hd_title_bar_top_left_clicked), bar);
  g_signal_connect_swapped (actor, "button-press-event",
                            G_CALLBACK (hd_title_bar_top_left_press), bar);
  g_signal_connect_swapped (actor, "leave-event",
                            G_CALLBACK (hd_title_bar_top_left_leave), bar);
  g_signal_connect (actor, "touch-event",
                    G_CALLBACK (hd_title_bar_top_left_touch), bar);
}

static void
hd_title_bar_add_right_signals(HdTitleBar *bar, ClutterActor *actor)
{
  clutter_actor_set_reactive(actor, TRUE);
  g_signal_connect_swapped (actor, "button-release-event",
                            G_CALLBACK (hd_title_bar_top_right_clicked), bar);
  g_signal_connect_swapped (actor, "button-press-event",
                            G_CALLBACK (hd_title_bar_top_right_press), bar);
  g_signal_connect_swapped (actor, "leave-event",
                            G_CALLBACK (hd_title_bar_top_right_leave), bar);
  g_signal_connect (actor, "touch-event",
                    G_CALLBACK (hd_title_bar_top_right_touch), bar);
}

/* Create a fake version of the title bar that can be used in the switcher */
ClutterActor *
hd_title_bar_create_fake(gint width)
{
  ClutterActor *group = clutter_group_new();
  clutter_actor_set_name(group, "hd_title_bar_create_fake");
  ClutterActor *title_bar = hd_clutter_cache_get_texture(
      HD_THEME_IMG_TITLE_BAR, TRUE);
  clutter_actor_set_width(title_bar, width);
  clutter_actor_add_child(CLUTTER_ACTOR(group), title_bar);

  return CLUTTER_ACTOR(group);
}

void
hd_title_bar_get_xy (HdTitleBar *bar, int *x, int *y)
{
  ClutterActor *titlebar;
  /* 'adj' depends on title bar font and legacy menu padding */
  static const int adj = 14;

  if (!HD_IS_TITLE_BAR(bar) || !x || !y)
    return;

  titlebar = CLUTTER_ACTOR(bar->priv->title);

  if (titlebar)
  {
    int tmp = clutter_actor_get_x (titlebar);

    /* check that it's a sensible value */
    if (tmp < 2 * 112 + adj)
      *x = 2 * 112 + adj;
    else
      *x = tmp - adj;

    *y = clutter_actor_get_y (titlebar) + clutter_actor_get_height (titlebar);
  }
  else
  {
    *x = 2 * 112 + adj;
    *y = 56;
  }
}
