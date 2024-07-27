queue: queue.c
	gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 -pthread -c queue.c