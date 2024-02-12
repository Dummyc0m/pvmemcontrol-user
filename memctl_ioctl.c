#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>

#include "memctl.h"

int pagemap_get_entry(int pagemap_fd, uintptr_t vaddr, uintptr_t *paddr)
{
	size_t nread;
	ssize_t ret;
	uint64_t data;

	nread = 0;
	while (nread < sizeof(data)) {
		ret = pread(pagemap_fd, ((uint8_t *)&data) + nread, sizeof(data) - nread,
				(vaddr / getpagesize()) * sizeof(data) + nread);
		nread += ret;
		if (ret <= 0) {
			perror("pagemap_get_entry pread");
			return ret;
		}
	}
	printf("vaddr %lx\n", vaddr);
	printf("converted data %lx\n", data);
	*paddr = (data & (((uint64_t)1 << 55) - 1)) << 9;
	printf("converted paddr %lx\n", *paddr);
	return 0;
}

uint64_t time_us(void) {
  struct timespec ts;
  int err = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  if (err == -1) {
    perror("time");
    abort();
  }
  return ((uint64_t)ts.tv_sec) * 1000000 + ((uint64_t)ts.tv_nsec) / 1000;
}

int main(void) {
  int memctl_fd = open("/dev/memctl", O_RDWR);
  if (memctl_fd < 0) {
    perror("memctl open");
    abort();
  }

  size_t size = 1024 * 1024 * 1024;
  int flags = MAP_ANONYMOUS | MAP_HUGETLB | MAP_PRIVATE;
  void *arena = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);

  if (arena == MAP_FAILED) {
    perror("mmap");
    abort();
  }

  memset(arena, 1, size);

  int pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  if (pagemap_fd < 0) {
    perror("pagemap open");
    abort();
  }

  uintptr_t arena_paddr;
  int err = pagemap_get_entry(pagemap_fd, (uintptr_t)arena, &arena_paddr);
  if (err) {
    abort();
  }

  if (arena_paddr == 0) {
    fprintf(stderr, "/proc/self/pagemap read error\n");
    abort();
  }

  union memctl_vmm memctl_param;

  uint64_t total_time = 0;
  for (int i = 0; i < 100; ++i) {
    memset(arena, 1, size);

    memset(&memctl_param, 0, sizeof(memctl_param));
    memctl_param.call.addr = (__u64)arena_paddr;
    memctl_param.call.func_code = MEMCTL_DONTNEED;
    memctl_param.call.length = size;
    memctl_param.call.arg = 0;

    uint64_t time_elapsed = -time_us();
    if (ioctl(memctl_fd, MEMCTL_IOCTL_VMM, &memctl_param) < 0) {
      perror("ioctl");
      abort();
    }
    time_elapsed += time_us();

    if (memctl_param.ret.ret_errno || memctl_param.ret.ret_code) {
      fprintf(stderr, "memctl error: errno %d, code %d",
              memctl_param.ret.ret_errno, memctl_param.ret.ret_code);
      abort();
    }

    printf("run %d, %lu\n", i, time_elapsed);
    total_time += time_elapsed;
  }
  printf("average time %lu us\n", total_time / 100);

  close(pagemap_fd);
  close(memctl_fd);
  return 0;
}
