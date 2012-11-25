/*
 * =====================================================================================
 *
 *       Filename:  ril_con.c
 *
 *    Description:  for Meizu Inc Modem test.
 *
 *        Version:  1.0
 *        Created:  2012年11月08日 15时00分07秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  KarlZheng (zhengkarl#gmail.com)
	            JianPing Liu
 *        Company:  Meizu, Inc
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include "readline/readline.h"
#include "readline/history.h"

/*define max count of chars to be print*/
#define	MAXCHARS 1024
/* CHARS_PER_LINE */
#define CPL 16

char* format_hex_string(const unsigned char *buf, int count) 
{
	const static char hexchar_table[] = {'0', '1', '2', '3', '4', '5', '6',
		'7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	/*a char=2hex+a space+a printable char+(MAXCHARS+CPL-1)/CPL '\n'+'\0'*/
	static char line[4 * MAXCHARS + (MAXCHARS + CPL - 1)/CPL + 1];
	int actcount = (count < MAXCHARS) ? count : MAXCHARS;
	int index = 0;
	int i, r;

	r = actcount % CPL;
	for (i = 0; i < actcount; i++) {
		index = i/CPL*CPL*4 + i/CPL + i%CPL*3;
		line[index + 0] = hexchar_table[buf[i] >> 4]; 
		line[index + 1] = hexchar_table[buf[i] & 0x0f]; 
		line[index + 2] = ' ';

		if (r && (i >= actcount-r))
			index = i/CPL*CPL*4 + i/CPL + r*3 + i%CPL;
		else
			index = i/CPL*CPL*4 + i/CPL + CPL*3 + i%CPL;

		line[index] = isprint(buf[i]) ?  buf[i]: '.' ;

		if (i % CPL == CPL - 1) 
			line[++index] = '\n';
	}

	line[++index] = 0;

	return line;
}

#define xprintf(...) my_rl_printf(__VA_ARGS__)
void my_rl_printf(char *fmt, ...)
{
    int need_hack = (rl_readline_state & RL_STATE_READCMD) > 0;
    char *saved_line;
    int saved_point;

    if (need_hack) {
        saved_point = rl_point;
        saved_line = rl_copy_text(0, rl_end);
        rl_save_prompt();
        rl_replace_line("", 0);
        rl_redisplay();
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    if (need_hack) {
        rl_restore_prompt();
        rl_replace_line(saved_line, 0);
        rl_point = saved_point;
        rl_redisplay();
        free(saved_line);
    }
}

#define BUFFER_SIZE (4096 * 4)
struct phy_device {
	int	fd;
	char	buffer[BUFFER_SIZE];
};

int done = 0;
int atdebug = 1;

static pthread_mutex_t s_state_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ril command buffer */
int at_send(int fd, const char *cmd);
int open_phy_device(char *serial);

void interrupt_handler(int signum)
{
	done = 1;
	printf("\n");
}

int ril_con_exit_cb(int fd)
{
	close(fd);
	write_history(".rilcon_history");
	return 0;
}

int process_command(struct phy_device *phy, const char* cmdline)
{
	if (strcmp(cmdline, "q") == 0)
		done = 1;
	else if (strcmp(cmdline, "quit") == 0)
		done = 1;
	else if (strcmp(cmdline, "atdebug") == 0) {
		atdebug = !atdebug;
		if (atdebug)
			printf("atdebug enabled!\n");
		else
			printf("atdebug disabled!\n");
	} else if (strcmp(cmdline, "exit") == 0) {
		ril_con_exit_cb(phy->fd);
		exit(0);
	} else
		at_send(phy->fd, cmdline);

	return 0;
}

void *revcLoop(void *param)
{
	int fd;
	int ret;
	char *ptr;
	fd_set rdfs;
	struct phy_device *phy = (struct phy_device *)param;

	fd = phy->fd;
	if (fd <= 0) {
		printf("%s phy->fd:%d\n", __func__, fd);
		return;
	}
	ptr = phy->buffer;
	FD_ZERO(&rdfs);
	FD_SET(fd, &rdfs);

	while (select(fd + 1, &rdfs, 0, 0, NULL)) {

		pthread_mutex_lock(&s_state_mutex);
		ret = read(fd, ptr, sizeof(phy->buffer));
		pthread_mutex_unlock(&s_state_mutex);

		if (ret > 0) {
			if (atdebug)
				xprintf("RX:\n%s\n", \
						format_hex_string(ptr, ret));
			else
				xprintf("RX:\n%s\n", ptr);
			memset(ptr, 0, BUFFER_SIZE);
		} else {
			fprintf(stderr, "recv failure!\n");
			return 0;
		}
	}
}

int main(int argc, const char* const argv[])
{
	int ret = 0;
	static char serial[256];
	struct phy_device phy;
	pthread_attr_t attr;
	pthread_t s_tid_mainloop;
	FILE *script_fp = NULL;

	while ((ret = getopt(argc, argv, "d:e:")) != -1) {
		switch (ret) {
			case 'd':
				strcpy(serial, optarg);
				break;
			case 'e':
				printf("Auto exec script file: %s\n", optarg);
				if((script_fp  = fopen(optarg, "rb")) == NULL)
					printf("%s open script_fp:%s error!\n", __func__, optarg);
				break;
			default:
				fprintf(stderr, "Error: Unknown option '%c'\n", ret);
		}
	}
	
	if (strlen(serial) == 0) {
		printf("set default serial: /dev/ttyACM1\n");
		strcpy(serial, "/dev/ttyACM1");
	}
	
	phy.fd = open_phy_device(serial);
	if (phy.fd < 0)
		printf("fail at open serial file:%s\n", serial);
	
	/* sleep for open */
	sleep(0.1);
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&s_tid_mainloop, &attr, revcLoop, &phy);
	if (ret != 0)
		fprintf(stderr, "create thread revcLoop failure!\n");

	signal(SIGINT,  interrupt_handler); 
	signal(SIGHUP,  interrupt_handler); 
	signal(SIGALRM, interrupt_handler); 
	signal(SIGTERM, interrupt_handler); 

	rl_set_signals();
	using_history();
	read_history(".rilcon_history");

	if (script_fp) {
		char *line = NULL;
		size_t len = 0;
		ssize_t read;
		while ((read = getline(&line, &len, script_fp)) != -1) {
			/*printf("%s", line);*/
			while(read > 0) {
				--read;
				if ((line[read] == '\r') || (line[read] == '\n'))
					line[read] = '\x0';
				else
					break;

			}
			process_command(&phy, line);
			add_history(line);
		}
		if(line)
			free(line);
		fclose(script_fp);
	}

	done = 0;
	while (!done) {
		char* line = readline("MEIZU_RILCON # ");

		if(line && *line) {
			char* expanded;
			int result = history_expand(line, &expanded);

			if(result < 0 || result == 2) {
				if(result == 2)
					printf("%s\n", expanded);
				free(line);
				free(expanded);
				continue;
			}
			process_command(&phy, expanded);
			add_history(expanded);
			free(expanded);
		}
		if (line)
			free(line);
	}

	ril_con_exit_cb(phy.fd);

	return 0;
}

int open_phy_device(char *serial)
{
	int fd;
	int status = TIOCM_DTR | TIOCM_RTS;
	struct termios attr;
	int ret;

	printf("open device: %s\n", serial);

	fd = open(serial, O_RDWR);
	if(fd < 0)
		return fd;

	tcgetattr(fd, &attr);
	attr.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD);

	attr.c_cflag |= CREAD | CLOCAL | CS8 | CRTSCTS;

	attr.c_cflag |= CREAD | CLOCAL | CS8;
	attr.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
	attr.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL);
	attr.c_iflag &= ~(IXON | IXOFF);
	attr.c_oflag &= ~(OPOST | OCRNL);

	attr.c_cflag |= IXON | IXOFF | IXANY;

	cfsetispeed(&attr, B115200);
	cfsetospeed(&attr, B115200);
	tcsetattr(fd, TCSANOW, &attr);
	ioctl(fd, TIOCMBIS, &status);
	tcflush(fd, TCIOFLUSH);

	return fd;
}

int at_send(int fd, const char *cmd)
{
	int ret;
	char buf[512];
#if 0	
	struct timeval time;
	gettimeofday(&time, NULL);
	printf("\n[rilsend@%d]:%s\n", (int)time.tv_sec, cmd);
#endif
	strcpy(buf, cmd);

	rl_save_prompt();
	if (atdebug)
		printf("TX:\n%s\n", \
				format_hex_string(cmd, strlen(cmd)));
	else
		printf("TX:\n%s\n", cmd);
	rl_restore_prompt();
	
	if (fd <= 0) {
		printf("%s fd:%d, command has not been sent!\n", __func__, fd);
		return fd;
	}

	buf[strlen(buf)] = 0x0d;

	pthread_mutex_lock(&s_state_mutex);
	do {
		ret = write(fd, buf, strlen(buf));
	} while (ret < 0 && errno == EINTR);

	/* drain the buffer */
	ioctl(fd, TCSBRK, 1);
	pthread_mutex_unlock(&s_state_mutex);

	return ret;
}
