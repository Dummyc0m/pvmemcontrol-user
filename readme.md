# Example program using memctl

## Prerequisite
- Guest kernel with memctl support
- Cloud Hypervisor build with memctl support

## Usage
Enable memctl in Cloud Hypervisor with `--memctl`, and reserve at least one 1GB hugepage with `hugepagesz=1G hugepages=1 default_hugepagesz=1G`.

Within the guest, run `memctl_dev.sh` to mount the memctl chardev for the userspace interface.

Run the `memctl_ioctl` binary to perform PR_SET_ANON_VMA_NAME on a hugetlb page, visible in the host via /proc/pid/smaps.

