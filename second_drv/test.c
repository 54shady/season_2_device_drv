#include <stdio.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	int fd;
	unsigned char key_val[6];
	int i;
	int ret;
	static int cnt = 0;
	
	fd = open("/dev/buttons",O_RDWR);
	if (fd < 0)
	{
		printf("Oops........can't open the file\n");
		return -1;
	}

	while (1)
	{
		read(fd, key_val, sizeof(key_val));
		if (!key_val[0] || !key_val[1] || !key_val[2] || !key_val[3]|| !key_val[4]|| !key_val[5])
		{
			printf("%04d key pressed: %d %d %d %d %d %d\n", cnt++, key_val[0], key_val[1], key_val[2], key_val[3], key_val[4], key_val[5]);
		}
	}

	close(fd);
	return 0;
}
