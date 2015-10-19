/**************************************************************
    ttynet - connect a virtual tty port to a network
    exported tty device

    (C)2013-2015 by Rumen G.Bogdanovski
***************************************************************/
#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "config.h"

typedef struct {
	int port;
	char address[255];
	int timeout;
} config;

config conf;

//#define VERSION "0.1-a1"

#define h_addr h_addr_list[0] /* for backward compatibility */

#define BUFSIZZ 1024


int open_ptym(char *pts_name_s, int pts_namesz) {
	char *ptr;
	int fdm;

	fdm = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fdm < 0)
		return -1;
	if (grantpt(fdm) < 0) {
		close(fdm);
		return -1;
	}
	if (unlockpt(fdm) < 0) {
		close(fdm);
		return -1;
	}
	if ((ptr = ptsname(fdm)) == NULL) {
		close(fdm);
		return -1;
	}

	strncpy(pts_name_s, ptr, pts_namesz);
	return (fdm);
}

int open_tcp(char *host, int port) {
	struct sockaddr_in srv_info;
	struct hostent *he;
	int sock;
	struct timeval timeout;

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	if ((he = gethostbyname(host))==NULL) {
		return -1;
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0))== -1) {
		return -1;
	}

	memset(&srv_info, 0, sizeof(srv_info));
	srv_info.sin_family = AF_INET;
	srv_info.sin_port = htons(port);
	srv_info.sin_addr = *((struct in_addr *)he->h_addr);
	if (connect(sock, (struct sockaddr *)&srv_info, sizeof(struct sockaddr))<0) {
		return -1;
	}

	return sock;
}

int data_pump(int net_fd, int pty_fd) {
	char buf[BUFSIZZ];
	int r;
	int max;
	fd_set readset;

	do {
		do {
			FD_ZERO(&readset);
			FD_SET(net_fd, &readset);
			FD_SET(pty_fd, &readset);
			max = (net_fd > pty_fd) ? net_fd : pty_fd;
			r = select(max + 1, &readset, NULL, NULL, NULL);
		} while (r == -1 && errno == EINTR);

		if ( r < 0 ) return -1;

		if(FD_ISSET(net_fd,&readset)) {
			r = read(net_fd,buf,BUFSIZZ-1);
			if (r <= 0) {
				if(r < 0) printf("read(net_fd): %s\n",strerror(errno));
				return -1;
			}
			r = write(pty_fd,buf,r);
			if (r <= 0) {
				if ((r < 0) && (errno != EAGAIN)) { // ignore the error if the buffer is full
					printf("write(pty_fd): %s\n",strerror(errno));
				}
				continue;
			}
		}

		if(FD_ISSET(pty_fd,&readset)) {
			r = read(pty_fd,buf,BUFSIZZ-1);
			if (r <= 0) {
				//if(r<0) printf("read(pty_fd): %s\n",strerror(errno));
				usleep(50000); // offload the cpu as once the client closes tty -> FD_ISSET. Wired?!
				continue;
			}
			r = write(net_fd,buf,r);
			if (r <= 0) {
				if(r < 0) printf("write(net_fd): %s\n",strerror(errno));
				return -1;
			}
		}
	} while (1);

	return 0;
}

/*
int data_pump1(int net_fd, int pty_fd) {
	char buf[BUFSIZZ];
	int r;
	int max;
	fd_set readset;

	do {
		do {
			FD_ZERO(&readset);
			FD_SET(net_fd, &readset);
			FD_SET(pty_fd, &readset);
			max = (net_fd > pty_fd) ? net_fd : pty_fd;
			r = select(max + 1, &readset, NULL, NULL, NULL);
		} while (r == -1 && errno == EINTR);

		if ( r < 0 ) return -1;

		if(FD_ISSET(net_fd,&readset)) {
			r = read(net_fd,buf,BUFSIZZ-1);
			if (r <= 0) {
				if(r < 0) printf("read(net_fd): %s\n",strerror(errno));
				return -1;
			}
			r = write(pty_fd,buf,r);
			if (r <= 0) {
				if(r < 0) printf("write(pty_fd): %s\n",strerror(errno));
				return -2;
			}
		}

		if(FD_ISSET(pty_fd,&readset)) {
			r = read(pty_fd,buf,BUFSIZZ-1);
			if (r <= 0) {
				if(r < 0) printf("read(pty_fd): %s\n",strerror(errno));
				return -2;
			}
			r = write(net_fd,buf,r);
			if (r <= 0) {
				if(r < 0) printf("write(net_fd): %s\n",strerror(errno));
				return -1;
			}
		}
	} while (1);

	return 0;
}
*/

void print_usage(char *name) {
	printf( "%s %s\n", name, VERSION);
	printf( "usage: %s [-v] -a address -p port [-t timeout]\n"
		"    -a  IP address to connect to\n"
		"    -p  TCP port to connect to\n"
		"    -t  session timeout in seconds (0 never times out) [default: 0]\n"
		"    -v  print version\n"
		"    -h  print this help message\n\n", name);
	printf( " Copyright (c)2014-2015 by Rumen Bogdanovski\n\n");
}

void config_defaults() {
	conf.port = 0;
	conf.address[0] = '\0';
	conf.timeout = 0;
}

int main(int argc, char **argv) {
	char tty_name[1024];
	int res, c;
	int tcp_fd;
	int tty_fd;

	while((c=getopt(argc,argv,"hva:p:t:"))!=-1){
		switch(c){
		case 'a':
			strncpy(conf.address, optarg, 255);
			break;
		case 'p':
			conf.port = atoi(optarg);
			break;
		case 't':
			conf.timeout = atoi(optarg);
			break;
		case 'h':
			print_usage(argv[0]);
			exit(1);
		case 'v':
			printf("tty2net version %s\n", VERSION);
			exit(1);
		case '?':
		default:
			printf("for help: %s -h\n", argv[0]);
			printf("for help: %s -h\n", argv[0]);
			exit(1);
		}
	}

	if ((conf.address == '\0') || (conf.port == 0)) {
		printf("Please specify address and port, for help: %s -h\n", argv[0]);
		exit(1);
	}

	tcp_fd = open_tcp(conf.address, conf.port);
	if (tcp_fd == -1) {
		printf("Can not connect to %s:%d.\n", conf.address, conf.port);
		exit(1);
	}

	do {	// recreate the pty if the file is closed by the app otherwise select() always returns
		tty_fd = open_ptym(tty_name, 1024);
		if (tty_fd == -1) {
			close(tcp_fd);
			printf("Can not allocate virtual tty.\n");
			exit(1);
		}
		printf("Created link: [%s] <=> [%s:%d]\n", tty_name, conf.address, conf.port);
		res = data_pump(tcp_fd, tty_fd);
		close(tty_fd);
	} while (res == -2);

	close(tcp_fd);
	printf("Remote connection closed\n");

	return EXIT_SUCCESS;
}
