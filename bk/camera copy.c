#include "camera.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define FRAME_WIDTH     640
#define FRAME_HEIGHT    480
#define CROP_SIZE       480
#define DISPLAY_SIZE    480
#define BUFFER_COUNT    2

typedef struct {
    void *start;
    size_t length;
} buffer_t;

static int cam_fd = -1;
static buffer_t buffers[BUFFER_COUNT];
static lv_obj_t *g_img_obj = NULL;
static lv_timer_t *g_cam_timer = NULL;

static uint8_t rgb888_buf[CROP_SIZE * CROP_SIZE * 3];
static uint8_t rgb666_buf[DISPLAY_SIZE * DISPLAY_SIZE * 3];

static lv_img_dsc_t img_dsc = {
    .header = {
        .always_zero = 0,
        .w = DISPLAY_SIZE,
        .h = DISPLAY_SIZE,
        .cf = LV_IMG_CF_TRUE_COLOR
    },
    .data_size = DISPLAY_SIZE * DISPLAY_SIZE * 3,
    .data = rgb666_buf
};

// Simple nearest neighbor resize
static void resize_rgb888(uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh) {
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            int src_x = x * sw / dw;
            int src_y = y * sh / dh;
            memcpy(&dst[(y * dw + x) * 3], &src[(src_y * sw + src_x) * 3], 3);
        }
    }
}

// Convert NV12 to RGB888
static void nv12_to_rgb888(uint8_t *yuv, uint8_t *rgb, int width, int height) {
    uint8_t *y_plane = yuv;
    uint8_t *uv_plane = yuv + width * height;

    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int y = y_plane[j * width + i];
            int uv_index = (j / 2) * width + (i & ~1);
            int u = uv_plane[uv_index] - 128;
            int v = uv_plane[uv_index + 1] - 128;

            int r = y + 1.370705 * v;
            int g = y - 0.698001 * v - 0.337633 * u;
            int b = y + 1.732446 * u;

            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);

            int index = (j * width + i) * 3;
            rgb[index + 0] = r;
            rgb[index + 1] = g;
            rgb[index + 2] = b;
        }
    }
}

// Convert RGB888 to RGB666 (packed format)
static void rgb888_to_rgb666(uint8_t *in, uint8_t *out, int width, int height) {
    for (int i = 0; i < width * height; ++i) {
        out[i * 3 + 0] = in[i * 3 + 0] >> 2;
        out[i * 3 + 1] = in[i * 3 + 1] >> 2;
        out[i * 3 + 2] = in[i * 3 + 2] >> 2;
    }
}

void camera_lvgl_init(const char *dev_path) {
    cam_fd = open(dev_path, O_RDWR);
    if (cam_fd < 0) {
        perror("open camera");
        return;
    }

    struct v4l2_format fmt = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
    };
    fmt.fmt.pix_mp.width = FRAME_WIDTH;
    fmt.fmt.pix_mp.height = FRAME_HEIGHT;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 1;

    if (ioctl(cam_fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        return;
    }

    struct v4l2_requestbuffers req = {
        .count = BUFFER_COUNT,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        .memory = V4L2_MEMORY_MMAP
    };

    if (ioctl(cam_fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        return;
    }

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            .memory = V4L2_MEMORY_MMAP,
            .index = i
        };
        struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
        buf.m.planes = planes;
        buf.length = 1;

        if (ioctl(cam_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            return;
        }

        buffers[i].length = buf.m.planes[0].length;
        buffers[i].start = mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, cam_fd, buf.m.planes[0].m.mem_offset);

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            return;
        }

        if (ioctl(cam_fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            return;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(cam_fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
    }
}

static void camera_lvgl_display_once(lv_obj_t *img) {
    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        .memory = V4L2_MEMORY_MMAP
    };
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    buf.m.planes = planes;
    buf.length = 1;

    if (ioctl(cam_fd, VIDIOC_DQBUF, &buf) < 0) return;

    uint8_t *nv12 = buffers[buf.index].start;

    static uint8_t rgb_full[FRAME_WIDTH * FRAME_HEIGHT * 3];
    nv12_to_rgb888(nv12, rgb_full, FRAME_WIDTH, FRAME_HEIGHT);

    // Crop center
    for (int y = 0; y < CROP_SIZE; ++y) {
        memcpy(&rgb888_buf[y * CROP_SIZE * 3],
               &rgb_full[((y + (FRAME_HEIGHT - CROP_SIZE)/2) * FRAME_WIDTH + (FRAME_WIDTH - CROP_SIZE)/2) * 3],
               CROP_SIZE * 3);
    }

    // Resize (here, same size)
    resize_rgb888(rgb888_buf, CROP_SIZE, CROP_SIZE, rgb888_buf, DISPLAY_SIZE, DISPLAY_SIZE);

    // Convert to RGB666
    rgb888_to_rgb666(rgb888_buf, rgb666_buf, DISPLAY_SIZE, DISPLAY_SIZE);

    lv_img_set_src(img, &img_dsc);

    if (ioctl(cam_fd, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF");
    }
}

static void camera_timer_cb(lv_timer_t *timer) {
    if (g_img_obj) {
        camera_lvgl_display_once(g_img_obj);
    }
}

void camera_lvgl_start_streaming(void) {
    g_img_obj = lv_img_create(lv_scr_act());
    if (!g_img_obj) {
        LV_LOG_ERROR("Failed to create image object");
        return;
    }

    lv_obj_set_size(g_img_obj, DISPLAY_SIZE, DISPLAY_SIZE);
    lv_obj_center(g_img_obj);
    lv_img_set_src(g_img_obj, &img_dsc);

    g_cam_timer = lv_timer_create(camera_timer_cb, 33, NULL); // ~30fps
}

void camera_lvgl_stop_streaming(void) {
    if (g_cam_timer) {
        lv_timer_del(g_cam_timer);
        g_cam_timer = NULL;
    }
}
