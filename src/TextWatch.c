#include <Pebble.h>
#include <ctype.h>
#include "num2words-en.h"

#define BUFFER_SIZE 44

#define KEY_TEMPERATURE 0
#define KEY_LOW 1
#define KEY_HIGH 2
#define KEY_CONDITIONS 3

Window *window;

typedef struct {
    TextLayer *currentLayer;
    TextLayer *nextLayer;
    PropertyAnimation *currentAnimation;
    PropertyAnimation *nextAnimation;
} Line;

Line line1;
Line line2;
Line line3;
TextLayer *date;
TextLayer *day;
TextLayer *weather;
TextLayer *battery;

static char line1Str[2][BUFFER_SIZE];
static char line2Str[2][BUFFER_SIZE];
static char line3Str[2][BUFFER_SIZE];
static char weatherString[32];

static void destroy_property_animation(PropertyAnimation **prop_animation) {
    if (*prop_animation == NULL) {
        return;
    }

    if (animation_is_scheduled((Animation*) *prop_animation)) {
        animation_unschedule((Animation*) *prop_animation);
    }

    property_animation_destroy(*prop_animation);
    *prop_animation = NULL;
}

//Handle Date
void setDate(struct tm *tm)
{
    static char dateString[] = "september 99th";
    static char dayString[] = "wednesday";
    switch(tm->tm_mday)
    {
        case 1 :
        case 21 :
        case 31 :
            strftime(dateString, sizeof(dateString), "%B %est", tm);
            break;
        case 2 :
        case 22 :
            strftime(dateString, sizeof(dateString), "%B %end", tm);
            break;
        case 3 :
        case 23 :
            strftime(dateString, sizeof(dateString), "%B %erd", tm);
            break;
        default :
            strftime(dateString, sizeof(dateString), "%B %eth", tm);
            break;
    }
    strftime(dayString, sizeof(dayString), "%A", tm);
    dateString[0] = tolower((int)dateString[0]);
    dayString[0] = tolower((int)dayString[0]);
    text_layer_set_text(date, dateString);
    text_layer_set_text(day, dayString);
}

// Animation handler
void animationStoppedHandler(struct Animation *animation, bool finished, void *context)
{
    Layer *current = (Layer *)context;
    GRect rect = layer_get_frame(current);
    rect.origin.x = 144;
    layer_set_frame(current, rect);
}

// Animate line
void makeAnimationsForLayers(Line *line, TextLayer *current, TextLayer *next)
{
    GRect rect = layer_get_frame((Layer*)next);
    rect.origin.x -= 144;

    destroy_property_animation(&(line->nextAnimation));

    line->nextAnimation = property_animation_create_layer_frame((Layer*)next, NULL, &rect);
    animation_set_duration((Animation*)line->nextAnimation, 400);
    animation_set_curve((Animation*)line->nextAnimation, AnimationCurveEaseOut);

    animation_schedule((Animation*)line->nextAnimation);

    GRect rect2 = layer_get_frame((Layer*)current);
    rect2.origin.x -= 144;

    destroy_property_animation(&(line->currentAnimation));

    line->currentAnimation = property_animation_create_layer_frame((Layer*)current, NULL, &rect2);
    animation_set_duration((Animation*)line->currentAnimation, 400);
    animation_set_curve((Animation*)line->currentAnimation, AnimationCurveEaseOut);

    animation_set_handlers((Animation*)line->currentAnimation, (AnimationHandlers) {
            .stopped = (AnimationStoppedHandler)animationStoppedHandler
            }, current);

    animation_schedule((Animation*)line->currentAnimation);
}

// Update line
void updateLineTo(Line *line, char lineStr[2][BUFFER_SIZE], char *value)
{
    TextLayer *next, *current;

    GRect rect = layer_get_frame((Layer*)line->currentLayer);
    current = (rect.origin.x == 0) ? line->currentLayer : line->nextLayer;
    next = (current == line->currentLayer) ? line->nextLayer : line->currentLayer;

    // Update correct text only
    if (current == line->currentLayer) {
        memset(lineStr[1], 0, BUFFER_SIZE);
        memcpy(lineStr[1], value, strlen(value));
        text_layer_set_text(next, lineStr[1]);
    } else {
        memset(lineStr[0], 0, BUFFER_SIZE);
        memcpy(lineStr[0], value, strlen(value));
        text_layer_set_text(next, lineStr[0]);
    }

    makeAnimationsForLayers(line, current, next);
}

// Check to see if the current line needs to be updated
bool needToUpdateLine(Line *line, char lineStr[2][BUFFER_SIZE], char *nextValue)
{
    char *currentStr;
    GRect rect = layer_get_frame((Layer*)line->currentLayer);
    currentStr = (rect.origin.x == 0) ? lineStr[0] : lineStr[1];

    if (memcmp(currentStr, nextValue, strlen(nextValue)) != 0 ||
            (strlen(nextValue) == 0 && strlen(currentStr) != 0)) {
        return true;
    }
    return false;
}

// Update screen based on new time
void display_time(struct tm *t)
{
    // The current time text will be stored in the following 3 strings
    char textLine1[BUFFER_SIZE];
    char textLine2[BUFFER_SIZE];
    char textLine3[BUFFER_SIZE];

    time_to_3words(t->tm_hour, t->tm_min, textLine1, textLine2, textLine3, BUFFER_SIZE);

    if (needToUpdateLine(&line1, line1Str, textLine1)) {
        updateLineTo(&line1, line1Str, textLine1);
    }
    if (needToUpdateLine(&line2, line2Str, textLine2)) {
        updateLineTo(&line2, line2Str, textLine2);
    }
    if (needToUpdateLine(&line3, line3Str, textLine3)) {
        updateLineTo(&line3, line3Str, textLine3);
    }
}

// Update screen without animation first time we start the watchface
void display_initial_time(struct tm *t)
{
    time_to_3words(t->tm_hour, t->tm_min, line1Str[0], line2Str[0], line3Str[0], BUFFER_SIZE);

    text_layer_set_text(line1.currentLayer, line1Str[0]);
    text_layer_set_text(line2.currentLayer, line2Str[0]);
    text_layer_set_text(line3.currentLayer, line3Str[0]);
    setDate(t);
}

static void update_battery() {
    static char battery_buffer[5] = "XX%";
    BatteryChargeState charge_state = battery_state_service_peek();
    snprintf(battery_buffer, sizeof(battery_buffer), "%d%%", charge_state.charge_percent);
    text_layer_set_text(battery, battery_buffer);
}

void ask_for_weather() {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);
    dict_write_end(iter);

    // Send the message!
    app_message_outbox_send();
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    static char temperature_buffer[8];
    static char low_buffer[8];
    static char high_buffer[8];
    static char conditions_buffer[32];

    Tuple *t = dict_read_first(iterator);

    while (t != NULL) {
        switch (t->key) {
            case KEY_TEMPERATURE:
                snprintf(temperature_buffer, sizeof(temperature_buffer), "%d", (int)t->value->int32);
                break;
            case KEY_LOW:
                snprintf(low_buffer, sizeof(low_buffer), "%d", (int)t->value->int32);
                break;
            case KEY_HIGH:
                snprintf(high_buffer, sizeof(high_buffer), "%d", (int)t->value->int32);
                break;
            case KEY_CONDITIONS:
                snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", t->value->cstring);
                break;
            default:
                APP_LOG(APP_LOG_LEVEL_ERROR, "App key: %d not found", (int)t->key);
                break;
        }
        t = dict_read_next(iterator);
    }

    if (conditions_buffer[0]=='X') {
        snprintf(weatherString, sizeof(weatherString), "X");
    } else {
        snprintf(weatherString, sizeof(weatherString), "%sC (%s/%s) %s", temperature_buffer, low_buffer, high_buffer, conditions_buffer);
    }
    text_layer_set_text(weather, weatherString);

}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    snprintf(weatherString, sizeof(weatherString), "X");
    text_layer_set_text(weather, weatherString);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    snprintf(weatherString, sizeof(weatherString), "X");
    text_layer_set_text(weather, weatherString);
}

void app_message_init() {
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

    #if INVERT
// Configure the first line of text
void configureBoldLayer(TextLayer *textlayer)
{
    text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_color(textlayer, GColorBlack);
    text_layer_set_background_color(textlayer, GColorClear);
    text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Configure for the 2nd and 3rd lines
void configureLightLayer(TextLayer *textlayer)
{
    text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
    text_layer_set_text_color(textlayer, GColorBlack);
    text_layer_set_background_color(textlayer, GColorClear);
    text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);

void init() {

    window = window_create();
    window_stack_push(window, true);
    window_set_background_color(window, GColorWhite);

    #else
// Configure the first line of text
void configureBoldLayer(TextLayer *textlayer)
{
    text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_color(textlayer, GColorWhite);
    text_layer_set_background_color(textlayer, GColorClear);
    text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

// Configure for the 2nd and 3rd lines
void configureLightLayer(TextLayer *textlayer)
{
    text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
    text_layer_set_text_color(textlayer, GColorWhite);
    text_layer_set_background_color(textlayer, GColorClear);
    text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);

void init() {

    window = window_create();
    window_stack_push(window, true);
    window_set_background_color(window, GColorBlack);

    #endif

    // 1st line layers
    line1.currentLayer = text_layer_create(GRect(0, 13, 144, 50));
    line1.nextLayer = text_layer_create(GRect(144, 13, 144, 50));
    configureBoldLayer(line1.currentLayer);
    configureBoldLayer(line1.nextLayer);

    // 2nd layers
    line2.currentLayer = text_layer_create(GRect(0, 50, 144, 50));
    line2.nextLayer = text_layer_create(GRect(144, 50, 144, 50));
    configureLightLayer(line2.currentLayer);
    configureLightLayer(line2.nextLayer);

    // 3rd layers
    line3.currentLayer = text_layer_create(GRect(0, 87, 144, 50));
    line3.nextLayer = text_layer_create(GRect(144, 87, 144, 50));
    configureLightLayer(line3.currentLayer);
    configureLightLayer(line3.nextLayer);

    #if INVERT
    //date & day layers
    date = text_layer_create(GRect(0, 150, 144, 168-150));
    text_layer_set_text_color(date, GColorBlack);
    text_layer_set_background_color(date, GColorClear);
    text_layer_set_font(date, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(date, GTextAlignmentRight);
    day = text_layer_create(GRect(0, 135, 144, 168-135));
    text_layer_set_text_color(day, GColorBlack);
    text_layer_set_background_color(day, GColorClear);
    text_layer_set_font(day, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_alignment(day, GTextAlignmentRight);
    //weather
    weather = text_layer_create(GRect(0, 0, 144, 20));
    text_layer_set_background_color(weather, GColorClear);
    text_layer_set_text_color(weather, GColorBlack);
    text_layer_set_font(weather, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(weather, GTextAlignmentRight);
    text_layer_set_text(weather, "loading...");
    //battery
    battery = text_layer_create(GRect(0, 150, 40, 168-150));
    text_layer_set_background_color(battery, GColorClear);
    text_layer_set_text_color(battery, GColorBlack);
    text_layer_set_font(battery, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(battery, GTextAlignmentLeft);
    text_layer_set_text(battery, "XX%");
    #else
    //date & day layers
    date = text_layer_create(GRect(40, 150, 104, 168-150));
    text_layer_set_text_color(date, GColorWhite);
    text_layer_set_background_color(date, GColorClear);
    text_layer_set_font(date, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(date, GTextAlignmentRight);
    day = text_layer_create(GRect(0, 135, 144, 168-135));
    text_layer_set_text_color(day, GColorWhite);
    text_layer_set_background_color(day, GColorClear);
    text_layer_set_font(day, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
    text_layer_set_text_alignment(day, GTextAlignmentRight);
    //weather
    weather = text_layer_create(GRect(0, 0, 144, 20));
    text_layer_set_background_color(weather, GColorClear);
    text_layer_set_text_color(weather, GColorWhite);
    text_layer_set_font(weather, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(weather, GTextAlignmentRight);
    text_layer_set_text(weather, "loading...");
    //battery
    battery = text_layer_create(GRect(0, 150, 40, 168-150));
    text_layer_set_background_color(battery, GColorClear);
    text_layer_set_text_color(battery, GColorWhite);
    text_layer_set_font(battery, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(battery, GTextAlignmentLeft);
    text_layer_set_text(battery, "XX%");
    #endif

    // Configure time on init
    time_t now = time(NULL);
    display_initial_time(localtime(&now));

    // Load layers
    Layer *window_layer = window_get_root_layer(window);
    layer_add_child(window_layer, (Layer*)line1.currentLayer);
    layer_add_child(window_layer, (Layer*)line1.nextLayer);
    layer_add_child(window_layer, (Layer*)line2.currentLayer);
    layer_add_child(window_layer, (Layer*)line2.nextLayer);
    layer_add_child(window_layer, (Layer*)line3.currentLayer);
    layer_add_child(window_layer, (Layer*)line3.nextLayer);
    layer_add_child(window_layer, (Layer*)date);
    layer_add_child(window_layer, (Layer*)day);
    layer_add_child(window_layer, (Layer*)weather);
    layer_add_child(window_layer, (Layer*)battery);

    app_message_init();
    ask_for_weather();
    update_battery();

    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

void deinit() {

    tick_timer_service_unsubscribe();
    app_message_deregister_callbacks();

    layer_destroy((Layer*)line1.currentLayer);
    layer_destroy((Layer*)line1.nextLayer);
    layer_destroy((Layer*)line2.currentLayer);
    layer_destroy((Layer*)line2.nextLayer);
    layer_destroy((Layer*)line3.currentLayer);
    layer_destroy((Layer*)line3.nextLayer);

    destroy_property_animation(&line1.currentAnimation);
    destroy_property_animation(&line1.nextAnimation);
    destroy_property_animation(&line2.currentAnimation);
    destroy_property_animation(&line2.nextAnimation);
    destroy_property_animation(&line3.currentAnimation);
    destroy_property_animation(&line3.nextAnimation);

    layer_destroy((Layer*)date);
    layer_destroy((Layer*)day);

    layer_destroy((Layer*)weather);
    layer_destroy((Layer*)battery);

    window_destroy(window);
}

// Time handler called every minute by the system
void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    display_time(tick_time);
    if (units_changed & DAY_UNIT) {
        setDate(tick_time);
    }
    if (units_changed & HOUR_UNIT) {
        ask_for_weather();
        update_battery();
    }
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
