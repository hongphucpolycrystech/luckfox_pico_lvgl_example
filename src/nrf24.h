#ifndef NRF24_H
#define NRF24_H

#include <vector>

bool nrf24_init(const char* spi_path, int ce_gpio, int csn_gpio);
std::vector<int> nrf24_scan();
std::vector<int> nrf24_scan_stat(int scans = 100);
#endif
