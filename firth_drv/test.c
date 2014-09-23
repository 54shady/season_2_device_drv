#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

/* 
 *	如何使用该测试文件:
 *	mknod /dev/leds c major 0   
 *	其中major是主设备号可通过看/proc/devices得到
 *	1 --> led 1
 *	2 --> led 2
 *	3 --> led 3
 *	4 --> led 4
 */
struct my_led
{
	char swich;
	char name;
};

int main(int argc, char **argv)
{

	int fd;
	struct my_led ml;
	int wBytes;
	
	if (argc != 3)
	{
		printf("Usage: %s < 1 | 2 | 3 | 4 | all > <on | off>\n", argv[0]);
		printf(" 1 --> led 1 \n");
		printf(" 2 --> led 2 \n");
		printf(" 3 --> led 3 \n");
		printf(" 4 --> led 4 \n");
		return -2;
	}

	fd = open("/dev/leds", O_RDWR);
	if (fd < 0)
	{
		printf("Can't open\n");
		return -1;
	}

	if (!(strcmp(argv[2], "on")))
		ml.swich = 1;
	else
		ml.swich = 0;	

	if (!(strcmp(argv[1], "1")))
		ml.name = 1;
	else if (!(strcmp(argv[1], "2")))
		ml.name = 2;
	else if (!(strcmp(argv[1], "3")))
		ml.name = 3;
	else if (!(strcmp(argv[1], "4")))
		ml.name = 4;
	else if (!(strcmp(argv[1], "all")))
		ml.name = 5;
	else
		ml.name = 0;

	wBytes = write(fd, &ml, sizeof(ml));
	printf("wBytes = %d\n", wBytes);
	close(fd);

	return 0;
}
