#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>


static int spi_fd = -1;
static int gpio_ce = 0;
static int gpio_csn = 0;

void gpio_export(int gpio) {
    char buf[64];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, strlen(buf));
    close(fd);
}

void gpio_direction(int gpio, const char* dir) {
    char buf[64];
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", gpio);
    int fd = open(buf, O_WRONLY);
    write(fd, dir, strlen(dir));
    close(fd);
}

void gpio_write(int gpio, int val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(buf, O_WRONLY);
    write(fd, val ? "1" : "0", 1);
    close(fd);
}

uint8_t spi_transfer(uint8_t reg) {
    uint8_t tx[] = { reg, 0xFF };
    uint8_t rx[2];
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 2,
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
    return rx[1];
}

void write_register(uint8_t reg, uint8_t val) {
    uint8_t tx[] = { (uint8_t)(0x20 | reg), val };
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .len = 2,
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

bool nrf24_init(const char* spi_path, int ce, int csn) {
    //gpio_ce = ce;
    gpio_csn = csn;

    //gpio_export(ce);
    gpio_export(csn);
    usleep(100000);

    //gpio_direction(ce, "out");
    gpio_direction(csn, "out");

    //gpio_write(ce, 0);
    gpio_write(csn, 1);

    spi_fd = open(spi_path, O_RDWR);
    if (spi_fd < 0) return false;

    uint8_t mode = 0;
    uint8_t bits = 8;
    uint32_t speed = 1000000;

    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    write_register(0x00, 0x03); // PRIM_RX + PWR_UP
    usleep(2000);
    return true;
}

std::vector<int> nrf24_scan() {
    std::vector<int> rpd(126, 0);

    for (int ch = 0; ch <= 125; ch++) {
        write_register(0x05, ch); // set RF_CH
        // gpio_write(gpio_ce, 1);
         usleep(140);
        // gpio_write(gpio_ce, 0);

        int r = spi_transfer(0x09) & 0x01; // read RPD
        //int r = spi_transfer(0x09) ; // read RPD
        rpd[ch] = r;
    }
    printf("RPD: ");
    for (int i = 0; i < 126; i++) {
        printf("%d ", rpd[i]);
    }
    printf("\n");
    return rpd;
}
std::vector<int> nrf24_scan_stat(int scans = 100) {
    std::vector<int> count(126, 0);

    write_register(0x00, 0x03); // PRIM_RX + PWR_UP
    usleep(1500);

    for (int ch = 0; ch <= 125; ch++) {
        write_register(0x05, ch); // set RF_CH
        usleep(100);

        int rpd_hits = 0;
        for (int i = 0; i < scans; i++) {
            // gpio_write(gpio_ce, 1);
            // usleep(140);
            // gpio_write(gpio_ce, 0);

            int r = spi_transfer(0x09) & 0x01;
            if (r) rpd_hits++;
        }

        count[ch] = rpd_hits*100/scans;
    }
    printf("RPD: ");
    for (int i = 0; i < 126; i++) {
        printf("%d ", count[i]);
    }
    printf("\n");
    return count;
}

