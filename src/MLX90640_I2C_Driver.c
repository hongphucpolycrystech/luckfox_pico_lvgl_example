#include "MLX90640_I2C_Driver.h"
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>

#define I2C_DEVICE "/dev/i2c-2"
static int i2c_fd = -1;

int MLX90640_I2CInit(void) {
    if (i2c_fd < 0) {
        i2c_fd = open(I2C_DEVICE, O_RDWR);
        if (i2c_fd < 0) {
            perror("Failed to open I2C device");
            return -1;
        }
    }
    return 0;
}

// int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data) {
//     uint8_t buf[2] = {startAddress >> 8, startAddress & 0xFF};
//     if (ioctl(i2c_fd, I2C_SLAVE, slaveAddr) < 0) return -1;
//     if (write(i2c_fd, buf, 2) != 2) return -1;

//     for (int i = 0; i < nMemAddressRead; i++) {
//         uint8_t tmp[2];
//         if (read(i2c_fd, tmp, 2) != 2) return -1;
//         data[i] = (tmp[0] << 8) | tmp[1];
//         usleep(100);
//     }
//     return 0;
// }
// int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data) {
//     if (ioctl(i2c_fd, I2C_SLAVE, slaveAddr) < 0) return -1;

//     uint8_t addr[2] = {startAddress >> 8, startAddress & 0xFF};
//     if (write(i2c_fd, addr, 2) != 2) return -1;

//     uint8_t raw[2 * nMemAddressRead];
//     if (read(i2c_fd, raw, 2 * nMemAddressRead) != 2 * nMemAddressRead) return -1;

//     for (int i = 0; i < nMemAddressRead; i++) {
//         data[i] = (raw[2 * i] << 8) | raw[2 * i + 1];
//     }

//     return 0;
// }

#define MAX_READ_SIZE 1024  // Giới hạn số lượng byte đọc
int MLX90640_I2CReadInternal(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data) {
    struct i2c_rdwr_ioctl_data packets;
    struct i2c_msg messages[2];

    uint8_t addr_buf[2] = {startAddress >> 8, startAddress & 0xFF};
    uint8_t read_buf[2 * nMemAddressRead];

    // Thiết lập gói write với địa chỉ cần đọc
    messages[0].addr = slaveAddr;
    messages[0].flags = 0;  // Write
    messages[0].len = 2;
    messages[0].buf = addr_buf;

    // Thiết lập gói read
    messages[1].addr = slaveAddr;
    messages[1].flags = I2C_M_RD;  // Read
    messages[1].len = 2 * nMemAddressRead;
    messages[1].buf = read_buf;

    packets.msgs = messages;
    packets.nmsgs = 2;

    // Thực hiện truyền dữ liệu qua I2C
    if (ioctl(i2c_fd, I2C_RDWR, &packets) < 0) {
        perror("I2C_RDWR");
        return -1;
    }

    // Lưu dữ liệu vào mảng
    for (int i = 0; i < nMemAddressRead; i++) {
        data[i] = (read_buf[2 * i] << 8) | read_buf[2 * i + 1];
    }

    return 0;  // Thành công
}

int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data) {
    // Nếu số lượng bytes cần đọc vượt quá 1024, chia nhỏ thành nhiều lần
    if (nMemAddressRead > MAX_READ_SIZE / 2) {
        uint16_t remaining = nMemAddressRead;
        uint16_t offset = 0;
        while (remaining > 0) {
            uint16_t read_size = (remaining > MAX_READ_SIZE / 2) ? MAX_READ_SIZE / 2 : remaining;

            // Gọi lại hàm với số lượng đọc nhỏ hơn hoặc bằng giới hạn
            if (MLX90640_I2CReadInternal(slaveAddr, startAddress + offset, read_size, data + offset) != 0) {
                return -1;  // Lỗi đọc dữ liệu
            }

            remaining -= read_size;
            offset += read_size;
        }
        return 0;  // Đọc hoàn tất
    }

    // Nếu số lượng nhỏ hơn hoặc bằng 1024 bytes, đọc trực tiếp
    return MLX90640_I2CReadInternal(slaveAddr, startAddress, nMemAddressRead, data);
}



// int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
//     uint8_t buf[4] = {
//         writeAddress >> 8, writeAddress & 0xFF,
//         data >> 8, data & 0xFF
//     };
//     if (ioctl(i2c_fd, I2C_SLAVE, slaveAddr) < 0) return -1;
//     return write(i2c_fd, buf, 4) == 4 ? 0 : -1;
// }

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
    struct i2c_rdwr_ioctl_data packets;
    struct i2c_msg messages[1];

    // Buffer chứa địa chỉ và dữ liệu cần ghi
    uint8_t write_buf[4] = {
        writeAddress >> 8, writeAddress & 0xFF,  // Địa chỉ (2 byte)
        data >> 8, data & 0xFF                   // Dữ liệu (2 byte)
    };

    messages[0].addr = slaveAddr;
    messages[0].flags = 0;  // Write
    messages[0].len = 4;    // Địa chỉ + Dữ liệu
    messages[0].buf = write_buf;

    packets.msgs = messages;
    packets.nmsgs = 1;

    // Thực hiện truyền dữ liệu qua I2C
    if (ioctl(i2c_fd, I2C_RDWR, &packets) < 0) {
        perror("I2C_RDWR");
        return -1;
    }

    return 0;
}

int MLX90640_I2CWrite_mul(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressWrite, uint16_t *data) {
    struct i2c_rdwr_ioctl_data packets;
    struct i2c_msg messages[1];

    // Buffer để lưu trữ địa chỉ và dữ liệu cần ghi
    uint8_t write_buf[2 + 2 * nMemAddressWrite];

    // Địa chỉ (2 byte)
    write_buf[0] = startAddress >> 8;
    write_buf[1] = startAddress & 0xFF;

    // Dữ liệu (2 byte cho mỗi word)
    for (int i = 0; i < nMemAddressWrite; i++) {
        write_buf[2 + 2 * i] = data[i] >> 8;  // Byte cao
        write_buf[2 + 2 * i + 1] = data[i] & 0xFF;  // Byte thấp
    }

    messages[0].addr = slaveAddr;
    messages[0].flags = 0;  // Write
    messages[0].len = 2 + 2 * nMemAddressWrite;  // Địa chỉ + Dữ liệu
    messages[0].buf = write_buf;

    packets.msgs = messages;
    packets.nmsgs = 1;

    // Thực hiện truyền dữ liệu qua I2C
    if (ioctl(i2c_fd, I2C_RDWR, &packets) < 0) {
        perror("I2C_RDWR");
        return -1;
    }

    return 0;
}

void MLX90640_I2CClose() {
    if (i2c_fd >= 0) {
        close(i2c_fd);
        i2c_fd = -1;
    }
}

int MLX90640_I2CGeneralReset() {
    // Địa chỉ reset I2C chung (general call) là 0x00
    uint8_t reset_cmd[2] = {0x00, 0x06};
    if (ioctl(i2c_fd, I2C_SLAVE, 0x00) < 0) return -1;
    return write(i2c_fd, reset_cmd, 2) == 2 ? 0 : -1;
}