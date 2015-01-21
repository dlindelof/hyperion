#ifndef STORAGE_LOGGER_H_
#define STORAGE_LOGGER_H_

typedef struct {
  uint32_t s; // sector number
  uint32_t p; // page number in sector
  uint32_t b; // byte offset in page
} StorageIndex;

void StorageLogger_Initialize();

uint32_t StorageLogger_Write(const char* buffer, const uint32_t length);

void StorageLogger_InitializeReader(StorageIndex *read_index);
uint32_t StorageLogger_Read(char *buffer, uint32_t length, StorageIndex *read_index);

void StorageLogger_EraseAll();

uint32_t StorageLogger_GetPacketSize();

#endif // STORAGE_LOGGER_H_
