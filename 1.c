#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdatomic.h>

#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h> /* For O_* constants */
static int shmem_fd = 0;
static uint8_t* shmem_mem = NULL;

#include <linux/futex.h>      /* Definition of FUTEX_* constants */
#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <unistd.h>

static const int S = 8 + 128;

int main(void) {
	if (shmem_fd == 0) {
		shmem_fd = shm_open("__plc_check", O_RDWR | O_CREAT, 0777);

		for (int i = 0; i < S; i++) {
			write(shmem_fd, (uint8_t[]){0x00}, 1);
		}

		shmem_mem = mmap(NULL, S, PROT_READ | PROT_WRITE, MAP_SHARED, shmem_fd, 0);
	}

	// [0...4)   futex worker blocking on
	// [4...8)   futex master blocking on
	// [8...128) user data
	_Atomic uint8_t *worker = (_Atomic uint8_t*) &shmem_mem[0];
	_Atomic uint8_t *master = (_Atomic uint8_t*) &shmem_mem[4];
	uint8_t *usdata = &shmem_mem[8];

	while(true) {
		// wait master wake me
		int count = 1000;
		while (!atomic_load(worker) && count) {
			count --;
		}
		if (!count) {
			syscall(SYS_futex, worker, FUTEX_WAIT, 0, NULL, NULL, 0);
		}
		atomic_store(worker, 0);

		// read data from memory and process
		for (int i = 0; i < 128; i++) {
			usdata[i] = 1;
		}

		// wake up master
		atomic_store(master, 1);
		syscall(SYS_futex, master, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
	}

	close(shmem_fd);
	shm_unlink("__plc_check");
	return 0;
}
