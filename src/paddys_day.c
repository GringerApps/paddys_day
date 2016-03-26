#include <pebble.h>

const uint32_t FIRST_DELAY_MS = 10;

static Window *s_main_window;
static TextLayer *s_time_layer;
static GBitmapSequence *s_sequence;
static GBitmap *s_bitmap;
static BitmapLayer *s_bitmap_layer;

uint32_t next_delay;
bool playing = false;

static void reset_gif(){
  gbitmap_sequence_restart(s_sequence);
  gbitmap_sequence_update_bitmap_next_frame(s_sequence, s_bitmap, &next_delay);
  bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
  gbitmap_sequence_restart(s_sequence);
}

static void play_gif(void *context){
  if(gbitmap_sequence_update_bitmap_next_frame(s_sequence, s_bitmap, &next_delay)) {
    bitmap_layer_set_bitmap(s_bitmap_layer, s_bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(s_bitmap_layer));
    app_timer_register(next_delay, play_gif, NULL);
  }else{
    reset_gif();
    playing = false;
  }
}

static void try_play_gif(void *context) {
  if(!playing){
    playing = true;
    app_timer_register(next_delay, play_gif, NULL);
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  app_timer_register(next_delay, try_play_gif, NULL);
}

static void update_time(const bool play_gif) {
  const time_t temp = time(NULL);
  const struct tm *tick_time = localtime(&temp);
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
  if(tick_time->tm_min % 5 == 0 && play_gif){
    app_timer_register(FIRST_DELAY_MS, try_play_gif, NULL);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(true);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);
  const GSize window_size = bounds.size;

  const GSize bitmap_size = gbitmap_sequence_get_bitmap_size(s_sequence);
  const int x = (window_size.w - bitmap_size.w) / 2;
  const GRect bitmap_layer_bounds = (GRect){ .origin = GPoint(x,0)  ,.size = bitmap_size };
  s_bitmap_layer = bitmap_layer_create(bitmap_layer_bounds);
  reset_gif();
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bitmap_layer));

  s_time_layer = text_layer_create(GRect(0, bitmap_size.h, window_size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorSpringBud);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  bitmap_layer_destroy(s_bitmap_layer);
}

static void init() {
  s_sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_PADDY);
  gbitmap_sequence_set_play_count(s_sequence, 1);
  const GSize frame_size = gbitmap_sequence_get_bitmap_size(s_sequence);
  s_bitmap = gbitmap_create_blank(frame_size, GBitmapFormat8Bit);

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorSpringBud);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  update_time(false);
  app_timer_register(FIRST_DELAY_MS, try_play_gif, NULL);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  accel_tap_service_subscribe(accel_tap_handler);
}

static void deinit() {
  accel_tap_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
  gbitmap_destroy(s_bitmap);
  gbitmap_sequence_destroy(s_sequence);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
