#include <stdbool.h>

#define APP_PAYLOAD_SIZE 256

// our key (unique name) to the shared memory
#define APP_SHM_KEY 1111

// shared data
struct SharedMemoryDataStruct {
  int version;
  bool shutdown;
  char payload[APP_PAYLOAD_SIZE];
  char ack;
};

typedef struct SharedMemoryDataStruct AppData;

// size of shared memory
#define APP_SHM_SIZE (size_t)sizeof(AppData)
