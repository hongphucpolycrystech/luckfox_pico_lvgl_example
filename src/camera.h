#ifndef CAMERA_H
#define CAMERA_H
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void camera_start() ;
void camera_stop() ;
void camera_init() ;
void camera_capture(cv::Mat *rgba_frame);
void camera_get_frame(uint8_t *buf,int w, int h);
void camera_lvgl_init(const char *dev_path);
void camera_lvgl_start_streaming(void);
void camera_lvgl_stop_streaming(void);
uint32_t custom_tick_get(void);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_H
