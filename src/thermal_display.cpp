#include "thermal_display.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <lvgl.h>
#include <unistd.h>

static lv_obj_t *thermal_img = nullptr;
static lv_obj_t *bar_img = nullptr;
static lv_obj_t *slider_min = nullptr;
static lv_obj_t *slider_max = nullptr;
static lv_obj_t *label_min = nullptr;
static lv_obj_t *label_max = nullptr;

static float temp_min = 20.0f;
static float temp_max = 40.0f;

static uint8_t thermal_colormap[256][3];
static cv::Mat frame_bgra(RGB_HEIGHT, RGB_WIDTH, CV_8UC4, cv::Scalar(0, 0, 0, 0));  // Khởi tạo frame BGRA

static lv_img_dsc_t bar_img_dsc;
static lv_img_dsc_t thermal_img_dsc;
template <typename T>
T clamp(T value, T min_val, T max_val) {
    return (value < min_val) ? min_val : (value > max_val) ? max_val : value;
}
// --- Nội suy nhiệt độ sang màu ---
static void temperature_to_rgb(float temp, uint8_t &r, uint8_t &g, uint8_t &b) {
    float t = (temp - temp_min) / (temp_max - temp_min);
    t = clamp(t, 0.0f, 1.0f);
    uint8_t index = static_cast<uint8_t>(t * 255);
    r = thermal_colormap[index][0];
    g = thermal_colormap[index][1];
    b = thermal_colormap[index][2];
}


static void generate_colormap() {
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        uint8_t r = static_cast<uint8_t>(255 * clamp(1.5f - std::fabs(4.0f * t - 3.0f), 0.0f, 1.0f));
        uint8_t g = static_cast<uint8_t>(255 * clamp(1.5f - std::fabs(4.0f * t - 2.0f), 0.0f, 1.0f));
        uint8_t b = static_cast<uint8_t>(255 * clamp(1.5f - std::fabs(4.0f * t - 1.0f), 0.0f, 1.0f));
        thermal_colormap[i][0] = r;
        thermal_colormap[i][1] = g;
        thermal_colormap[i][2] = b;
    }
}

static void update_color_bar() {
    uint8_t bar_buf[20 * 256];
    for (int y = 0; y < 256; ++y) {
        uint8_t *c = thermal_colormap[y];
        lv_color_t color = lv_color_make(c[0], c[1], c[2]);
        for (int x = 0; x < 20; ++x) {
            //bar_buf[y * 20 + x] = color;
            // Trích xuất RGBA từ lv_color_t (giả sử bạn dùng RGB không có alpha)
            uint8_t r_out = color.ch.red;
            uint8_t g_out = color.ch.green;
            uint8_t b_out = color.ch.blue;

            // Nếu cần gán các giá trị này vào mảng `bar_buf`, thay vì sử dụng `lv_color_t`, bạn có thể làm:
            bar_buf[y * 20 + x] = (r_out << 16) | (g_out << 8) | b_out;  // Chuyển thành một giá trị 24-bit
        }
    }
    bar_img_dsc.header.always_zero = 0;
    bar_img_dsc.header.w = 20;
    bar_img_dsc.header.h = 256;
    bar_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    bar_img_dsc.data = reinterpret_cast<const uint8_t *>(bar_buf);
    bar_img_dsc.data_size = sizeof(bar_buf);

    lv_img_set_src(bar_img, &bar_img_dsc);
}

static void slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);

    if (slider == slider_min) {
        temp_min = val / 10.0f;
        lv_label_set_text_fmt(label_min, "Min: %.1f°C", temp_min);
    } else if (slider == slider_max) {
        temp_max = val / 10.0f;
        lv_label_set_text_fmt(label_max, "Max: %.1f°C", temp_max);
    }
    generate_colormap();
    update_color_bar();
}

void thermal_display_init(lv_obj_t *parent) {
    generate_colormap();

    thermal_img = lv_img_create(parent);
    lv_obj_set_size(thermal_img, RGB_WIDTH, RGB_HEIGHT);

    bar_img = lv_img_create(parent);
    lv_obj_align(bar_img, LV_ALIGN_TOP_RIGHT, -10, 10);

    label_min = lv_label_create(parent);
    lv_obj_align(label_min, LV_ALIGN_BOTTOM_RIGHT, -10, -70);
    label_max = lv_label_create(parent);
    lv_obj_align(label_max, LV_ALIGN_BOTTOM_RIGHT, -10, -20);

    slider_min = lv_slider_create(parent);
    lv_obj_set_size(slider_min, 20, 100);
    lv_slider_set_range(slider_min, 0, 1000);
    lv_obj_align(slider_min, LV_ALIGN_BOTTOM_RIGHT, -40, -70);
    lv_obj_add_event_cb(slider_min, slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_slider_set_value(slider_min, temp_min * 10, LV_ANIM_OFF);

    slider_max = lv_slider_create(parent);
    lv_obj_set_size(slider_max, 20, 100);
    lv_slider_set_range(slider_max, 0, 1000);
    lv_obj_align(slider_max, LV_ALIGN_BOTTOM_RIGHT, -70, -20);
    lv_obj_add_event_cb(slider_max, slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_slider_set_value(slider_max, temp_max * 10, LV_ANIM_OFF);

    update_color_bar();
}

void update_frame_with_thermal_data(cv::Mat &frame_bgra, const float *mlx90640_frame) {
    // Duyệt qua từng pixel trong frame_bgra và cập nhật với dữ liệu nhiệt độ
    for (int ty = 0; ty < THERMAL_HEIGHT; ++ty) {
        for (int tx = 0; tx < THERMAL_WIDTH; ++tx) {
            int idx = ty * THERMAL_WIDTH + tx;
            float temp = mlx90640_frame[idx];  // Lấy nhiệt độ tại điểm ảnh (tx, ty)

            // Chuyển đổi nhiệt độ thành màu RGB
            uint8_t r, g, b;
            temperature_to_rgb(temp, r, g, b);
            
            // Thiết lập Alpha là 255 cho BGRA (hoàn toàn không trong suốt)
            uint8_t a = 255;
            
            // Mở rộng nhiệt độ sang toàn bộ vùng ảnh
            for (int y = 0; y < SCALE_Y; ++y) {
                for (int x = 0; x < SCALE_X; ++x) {
                    int px = tx * SCALE_X + x;
                    int py = ty * SCALE_Y + y;
                    if (px < RGB_WIDTH && py < RGB_HEIGHT) {
                        // Cập nhật pixel trong frame_bgra với màu BGRA
                        frame_bgra.at<cv::Vec4b>(py, px) = cv::Vec4b(b, g, r, 255); // BGRA format
                    }
                }
            }
        }
    }

    // Chuyển đổi từ frame_bgra sang lv_img_dsc_t và cập nhật trên LVGL
    thermal_img_dsc.header.always_zero = 0;
    thermal_img_dsc.header.w = RGB_WIDTH;
    thermal_img_dsc.header.h = RGB_HEIGHT;
    thermal_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    thermal_img_dsc.data = frame_bgra.data;
    thermal_img_dsc.data_size = frame_bgra.total() * frame_bgra.elemSize();

    lv_img_set_src(thermal_img, &thermal_img_dsc);
}
