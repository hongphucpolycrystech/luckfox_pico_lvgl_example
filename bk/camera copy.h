#ifndef CAMERA_H
#define CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Khởi tạo camera với đường dẫn device
void camera_lvgl_init(const char *dev_path);

// Bắt đầu streaming và hiển thị lên LVGL
void camera_lvgl_start_streaming(void);

// Dừng streaming
void camera_lvgl_stop_streaming(void);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_H
