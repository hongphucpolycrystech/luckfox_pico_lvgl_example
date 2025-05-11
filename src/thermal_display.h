// thermal_display.h
#ifndef THERMAL_DISPLAY_H
#define THERMAL_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define THERMAL_WIDTH      32
#define THERMAL_HEIGHT     24
#define RGB_WIDTH          640
#define RGB_HEIGHT         480

// Cấu hình tỉ lệ scale giữa ảnh nhiệt và ảnh RGB
#define SCALE_X (RGB_WIDTH / THERMAL_WIDTH)
#define SCALE_Y (RGB_HEIGHT / THERMAL_HEIGHT)

#ifdef __cplusplus
extern "C" {
#endif

void thermal_display_init(lv_obj_t *parent);
void update_frame_with_thermal_data(cv::Mat &frame_bgra, const float *mlx90640_frame);

#ifdef __cplusplus
}
#endif

#endif // THERMAL_DISPLAY_H