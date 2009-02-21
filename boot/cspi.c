#include "types.h"
#include "mxc91231.h"
#include "common.h"
#include "cspi.h"

#define CSPI_RXDATA(base)    ((base) + 0x00)
#define CSPI_TXDATA(base)    ((base) + 0x04)
#define CSPI_CONREG(base)    ((base) + 0x08)
#define CSPI_INTREG(base)    ((base) + 0x0c)
#define CSPI_DMAREG(base)    ((base) + 0x10)
#define CSPI_STATREG(base)   ((base) + 0x14)
#define CSPI_PERIODREG(base) ((base) + 0x18)
#define CSPI_TESTREG(base)   ((base) + 0x1c)

static addr_t cspi_modules[] = {
  CSPI1_BASE_ADDR,
  CSPI2_BASE_ADDR,
};

#define CSPI_MAX_MODULE (sizeof cspi_modules / sizeof cspi_modules[0])

static int cspi_get_divider(unsigned int data_rate) {
  unsigned int ipg_clk;
  int i;

  ipg_clk = clock_get("ipg_clk");
  for (i = 0; i < 7; ++i) {
    if ((ipg_clk >> 2 + i) <= data_rate) {
      return i;
    }
  }
  return -1;
}

static int cspi_burst(addr_t base) {
  modify_register32(CSPI_CONREG(base), 0, (1 << 2)); // initiate transfer
  while ((read32(CSPI_CONREG(base)) & (1 << 2)) != 0) {
  }
  return 0;
}

int cspi_init(int module, struct cspi_config *cfg) {
  addr_t base;
  uint32_t conreg = 0;
  int clk_divider;

  if (module > CSPI_MAX_MODULE) {
    return -1;
  }
  clk_divider = cspi_get_divider(cfg->data_rate);
  if (clk_divider == -1) {
    return -1;
  }
  base = cspi_modules[module];
  write32(0, CSPI_CONREG(base)); // disable CSPI
  conreg = (cfg->chip_select & 0x3) << 12;
  conreg |= clk_divider << 16;
  conreg |= ((cfg->bit_count - 1) & 0x1f) << 20;
  if (cfg->active_high_ss_polarity) {
    conreg |= (1 << 7);
  }
  if (cfg->ss_waveform) {
    conreg |= (1 << 6);
  }
  conreg |= ((cfg->phase & 1) << 5);
  if (cfg->active_high_clk_polarity) {
    conreg |= (1 << 4);
  }
  conreg |= (1 << 1); // master mode
  conreg |= (1 << 0); // module enable;
  write32(conreg, CSPI_CONREG(base));
  return 0;
}

int cspi_send(int module, uint32_t *data, size_t len) {
  addr_t base;
  size_t pushed;

  if (module > CSPI_MAX_MODULE) {
    return -1;
  }
  base = cspi_modules[module];

  if ((read32(CSPI_STATREG(base)) & 1) == 0) {
    printf("txfifo is not empty\n");
    return -1;
  }

  pushed = 0;
  while (pushed < len) {
    write32(data[pushed], CSPI_TXDATA(base));
    if ((CSPI_STATREG(base)) & (1 << 2)) {
      // txfifo full, go ahead
      cspi_burst(base);
    }
    pushed++;
  }
  return pushed;
}

int cspi_recv(int module, uint32_t *buf, size_t size) {
  addr_t base;
  size_t popped;

  if (module > CSPI_MAX_MODULE) {
    return -1;
  }
  base = cspi_modules[module];

  popped = 0;
  while ((popped < size) && (read32(CSPI_STATREG(base)) & (1 << 3))) {
    buf[popped] = read32(CSPI_RXDATA(base));
    popped++;
  }
  return popped;
}