CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

all: daemon demo_worker

daemon: daemon.c
	$(CC) $(CFLAGS) -o daemon daemon.c

demo_worker: demo_worker.c
	$(CC) $(CFLAGS) -o demo_worker demo_worker.c

clean:
	rm -f daemon demo_worker
	rm -f server_runtime/REQUESTS
	rm -f server_runtime/*.0
	rm -f server_runtime/*.1
	-./scripts/stop.sh 2>/dev/null || true