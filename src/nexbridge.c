/**************************************************************
        nexbridge - export tty port on the network

        (C)2013-2016 by Rumen G.Bogdanovski
***************************************************************/
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>

#include "nexbridge.h"
#include "mdns_avahi.h"
#include "config.h"

#define BUFSIZZ 1024

volatile int conn_count=0;

config conf;

#define ATOMIC_INC(i) ((void)__sync_add_and_fetch(i,1))
#define ATOMIC_DEC(i) ((void)__sync_sub_and_fetch(i,1))

sbaud_rate br[] = {
	BR(  "1200", B1200),
	BR(  "1800", B1800),
	BR(  "2400", B2400),
	BR(  "4800", B4800),
	BR(  "9600", B9600),
	BR( "19200", B19200),
	BR( "38400", B38400),
	BR( "57600", B57600),
	BR("115200", B115200),
	BR("230400", B230400),
	BR("460800", B460800),
	BR(      "", 0),
};

/* map string to actual baudrate value */
int map_str_baudrate(char *baudrate) {
	sbaud_rate *brp = br;
	do {
		if (brp->str[0]=='\0') return -1;
		brp++;
	} while (strncmp(brp->str, baudrate, brp->len));

	return brp->value;
}

/* get sockaddr, IPv4 or IPv6: */
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void sig_handler(int sig) {
	pid_t pgrp;

	#ifdef SIG_DEBUG
	LOG_DBG("SIG: pid=%d, signal=%d", getpid(), sig);
	#endif
	switch (sig) {
	case SIGHUP:
		break;
	case SIGPIPE:
		break;
	case SIGTERM:
	case SIGINT:
	case SIGQUIT:
		pgrp = getpgrp();
		if (pgrp==getpid()) {
			LOG("Daemon dieing with signal=%d", sig);
			if (conf.svc_name[0]) mdns_stop();
			killpg(pgrp,SIGINT);
			exit(0);
		} else {
			#ifdef SIG_DEBUG
			LOG_DBG("Signal %d caught by child", sig);
			#endif
			_exit(0);
		}
		break;
	case SIGCHLD:
		while(waitpid(-1, NULL, WNOHANG) > 0) {
			ATOMIC_DEC(&conn_count);
		}
		break;
	}
}

void session_timeout(int sig) {
	LOG("Session timed out");
	_exit(0);
}

void config_defaults() {
	conf.is_daemon = 1;
	conf.server_port = PORT;
	conf.address[0] = '\0';
	conf.svc_name[0] = '\0';
	sprintf(conf.svc_type, "%s.%s", SVC_TYPE, SVC_PROTO);
	strcpy(conf.tty_port, TTY_PORT);
	conf.baudrate = B9600;
	conf.timeout = SESS_TIMEOUT;
	conf.max_conn = MAXCON;
}

int open_telescope(char *dev_file, int baudrate) {
	int dev_fd;
	struct termios options;

	if ((dev_fd = open(dev_file, O_RDWR | O_NOCTTY | O_SYNC))==-1) {
		return -1;
	}

	memset(&options, 0, sizeof options);
	if (tcgetattr(dev_fd, &options) != 0) {
		close(dev_fd);
		return -1;
	}

	cfsetispeed(&options, baudrate);
	cfsetospeed(&options, baudrate);
	/* Finaly!!!!  Works on Linux & Solaris  */
	options.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG|IEXTEN);
	options.c_oflag &= ~(OPOST);
	options.c_iflag &= ~(INLCR|ICRNL|IXON|IXOFF|IXANY|IMAXBEL);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_cc[VMIN]  = 0;	// read doesn't block
	options.c_cc[VTIME] = 50;	// 5 seconds read timeout

	if (tcsetattr(dev_fd,TCSANOW, &options) != 0) {
		close(dev_fd);
		return -1;
	}

	return dev_fd;
}

int close_telescope(int devfd) {
	return close(devfd);
}

void handle_client(int fd1, int fd2) {
	char buf[BUFSIZZ];
	int r;
	int max;
	int end=0;
	fd_set readset;

	do {
		do {
			FD_ZERO(&readset);
			FD_SET(fd1, &readset);
			FD_SET(fd2, &readset);
			max = (fd1 > fd2) ? fd1 : fd2;
			r = select(max + 1, &readset, NULL, NULL, NULL);
		} while (r == -1 && errno == EINTR);

		if ( r<0 ) goto server_end;

		if(FD_ISSET(fd1,&readset)) {
			r = read(fd1,buf,BUFSIZZ-1);
			if (r <= 0) {
				if(r<0) LOG("read(fd1): %s",strerror(errno));
				end=1; break;
			}
			r = write(fd2,buf,r);
			if (r <= 0) {
				if(r<0) LOG("write(fd2): %s",strerror(errno));
				end=1; break;
			}
		}

		if(FD_ISSET(fd2,&readset)) {
			r = read(fd2,buf,BUFSIZZ-1);
			if (r <= 0) {
				if(r<0) LOG("read(fd2): %s",strerror(errno));
				end=1; break;
			}
			r = write(fd1,buf,r);
			if (r <= 0) {
				if(r<0) LOG("write(fd1): %s",strerror(errno));
				end=1; break;
			}
		}
	} while (!end);

	server_end:
	close(fd1);
	close(fd2);
	LOG("Connection closed.");
	_exit(0);
}

void serve_client(int socket) {
	int device;
	device = open_telescope(conf.tty_port, conf.baudrate);
	if ( device < 0) {
		LOG("open_telescope(): %s",strerror(errno));
		close(socket);
		_exit(1);
	}

	handle_client(device, socket);
}

int tcp_listen(in_addr_t addr, int port) {
	int sock;
	struct sockaddr_in sin;
	int val=1;

	if((sock=socket(AF_INET,SOCK_STREAM,0))<0) {
		LOG("socket(): %s",strerror(errno));
		exit(1);
	}

	memset(&sin,0,sizeof(sin));
	sin.sin_addr.s_addr = addr;
	sin.sin_family=AF_INET;
	sin.sin_port = port;
	setsockopt(sock,SOL_SOCKET,SO_REUSEADDR, &val,sizeof(val));

	if(bind(sock,(struct sockaddr *)&sin, sizeof(sin))<0) {
		LOG("bind(): %s",strerror(errno));
		exit(1);
	}

	if(listen(sock,5)<0) {
		LOG("listen(): %s",strerror(errno));
		exit(1);
	}

	return(sock);
}

void daemonize() {
	int res;

	if(getppid()==1) return; /* already a daemon */
	if((res=fork()) < 0) {
		LOG("fork: %s, Terminated",strerror(errno));
		exit(1);
	}
	if (res>0) exit(0); /* parent exits */
}

void print_usage(char *name) {
	printf( "Nexstar TCP to Serial Daemon version %s\n", VERSION);
	printf( "This software is intended to be used with libnexstar and NexStarCtl\n"
		"but it proved to be very useful for exporting all kinds of telescope\n"
		"mounts on the network like Sky-Watcher, Meade etc. Nexbridge can be\n"
		"used directly with software like SkySafari or through ttynet or other\n"
		"serial port emulator with software like Stellarium, to control the\n"
		"network exported telescopes. (see ttynet)\n\n" );
	printf( "usage: %s [-dn] [-a address] [-p port] [-m conns] [-P ttydev] [-B baudrate] [-t timeout]\n"
		"    -d  log debug information\n"
		"    -n  do not daemonize, log to stderr\n"
		"    -a  IP address to bind to [default: any]\n"
		"    -m  maximum simultaneous connections [default: 1]\n"
		"        Allowing More than one connection is not advisable!\n"
		"    -p  TCP port to bind to [default: %d]\n"
		#ifdef HAVE_MDNS
		"    -s  Bonjour service name, if not specified no service will published\n"
		"    -T  Bonjour service type [default: '_nexbridge'}\n"
		#endif
		"    -P  Serial port to connect to telescope [default: %s]\n"
		"    -B  baudrate (1200, 2400, 4800, 460800 etc) [default: %d]\n"
		"    -t  session timeout in seconds [default: %d]\n"
		"    -v  print version\n"
		"    -h  print this help message\n\n",
		name, PORT, TTY_PORT, 9600, SESS_TIMEOUT);
	printf( " Copyright (c)2014-2016 by Rumen Bogdanovski\n\n");
}


int main(int argc, char **argv) {
	int sock,s;
	int c;
	int baud;
	pid_t pid, pgrp;
	struct sockaddr_storage remote_addr;
	socklen_t addr_size;
	struct sigaction sa;
	char addrs[INET6_ADDRSTRLEN + 1]; // for zero termination
	in_addr_t addr;

	config_defaults();
	setlogmask(LOG_UPTO (LOG_INFO));
	while((c=getopt(argc,argv,"dhnva:b:m:p:P:s:T:t:"))!=-1){
		switch(c){
		case 'B':
			conf.baudrate = map_str_baudrate(optarg);
			LOG_DBG("baudrate = %d", conf.baudrate);
			if (conf.baudrate == -1) {
				printf("Baudrate is not valid: %d\n", conf.baudrate);
				exit(1);
			}
			break;
		case 'a':
			snprintf(conf.address,255,"%s", optarg);
			LOG_DBG("address = %s", conf.address);
			break;
		case 'p':
			conf.server_port = atoi(optarg);
			LOG_DBG("server_port = %d", conf.server_port);
			break;
		case 'P':
			snprintf(conf.tty_port,255,"%s", optarg);
			LOG_DBG("tty_port = %s", conf.tty_port);
			break;
		case 's':
			snprintf(conf.svc_name,255,"%s", optarg);
			LOG_DBG("svc_name = %s", conf.svc_name);
			break;
		case 'T':
			if (optarg[0] == '_')  // service type should start with '_' if not given add it
				snprintf(conf.svc_type,255,"%s.%s", optarg,SVC_PROTO);
			else
				snprintf(conf.svc_type,255,"_%s.%s", optarg,SVC_PROTO);
			LOG_DBG("svc_type = %s", conf.svc_type);
			break;
		case 't':
			conf.timeout = atoi(optarg);
			LOG_DBG("timeout = %d", conf.timeout);
			break;
		case 'm':
			conf.max_conn = atoi(optarg);
			LOG_DBG("max_conn = %d", conf.max_conn);
			break;
		case 'd':
			setlogmask(LOG_UPTO (LOG_DEBUG));
			break;
		case 'n':
			conf.is_daemon=0;
			break;
		case 'h':
			print_usage(argv[0]);
			exit(1);
		case 'v':
			printf("%s version %s\n", argv[0], VERSION);
			exit(1);
		case '?':
		default:
			printf("for help: %s -h\n", argv[0]);
			exit(1);
		}
	}

	if(((addr = inet_addr(conf.address)) == -1) && (conf.address[0]!='\0')) {
		printf("Bad address: %s\n", conf.address);
		exit(1);
	} else if(conf.address[0]=='\0') {
		addr = INADDR_ANY;
	}

	if (conf.is_daemon) daemonize();

	sa.sa_handler = sig_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		LOG("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGHUP, &sa, NULL) == -1) {
		LOG("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		LOG("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		LOG("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		LOG("sigaction(): %s",strerror(errno));
		exit(1);
	}
	if (sigaction(SIGQUIT, &sa, NULL) == -1) {
		LOG("sigaction(): %s",strerror(errno));
		exit(1);
	}
		
	/* Create a new SID for the child process */
	pgrp = setpgrp();
	if (pgrp < 0) {
		LOG("setpgrp(): %s",strerror(errno));
		exit(1);
	}

	sock=tcp_listen(addr,htons(conf.server_port));

	if (conf.svc_name[0]) {
		mdns_init(conf.svc_name, conf.svc_type, conf.tty_port, conf.server_port);
		mdns_start();
	}

	LOG("Version %s started on %s:%d",VERSION, conf.address, conf.server_port);

	while(1) {
		addr_size = sizeof remote_addr;
		if ((s=accept(sock,(struct sockaddr *)&remote_addr, &addr_size))<0) {
			LOG("accept(): %s", strerror(errno));
			/* exit(1); */ /* Which is the correct exit() or cntinue?! */
			continue;
		}

		memset(addrs, 0, sizeof(addrs));
		inet_ntop(remote_addr.ss_family, get_in_addr((struct sockaddr *)&remote_addr),
			addrs, sizeof addrs);
		if ((!conf.max_conn) || (conf.max_conn > conn_count)) {
			LOG("accept(): got connection #%d from %s fd=%d", conn_count+1, addrs, s);
		} else {
			close(s);
			LOG("accept(): connection from %s dropped, too many connections",
			     addrs);
			continue;
		}
		if ((pid=fork())) {
			if(pid > 0) ATOMIC_INC(&conn_count);
			else LOG("fork(): %s", strerror(errno));
			close(s);
		} else {
			signal(SIGALRM, session_timeout);
			alarm(conf.timeout);
			serve_client(s);
			_exit(0);
		}
	}
	exit(0);
}
