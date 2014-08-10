#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <libaio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/eventfd.h>

bool isCallback = false;

void aio_callback(io_context_t ctx, struct iocb* iocb, long res, long res2) {
	isCallback = true;
	printf("callback\n");	
}

int main() {
	int output_fd;
	char* content = "hello world!";
	char* outputfile = "hello.txt";
	io_context_t ctx;
	struct iocb io;
	struct iocb *p = &io;
	struct io_event e;
	struct timespec timeout;

	memset(&ctx, 0, sizeof(io_context_t));
	if (io_setup(10, &ctx) != 0) {
		puts("io_setup error");
	}

	output_fd = open(outputfile, O_CREAT | O_WRONLY, 0644);
	if (output_fd < 0) {
		puts("open error");
		io_destroy(ctx);
		return -1;
	}

	int evfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	io_prep_pwrite(&io, output_fd, content, strlen(content), 0);
	io_set_callback(&io, aio_callback);
	io_set_eventfd(&io, evfd);
	io.data = content;
	if (io_submit(ctx, 1, &p) != 1) {
		puts("submit error");
		io_destroy(ctx);
		return -1;
	} else {
		puts("submit");
	}

	
	//*/
	while (true) {
		timeout.tv_sec = 0;
		timeout.tv_nsec = 500000000;
		if (io_getevents(ctx, 0, 1, &e, &timeout) == 1) {
			close(output_fd);
			break;
		}
		printf("haven't done");
		sleep(1);
	}
	//*/
    while (!isCallback) {
	}
	//*/	
	io_destroy(ctx);

	return 0;
}
