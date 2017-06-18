#include "gpio.h"

/* */

int gpio_setup(int port, char *name, int dir)
{
	char gpio[128];
	FILE *file;
	int ret = 0;

	printf("setup gpio=%d name=%s dir=%d\n", port, name, dir);

	file = fopen("/sys/class/gpio/export", "w");
	if (!file) {
		perror("can't open gpio export");
		return -1;
	}

	ret = fprintf(file, "%d\n", port);
	if (ret < 0) {
		perror("can't export gpio");
		return -1;
	}

	fclose(file);

	sprintf(gpio, "/sys/class/gpio/%s/direction", name);

	file = fopen(gpio, "w");
	if (!file) {
		perror("can't open gpio direction");
		return -1;
	}

	if (dir == 0) {
		ret = fprintf(file, "in\n");
		if (ret < 0) {
			perror("can't write gpio direction");
			return -1;
		}
	}
	else
	{
		ret = fprintf(file, "out\n");
		if (ret < 0) {
			perror("can't write gpio direction");
			return -1;
		}
	}

	fclose(file);
	return 0;
}

int gpio_close(int port)
{
	FILE *file;
	int ret = 0;

	file = fopen("/sys/class/gpio/unexport", "w");
	if (!file) {
		perror("can't open gpio unexport");
		return -1;
	}

	ret = fprintf(file, "%d\n", port);
	if (ret < 0) {
		perror("can't unexport gpio");
		return -1;
	}

	fclose(file);
	return 0;
}

int gpio_read(char *name)
{
	char gpio[128];
	int val, ret = 0;
	FILE *file;

	sprintf(gpio, "/sys/class/gpio/%s/value", name);

	file = fopen(gpio, "r");
	if (!file) {
		perror("can't open gpio value for read");
		return -1;
	}

	ret = fscanf(file, "%d", &val);
	if (ret < 0) {
		perror("can't read gpio value");
		return -1;
	}

	fclose(file);
	return val;
}

int gpio_write(char *name, int value)
{
	char gpio[128];
	FILE *file;
	int ret = 0;

	sprintf(gpio, "/sys/class/gpio/%s/value", name);
	file = fopen(gpio, "w");
	if (!file) {
		perror("can't open gpio value for write");
		return -1;
	}

	if (value == 0) {
		ret = fprintf(file, "0\n");
		if (ret < 0) {
			perror("can't write 0 to gpio");
			return -1;
		}
	} else {
		ret = fprintf(file, "1\n");
		if (ret < 0) {
			perror("can't write 1 to gpio");
			return -1;
		}
	}

	fclose(file);

	return 0;
}
