#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>


int main() {
	int fd = open("file2", O_CREAT | O_RDWR, 0666);
	assert(fd > 0);
	int off = 65536 * 2;
	char buf[off] = "0";
	for (int i = 0; i < off; i++)
		buf[i] = '0';
	int ret = write(fd, buf, off);
	assert(ret == off);
	ret = pwrite(fd, "MYfoo\n", 6, off);
	assert(ret == 6);
	ret = close(fd);
	assert(ret == 0);
	printf("Initiated\n");
}
