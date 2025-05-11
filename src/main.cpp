#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <camera.h>
#include <lv_conf.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "MLX90640_lvgl.h"

#define DISPLAY_SIZE 480
#define IMG_WIDTH_SIZE 480
#define IMG_HIGH_SIZE 320
static uint8_t img_buf[IMG_WIDTH_SIZE * IMG_HIGH_SIZE * 4]; // RGBA
static lv_img_dsc_t img_dsc = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,//LV_IMG_CF_TRUE_COLOR_ALPHA,
        .always_zero = 0,
        .w = IMG_WIDTH_SIZE,
        .h = IMG_HIGH_SIZE,
    },
    .data_size = sizeof(img_buf),
    .data = img_buf
};

//#include <opencv2/opencv.hpp>
#include <algorithm> // for std::clamp
#include <cmath>
template <typename T>
T clamp(T value, T min_val, T max_val) {
    return (value < min_val) ? min_val : (value > max_val) ? max_val : value;
}
// Optional: apply colormap (e.g., inferno, jet) manually or with OpenCV

void temperature_to_bgr(const float* temp_data, int width, int height, float t_min, float t_max, cv::Mat& output_bgr)
{
    // Create grayscale image from temperature
    cv::Mat gray_img(height, width, CV_8UC1);
    for (int i = 0; i < width * height; ++i) {
        float norm = (temp_data[i] - t_min) / (t_max - t_min);
        norm = clamp(norm, 0.0f, 1.0f);
        gray_img.data[i] = static_cast<uint8_t>(norm * 255.0f);
    }

    // Convert to BGR using a colormap
    cv::applyColorMap(gray_img, output_bgr, cv::COLORMAP_JET);  // Or COLORMAP_INFERNO, etc.
}
const int WIDTH = 32;
const int HEIGHT = 24;
float temperature_data[WIDTH * HEIGHT];

void generate_sample_temperature_data(float* data, int width, int height, float min_temp, float max_temp)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Tạo gradient nhiệt độ theo chiều ngang
            float t = min_temp + (max_temp - min_temp) * (float)x / (width - 1);
            data[y * width + x] = t;
        }
    }
}

int main(void) {
    lv_init();
   // /* Framebuffer device initialization */
    fbdev_init();
   static lv_disp_draw_buf_t draw_buf;
   static lv_color_t buf[480 * 10];
   lv_disp_draw_buf_init(&draw_buf, buf, NULL, 480 * 10);

   static lv_disp_drv_t disp_drv;
   lv_disp_drv_init(&disp_drv);
   disp_drv.draw_buf = &draw_buf;
   disp_drv.flush_cb = fbdev_flush;
   disp_drv.hor_res = 480;
   disp_drv.ver_res = 480;
   lv_disp_drv_register(&disp_drv);

   /* Input device (touchscreen GT911) */
   evdev_init();
   static lv_indev_drv_t indev_drv;
   lv_indev_drv_init(&indev_drv);
   indev_drv.type = LV_INDEV_TYPE_POINTER;
   indev_drv.read_cb = evdev_read;
   lv_indev_drv_register(&indev_drv);

    lv_obj_t *g_img_obj = lv_img_create(lv_scr_act());
    lv_obj_set_size(g_img_obj, IMG_WIDTH_SIZE, IMG_HIGH_SIZE);
    //lv_obj_center(g_img_obj);
    lv_obj_align(g_img_obj, LV_ALIGN_TOP_MID, 0, 0);
    lv_img_set_src(g_img_obj, &img_dsc);

    float temperature_data[768]; // 32x24 = 768
    // Fill temperature_data with your thermal sensor data
    generate_sample_temperature_data(temperature_data, 32, 24, 20.0f, 40.0f); // Min/max temperature in °C
    cv::Mat bgr_image=cv::Mat(24, 32, CV_8UC3, cv::Scalar(0, 0, 0)); // black color;
    cv::Mat *bgr_image_lvgl=new cv::Mat(IMG_HIGH_SIZE, IMG_WIDTH_SIZE, CV_8UC3, cv::Scalar(0, 0, 0)); // black color;
    temperature_to_bgr(temperature_data, 32, 24, 20.0f, 40.0f, bgr_image);  // Min/max temperature in °C

   camera_init() ;
   camera_start();
   mlx90640_init();
   cv::Mat *brg_frame= new cv::Mat(IMG_HIGH_SIZE, IMG_WIDTH_SIZE, CV_8UC3, cv::Scalar(0, 0, 0)); // black color
   cv::Mat  rgba;
   while (true) {
        get_thermal_data(temperature_data);
        temperature_to_bgr(temperature_data, 32, 24, 20.0f, 40.0f, bgr_image);  // Min/max temperature in °C

        //camera_get_frame(img_buf,IMG_WIDTH_SIZE, IMG_HIGH_SIZE);
        cv::resize(bgr_image, *bgr_image_lvgl, cv::Size(480, 320), 0, 0, cv::INTER_CUBIC);      
        camera_capture(brg_frame);
        //cv::rotate(*brg_frame, *brg_frame, cv::ROTATE_180);                // 180 độ
        cv::flip(*brg_frame, *brg_frame, -1);
        float alpha = 0.4f; // mức độ ảnh nhiệt phủ lên, từ 0.0 đến 1.0
        cv::Mat blended;
        cv::addWeighted(*brg_frame, 1.0f - alpha, *bgr_image_lvgl, alpha, 0.0, blended);
        //cv::cvtColor(*brg_frame, rgba, cv::COLOR_BGR2BGRA);
        //cv::cvtColor(*bgr_image_lvgl, rgba, cv::COLOR_BGR2BGRA);
        cv::cvtColor(blended, rgba, cv::COLOR_BGR2BGRA);

        // Copy to LVGL buffer
        memcpy(img_buf, rgba.data, rgba.total() * rgba.elemSize()); 
        
        //memcpy(img_buf, brg_frame->data, brg_frame->total() * brg_frame->elemSize()); 

        
        lv_img_set_src(g_img_obj, &img_dsc);
        // Cập nhật LVGL
        lv_timer_handler();
        usleep(10000); // Đợi một chút
    }
    
    return 0;
}

// #include <opencv2/core/core.hpp>
// #include <opencv2/highgui/highgui.hpp>
// #include <opencv2/imgproc/imgproc.hpp>
// #include <lvgl.h>
// #include <lv_conf.h>
// #include "lv_drivers/display/fbdev.h"
// #include "lv_drivers/indev/evdev.h"
// #include "thermal_display.h"
// #include <iostream>
// #include <sys/time.h>
// #include <unistd.h>
// #include <stdio.h>
// #include <stdlib.h>
// static cv::VideoCapture cap;
// // Trả về khung hình 480x480, định dạng BGRA (phù hợp với LV_IMG_CF_TRUE_COLOR_ALPHA)
// cv::Mat capture_frame(cv::VideoCapture &cap) {
//     cv::Mat frame_bgr;
//     cap >> frame_bgr;

//     if (frame_bgr.empty()) {
//         // Nếu không có hình ảnh, trả về khung đen
//         return cv::Mat(480, 480, CV_8UC4, cv::Scalar(0, 0, 0, 255));
//     }

//     // Resize về hình vuông 480x480, crop nếu cần
//     int w = frame_bgr.cols;
//     int h = frame_bgr.rows;
//     int crop = std::min(w, h);
//     cv::Rect roi((w - crop)/2, (h - crop)/2, crop, crop);
//     cv::Mat cropped = frame_bgr(roi);

//     cv::Mat resized;
//     cv::resize(cropped, resized, cv::Size(480, 480));

//     // Chuyển sang BGRA (4 channel) để dùng với LV_IMG_CF_TRUE_COLOR_ALPHA
//     cv::Mat frame_bgra;
//     cv::cvtColor(resized, frame_bgra, cv::COLOR_BGR2BGRA);

//     return frame_bgra;
// }

// // void update_frame_with_thermal_data(const cv::Mat &rgb_frame, const float *mlx90640_frame) {
// //      uint8_t rgb_data[RGB_WIDTH * RGB_HEIGHT * 3];
    
// //      cv::Mat resized, rgba;

// //      // Resize and convert to RGBA
// //      cv::resize(rgb_frame, resized, cv::Size(480, 480));
// //      cv::cvtColor(resized, rgba, cv::COLOR_BGR2BGRA);
 
// //      // Copy to LVGL buffer
// //      memcpy(rgb_data, rgba.data, rgba.total() * rgba.elemSize());
    
// //     // // Cập nhật dữ liệu nhiệt và ghép vào ảnh RGB
// //     thermal_display_update_frame(mlx90640_frame, rgb_data);
// // }

// int main() {
//     lv_init();  // Khởi tạo LVGL
//      // /* Framebuffer device initialization */
//      fbdev_init();
//     static lv_disp_draw_buf_t draw_buf;
//     static lv_color_t buf[480 * 10];
//     lv_disp_draw_buf_init(&draw_buf, buf, NULL, 480 * 10);

//     static lv_disp_drv_t disp_drv;
//     lv_disp_drv_init(&disp_drv);
//     disp_drv.draw_buf = &draw_buf;
//     disp_drv.flush_cb = fbdev_flush;
//     disp_drv.hor_res = 480;
//     disp_drv.ver_res = 480;
//     lv_disp_drv_register(&disp_drv);

//     /* Input device (touchscreen GT911) */
//     evdev_init();
//     static lv_indev_drv_t indev_drv;
//     lv_indev_drv_init(&indev_drv);
//     indev_drv.type = LV_INDEV_TYPE_POINTER;
//     indev_drv.read_cb = evdev_read;
//     lv_indev_drv_register(&indev_drv);

//     // Khởi tạo webcam
//     // cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
//     // cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
//     cap.open(0); // Mở camera 0

//     if (!cap.isOpened()) {
//         fprintf(stderr, "Failed to open camera\n");
//         return 0;
//     }

//     // Tạo màn hình và khởi tạo các đối tượng
//     lv_obj_t *scr = lv_scr_act();
//     thermal_display_init(scr);
//     cv::Mat frame;
//     // Dữ liệu nhiệt giả lập từ MLX90640 (thay thế bằng dữ liệu thực)
//     float mlx90640_frame[THERMAL_WIDTH * THERMAL_HEIGHT] = { /* Dữ liệu nhiệt giả lập */ };
        
//     while (true) {
//         // Lấy ảnh webcam
//         frame = capture_frame(cap);
        
        
//         // Cập nhật ảnh với dữ liệu nhiệt
//         update_frame_with_thermal_data(frame, mlx90640_frame);
        
//         // Cập nhật LVGL
//         lv_timer_handler();
//         usleep(10000); // Đợi một chút
//     }

//     return 0;
// }
