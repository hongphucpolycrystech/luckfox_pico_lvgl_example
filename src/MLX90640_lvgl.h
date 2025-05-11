


#ifndef MLX90640_LVGL_H
#define MLX90640_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"




void mlx90640_lvgl_init(lv_obj_t *parent) ;

void mlx90640_lvgl_update() ;
void get_thermal_data(float *data) ;
void mlx90640_init(void);

#ifdef __cplusplus
}
#endif

#endif // MLX90640_LVGL_H