/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *                    sekigon-gonnoc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

// This example runs both host and device concurrently. The USB host receive
// reports from HID device and print it out over USB Device CDC interface.
// For TinyUSB roothub port0 is native usb controller, roothub port1 is
// pico-pio-usb.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "pio_usb.h"
#include "tusb.h"

#include "ff.h"
#include "diskio.h"

/*------------- MAIN -------------*/

#define CMD_BUF_LEN 128

static bool disk_io_complete(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data);

enum CdcState
{
  // Waiting for USB device to be mounted
  CDC_WAIT_CONNECT,
  // After connecting, we need to wait a little bit before
  // sending the first message to make sure the host actually
  // recieves it
  CDC_WAIT_DELAY_START,
  CDC_WAIT_DELAYING,
  // Send the init message
  CDC_INIT,
  // Wait for command
  CDC_IDLE,
  // Handle command
  CDC_RUN_CMD
};

enum CdcState cdc_state = CDC_WAIT_CONNECT;
char cmd_buf[CMD_BUF_LEN] = {0};



// MSC dev_addr is always 1?
const uint8_t msc_dev_addr = 1;

void check_mounted()
{
  if (tuh_msc_mounted(msc_dev_addr))
  {
    tud_cdc_write_str("USB drive is mounted\r\n");
  }
  else
  {
    tud_cdc_write_str("USB drive not is mounted\r\n");
  }
}

void get_max_lun() {
  char resp[256];
  uint32_t resp_count;
  uint8_t max_lun = tuh_msc_get_maxlun(msc_dev_addr);
  resp_count = sprintf(resp, "Max LUN: %d\r\n", max_lun);
  tud_cdc_write(resp, resp_count);
  tud_cdc_write_flush();
}

void get_block_size(uint8_t lun) {
  char resp[256];
  uint32_t resp_count;
  uint32_t block_size = tuh_msc_get_block_size(msc_dev_addr, lun);
  resp_count = sprintf(resp, "Block size: %ld\r\n", block_size);
  tud_cdc_write(resp, resp_count);
  tud_cdc_write_flush();
}

void get_block_count(uint8_t lun) {
  char resp[256];
  uint32_t resp_count;
  uint32_t block_count = tuh_msc_get_block_count(msc_dev_addr, lun);
  resp_count = sprintf(resp, "Block count: %ld\r\n", block_count);
  tud_cdc_write(resp, resp_count);
  tud_cdc_write_flush();
}

// TODO: large writes don't really work?
void printbuf(uint8_t buf[], size_t len)
{
  char resp[128];
  uint32_t resp_count;
  uint32_t resp_idx = 0;
  for (size_t i = 0; i < len; ++i)
  {
    if (i % 16 == 15) {
      resp_count = sprintf(resp + resp_idx, "%02x\r\n", buf[i]);
      resp_idx += resp_count;
      tud_cdc_write(resp, resp_idx);
      tud_cdc_write_flush();
      resp_idx = 0;
    }
    else
    {
      resp_count = sprintf(resp + resp_idx, "%02x ", buf[i]);
      resp_idx += resp_count;
    }
  }
  tud_cdc_write_str("\r\n");
}

void read_block(uint8_t lun, uint32_t block) {
  uint32_t block_size = tuh_msc_get_block_size(msc_dev_addr, lun);
  uint8_t *buf = malloc(block_size*sizeof(uint8_t));
  disk_read(msc_dev_addr - 1, buf, block, 1);
  printbuf(buf, block_size);
}

void fill_block(uint8_t lun, uint32_t block, uint8_t val) {
  uint32_t block_size = tuh_msc_get_block_size(msc_dev_addr, lun);
  uint8_t *buf = malloc(block_size*sizeof(uint8_t));
  for (uint32_t i=0; i < block_size; ++i) {
    buf[i] = val;
  }
  disk_write(msc_dev_addr - 1, buf, block, 1);
}

void handle_command()
{
  char _cmd_buf[CMD_BUF_LEN];
  strcpy(_cmd_buf, cmd_buf);
  char* cmd = strtok(_cmd_buf, " ");
  char* arg;
  char resp[256];
  uint32_t resp_count;
  uint8_t lun;
  uint32_t block;
  uint8_t val;
  if (strcmp(cmd, "mounted") == 0)
  {
    check_mounted();
  }
  else if (strcmp(cmd, "maxlun") == 0)
  {
    get_max_lun();
  }
  else if (strcmp(cmd, "blocksize") == 0)
  {
    arg = strtok(NULL, " ");
    lun = atoi(arg);
    get_block_size(lun);
  }
  else if (strcmp(cmd, "blockcount") == 0)
  {
    arg = strtok(NULL, " ");
    lun = atoi(arg);
    get_block_count(lun);
  }
  else if (strcmp(cmd, "readblock") == 0)
  {
    arg = strtok(NULL, " ");
    lun = atoi(arg);
    arg = strtok(NULL, " ");
    block = atoi(arg);
    read_block(lun, block);
  }
  else if (strcmp(cmd, "fillblock") == 0)
  {
    arg = strtok(NULL, " ");
    lun = atoi(arg);
    arg = strtok(NULL, " ");
    block = atoi(arg);
    arg = strtok(NULL, " ");
    val = atoi(arg);
    fill_block(lun, block, val);
  }
  else
  {
    resp_count = sprintf(resp, "Unknown command '%s'\r\n", cmd_buf);
    tud_cdc_write(resp, resp_count);
    tud_cdc_write_flush();
  }
}

void cdc_task()
{
  static uint32_t delay_start;
  const uint32_t delay_us = 100000;

  switch (cdc_state)
  {
  case CDC_WAIT_CONNECT:
    if (tud_cdc_connected())
      cdc_state = CDC_WAIT_DELAY_START;
    break;
  case CDC_WAIT_DELAY_START:
    delay_start = time_us_32();
    cdc_state = CDC_WAIT_DELAYING;
    break;
  case CDC_WAIT_DELAYING:
    if (time_us_32() - delay_start > delay_us)
      cdc_state = CDC_INIT;
    break;
  case CDC_INIT:
    tud_cdc_write_str("MSC to CDC Demo\r\n> ");
    cdc_state = CDC_IDLE;
    break;
  case CDC_RUN_CMD:
    tud_cdc_write_str("\r\n");
    handle_command();
    tud_cdc_write_str("> ");
    cdc_state = CDC_IDLE;
    break;
  default:
    break;
  }
}

#ifdef VBUS_EN_PIN
void enable_5v()
{
  gpio_init(VBUS_EN_PIN);
  gpio_set_dir(VBUS_EN_PIN, GPIO_OUT);
  gpio_put(VBUS_EN_PIN, VBUS_EN_VAL);
}
#endif

// core1: handle host events
void core1_main()
{
#ifdef VBUS_EN_PIN
  enable_5v();
#endif
  sleep_ms(10);

  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
  // port1) on core1
  tuh_init(1);

  while (true)
  {
    tuh_task(); // tinyusb host task
  }
}

#define LED_PIN 13
// core0: handle device events
int main(void)
{
  // default 125MHz is not appropreate. Sysclock should be multiple of 12MHz.
  set_sys_clock_khz(120000, true);

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN, 0);

  sleep_ms(10);

  multicore_reset_core1();
  // all USB task run in core1
  multicore_launch_core1(core1_main);

  // init device stack on native usb (roothub port0)
  tud_init(0);

  while (true)
  {
    tud_task(); // tinyusb device task
    tud_cdc_write_flush();
    cdc_task();
  }

  return 0;
}

//--------------------------------------------------------------------+
// Device CDC
//--------------------------------------------------------------------+

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void)itf;

  static uint32_t cmd_buf_idx = 0;

  char c;
  uint32_t count = tud_cdc_read(&c, 1);
  tud_cdc_write(&c, count);
  tud_cdc_write_flush();

  if (c == '\r' || c == '\n')
  {
    cdc_state = CDC_RUN_CMD;
    cmd_buf[cmd_buf_idx] = '\0';
    cmd_buf_idx = 0;
    return;
  }

  cmd_buf[cmd_buf_idx++] = c;
}

static FATFS fatfs[CFG_TUH_DEVICE_MAX]; // for simplicity only support 1 LUN per device
static volatile bool _disk_busy[CFG_TUH_DEVICE_MAX];

static scsi_inquiry_resp_t inquiry_resp;

//--------------------------------------------------------------------+
// DiskIO
//--------------------------------------------------------------------+

static void wait_for_disk_io(BYTE pdrv)
{
  while (_disk_busy[pdrv])
  {
    tuh_task();
  }
}

static bool disk_io_complete(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data)
{
  (void)dev_addr;
  (void)cb_data;
  _disk_busy[dev_addr - 1] = false;
  return true;
}

DSTATUS disk_status(
    BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
  uint8_t dev_addr = pdrv + 1;
  return tuh_msc_mounted(dev_addr) ? 0 : STA_NODISK;
}

DSTATUS disk_initialize(
    BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
  (void)pdrv;
  return 0; // nothing to do
}

DRESULT disk_read(
    BYTE pdrv,    /* Physical drive nmuber to identify the drive */
    BYTE *buff,   /* Data buffer to store read data */
    LBA_t sector, /* Start sector in LBA */
    UINT count    /* Number of sectors to read */
)
{
  uint8_t const dev_addr = pdrv + 1;
  uint8_t const lun = 0;

  _disk_busy[pdrv] = true;
  tuh_msc_read10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
  wait_for_disk_io(pdrv);

  return RES_OK;
}

#if FF_FS_READONLY == 0

DRESULT disk_write(
    BYTE pdrv,        /* Physical drive nmuber to identify the drive */
    const BYTE *buff, /* Data to be written */
    LBA_t sector,     /* Start sector in LBA */
    UINT count        /* Number of sectors to write */
)
{
  uint8_t const dev_addr = pdrv + 1;
  uint8_t const lun = 0;

  _disk_busy[pdrv] = true;
  tuh_msc_write10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
  wait_for_disk_io(pdrv);

  return RES_OK;
}

#endif

DRESULT disk_ioctl(
    BYTE pdrv, /* Physical drive nmuber (0..) */
    BYTE cmd,  /* Control code */
    void *buff /* Buffer to send/receive control data */
)
{
  uint8_t const dev_addr = pdrv + 1;
  uint8_t const lun = 0;
  switch (cmd)
  {
  case CTRL_SYNC:
    // nothing to do since we do blocking
    return RES_OK;

  case GET_SECTOR_COUNT:
    *((DWORD *)buff) = (WORD)tuh_msc_get_block_count(dev_addr, lun);
    return RES_OK;

  case GET_SECTOR_SIZE:
    *((WORD *)buff) = (WORD)tuh_msc_get_block_size(dev_addr, lun);
    return RES_OK;

  case GET_BLOCK_SIZE:
    *((DWORD *)buff) = 1; // erase block size in units of sector size
    return RES_OK;

  default:
    return RES_PARERR;
  }

  return RES_OK;
}

// ----

bool inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const *cb_data)
{
  msc_cbw_t const *cbw = cb_data->cbw;
  msc_csw_t const *csw = cb_data->csw;

  if (csw->status != 0)
  {
    printf("Inquiry failed\r\n");
    return false;
  }

  // Print out Vendor ID, Product ID and Rev
  printf("%.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

  // Get capacity of device
  uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
  uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

  printf("Disk Size: %lu MB\r\n", block_count / ((1024 * 1024) / block_size));
  // printf("Block Count = %lu, Block Size: %lu\r\n", block_count, block_size);

  // For simplicity: we only mount 1 LUN per device
  uint8_t const drive_num = dev_addr - 1;
  char drive_path[3] = "0:";
  drive_path[0] += drive_num;

  if (f_mount(&fatfs[drive_num], drive_path, 1) != FR_OK)
  {
    puts("mount failed");
  }

  // change to newly mounted drive
  f_chdir(drive_path);

  // print the drive label
  //  char label[34];
  //  if ( FR_OK == f_getlabel(drive_path, label, NULL) )
  //  {
  //    puts(label);
  //  }

  return true;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
  char buf[256];
  uint32_t count = sprintf(buf, "A MassStorage device is mounted, dev_addr=%d\r\n", dev_addr);
  tud_cdc_write(buf, count);
  tud_cdc_write_flush();

  uint8_t const lun = 0;
  tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb, 0);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
  tud_cdc_write_str("A MassStorage device is unmounted\r\n");

  uint8_t const drive_num = dev_addr - 1;
  char drive_path[3] = "0:";
  drive_path[0] += drive_num;

  f_unmount(drive_path);

  //  if ( phy_disk == f_get_current_drive() )
  //  { // active drive is unplugged --> change to other drive
  //    for(uint8_t i=0; i<CFG_TUH_DEVICE_MAX; i++)
  //    {
  //      if ( disk_is_ready(i) )
  //      {
  //        f_chdrive(i);
  //        cli_init(); // refractor, rename
  //      }
  //    }
  //  }
}
