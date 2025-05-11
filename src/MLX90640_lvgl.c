#include "lvgl.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <pthread.h>

#define MLX90640_ADDR 0x33
#define MLX_WIDTH 32
#define MLX_HEIGHT 24

// Dữ liệu nhiệt
static uint16_t mlx90640Frame[834];
static float mlx90640To[MLX_WIDTH * MLX_HEIGHT];
static paramsMLX90640 mlx90640Params;

// Ảnh LVGL
static lv_obj_t *img_obj;
static lv_color32_t img_buf[MLX_WIDTH*10 * MLX_HEIGHT*10];

// Chuyển nhiệt độ thành màu
// static lv_color_t temp_to_color(float t) {
//     uint8_t r = 0, g = 0, b = 0;
//     if (t < 20) t = 20;
//     if (t > 40) t = 40;
//     float ratio = (t - 20.0f) / 20.0f;

//     r = (uint8_t)(ratio * 255);
//     g = (uint8_t)((1.0f - ratio) * 255);
//     b = 255 - abs(128 - r);

//     return lv_color_make(r, g, b);  // Chuẩn RGB565
// }

static lv_color32_t temp_to_color(float temp) {
    float min_temp=35.0f;
    float max_temp = 37.0f;
    float ratio = (temp - min_temp) / (max_temp - min_temp);
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;

    uint8_t r = (uint8_t)(255 * ratio);
    uint8_t g = (uint8_t)(255 * (1 - ratio));
    uint8_t b = 0;
    return (lv_color32_t){ .ch.red = r, .ch.green = g, .ch.blue = b, .ch.alpha = 255 };
}

// // Chuyển nhiệt độ thành màu (một gradient đơn giản từ xanh dương đến đỏ)
// static lv_color_t map_temp_to_color(float temp) {
//     uint8_t r = 0, g = 0, b = 0;
//     if (temp < 65.0f) {
//         r = 0; g = 0; b = 255;
//     } else if (temp < 75.0f) {
//         float ratio = (temp - 65.0f) / 10.0f;
//         r = 0;
//         g = (uint8_t)(255 * ratio);
//         b = (uint8_t)(255 * (1.0f - ratio));
//     } else if (temp < 85.0f) {
//         float ratio = (temp - 75.0f) / 10.0f;
//         r = (uint8_t)(255 * ratio);
//         g = 255;
//         b = 0;
//     } else {
//         float ratio = fminf((temp - 85.0f) / 10.0f, 1.0f);
//         r = 255;
//         g = (uint8_t)(255 * (1.0f - ratio));
//         b = 0;
//     }
//     return lv_color_make(r, g, b);
// }
// Interpolate temperature to RGB color (smooth rainbow-like gradient from blue to red)
// static lv_color_t map_temp_to_color(float temp) {
//     float t_min = 20.0f;
//     float t_max = 40.0f;
//     float ratio = (temp - t_min) / (t_max - t_min);
//     if (ratio < 0.0f) ratio = 0.0f;
//     if (ratio > 1.0f) ratio = 1.0f;

//     uint8_t r = 0, g = 0, b = 0;
//     if (ratio < 0.25f) {
//         r = 0;
//         g = (uint8_t)(255 * ratio * 4);
//         b = 255;
//     } else if (ratio < 0.5f) {
//         r = 0;
//         g = 255;
//         b = (uint8_t)(255 * (1.0f - (ratio - 0.25f) * 4));
//     } else if (ratio < 0.75f) {
//         r = (uint8_t)(255 * (ratio - 0.5f) * 4);
//         g = 255;
//         b = 0;
//     } else {
//         r = 255;
//         g = (uint8_t)(255 * (1.0f - (ratio - 0.75f) * 4));
//         b = 0;
//     }

//     return lv_color_make(r, g, b);
// }

// Interpolate temperature to RGB color (smooth rainbow-like gradient from blue to red)
static lv_color_t map_temp_to_color(float temp) {
    float t_min = 29.5f;
    float t_max = 40.0f;
    float ratio = (temp - t_min) / (t_max - t_min);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    uint8_t r = 0, g = 0, b = 0;
    if (ratio < 0.25f) {
        r = 0;
        g = (uint8_t)(255 * ratio * 4);
        b = 255;
    } else if (ratio < 0.5f) {
        r = 0;
        g = 255;
        b = (uint8_t)(255 * (1.0f - (ratio - 0.25f) * 4));
    } else if (ratio < 0.75f) {
        r = (uint8_t)(255 * (ratio - 0.5f) * 4);
        g = 255;
        b = 0;
    } else {
        r = 255;
        g = (uint8_t)(255 * (1.0f - (ratio - 0.75f) * 4));
        b = 0;
    }

    return lv_color_make(r, g, b);
}

// Bicubic interpolation helper
static float get_pixel(float *src, int w, int h, int x, int y) {
    if (x < 0) x = 0;
    if (x >= w) x = w - 1;
    if (y < 0) y = 0;
    if (y >= h) y = h - 1;
    return src[y * w + x];
}

static float cubic_interp(float v0, float v1, float v2, float v3, float x) {
    return v1 + 0.5f * x * (v2 - v0 + x * (2.0f*v0 - 5.0f*v1 + 4.0f*v2 - v3 + x * (3.0f*(v1 - v2) + v3 - v0)));
}

static float bicubic_interp(float *src, int w, int h, float x, float y) {
    int ix = (int)x;
    int iy = (int)y;
    float fx = x - ix;
    float fy = y - iy;
    float col[4];
    for (int i = -1; i <= 2; i++) {
        float row[4];
        for (int j = -1; j <= 2; j++) {
            row[j + 1] = get_pixel(src, w, h, ix + j, iy + i);
        }
        col[i + 1] = cubic_interp(row[0], row[1], row[2], row[3], fx);
    }
    return cubic_interp(col[0], col[1], col[2], col[3], fy);
}

static lv_color_t dst_buf[320 * 240];
static lv_img_dsc_t thermal_img = {
    .header.always_zero = 0,
    .header.w = 320,
    .header.h = 240,
    .data_size = 320 * 240 * sizeof(lv_color_t),
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = (const uint8_t *)dst_buf
};
void draw_thermal_image(lv_obj_t *parent) {
    const int src_w = 32, src_h = 24;
    const int dst_w = 320, dst_h = 240;

    // Calculate scale factors
    float scale_x = (float)src_w / dst_w;
    float scale_y = (float)src_h / dst_h;

    // Fill destination buffer with scaled pixels using bicubic interpolation
    for (int dy = 0; dy < dst_h; dy++) {
        for (int dx = 0; dx < dst_w; dx++) {
            float src_x = dx * scale_x;
            float src_y = dy * scale_y;

            float temp = bicubic_interp(mlx90640To, src_w, src_h, src_x, src_y);
            lv_color_t color = map_temp_to_color(temp);
            dst_buf[dy * dst_w + dx] = color;
        }
    }

    // Create an image object to show the buffer
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, &thermal_img);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 0);
}

static void *mlx90640_thread(void *arg) {
    // const int src_w = MLX_WIDTH, src_h = MLX_HEIGHT;
    // const int dst_w = 320, dst_h = 240;
    // float scale_x = (float)src_w / dst_w;
    // float scale_y = (float)src_h / dst_h;

    while (1) {
        // int status;
        // for (int i = 0; i < 2; i++) {
        //     status = MLX90640_GetFrameData(MLX90640_ADDR, mlx90640Frame);
        //     if (status < 0) {
        //         usleep(10000);
        //         continue;
        //     }
        // }
        // MLX90640_CalculateTo(mlx90640Frame, &mlx90640Params, 0.95f, 25.0f, mlx90640To);

        // for (int dy = 0; dy < dst_h; dy++) {
        //     for (int dx = 0; dx < dst_w; dx++) {
        //         float src_x = dx * scale_x;
        //         float src_y = dy * scale_y;
        //         float temp = bicubic_interp(mlx90640To, src_w, src_h, src_x, src_y);
        //         lv_color_t color = map_temp_to_color(temp);
        //         dst_buf[dy * dst_w + dx] = color;
        //     }
        // }
        // lv_img_set_src(img, &thermal_img);
        // lv_obj_invalidate(img);
        mlx90640_lvgl_update() ;
        usleep(33000); // ~30 fps
    }
    return NULL;
}


// Khởi tạo LVGL hiển thị
void mlx90640_lvgl_init(lv_obj_t *parent) {
    // static lv_img_dsc_t img_dsc = {
    //     .header.always_zero = 0,
    //     .header.w = MLX_WIDTH,
    //     .header.h = MLX_HEIGHT,
    //     .header.cf = LV_IMG_CF_TRUE_COLOR,
    //     .data = (const uint8_t *)img_buf,
    //     //.data_size = sizeof(img_buf)*4,
    //     .data_size = 768*10*4,
    //     // .header = {
    //     //     .cf = LV_IMG_CF_TRUE_COLOR,//LV_IMG_CF_TRUE_COLOR_ALPHA,
    //     //     .always_zero = 0,
    //     //     .w = MLX_WIDTH,
    //     //     .h = MLX_HEIGHT,
    //     // },
    //     // .data_size = sizeof(img_buf),
    //     // .data = 768*4,
    // };

    // img_obj = lv_img_create(parent);
    // lv_img_set_src(img_obj, &img_dsc);
    // lv_obj_set_size(img_obj, MLX_WIDTH*10, MLX_HEIGHT*10);
    // lv_obj_center(img_obj);

    // Reset thiết bị trước khi bắt đầu
    
    MLX90640_I2CInit();
        //MLX90640_I2CGeneralReset();
        //MLX90640_I2CWrite(0x33, 0x800D, 0x0030); // Đặt chế độ đo
        MLX90640_I2CGeneralReset();
        usleep(50000);
     //Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832];
    status = MLX90640_DumpEE(0x33, eeMLX90640);
    if (status != 0)
        printf("Failed to load system parameters");

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640Params);
    if (status != 0)
        printf("Parameter extraction failed");

    //Once params are extracted, we can release eeMLX90640 array

    //MLX90640_SetRefreshRate(MLX90640_ADDR, 0x02); //Set rate to 2Hz
    //MLX90640_SetRefreshRate(MLX90640_ADDR, 0x03); //Set rate to 4Hz
    //MLX90640_SetRefreshRate(MLX90640_ADDR, 0x07); //Set rate to 64Hz

    // Cấu hình chế độ Interleaved (subpage mode)
    // status = MLX90640_SetInterleavedMode(MLX90640_ADDR);
    // if (status < 0) {
    //     printf("Error: Failed to set Chess mode (Interleaved mode)\n");
    //     return;
    // }
    //MLX90640_SetChessMode
    // status = MLX90640_SetChessMode(MLX90640_ADDR);
    // if (status < 0) {
    //     printf("Error: Failed to set Chess mode (Interleaved mode)\n");
    //     return;
    // }


    uint16_t controlReg = 0x0001; // Normal mode, Interleaved, set toggle subpage
    MLX90640_I2CWrite(MLX90640_ADDR, 0x800D, controlReg);

    
    
    // Chế độ Interleaved + 4Hz
    // (Refresh rate code 3 = 4Hz, Interleaved Mode = 0)
    // status = MLX90640_SetRefreshRate(MLX90640_ADDR, 0x03); // 0x03 = 4Hz
    // if (status != 0) {
    //     printf("Failed to set refresh rate: %d\n", status);
    // }

    status = MLX90640_SetChessMode(MLX90640_ADDR); // Nếu dùng ChessMode thì subpage sẽ toggle
    if (status != 0) {
        printf("Failed to set chess mode: %d\n", status);
    }

     // Hoặc nếu muốn Interleaved:
    //status = MLX90640_SetInterleavedMode(MLX90640_ADDR);
    status= MLX90640_GetCurMode(MLX90640_ADDR);
    printf("Current mode: %d\n", status);
    if (status < 0) {
        printf("Error: Failed to get current mode\n");
        return;
    }
    MLX90640_SetResolution(MLX90640_ADDR, 0x03); //Set resolution to 19 bit
    //MLX90640_SetResolution(MLX90640_ADDR, 0x02); //Set resolution to 18 bit
    //MLX90640_SetResolution(MLX90640_ADDR, 0x01); //Set resolution to 17 bit
    //MLX90640_SetResolution(MLX90640_ADDR, 0x00); //Set resolution to 16 bit
   


    MLX90640_SetRefreshRate(MLX90640_ADDR, 0x02); //Set rate to 32Hz

    pthread_t th;
    pthread_create(&th, NULL, mlx90640_thread, NULL);
    pthread_detach(th);

    printf("MLX90640 initialized successfully\n");
}

// Khởi tạo LVGL hiển thị
void mlx90640_init(void) {
    
    // Reset thiết bị trước khi bắt đầu
    
    MLX90640_I2CInit();
        //MLX90640_I2CGeneralReset();
        //MLX90640_I2CWrite(0x33, 0x800D, 0x0030); // Đặt chế độ đo
        MLX90640_I2CGeneralReset();
        usleep(50000);
     //Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832];
    status = MLX90640_DumpEE(0x33, eeMLX90640);
    if (status != 0)
        printf("Failed to load system parameters");

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640Params);
    if (status != 0)
        printf("Parameter extraction failed");


    uint16_t controlReg = 0x0001; // Normal mode, Interleaved, set toggle subpage
    MLX90640_I2CWrite(MLX90640_ADDR, 0x800D, controlReg);

    
    
    // Chế độ Interleaved + 4Hz
    // (Refresh rate code 3 = 4Hz, Interleaved Mode = 0)
    // status = MLX90640_SetRefreshRate(MLX90640_ADDR, 0x03); // 0x03 = 4Hz
    // if (status != 0) {
    //     printf("Failed to set refresh rate: %d\n", status);
    // }

    status = MLX90640_SetChessMode(MLX90640_ADDR); // Nếu dùng ChessMode thì subpage sẽ toggle
    if (status != 0) {
        printf("Failed to set chess mode: %d\n", status);
    }

     // Hoặc nếu muốn Interleaved:
    //status = MLX90640_SetInterleavedMode(MLX90640_ADDR);
    status= MLX90640_GetCurMode(MLX90640_ADDR);
    printf("Current mode: %d\n", status);
    if (status < 0) {
        printf("Error: Failed to get current mode\n");
        return;
    }
    MLX90640_SetResolution(MLX90640_ADDR, 0x03); //Set resolution to 19 bit
    //MLX90640_SetResolution(MLX90640_ADDR, 0x02); //Set resolution to 18 bit
    //MLX90640_SetResolution(MLX90640_ADDR, 0x01); //Set resolution to 17 bit
    //MLX90640_SetResolution(MLX90640_ADDR, 0x00); //Set resolution to 16 bit
   


    MLX90640_SetRefreshRate(MLX90640_ADDR, 0x02); //Set rate to 32Hz

    pthread_t th;
    pthread_create(&th, NULL, mlx90640_thread, NULL);
    pthread_detach(th);

    printf("MLX90640 initialized successfully\n");
}




// Cập nhật ảnh từ dữ liệu cảm biến
void mlx90640_lvgl_update() {
    int status = -1;  
     status = MLX90640_GetFrameData(MLX90640_ADDR, mlx90640Frame);
     //usleep(50000);
     //status = MLX90640_GetFrameData(MLX90640_ADDR, mlx90640Frame);
     if (status < 0) {
         printf("GetFrameData failed\n");
         return;
     }
     // Tính toán nhiệt độ
     float emissivity = 0.95;
     float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640Params);
     printf("Ta: %f\n", Ta);
     float tr = Ta - 8; // hoặc tự chọn
     MLX90640_CalculateTo(mlx90640Frame, &mlx90640Params, emissivity, tr, mlx90640To);   

    // Debug MLX_WIDTH cột
    for (int y = 0; y < MLX_HEIGHT; y++) {
        for (int x = 0; x < MLX_WIDTH; x++) {
            printf("%5.1f ", mlx90640To[y * MLX_WIDTH + x]);
        }
        printf("\n");
    }

    //lv_obj_invalidate(img_obj);
    //draw_thermal_image( lv_scr_act());
    // for (int i = 0; i < MLX_WIDTH * MLX_HEIGHT; i++) {
    //     img_buf[i] = temp_to_color(mlx90640To[i]);
    // }
    //lv_obj_invalidate(img_obj);
}

void get_thermal_data(float *data) {
    memcpy(data, mlx90640To, sizeof(mlx90640To));
}
