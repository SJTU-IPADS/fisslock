  
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
  
int main(int argc, const char* argv[]) {
  int fd = open("/tmp/fisslock_fault", O_RDWR | O_CREAT, 0644);
  ftruncate(fd, 4096) == 0;
  auto fault_signal = (uint64_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
    MAP_SHARED, fd, 0);
  
  uint64_t bitmap = 0;
  for (int i = 0; i < strlen(argv[1]); i++) {
    if (argv[1][i] == '1') bitmap |= (1 << i);
  }

  *fault_signal = bitmap;
  return 0;
}