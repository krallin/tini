#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
	printf("Going to eat your memories!\n");

	while(1) {
		malloc(200 * sizeof(int));
		usleep(5);
	}

	return 0;
}
