# Example program using pvmemcontrol

## Prerequisite
- Guest Linux kernel with pvmemcontrol support
- Cloud Hypervisor with pvmemcontrol support

## Usage
Enable pvmemcontrol in Cloud Hypervisor with `--pvmemcontrol`, and reserve at least one 1GB hugepage with `hugepagesz=1G hugepages=1 default_hugepagesz=1G`.

Run the `pvmemcontrol_ioctl` binary to perform PVMEMCONTROL_DONTNEED (MADV_DONTNEED) on a hugetlb page. The change in memory usage is visible in the host via /proc/pid/smaps.

