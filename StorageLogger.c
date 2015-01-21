#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "Global.h"
#include "Service.h"
#include "Storage.h"
#include "System.h"
#include "Utils.h"
#include "Os.h"

#include "StorageLogger.h"

#include "lzss.h"

/*
 * For Logger we have 12 sectors storage where each sector has 256 bytes and each page is 256 bytes (64 KB per sector).
 * Flash can be erased only totally or at sector granularity. All bytes are set to 0xff after erasing.
 * It can be programmed from 1 to 256 bytes at a time but only in the same page.
 */

#define C_NUMBER_OF_SECTORS           (12)
#define C_NUMBER_OF_PAGES_PER_SECTOR  (256)
#define C_PAGE_SIZE                   (256)
#define C_SECTOR_SIZE                 (C_NUMBER_OF_PAGES_PER_SECTOR*C_PAGE_SIZE)

#define C_NUMBER_OF_PAGES_PER_PACKET  (4)
// dictionary will reset when these many bytes have been written to input.
#define C_PACKET_SIZE                 (C_NUMBER_OF_PAGES_PER_PACKET*C_PAGE_SIZE)


// address in flash is fixed by the memory map in eCos: keep this value in synch!
static uint8_t * const g_storage_start_of_log_address = &g_storage_data[0x140000];

static char temp_buffer[C_PAGE_SIZE];
static char compression_buffer[C_PAGE_SIZE];

static StorageIndex write_index = { .s = 0, .p = 0, .b = 0 };

static Dictionary write_dictionary, read_dictionary;

static int write_packet_length, read_packet_length;

/*
 * Private Functions
 */

static uint8_t * StorageLogger_get_physical_address(const StorageIndex *index) {
  uint32_t la = (index->s % C_NUMBER_OF_SECTORS) * C_SECTOR_SIZE
    + (index->p % C_NUMBER_OF_PAGES_PER_SECTOR) * C_PAGE_SIZE
    + (index->b % C_PAGE_SIZE);
  uint8_t *pa = &g_storage_start_of_log_address[la];
  return g_storage_start_of_log_address <= pa && pa <= &g_storage_data[0x1fffff] ? pa : 0;
}

static void StorageLogger_erase_sector(uint32_t sector) {
  Os_watchdog_reset();
  const StorageIndex index = { .s = sector, .p = 0, .b = 0 };
  const uint8_t *pa = StorageLogger_get_physical_address(&index);
  Storage_Erase((void *) pa, C_SECTOR_SIZE);
}

static uint32_t StorageLogger_storage_write(const StorageIndex *index, const char *buffer, uint32_t length) {
  const uint8_t *pa = StorageLogger_get_physical_address(index);

  for(int try = 20; try > 0; try --) {
    Os_watchdog_reset();
    Storage_WriteNoErase((void *) pa, (void *) buffer, length);
    Storage_Read((void *) pa, (void *) temp_buffer, length);
    if(memcmp(buffer, temp_buffer, length) == 0) {
      return length;
    }
  }
  return 0;
}

static uint32_t StorageLogger_storage_read(const StorageIndex *index, char *buffer, uint32_t length) {
  Os_watchdog_reset();
  const uint8_t *pa = StorageLogger_get_physical_address(index);

  if(length > 0 && Storage_Read((void *) pa, (void *) buffer, length)) {
    return length;
  }
  return 0;
}

static boolean_t StorageLogger_page_is_empty(uint32_t sector, uint32_t page) {
  const StorageIndex index = { .s = sector, .p = page, .b = 0 };

  StorageLogger_storage_read(&index, temp_buffer, C_PAGE_SIZE);

  for(int i = 0; i < C_PAGE_SIZE; i ++) {
    if(temp_buffer[i] != 0xff) {
      return FALSE;
    }
  }
  return TRUE;
}

static boolean_t StorageLogger_find_last_used_sector(uint32_t *sector) {
  // a sector was last used if at least its last page is entirely empty or half-empty;
  // here we only check for entirely empty.
  // empty bytes have a value of 0xff.
  for(int i = 0; i < C_NUMBER_OF_SECTORS; i ++) {
    boolean_t last_page_is_empty = StorageLogger_page_is_empty(i, C_NUMBER_OF_PAGES_PER_SECTOR - 1);
    if(last_page_is_empty) {
      *sector = i;
      return TRUE;
    }
  }
  return FALSE;
}

static boolean_t StorageLogger_find_first_unused_page(uint32_t *sector, uint32_t *page) {
  if(StorageLogger_find_last_used_sector(sector)) {
    for(int p = C_NUMBER_OF_PAGES_PER_SECTOR - 2; p >= 0; p --) { // the last page is empty so start from the page before that
      if(!StorageLogger_page_is_empty(*sector, p)) { // if the current page is not empty then the page after it was the last empty page
        *page = p + 1;
        return TRUE;
      }
    }
  }
  return FALSE;
}

static void StorageLogger_align_sector_and_page_numbers_with_packet_boundary(uint32_t *sector, uint32_t *page) {
  int s = *sector, p = *page;
  const int rem = p % C_NUMBER_OF_PAGES_PER_PACKET;
  if(rem != 0) {
    p += C_NUMBER_OF_PAGES_PER_PACKET - rem;
    if(p >= C_NUMBER_OF_PAGES_PER_SECTOR) {
      s ++;
      s %= C_NUMBER_OF_SECTORS;
      p = 0;
    }
    *sector = s;
    *page = p;
  }
}

static inline boolean_t StorageLogger_sector_allocation_required(uint32_t old_sector, uint32_t new_sector) {
  return old_sector != new_sector;
}

static void StorageLogger_initialize_write_index() {
  uint32_t sector, page;

  if(StorageLogger_find_first_unused_page(&sector, &page)) {
    uint32_t old_sector = sector;
    StorageLogger_align_sector_and_page_numbers_with_packet_boundary(&sector, &page);
    if(StorageLogger_sector_allocation_required(old_sector, sector)) { // moved to a new sector, so erase it
      StorageLogger_erase_sector(sector);
    }
  } else { // if no last page was found then start logging from the first sector
    sector = 0;
    page = 0;
    StorageLogger_erase_sector(0);
  }

  write_index.s = sector;
  write_index.p = page;
}

static void StorageLogger_increment_index(StorageIndex *index, uint32_t len) {
  index->b += len;
  if(index->b >= C_PAGE_SIZE) {
    index->p += index->b / C_PAGE_SIZE;
    index->b %= C_PAGE_SIZE;
    if(index->p >= C_NUMBER_OF_PAGES_PER_SECTOR) {
      index->s += index->p / C_NUMBER_OF_PAGES_PER_SECTOR;
      index->s %= C_NUMBER_OF_SECTORS;
      index->p %= C_NUMBER_OF_PAGES_PER_SECTOR;
    }
  }
}

static void StorageLogger_decrement_index(StorageIndex *index, uint32_t len) {
  int b = len % C_PAGE_SIZE;
  int p = (len / C_PAGE_SIZE) % C_NUMBER_OF_PAGES_PER_SECTOR;
  int s = (len / C_SECTOR_SIZE) % C_NUMBER_OF_SECTORS;

  if(index->b < b) {
    index->b = C_PAGE_SIZE + index->b - b;
    p ++;
  } else {
    index-> b -= b;
  }
  if(index->p < p) {
    index->p = C_NUMBER_OF_PAGES_PER_SECTOR + index->p - p;
    s ++;
  } else {
    index->p -= p;
  }
  if(index->s < s) {
    index->s = C_NUMBER_OF_SECTORS + index->s - s;
  } else {
    index->s -= s;
  }
}

static inline uint32_t StorageLogger_get_packet_number(uint32_t page) {
  return page / C_NUMBER_OF_PAGES_PER_PACKET;
}

static inline boolean_t StorageLogger_packet_boundary_was_crossed(uint32_t old_sector, uint32_t old_page, uint32_t new_sector, uint32_t new_page) {
  return old_sector != new_sector
    || StorageLogger_get_packet_number(old_page) != StorageLogger_get_packet_number(new_page);
}

static uint32_t StorageLogger_calculate_max_readable_bytes(StorageIndex *read_index) {
  const int sectors_difference = (C_NUMBER_OF_SECTORS + write_index.s - read_index->s) % C_NUMBER_OF_SECTORS;
  const int page_difference = write_index.p - read_index->p;
  const int byte_difference = write_index.b - read_index->b;

  return sectors_difference * C_SECTOR_SIZE + page_difference * C_PAGE_SIZE + byte_difference;
}

static boolean_t StorageLogger_sector_is_empty(uint32_t sector) {
  // a sector is empty if its first page is completely empty
  return StorageLogger_page_is_empty(sector, 0);
}

static uint32_t StorageLogger_find_start_of_log_sector() {
  // the start sector is the first sector after the current write sector
  // that has a non-empty page
  uint32_t sector = (write_index.s + 1) % C_NUMBER_OF_SECTORS;
  int s = 1;
  while(StorageLogger_sector_is_empty(sector) && s < C_NUMBER_OF_SECTORS) {
    sector ++;
    sector %= C_NUMBER_OF_SECTORS;
    s ++;
  }
  return sector;
}

uint32_t StorageLogger_Write_Compressed(const char* buffer, uint32_t length) {
  uint32_t written_count = 0;

  while(length > 0) {
    uint32_t len = C_PAGE_SIZE - write_index.b; // calculate free space in the page
    if(length < len) {
      len = length;
    }
    length -= len;
    written_count += StorageLogger_storage_write(&write_index, (void *) buffer, len);

    uint32_t old_sector = write_index.s;
    uint32_t old_page = write_index.p;
    StorageLogger_increment_index(&write_index, len);

    if(StorageLogger_sector_allocation_required(old_sector, write_index.s)) {
      StorageLogger_erase_sector(write_index.s);
    }
    if(StorageLogger_packet_boundary_was_crossed(old_sector, old_page, write_index.s, write_index.p)) { // terminate the write if packet boundary was crossed
      break;
    }
  }
  return written_count;
}

uint32_t StorageLogger_Read_Compressed(char *buffer, uint32_t length, StorageIndex *read_index) {
  uint32_t len = StorageLogger_calculate_max_readable_bytes(read_index);
  if(length > len) {
    length = len;
  }
  len = StorageLogger_storage_read(read_index, buffer, length);
  StorageLogger_increment_index(read_index, len);
  return len;
}

/*
 * Public Functions
 */

void StorageLogger_Initialize() {
  Utils_printf("Initializing storage for logger ...");
  Os_mutex_lock(OS_RESOURCE_LOG);
  StorageLogger_initialize_write_index();
  Utils_printf("Write index was initialized to [%02d:%03d]\n", write_index.s, write_index.p);
  dictionray_init(&write_dictionary);
  write_packet_length = C_PACKET_SIZE;
  dictionray_init(&read_dictionary);
  read_packet_length = C_PACKET_SIZE;
  Os_mutex_unlock(OS_RESOURCE_LOG);
  Utils_printf("done!\n");
}

void StorageLogger_EraseAll() {
  Os_mutex_lock(OS_RESOURCE_LOG);
  for(int s = 0; s < C_NUMBER_OF_SECTORS; s ++) {
    StorageLogger_erase_sector(s);
  }
  write_index = (StorageIndex) { .s = 0, .p = 0, .b = 0 };
  Os_mutex_unlock(OS_RESOURCE_LOG);
}

uint32_t StorageLogger_Write(const char* buffer, const uint32_t length) {
  Os_mutex_lock(OS_RESOURCE_LOG);
  int s_len = length;

  while(s_len) {
    const int original_s_len = s_len;
    const int len = compress(&write_dictionary, compression_buffer, C_PAGE_SIZE, buffer, &s_len, write_packet_length);

    StorageLogger_Write_Compressed(compression_buffer, len);

    write_packet_length -= len;
    buffer += original_s_len - s_len;

    if(write_packet_length == 0) {
      write_packet_length = C_PACKET_SIZE;
      dictionray_init(&write_dictionary);
    }
  }
  Os_mutex_unlock(OS_RESOURCE_LOG);
  return length;
}

void StorageLogger_InitializeReader(StorageIndex *read_index) {
  Os_mutex_lock(OS_RESOURCE_LOG);
  read_index->s = StorageLogger_find_start_of_log_sector();
  read_index->p = 0;
  read_index->b = 0;
  read_packet_length = C_PACKET_SIZE;
  dictionray_init(&read_dictionary);
  Os_mutex_unlock(OS_RESOURCE_LOG);
}

uint32_t StorageLogger_Read(char *buffer, uint32_t length, StorageIndex *read_index) {
  Os_mutex_lock(OS_RESOURCE_LOG);

  int d_len = length;
  int len = 42;
  uint32_t bytes_written = 0;

  while(d_len) {
    int s_len = StorageLogger_Read_Compressed(compression_buffer, C_PAGE_SIZE, read_index);
    const int original_s_len = s_len;

    len = decompress(&read_dictionary, buffer, d_len, compression_buffer, &s_len, read_packet_length);
    bytes_written += len;

    d_len -= len;
    read_packet_length -= original_s_len - s_len;

    if(read_packet_length == 0) {
      read_packet_length = C_PACKET_SIZE;
      dictionray_init(&read_dictionary);
    }

    StorageLogger_decrement_index(read_index, s_len);
  }
  Os_mutex_unlock(OS_RESOURCE_LOG);
  return bytes_written;
}













