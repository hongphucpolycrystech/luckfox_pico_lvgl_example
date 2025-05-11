#include "lvgl_chart.h"
#include "lvgl/lvgl.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static lv_obj_t* chart;
static lv_chart_series_t* series;

void chart_create() {
    chart = lv_chart_create(lv_scr_act());
    lv_obj_set_size(chart, 480, 240);
    lv_obj_center(chart);

    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
    lv_chart_set_point_count(chart, 126);

    series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
}

void chart_update(const std::vector<int>& data) {
    for (int i = 0; i < 126; i++) {
        series->y_points[i] = data[i];
    }
    lv_chart_refresh(chart);
}

lv_obj_t* create_rpd_histogram(const std::vector<int>& rpd_data) {
    // lv_obj_t* chart = lv_chart_create(lv_scr_act());
    // lv_obj_set_size(chart, 480, 240);  // Thay theo kích thước LCD
    // lv_obj_center(chart);

    // lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    // lv_chart_set_point_count(chart, rpd_data.size());
    // lv_chart_set_div_line_count(chart, 5, 10);  // Đường chia

    // lv_chart_series_t* ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    // int max_val = 0;
    // for (int val : rpd_data) {
    //     if (val > max_val) max_val = val;
    // }

    // // Normalize nếu muốn hiển thị % thay vì giá trị tuyệt đối
    // for (size_t i = 0; i < rpd_data.size(); ++i) {
    //     int norm = max_val > 0 ? (rpd_data[i] * 100 / max_val) : 0;
    //     ser->y_points[i] = norm;
    // }

    // lv_chart_refresh(chart);  // Update display

    lv_obj_t* chart = lv_chart_create(lv_scr_act());
    static lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_obj_set_size(chart, 480, 150);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart, 126);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100); // 0–100%

    lv_color_t blue = lv_color_make(0, 0, 255);
    lv_chart_series_t* ser = lv_chart_add_series(chart, blue, LV_CHART_AXIS_PRIMARY_Y);
    int max_val = -1;
    int max_ch = -1;

    for (int i = 0; i < 126; i++) {
        ser->y_points[i] = rpd_data[i];
        if (rpd_data[i] > max_val) {
            max_val = rpd_data[i];
            max_ch = i;
        }
    }
    lv_chart_refresh(chart);
     // Tính tần số tương ứng với channel
     int freq_mhz = 2400 + max_ch;

    // Hiển thị kênh có histogram lớn nhất (nhiễu mạnh nhất)
    char msg[128];
    snprintf(msg, sizeof(msg),
             "Max noise @ CH%d (%d MHz): %d%%",
             max_ch, freq_mhz, max_val);
   
    lv_label_set_text(label, msg);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -10);

    return chart;
}