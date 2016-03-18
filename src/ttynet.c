/**************************************************************
    ttynet - connect a virtual tty port to a network
    exported tty device

    (C)2013-2016 by Rumen G.Bogdanovski
***************************************************************/
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/stat.h>
#include "config.h"

#define NAME_SIZZ 1024
#define BUFSIZZ 1024
#define h_addr h_addr_list[0] /* for backward compatibility */
#define unlink_tty(tty_name) if ((tty_name[0]) != '\0') unlink(tty_name)

typedef struct {
	int tcp_port;
	char reconnect;
	int reconnect_time;
	char address[NAME_SIZZ];
	char tty_name[NAME_SIZZ];
} config;
config conf;


int open_pts(char *pts_name, int pts_name_size) {
	char *pname;
	int fd;

	fd = posix_openpt(O_RDWR | O_NOCTTY);
	if (fd < 0)
		return -1;
	if (grantpt(fd) < 0) {
		close(fd);
		return -1;
	}
	if (unlockpt(fd) < 0) {
		close(fd);
		return -1;
	}
	if ((pname = ptsname(fd)) == NULL) {
		close(fd);
		return -1;
	}

	strncpy(pts_name, pname, pts_name_size);
	return fd;
}


int open_tcp(char *host, int port) {
	struct sockaddr_in srv_info;
	struct hostent *he;
	int sock;

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


int data_pump(int net_fd, int pty_fd, char exit_on_close) {
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
				if ((r < 0) && (errno != EAGAIN)) { /* ignore the error if the buffer is full */
					printf("write(pty_fd): %s\n",strerror(errno));
				}
				continue;
			}
		}

		if(FD_ISSET(pty_fd,&readset)) {
			r = read(pty_fd,buf,BUFSIZZ-1);
			if (r <= 0) {
				if (exit_on_close) {
					return -2;
				} else {
					usleep(50000); /* offload the cpu as once the client closes tty -> FD_ISSET. Wired?! */
					continue;
				}
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


void sig_handler(int sig) {
	pid_t pgrp;

	#ifdef SIG_DEBUG
	printf("SIG: pid=%d, signal=%d", getpid(), sig);
	#endif
	switch (sig) {
	case SIGHUP:
	case SIGPIPE:
	case SIGTERM:
	case SIGINT:
	case SIGQUIT:
	case SIGCHLD:
		unlink_tty(conf.tty_name);
		exit(0);
		break;
	}
}


void print_usage(char *name) {
	printf( "%s version %s\n"
		"This app connects a virtual serial port to a network exported serial port.\n"
		"The serial port can be exported with nexbridge, SkyFi device etc. The app\n"
		"is intended to be used with software like Stellarium that relies on serial\n"
		"port to control telescope mounts, thus enabling it to control network\n"
		"exported mounts too. (see nexbridge)\n\n", name, VERSION);
	printf( "usage: %s [-vr] -a address -p port [-T tty] [-t seconds]\n"
		"    -a  IP address to connect to\n"
		"    -p  TCP port to connect to\n"
		"    -r  reconnect when virtual port is closed\n"
		"    -t  delay between reconnects in seconds (used with -r) [default: %d]\n"
		"    -T  virtual tty name to create\n"
		"    -v  print version\n"
		"    -h  print this help message\n\n", name, RECONNECT_TIME);
	printf( " Copyright (c)2014-2016 by Rumen Bogdanovski\n\n");
}


void config_defaults() {
	conf.tcp_port = 0;
	conf.address[0] = '\0';
	conf.tty_name[0] = '\0';
	conf.reconnect = 0;
	conf.reconnect_time = RECONNECT_TIME;
}


int main(int argc, char **argv) {
	char tty_name[NAME_SIZZ];
	int res, c;
	int tcp_fd;
	int tty_fd;
	struct sigaction sa;

	config_defaults();
	while((c=getopt(argc,argv,"hvra:p:T:t:"))!=-1){
		switch(c){
		case 'a':
			strncpy(conf.address, optarg, 255);
			break;
		case 'p':
			conf.tcp_port = atoi(optarg);
			break;
		case 'r':
			conf.reconnect = 1;
			break;
		case 'T':
			strncpy(conf.tty_name, optarg, 255);
			break;
		case 'h':
			print_usage(argv[0]);
			exit(0);
		case 'v':
			printf("%s version %s\n", argv[0], VERSION);
			exit(0);
		case 't':
			conf.reconnect_time = atoi(optarg);
			break;
		case '?':
		default:
			printf("for help: %s -h\n", argv[0]);
			exit(1);
		}
	}

	if ((conf.address[0] == '\0') || (conf.tcp_port == 0)) {
		printf("Please specify address and port, for help: %s -h\n", argv[0]);
		exit(1);
	}

	if ((conf.reconnect_time < 1) || (conf.reconnect_time > 3600)) {
		printf("Reconnection time should be between 1 and 3600 seconds, for help: %s -h\n", argv[0]);
		exit(1);
	}

	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		printf("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGHUP, &sa, NULL) == -1) {
		printf("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		printf("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		printf("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		printf("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGQUIT, &sa, NULL) == -1) {
		printf("sigaction(): %s",strerror(errno));
		exit(1);
	}

	do { /* recreate the pty if the file is closed by the app otherwise select() always returns */
		tcp_fd = open_tcp(conf.address, conf.tcp_port);
		if (tcp_fd == -1) {
			printf("Can not connect to %s:%d.\n", conf.address, conf.tcp_port);
			exit(1);
		}
		tty_fd = open_pts(tty_name, NAME_SIZZ);
		if (tty_fd == -1) {
			close(tcp_fd);
			printf("Can not allocate virtual tty.\n");
			exit(1);
		}
		printf("Connection: [%s] <=> [%s:%d]\n", tty_name, conf.address, conf.tcp_port);
		if(conf.tty_name[0] != '\0') {
			res = symlink(tty_name,conf.tty_name);
			if (res < 0) {
				printf("Can not create a symbolic link: %s\n",strerror(errno));
				conf.tty_name[0]='\n';
			} else {
				printf("Use: '%s' to connect.\n", conf.tty_name);
			}
			if(geteuid() == 0) { /* if root allow everyone to use it (crw-rw-rw-) */
				res = chmod(conf.tty_name, 0666);
				if (res < 0) {
					printf("Could not change mode: %s\n",strerror(errno));
				}
			}
		}

		res = data_pump(tcp_fd, tty_fd, conf.reconnect);

		close(tty_fd);
		unlink_tty(conf.tty_name);
		close(tcp_fd);
		printf("Remote connection closed.\n");
		if (conf.reconnect) {
			printf("Will reconnect in %d sec...\n", conf.reconnect_time);
			sleep(conf.reconnect_time);
		}
	} while (res == -2);

	printf("Exitting.\n");

	return EXIT_SUCCESS;
}
