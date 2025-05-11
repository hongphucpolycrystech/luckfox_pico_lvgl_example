#ifndef LVGL_CHART_H
#define LVGL_CHART_H

#include <vector>
#include "lvgl/lvgl.h"
void chart_create();
void chart_update(const std::vector<int>& data);
lv_obj_t* create_rpd_histogram(const std::vector<int>& rpd_data);
#endif
