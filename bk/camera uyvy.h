#ifndef CAMERA_H
#define CAMERA_H

#include "lvgl.h"

void camera_lvgl_init(const char *dev_path);
void camera_lvgl_start_streaming(void);
void camera_lvgl_stop_streaming(void);

#endif // CAMERA_H
