#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/date_time_input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <datetime.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TAG "WalletApp"

#define WALLET_APP_DIR      "wallet"
#define WALLET_CONFIG_PATH  APP_DATA_PATH("wallet/wallet.conf")
#define WALLET_CONFIG_TYPE  "Flipper Wallet Config"
#define WALLET_DATA_TYPE    "Flipper Wallet Data"

#define WALLET_MAX_CATEGORIES  8
#define WALLET_MAX_FIELDS      6
#define WALLET_MAX_ENTRIES     64
#define WALLET_NAME_LEN        24
#define WALLET_FIELD_LABEL_LEN 24
#define WALLET_FIELD_LEN       32

#define WALLET_LIST_ITEM_HEIGHT   16
#define WALLET_LIST_VISIBLE_ITEMS 3

typedef enum {
    WalletFieldTypeString,
    WalletFieldTypeNumber,
    WalletFieldTypeDate,
} WalletFieldType;

typedef struct {
    char label[WALLET_FIELD_LABEL_LEN];
    WalletFieldType type;
} WalletField;

typedef struct {
    char name[WALLET_NAME_LEN];
    uint8_t field_count;
    WalletField fields[WALLET_MAX_FIELDS];
} WalletCategory;

typedef struct {
    char values[WALLET_MAX_FIELDS][WALLET_FIELD_LEN];
} WalletEntry;

typedef enum {
    WalletViewMainMenu = 0,
    WalletViewEntryList = 1,
    WalletViewEntryDetail = 2,
    WalletViewTextInput = 3,
    WalletViewNumberInput = 4,
    WalletViewDateInput = 5,
    WalletViewEntryMenu = 6,
} WalletView;

typedef enum {
    WalletScreenMainMenu,
    WalletScreenEntryList,
    WalletScreenEntryDetail,
    WalletScreenEditing,
    WalletScreenEntryMenu,
} WalletScreen;

typedef struct WalletApp WalletApp;

typedef struct {
    WalletApp* app;
    uint16_t position; // 0..entry_count-1 = existing entries, entry_count = "+New"
    uint16_t window_position;
} EntryListModel;

struct WalletApp {
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Storage* storage;

    Submenu* main_menu;
    View* entry_list_view;
    Widget* detail_widget;
    TextInput* text_input;
    NumberInput* number_input;
    DateTimeInput* date_input;
    Submenu* entry_action_menu;

    WalletCategory categories[WALLET_MAX_CATEGORIES];
    uint8_t category_count;
    uint8_t current_category;

    WalletEntry entries[WALLET_MAX_ENTRIES];
    uint16_t entry_count;
    uint16_t selected_index;

    uint8_t editing_field;
    char edit_buffer[WALLET_FIELD_LEN];
    DateTime edit_datetime;
    bool new_entry_pending; // true until first field of a new entry is confirmed

    char file_path[128];
    WalletScreen current_screen;
};

static void wallet_app_begin_field_edit(WalletApp* app, uint8_t field_index);
static void wallet_app_show_detail(WalletApp* app, uint16_t index);
static void wallet_app_open_detail(WalletApp* app, uint16_t index);
static void wallet_app_add_new_entry(WalletApp* app);
static bool wallet_app_save_entries(WalletApp* app);

/* ---------- config parsing ---------- */

static WalletFieldType wallet_app_parse_field_type(const char* type_str) {
    if(strcmp(type_str, "Number") == 0) return WalletFieldTypeNumber;
    if(strcmp(type_str, "Date") == 0) return WalletFieldTypeDate;
    return WalletFieldTypeString;
}

/* Parses "Label[Type],Label[Type],..." into a category's field list. */
static void wallet_app_parse_fields(WalletCategory* category, const char* fields_str) {
    category->field_count = 0;
    const char* p = fields_str;

    while(*p && category->field_count < WALLET_MAX_FIELDS) {
        while(*p == ' ') p++;

        const char* comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);

        char token[64];
        if(len > sizeof(token) - 1) len = sizeof(token) - 1;
        memcpy(token, p, len);
        token[len] = '\0';

        WalletField* field = &category->fields[category->field_count];
        field->type = WalletFieldTypeString;

        char* bracket = strchr(token, '[');
        if(bracket) {
            char* close = strchr(bracket, ']');
            if(close) *close = '\0';
            field->type = wallet_app_parse_field_type(bracket + 1);
            *bracket = '\0';
        }

        strlcpy(field->label, token, sizeof(field->label));
        category->field_count++;

        p = comma ? comma + 1 : p + strlen(p);
    }
}

static void wallet_app_populate_default_categories(WalletApp* app) {
    app->category_count = 0;

    WalletCategory* vehicles = &app->categories[app->category_count++];
    strlcpy(vehicles->name, "Vehicles", sizeof(vehicles->name));
    wallet_app_parse_fields(vehicles, "Name[String],Year[Number],LPlate[String],VIN[String]");

    WalletCategory* ids = &app->categories[app->category_count++];
    strlcpy(ids->name, "IDs", sizeof(ids->name));
    wallet_app_parse_fields(
        ids, "Name[String],ID Number[String],Valid from[Date],Expiry[Date],Issuer[String]");
}

static void wallet_app_write_default_config(WalletApp* app) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);

    if(flipper_format_file_open_always(ff, WALLET_CONFIG_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(ff, WALLET_CONFIG_TYPE, 1)) break;
            if(!flipper_format_write_string_cstr(ff, "Category", "Vehicles")) break;
            if(!flipper_format_write_string_cstr(
                   ff, "Fields", "Name[String],Year[Number],LPlate[String],VIN[String]"))
                break;
            if(!flipper_format_write_string_cstr(ff, "Category", "IDs")) break;
            flipper_format_write_string_cstr(
                ff,
                "Fields",
                "Name[String],ID Number[String],Valid from[Date],Expiry[Date],Issuer[String]");
        } while(0);
    }

    flipper_format_free(ff);
}

static void wallet_app_load_config(WalletApp* app) {
    app->category_count = 0;

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* value = furi_string_alloc();
    uint32_t version = 0;

    if(flipper_format_file_open_existing(ff, WALLET_CONFIG_PATH)) {
        if(flipper_format_read_header(ff, value, &version)) {
            while(app->category_count < WALLET_MAX_CATEGORIES &&
                  flipper_format_read_string(ff, "Category", value)) {
                WalletCategory* category = &app->categories[app->category_count];
                strlcpy(category->name, furi_string_get_cstr(value), sizeof(category->name));
                category->field_count = 0;

                if(flipper_format_read_string(ff, "Fields", value)) {
                    wallet_app_parse_fields(category, furi_string_get_cstr(value));
                }

                app->category_count++;
            }
        }
    }

    furi_string_free(value);
    flipper_format_free(ff);

    if(app->category_count == 0) {
        wallet_app_populate_default_categories(app);
        wallet_app_write_default_config(app);
    }
}

/* ---------- entry storage ---------- */

static void wallet_app_ensure_data_dir(WalletApp* app) {
    storage_common_mkdir(app->storage, APP_DATA_PATH(WALLET_APP_DIR));
}

static void wallet_app_set_file_path(WalletApp* app) {
    const WalletCategory* category = &app->categories[app->current_category];
    char safe_name[WALLET_NAME_LEN];

    size_t i = 0;
    for(; category->name[i] != '\0' && i < sizeof(safe_name) - 1; i++) {
        char c = category->name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_';
        safe_name[i] = ok ? c : '_';
    }
    safe_name[i] = '\0';

    snprintf(
        app->file_path,
        sizeof(app->file_path),
        "%s/%s.txt",
        APP_DATA_PATH(WALLET_APP_DIR),
        safe_name);
}

static void wallet_app_load_entries(WalletApp* app) {
    app->entry_count = 0;
    const WalletCategory* category = &app->categories[app->current_category];

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* value = furi_string_alloc();
    uint32_t version = 0;

    if(flipper_format_file_open_existing(ff, app->file_path)) {
        if(flipper_format_read_header(ff, value, &version)) {
            while(app->entry_count < WALLET_MAX_ENTRIES &&
                  flipper_format_read_string(ff, "Entry", value)) {
                WalletEntry* entry = &app->entries[app->entry_count];
                memset(entry, 0, sizeof(WalletEntry));

                const char* p = furi_string_get_cstr(value);
                for(uint8_t i = 0; i < category->field_count; i++) {
                    const char* sep = strchr(p, '|');
                    size_t len = sep ? (size_t)(sep - p) : strlen(p);
                    if(len > WALLET_FIELD_LEN - 1) len = WALLET_FIELD_LEN - 1;
                    memcpy(entry->values[i], p, len);
                    entry->values[i][len] = '\0';
                    p = sep ? sep + 1 : p + strlen(p);
                }

                app->entry_count++;
            }
        }
    }

    furi_string_free(value);
    flipper_format_free(ff);
}

static bool wallet_app_save_entries(WalletApp* app) {
    const WalletCategory* category = &app->categories[app->current_category];

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    bool success = false;

    if(flipper_format_file_open_always(ff, app->file_path)) {
        success = flipper_format_write_header_cstr(ff, WALLET_DATA_TYPE, 1);

        for(uint16_t i = 0; success && i < app->entry_count; i++) {
            char line[WALLET_MAX_FIELDS * (WALLET_FIELD_LEN + 1)];
            line[0] = '\0';

            for(uint8_t f = 0; f < category->field_count; f++) {
                strlcat(line, app->entries[i].values[f], sizeof(line));
                if(f + 1 < category->field_count) strlcat(line, "|", sizeof(line));
            }

            success = flipper_format_write_string_cstr(ff, "Entry", line);
        }
    }

    flipper_format_free(ff);
    return success;
}

/* ---------- field editing ---------- */

static void wallet_app_format_date(const DateTime* date, char* out, size_t out_size) {
    snprintf(out, out_size, "%04u-%02u-%02u", date->year, date->month, date->day);
}

static void wallet_app_parse_date(const char* str, DateTime* out) {
    int year = 0, month = 0, day = 0;
    if(sscanf(str, "%d-%d-%d", &year, &month, &day) == 3 && year >= 2000 && year <= 2099 &&
       month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        out->year = (uint16_t)year;
        out->month = (uint8_t)month;
        out->day = (uint8_t)day;
        out->hour = 0;
        out->minute = 0;
        out->second = 0;
    } else {
        furi_hal_rtc_get_datetime(out);
    }
}

static void wallet_app_commit_field(WalletApp* app, const char* value) {
    const WalletCategory* category = &app->categories[app->current_category];
    WalletEntry* entry = &app->entries[app->selected_index];
    strlcpy(entry->values[app->editing_field], value, WALLET_FIELD_LEN);
    app->new_entry_pending = false;
    wallet_app_save_entries(app);

    if(app->editing_field + 1 < category->field_count) {
        wallet_app_begin_field_edit(app, app->editing_field + 1);
    } else {
        app->current_screen = WalletScreenEntryDetail;
        wallet_app_show_detail(app, app->selected_index);
        view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryDetail);
    }
}

static void wallet_app_text_input_callback(void* context) {
    WalletApp* app = context;
    wallet_app_commit_field(app, app->edit_buffer);
}

static void wallet_app_number_input_callback(void* context, int32_t number) {
    WalletApp* app = context;
    char buffer[WALLET_FIELD_LEN];
    snprintf(buffer, sizeof(buffer), "%ld", (long)number);
    wallet_app_commit_field(app, buffer);
}

static void wallet_app_date_input_done_callback(void* context) {
    WalletApp* app = context;
    char buffer[WALLET_FIELD_LEN];
    wallet_app_format_date(&app->edit_datetime, buffer, sizeof(buffer));
    wallet_app_commit_field(app, buffer);
}

static void wallet_app_begin_field_edit(WalletApp* app, uint8_t field_index) {
    const WalletCategory* category = &app->categories[app->current_category];
    const WalletField* field = &category->fields[field_index];
    WalletEntry* entry = &app->entries[app->selected_index];

    app->editing_field = field_index;
    app->current_screen = WalletScreenEditing;

    if(field->type == WalletFieldTypeNumber) {
        int32_t current = entry->values[field_index][0] ? atoi(entry->values[field_index]) : 0;
        number_input_set_header_text(app->number_input, field->label);
        number_input_set_result_callback(
            app->number_input,
            wallet_app_number_input_callback,
            app,
            current,
            INT32_MIN,
            INT32_MAX);
        view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewNumberInput);
    } else if(field->type == WalletFieldTypeDate) {
        wallet_app_parse_date(entry->values[field_index], &app->edit_datetime);
        date_time_input_set_editable_fields(
            app->date_input, true, true, true, false, false, false);
        date_time_input_set_result_callback(
            app->date_input, NULL, wallet_app_date_input_done_callback, app, &app->edit_datetime);
        view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewDateInput);
    } else {
        memset(app->edit_buffer, 0, sizeof(app->edit_buffer));
        strlcpy(app->edit_buffer, entry->values[field_index], sizeof(app->edit_buffer));

        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, field->label);
        text_input_set_result_callback(
            app->text_input,
            wallet_app_text_input_callback,
            app,
            app->edit_buffer,
            sizeof(app->edit_buffer),
            false);
        view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewTextInput);
    }
}

/* ---------- entry detail ---------- */

static void wallet_app_detail_button_callback(
    GuiButtonType button_type,
    InputType input_type,
    void* context) {
    WalletApp* app = context;
    if(button_type == GuiButtonTypeCenter && input_type == InputTypeShort) {
        wallet_app_begin_field_edit(app, 0);
    }
}

static void wallet_app_show_detail(WalletApp* app, uint16_t index) {
    const WalletCategory* category = &app->categories[app->current_category];
    const WalletEntry* entry = &app->entries[index];

    FuriString* text = furi_string_alloc();
    for(uint8_t i = 0; i < category->field_count; i++) {
        furi_string_cat_printf(text, "%s: %s\n", category->fields[i].label, entry->values[i]);
    }

    widget_reset(app->detail_widget);
    widget_add_string_multiline_element(
        app->detail_widget, 4, 2, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
    widget_add_button_element(
        app->detail_widget, GuiButtonTypeCenter, "Edit", wallet_app_detail_button_callback, app);

    furi_string_free(text);
}

static void wallet_app_open_detail(WalletApp* app, uint16_t index) {
    app->selected_index = index;
    app->current_screen = WalletScreenEntryDetail;
    wallet_app_show_detail(app, index);
    view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryDetail);
}

static void wallet_app_add_new_entry(WalletApp* app) {
    if(app->entry_count >= WALLET_MAX_ENTRIES) return;

    WalletEntry* entry = &app->entries[app->entry_count++];
    memset(entry, 0, sizeof(WalletEntry));

    app->selected_index = app->entry_count - 1;
    app->new_entry_pending = true;
    wallet_app_begin_field_edit(app, 0);
}

static void wallet_app_delete_entry(WalletApp* app, uint16_t index) {
    if(index >= app->entry_count) return;

    for(uint16_t i = index; i + 1 < app->entry_count; i++) {
        app->entries[i] = app->entries[i + 1];
    }
    app->entry_count--;
    wallet_app_save_entries(app);
}

/* ---------- entry list (custom view: selection dots + long-press-right edit) ---------- */

static void wallet_app_entry_list_draw_callback(Canvas* canvas, void* model_ptr) {
    EntryListModel* model = model_ptr;
    WalletApp* app = model->app;
    const WalletCategory* category = &app->categories[app->current_category];
    const uint16_t total_items = 1 + app->entry_count;
    const uint8_t item_width = canvas_width(canvas) - 5;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 11, category->name);
    canvas_set_font(canvas, FontSecondary);

    for(uint16_t i = model->window_position;
        i < total_items && (i - model->window_position) < WALLET_LIST_VISIBLE_ITEMS;
        i++) {
        const uint16_t row = i - model->window_position;
        const uint8_t y_top = WALLET_LIST_ITEM_HEIGHT + row * WALLET_LIST_ITEM_HEIGHT;
        const bool selected = (i == model->position);
        const bool is_add_new = (i == app->entry_count);

        char label[48];
        if(is_add_new) {
            strlcpy(label, "+New", sizeof(label));
        } else {
            const WalletEntry* entry = &app->entries[i];
            strlcpy(
                label, entry->values[0][0] ? entry->values[0] : "(unnamed)", sizeof(label));
        }

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(
                canvas, 0, y_top + 1, item_width, WALLET_LIST_ITEM_HEIGHT - 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        FuriString* disp = furi_string_alloc_set(label);
        elements_string_fit_width(canvas, disp, item_width - (selected && !is_add_new ? 16 : 11));
        canvas_draw_str(
            canvas, 6, y_top + WALLET_LIST_ITEM_HEIGHT - 4, furi_string_get_cstr(disp));
        furi_string_free(disp);

        if(selected && !is_add_new) {
            const uint8_t dot_x = item_width - 6;
            const uint8_t dot_y = y_top + WALLET_LIST_ITEM_HEIGHT / 2;
            canvas_draw_disc(canvas, dot_x, dot_y - 4, 1);
            canvas_draw_disc(canvas, dot_x, dot_y, 1);
            canvas_draw_disc(canvas, dot_x, dot_y + 4, 1);
        }
    }

    canvas_set_color(canvas, ColorBlack);
    elements_scrollbar(canvas, model->position, total_items);
}

static void wallet_app_entry_list_process_up(WalletApp* app) {
    with_view_model(
        app->entry_list_view,
        EntryListModel * model,
        {
            const uint16_t total_items = 1 + app->entry_count;
            if(model->position > 0) {
                model->position--;
                if(model->position == model->window_position && model->window_position > 0) {
                    model->window_position--;
                }
            } else {
                model->position = total_items - 1;
                model->window_position = (total_items > WALLET_LIST_VISIBLE_ITEMS) ?
                                              (total_items - WALLET_LIST_VISIBLE_ITEMS) :
                                              0;
            }
        },
        true);
}

static void wallet_app_entry_list_process_down(WalletApp* app) {
    with_view_model(
        app->entry_list_view,
        EntryListModel * model,
        {
            const uint16_t total_items = 1 + app->entry_count;
            if(model->position < total_items - 1) {
                model->position++;
                if(total_items > WALLET_LIST_VISIBLE_ITEMS &&
                   (model->position - model->window_position) > (WALLET_LIST_VISIBLE_ITEMS - 2) &&
                   model->window_position < (total_items - WALLET_LIST_VISIBLE_ITEMS)) {
                    model->window_position++;
                }
            } else {
                model->position = 0;
                model->window_position = 0;
            }
        },
        true);
}

static bool wallet_app_entry_list_input_callback(InputEvent* event, void* context) {
    WalletApp* app = context;
    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        wallet_app_entry_list_process_up(app);
        consumed = true;
    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        wallet_app_entry_list_process_down(app);
        consumed = true;
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        uint16_t position = 0;
        with_view_model(
            app->entry_list_view, EntryListModel * model, { position = model->position; }, false);

        if(position == app->entry_count) {
            wallet_app_add_new_entry(app);
        } else {
            wallet_app_open_detail(app, position);
        }
        consumed = true;
    } else if(event->key == InputKeyRight && event->type == InputTypeLong) {
        uint16_t position = 0;
        with_view_model(
            app->entry_list_view, EntryListModel * model, { position = model->position; }, false);

        if(position < app->entry_count) {
            app->selected_index = position;
            app->current_screen = WalletScreenEntryMenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryMenu);
        }
        consumed = true;
    }

    return consumed;
}

static void wallet_app_entry_list_reset(WalletApp* app) {
    with_view_model(
        app->entry_list_view,
        EntryListModel * model,
        {
            model->position = 0;
            model->window_position = 0;
        },
        true);
}

static View* wallet_app_entry_list_alloc(WalletApp* app) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(EntryListModel));
    view_set_draw_callback(view, wallet_app_entry_list_draw_callback);
    view_set_input_callback(view, wallet_app_entry_list_input_callback);

    with_view_model(
        view,
        EntryListModel * model,
        {
            model->app = app;
            model->position = 0;
            model->window_position = 0;
        },
        true);

    return view;
}

/* ---------- top-level navigation ---------- */

static void wallet_app_main_menu_callback(void* context, uint32_t index) {
    WalletApp* app = context;
    if(index >= app->category_count) return;

    app->current_category = (uint8_t)index;
    app->current_screen = WalletScreenEntryList;
    wallet_app_set_file_path(app);
    wallet_app_load_entries(app);
    wallet_app_entry_list_reset(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryList);
}

static void wallet_app_entry_menu_callback(void* context, uint32_t index) {
    WalletApp* app = context;

    if(index == 0) {
        wallet_app_begin_field_edit(app, 0);
        return;
    }

    wallet_app_delete_entry(app, app->selected_index);

    with_view_model(
        app->entry_list_view,
        EntryListModel * model,
        {
            const uint16_t total_items = 1 + app->entry_count;
            if(model->position >= total_items) model->position = total_items - 1;
            if(total_items > WALLET_LIST_VISIBLE_ITEMS) {
                if(model->window_position > total_items - WALLET_LIST_VISIBLE_ITEMS) {
                    model->window_position = total_items - WALLET_LIST_VISIBLE_ITEMS;
                }
            } else {
                model->window_position = 0;
            }
        },
        true);

    app->current_screen = WalletScreenEntryList;
    view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryList);
}

static bool wallet_app_navigation_callback(void* context) {
    WalletApp* app = context;

    if(app->current_screen == WalletScreenEditing) {
        if(app->new_entry_pending) {
            // Back must never persist a partially-created entry.
            app->entry_count--;
            app->new_entry_pending = false;
            app->current_screen = WalletScreenEntryList;
            view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryList);
        } else {
            app->current_screen = WalletScreenEntryDetail;
            wallet_app_show_detail(app, app->selected_index);
            view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryDetail);
        }
    } else if(app->current_screen == WalletScreenEntryMenu) {
        app->current_screen = WalletScreenEntryList;
        view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryList);
    } else if(app->current_screen == WalletScreenEntryDetail) {
        app->current_screen = WalletScreenEntryList;
        view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewEntryList);
    } else if(app->current_screen == WalletScreenEntryList) {
        app->current_screen = WalletScreenMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewMainMenu);
    } else {
        view_dispatcher_stop(app->view_dispatcher);
    }

    return true;
}

/* ---------- app lifecycle ---------- */

static WalletApp* wallet_app_alloc(void) {
    WalletApp* app = malloc(sizeof(WalletApp));
    memset(app, 0, sizeof(WalletApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    wallet_app_ensure_data_dir(app);
    wallet_app_load_config(app);

    app->main_menu = submenu_alloc();
    for(uint8_t i = 0; i < app->category_count; i++) {
        submenu_add_item(
            app->main_menu, app->categories[i].name, i, wallet_app_main_menu_callback, app);
    }

    app->entry_list_view = wallet_app_entry_list_alloc(app);
    app->detail_widget = widget_alloc();
    app->text_input = text_input_alloc();
    app->number_input = number_input_alloc();
    app->date_input = date_time_input_alloc();

    app->entry_action_menu = submenu_alloc();
    submenu_add_item(app->entry_action_menu, "Edit", 0, wallet_app_entry_menu_callback, app);
    submenu_add_item(app->entry_action_menu, "Delete", 1, wallet_app_entry_menu_callback, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_add_view(
        app->view_dispatcher, WalletViewMainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(app->view_dispatcher, WalletViewEntryList, app->entry_list_view);
    view_dispatcher_add_view(
        app->view_dispatcher, WalletViewEntryDetail, widget_get_view(app->detail_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, WalletViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, WalletViewNumberInput, number_input_get_view(app->number_input));
    view_dispatcher_add_view(
        app->view_dispatcher, WalletViewDateInput, date_time_input_get_view(app->date_input));
    view_dispatcher_add_view(
        app->view_dispatcher, WalletViewEntryMenu, submenu_get_view(app->entry_action_menu));

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, wallet_app_navigation_callback);

    return app;
}

static void wallet_app_free(WalletApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, WalletViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, WalletViewEntryList);
    view_dispatcher_remove_view(app->view_dispatcher, WalletViewEntryDetail);
    view_dispatcher_remove_view(app->view_dispatcher, WalletViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, WalletViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, WalletViewDateInput);
    view_dispatcher_remove_view(app->view_dispatcher, WalletViewEntryMenu);
    view_dispatcher_free(app->view_dispatcher);

    submenu_free(app->main_menu);
    view_free(app->entry_list_view);
    widget_free(app->detail_widget);
    text_input_free(app->text_input);
    number_input_free(app->number_input);
    date_time_input_free(app->date_input);
    submenu_free(app->entry_action_menu);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t wallet_app_main(void* p) {
    UNUSED(p);
    WalletApp* app = wallet_app_alloc();

    app->current_screen = WalletScreenMainMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, WalletViewMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    wallet_app_free(app);
    return 0;
}
