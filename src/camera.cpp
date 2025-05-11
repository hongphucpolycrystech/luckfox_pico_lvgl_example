#include "camera.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <lvgl.h>
#include <unistd.h>
#include <iostream>

#define DISPLAY_SIZE 480

static lv_obj_t *g_img_obj = nullptr;
static lv_timer_t *g_cam_timer = nullptr;
static cv::VideoCapture cap;
static uint8_t img_buf[DISPLAY_SIZE * DISPLAY_SIZE * 4]; // RGBA

static lv_img_dsc_t img_dsc = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,//LV_IMG_CF_TRUE_COLOR_ALPHA,
        .always_zero = 0,
        .w = DISPLAY_SIZE,
        .h = DISPLAY_SIZE,
    },
    .data_size = sizeof(img_buf),
    .data = img_buf
};
void camera_start() {
    cap.open(0); // Mở camera 0

    if (!cap.isOpened()) {
        fprintf(stderr, "Failed to open camera\n");
        return;
    }
}
void camera_stop() {
    if (cap.isOpened()) {
        cap.release();
    }
}
void camera_init() {
    // Khởi tạo camera
    // cap.set(cv::CAP_PROP_FRAME_WIDTH, 480);
    // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 320);
}
static void opencv_to_lvgl_rgba(const cv::Mat &frame, uint8_t *buf) {
    cv::Mat resized, rgba;

    // Resize and convert to RGBA
    cv::resize(frame, resized, cv::Size(DISPLAY_SIZE, DISPLAY_SIZE));
    cv::cvtColor(resized, rgba, cv::COLOR_BGR2BGRA);

    // Copy to LVGL buffer
    memcpy(buf, rgba.data, rgba.total() * rgba.elemSize());
}
// Chuyển đổi ảnh từ OpenCV sang LVGL
void camera_get_frame(uint8_t *buf,int w, int h) {
    if (!cap.isOpened()) return;

    cv::Mat frame;
    cap >> frame;
    std::cout << "Type: " << frame.type() << ", Channels: " << frame.channels() << std::endl;
    if (frame.empty()) return;

    cv::Mat resized, rgba;

    // Resize and convert to RGBA
    cv::resize(frame, resized, cv::Size(w, h));
    cv::cvtColor(resized, rgba, cv::COLOR_BGR2BGRA);

    // Copy to LVGL buffer
    memcpy(buf, rgba.data, rgba.total() * rgba.elemSize());   
}

void camera_capture(cv::Mat *brg_frame) {
    if (!cap.isOpened()) return;

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return;    
    cv::resize(frame, *brg_frame, cv::Size(brg_frame->cols, brg_frame->rows));
     
}

static void camera_timer_cb(lv_timer_t *timer) {
    if (!cap.isOpened()) return;

    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) return;

    opencv_to_lvgl_rgba(frame, img_buf);
    lv_img_set_src(g_img_obj, &img_dsc);
}

void camera_lvgl_start_streaming() {
    cap.open(0); // Mở camera 0

    if (!cap.isOpened()) {
        fprintf(stderr, "Failed to open camera\n");
        return;
    }

    g_img_obj = lv_img_create(lv_scr_act());
    lv_obj_set_size(g_img_obj, DISPLAY_SIZE, DISPLAY_SIZE);
    lv_obj_center(g_img_obj);
    lv_img_set_src(g_img_obj, &img_dsc);

    g_cam_timer = lv_timer_create(camera_timer_cb,66, NULL);
}

void camera_lvgl_stop_streaming() {
    if (g_cam_timer) {
        lv_timer_del(g_cam_timer);
        g_cam_timer = nullptr;
    }
    if (cap.isOpened()) {
        cap.release();
    }
}













