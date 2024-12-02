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

#include "pvmemcontrol.h"

static void memset_volatile(volatile void *s, char c, size_t n)
{
    volatile char *p = s;
    while (n-- > 0) {
        *p++ = c;
    }
}

static void assert_char(volatile void *s, char c, size_t n)
{
    volatile char *p = s;
    while (n-- > 0) {
        if (*p != c)
					printf("error 0x%p is not 0x%x\n", p, c);
				p++;
    }
}

int pagemap_get_entry(int pagemap_fd, uintptr_t vaddr, uintptr_t *paddr)
{
	size_t nread = 0;
	ssize_t ret;
	uint64_t data;

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

	if (!(data & (1ULL << 63))) { // Check if page is present
		fprintf(stderr, "Page not present\n");
		return -1;
	}

	*paddr = (data & (((uint64_t)1 << 54) - 1)) * getpagesize();
	printf("converted paddr %lx\n", *paddr);
	return 0;
}

int get_physical_address(int fd, uintptr_t virtual_address, uintptr_t *paddr) {
    uint64_t offset = ((uintptr_t)virtual_address / getpagesize()) * 8; // 64-bit pagemap entries
    uint64_t pagemap_entry;

    if (pread(fd, &pagemap_entry, 8, offset) != 8) { 
        perror("pread");
        return -1;
    }

    if (!(pagemap_entry & (1ULL << 63))) { // Check if page is present
        fprintf(stderr, "Page not present\n");
        return -1;
    }

    uint64_t page_frame_number = pagemap_entry & ((1ULL << 54) - 1); // Mask out PFN
    uint64_t physical_address = page_frame_number * getpagesize() + 
                                ((uintptr_t)virtual_address % getpagesize());

    *paddr = physical_address;
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
  int pvmemcontrol_fd = open("/dev/pvmemcontrol0", O_RDWR);
  if (pvmemcontrol_fd < 0) {
    perror("pvmemcontrol open");
    abort();
  }

  size_t size = 1UL * 1024 * 1024 * 1024;
  int flags = MAP_ANONYMOUS | MAP_HUGETLB | MAP_PRIVATE;
  volatile void *arena = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);

  if (arena == MAP_FAILED) {
    perror("mmap");
    abort();
  }

  memset_volatile(arena, 0, size);

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
  struct pvmemcontrol_buf pvmemcontrol_param;

  uint64_t total_time = 0;
  for (int i = 0; i < 100; ++i) {
    assert_char(arena, 0, size);
    memset_volatile(arena, 1, size);
    assert_char(arena, 1, size);

    memset_volatile(&pvmemcontrol_param, 0, sizeof(pvmemcontrol_param));
    pvmemcontrol_param.call.addr = (__u64)arena_paddr;
    pvmemcontrol_param.call.func_code = PVMEMCONTROL_DONTNEED;
    /* pvmemcontrol_param.call.func_code = PVMEMCONTROL_SET_VMA_ANON_NAME; */
    pvmemcontrol_param.call.length = size;
    pvmemcontrol_param.call.arg = 0;

    uint64_t time_elapsed = -time_us();

    if (ioctl(pvmemcontrol_fd, PVMEMCONTROL_IOCTL_VMM, &pvmemcontrol_param) < 0) {
      perror("ioctl");
      abort();
    }

    time_elapsed += time_us();

    if (pvmemcontrol_param.ret.ret_errno || pvmemcontrol_param.ret.ret_code) {
      fprintf(stderr, "pvmemcontrol error: errno %d, code %d",
              pvmemcontrol_param.ret.ret_errno, pvmemcontrol_param.ret.ret_code);
      abort();
    }

    printf("run %d, %lu\n", i, time_elapsed);
    total_time += time_elapsed;
  }
  printf("average time %lu us\n", total_time / 100);


  close(pagemap_fd);
  close(pvmemcontrol_fd);
  getchar();
  return 0;
}
