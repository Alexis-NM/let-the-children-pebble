#include <pebble.h>

/* ---------- Réglages “mise en page cahier” ---------- */
#define MARGIN_X         20    // largeur de la marge dessinée dans l’image
#define PAD_LEFT_TEXT     6    // décalage à droite de la marge (effet "écriture")
#define MAJOR_STEP_Y     20    // pas entre deux lignes épaisses dans le fond
#define TIME_MAJOR_INDEX  3    // index de la ligne épaisse portante pour l'heure
#define DATE_MAJOR_INDEX  4    // index de la ligne épaisse portante pour la date
#define NOTE_POS_Y       10    // position Y de la note batterie (depuis le haut)
#define NOTE_PAD_RIGHT    6    // marge intérieure droite pour la note

// Décalage vertical global (ajustement fin pour coller au fond)
#define VERTICAL_OFFSET   4

static Window     *s_window;
static BitmapLayer *s_bg_layer;
static GBitmap    *s_bg_bitmap;

static TextLayer  *s_time_layer;
static TextLayer  *s_date_layer;
static TextLayer  *s_note_layer;

static GFont       s_font_time;   // Permanent Marker 40
static GFont       s_font_small;  // Permanent Marker 24

// Buffers + cache pour éviter des set_text inutiles
static char s_time_buf[8];   // "HH:MM"
static char s_time_prev[8];
static char s_date_buf[6];   // "dd/mm"
static char s_date_prev[6];
static char s_note_buf[4];   // "A+" / "B" / "F"
static char s_note_prev[4];

/* ---------- Utils: grade batterie ---------- */
static void battery_grade(int pct, char *out, size_t out_size) {
  const char *grade = "F";
  if (pct >= 95) grade = "A+";
  else if (pct >= 90) grade = "A";
  else if (pct >= 85) grade = "A-";
  else if (pct >= 80) grade = "B+";
  else if (pct >= 70) grade = "B";
  else if (pct >= 65) grade = "B-";
  else if (pct >= 55) grade = "C+";
  else if (pct >= 45) grade = "C";
  else if (pct >= 35) grade = "C-";
  else if (pct >= 25) grade = "D";
  else if (pct >= 15) grade = "D-";
  snprintf(out, out_size, "%s", grade);
}

/* ---------- Time / Date ---------- */
static void update_time(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  if (clock_is_24h_style()) {
    strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", t);
  } else {
    strftime(s_time_buf, sizeof(s_time_buf), "%I:%M", t);
  }

  if (strcmp(s_time_buf, s_time_prev) != 0) {
    text_layer_set_text(s_time_layer, s_time_buf);
    strncpy(s_time_prev, s_time_buf, sizeof(s_time_prev));
  }
}

static void update_date(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(s_date_buf, sizeof(s_date_buf), "%d/%m", t);

  if (strcmp(s_date_buf, s_date_prev) != 0) {
    text_layer_set_text(s_date_layer, s_date_buf);
    strncpy(s_date_prev, s_date_buf, sizeof(s_date_prev));
  }
}

/* ---------- Battery ---------- */
static void battery_handler(BatteryChargeState state) {
  battery_grade(state.charge_percent, s_note_buf, sizeof(s_note_buf));
  if (strcmp(s_note_buf, s_note_prev) != 0) {
    text_layer_set_text(s_note_layer, s_note_buf);
    strncpy(s_note_prev, s_note_buf, sizeof(s_note_prev));
  }
}

/* ---------- Tick (minute only) ---------- */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // On se réveille 1x/minute uniquement
  update_time();

  // Met à jour la date à minuit pour éviter un 2e abonnement (DAY_UNIT)
  if (tick_time->tm_hour == 0 && tick_time->tm_min == 0) {
    update_date();
  }
}

/* ---------- Window ---------- */
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Fond "cahier"
#ifdef PBL_ROUND
  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMG_BG_CAHIER_SEYES_180x180);
#else
  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMG_BG_CAHIER_SEYES_144x168);
#endif
  s_bg_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  bitmap_layer_set_compositing_mode(s_bg_layer, GCompOpSet);
  layer_add_child(root, bitmap_layer_get_layer(s_bg_layer));

  // Polices
  s_font_time  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PM_40));
  s_font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PM_24));

  // ----- Heure -----
  const int h_time = 52;
  int y_time = TIME_MAJOR_INDEX * MAJOR_STEP_Y - h_time / 2 + VERTICAL_OFFSET;
  if (y_time < 0) y_time = 0;

  GRect frame_time = GRect(
    MARGIN_X + PAD_LEFT_TEXT,
    y_time,
    bounds.size.w - (MARGIN_X + PAD_LEFT_TEXT),
    h_time
  );

  s_time_layer = text_layer_create(frame_time);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, s_font_time);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_time_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  // ----- Date -----
  const int h_date = 26;
  const int extra_spacing = 6;
  int y_date = DATE_MAJOR_INDEX * MAJOR_STEP_Y - h_date / 2 + extra_spacing + VERTICAL_OFFSET;
  if (y_date < 0) y_date = 0;

  GRect frame_date = GRect(
    MARGIN_X + PAD_LEFT_TEXT,
    y_date,
    bounds.size.w - (MARGIN_X + PAD_LEFT_TEXT),
    h_date
  );

  s_date_layer = text_layer_create(frame_date);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  text_layer_set_font(s_date_layer, s_font_small);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_date_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  // ----- Note batterie -----
  const int note_width = 36;
  GRect note_frame = GRect(
    bounds.size.w - note_width - NOTE_PAD_RIGHT,
    NOTE_POS_Y,
    note_width,
    28
  );

  s_note_layer = text_layer_create(note_frame);
  text_layer_set_background_color(s_note_layer, GColorClear);
  text_layer_set_text_color(s_note_layer, GColorBlack);
  text_layer_set_font(s_note_layer, s_font_small);
  text_layer_set_text_alignment(s_note_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_note_layer));

  // Init buffers pour éviter 1er strcmp faux-positif
  s_time_prev[0] = s_date_prev[0] = s_note_prev[0] = '\0';

  // Valeurs initiales
  update_time();
  update_date();
  battery_handler(battery_state_service_peek());
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_note_layer);

  fonts_unload_custom_font(s_font_time);
  fonts_unload_custom_font(s_font_small);

  bitmap_layer_destroy(s_bg_layer);
  gbitmap_destroy(s_bg_bitmap);
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(s_window, true);

  // Tick 1x/minute = max sommeil CPU/afficheur
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Batterie — callback seulement sur changement d'état
  battery_state_service_subscribe(battery_handler);
}

static void deinit(void) {
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}