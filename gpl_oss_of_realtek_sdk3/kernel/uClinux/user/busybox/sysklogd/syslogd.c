/* vi: set sw=4 ts=4: */
/*
 * Mini syslogd implementation for busybox
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2001 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/param.h>
#include <../../turnkey/config/autoconf.h>
#include <common/sys_log.h>

#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include "busybox.h"
#include "syslogd.h"

/* SYSLOG_NAMES defined to pull some extra junk from syslog.h */
#define SYSLOG_NAMES
#include <sys/syslog.h>
#include <sys/uio.h>

#ifdef CONFIG_USER_FLATFSD_FLATFSD
	#define DEFAULT_CONFIG_FILE "/etc/syslogd.conf"
#else
	#define DEFAULT_CONFIG_FILE "/etc/syslogd.conf"
#endif

#define DEFAULT_PID_FILE "/etc/syslogd.pid"

/* Path to the unix socket */
static char lfile[MAXPATHLEN];

#define MAXLINE             1024	/* maximum line length */
#define SYS_LOG_JUNK_MSG            "dont remove"

/* Global config handle */
static syslogd_config_t *syslogd_config;

static int reload_config;

static int timezone_diff = 0;

static int load_config(syslogd_config_t *config, int argc, char *argv[]);

static void log_console_message(const char *msg)
{
	int fd;

	/* Always send console messages to /dev/console so people will see them. */
	debug_printf("sending to console");

	if ((fd = device_open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY | O_NONBLOCK)) >= 0)
	{
		debug_printf("writing to console");
		write(fd, msg, strlen(msg));
		close(fd);
	} else {
		debug_printf("writing to stderr");
		fprintf(stderr, "Bummer, can't print: %s", msg);
		fflush(stderr);
	}
}

static void logOneMessage(int pri, const char *msg, const char *timestamp, struct timeval *tv)
{
	struct tm *tm;
	static char res[20] = "";
	char iso_time[22];
	const char *content;
	syslogd_target_t *target;
	char buf[1024];
	char tmp[1024];
	char remote[1024];
	char prefix_name_buf[32];
	unsigned char prefix_copy_len = 0;
	int first = 1;

    int fd;
    struct ifreq ifr;
    unsigned int ip = 0;
    char ipStr[16] = "";

    char *c;
    char *tmpp;
    char times[64] = "";
    
    time_t timep;
    time(&timep);
    timep += timezone_diff * 60;
    tm = localtime(&timep);
    strftime(times, 64, "%b %d %Y %T" , tm);

	memset(prefix_name_buf, '\0', 32);

#ifndef EMBED
	CODE *c_pri, *c_fac;

	if (pri != 0) {
		for (c_fac = facilitynames;
			 c_fac->c_name && !(c_fac->c_val == LOG_FAC(pri) << 3); c_fac++);
		for (c_pri = prioritynames;
			 c_pri->c_name && !(c_pri->c_val == LOG_PRI(pri)); c_pri++);
		if (c_fac->c_name == NULL || c_pri->c_name == NULL) {
			snprintf(res, sizeof(res), "<%d>", pri);
		} else {
			snprintf(res, sizeof(res), "%s.%s", c_fac->c_name, c_pri->c_name);
		}
	}
#else
	snprintf(res, sizeof(res), "<%d>", LOG_PRI(pri));
#endif

    /* Get Time */
	if (syslogd_config->iso) {
		tm = localtime(&tv->tv_sec);
		snprintf(iso_time, 22,  "(%.4d%.2d%.2dT%.2d%.2d%.2d%.3lu) ",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
			tm->tm_min, tm->tm_sec, tv->tv_usec / 1000);
		content = strchr(msg, ' ');
		if (content && (*(content - 1) == ':')) {
			prefix_copy_len = ((unsigned char)(content - msg)<32) ?
				               (unsigned char)(content - msg) : 31;
			strncpy(prefix_name_buf, msg, prefix_copy_len);
			content++;
		} else {
			content = "";
			iso_time[0] = '\0';
		}
	} else {
		content = "";
		iso_time[0] = '\0';
	}

	/* Get IP */
	fd = socket(PF_INET, SOCK_DGRAM, 0);
    if(fd >= 0)
    {
        
        strcpy(ifr.ifr_name, "eth0");
        if(ioctl(fd, SIOCGIFFLAGS, &ifr) >= 0)
        {
            if(ifr.ifr_flags & (IFF_UP |IFF_RUNNING))
            {
                ifr.ifr_addr.sa_family = AF_INET;
                if(ioctl(fd, SIOCGIFADDR, &ifr) == 0)
                    ip = htonl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);
            }
        }
        close(fd);
    }
    sprintf(ipStr, "%d.%d.%d.%d", ((ip & 0xFF000000) >> 24), ((ip & 0x00FF0000) >> 16),
                                        ((ip & 0x0000FF00) >> 8),(ip & 0x000000FF));

	debug_printf("About to send message to all targets. res=%s, iso_time=%s msg=%s", res, iso_time, msg);

	for (target = &syslogd_config->local.common; target; target = target->next)
	{
        target->msgnum++;

		if (!((1 << LOG_PRI(pri)) & target->level))
		{
			debug_printf("skipping message at pri %d when target level is %d", LOG_PRI(pri), target->level);
			continue;
		}
		debug_printf("Accepting message at pri %d when target level is %d", LOG_PRI(pri), target->level);

		if (first) {
			first = 0;
			debug_printf("Creating message:");
			debug_printf("res=%s", res);
			debug_printf("timestamp=%s", timestamp);
			debug_printf("msg=%s", msg);
			debug_printf("iso_time=%s", iso_time);
			debug_printf("content=%s", content);

#ifdef EMBED
            //snprintf(buf, sizeof(buf) - 1, "%s{%u}(%s) %s %s%s\n", res, target->msgnum,
            //    timestamp, (prefix_copy_len > 0) ? prefix_name_buf : msg,
            //    iso_time, content);
            //snprintf(buf, sizeof(buf) - 1, "%s %s %s \n", res, times, msg);
            /* Date IP %facility-sevirity: msg */
            snprintf(tmp, sizeof(tmp) - 1, "%s %s %s",times, ipStr, msg);
#else
            snprintf(buf, sizeof(buf) - 1, "%s %s %s {%u} %s\n",
                timestamp, syslogd_config->local_hostname, res, target->msgnum, msg);
#endif
#if 0
            tmpp = (strchr(msg, ':'));
            if(tmpp)
                *tmpp = ';';
#endif
			debug_printf("Created message: %s", buf);

		}
		switch (target->target) {
			case SYSLOG_TARGET_LOCAL:
				if (LOG_PRI(pri) <= LOG_ALERT)
					creat(SYS_LOG_USER_ALARM_FILE, 0666);

			case SYSLOG_TARGET_LOCAL_FLASH:
				memset(buf,0,1024);
				snprintf(buf, sizeof(buf) - 1, "%u;%s;%s\n", LOG_PRI(pri), times, msg);
				debug_printf("Logging to local ram target %s\n", buf);
				log_local_message((syslogd_local_config_t *)target, buf);
				break;

			case SYSLOG_TARGET_LOCAL_CONSOLE:
				memset(buf, 0, 1024);
				snprintf(buf, sizeof(buf) - 1, "*%s: %s\n", times, msg);
				log_local_message((syslogd_local_config_t *)target, buf);
				break;

#ifdef CONFIG_FEATURE_REMOTE_LOG
			case SYSLOG_TARGET_REMOTE:
                if(0 != strstr(tmp, SYS_LOG_JUNK_MSG))
                    break;

                c = tmp;
				c = strstr(c, "kernel:");

                if (NULL != c)
                {
                    c = c + 8;
                }
                else
                {
                    c = tmp;
    				c = strstr(tmp, "syslog:");

                    if (NULL != c)
                    {
                        c = c + 8;
                    }
                }

                if (NULL != c)
                {
                    if (*c == '`')
                    {
                        char *p = c;
                        c = strstr(c, "^");

                        if (NULL != c)
                        {
                            c = c + 1;
                        }
                        else
                        {
                            c = p;
                        }
                    }
                }
                else
                {
                    c = tmp;
                }

                pri &= 0x7;
                pri |= ((syslogd_remote_config_t *)target)->facility << 3;

                sprintf(remote, "<%d> %s\n", pri, c);
                
				debug_printf("Logging to remote target = %s\n", remote);
				log_remote_message((syslogd_remote_config_t *)target, remote);
				break;
#endif

#ifdef CONFIG_USER_SMTP_SMTPCLIENT
			case SYSLOG_TARGET_EMAIL:
				debug_printf("Logging to email target");
				log_email_message((syslogd_email_config_t *)target, buf);
				break;
#endif
			default:
				debug_printf("Skipping unknown target");
				break;
		}
	}
	debug_printf("done message");
}

int syslog_name_to_pri(const char *name)
{
	CODE *c_pri;

	for (c_pri = prioritynames; c_pri->c_name; c_pri++) {
		if (strcmp(c_pri->c_name, name) == 0) {
			return c_pri->c_val;
		}
	}

	return -1;
}

/* Format should include trailing newline */
void syslog_local_message(const char *format, ...)
{
	char buf[256];

	va_list args;
	va_start(args, format);

	debug_printf("syslog_local_message: format=%s, syslogd_config=%p, local=%p", format, syslogd_config, &syslogd_config->local);

	vsnprintf(buf, sizeof(buf) - 1, format, args);

	debug_printf("syslog_local_message: done vsnprintf()");

	log_local_message(&syslogd_config->local, buf);
	va_end(args);
}

void syslog_message(int pri, char *msg)
{
	struct timeval tv;
	char  timestamp[8];
    time_t epoch_time;
    char *tmp = NULL;

	/* Count messages repeats */
	//static int repeats = 0;
	//static int old_pri = 0;
	//static char old_msg[MAXLINE + 1];

/*
	gettimeofday(&tv,NULL);

	if (strlen(msg) < 16 || msg[3] != ' ' || msg[6] != ' ' ||
			msg[9] != ':' || msg[12] != ':' || msg[15] != ' ') {
		timestamp = ctime(&(tv.tv_sec)) + 4;
		timestamp[15] = '\0';
	} else {
		timestamp = msg;
		timestamp[15] = '\0';
		msg += 16;
	}
*/
    /* Our system only use user(1) and kernel(0) two types facility.
        Discard any syslog message from other service.*/
    if((pri >> 3) > 1)
        return;

	gettimeofday(&tv,NULL);
    time(&epoch_time);
    sprintf(timestamp, "%d", epoch_time);

#if 0
    /* remove the timestamp present in the msg but make sure it has 'kernel:'
     * or 'syslog:' tag
     */
    if (NULL != strstr(msg, "kernel:"))
    {
        msg = strstr(msg, ":"); /* "Jan  1 00:00:14 xxxxxxxxxx" */
        msg = strstr(msg + 1, ":"); /* "00:14 xxxxxxxxxx" */
        msg = msg + 4;
    }

    if (NULL != strstr(msg, "syslog:"))
    {
        msg = strstr(msg, ":"); /* "Jan  1 00:00:14 xxxxxxxxxx" */
        msg = strstr(msg + 1, ":"); /* "00:14 xxxxxxxxxx" */
        msg = msg + 4;
    }

    /* Now, is this a duplicate? */
	if (!syslogd_config->repeat && pri == old_pri && strcmp(msg, old_msg) == 0) {
		/* Yes, so remember it but don't log it */
		repeats++;
		return;
	} else {
		/* No */
		if (repeats) {
			/* Not a repeat, but we previously had repeats, so output a message */
			snprintf(old_msg, sizeof(old_msg), "last message repeated %d time(s)", repeats);
			logOneMessage(old_pri, old_msg, timestamp, &tv);
			repeats = 0;
		}

		/* Remember the previous message */
		old_pri = pri;
		strncpy(old_msg, msg, sizeof(old_msg));
		old_msg[MAXLINE] = 0;
	}
    
#else
    tmp = strstr(msg, "kernel:");
    if (NULL != tmp)
    {
        msg = tmp + strlen("kernel: ");
        pri = LOG_PRI(pri);
    }

    tmp = strstr(msg, "syslog:");
    if (NULL != tmp)
        msg = tmp + strlen("syslog: ");
#endif
    
	debug_printf("About to logOneMessage: pri=%d, msg=%s", pri, msg);

	/* Log this message */
	logOneMessage(pri, msg, timestamp, &tv);
}

static void quit_signal(int sig)
{
	syslog_message(LOG_SYSLOG | LOG_INFO, "syslogd: System log daemon exiting.");
	unlink(lfile);
#ifdef CONFIG_FEATURE_IPC_SYSLOG
	ipcsyslog_cleanup();
#endif

	exit(TRUE);
}

static void reload_signal(int sig)
{
	//syslog_message(LOG_SYSLOG | LOG_INFO, "syslogd: Reloading configuration...");
	debug_printf("syslogd: Reloading configuration...");

	reload_config = 1;

	signal(SIGHUP, reload_signal);
}

/* This must be a #define, since when CONFIG_DEBUG and BUFFERS_GO_IN_BSS are
 * enabled, we otherwise get a "storage size isn't constant error. */
static int serveConnection (char* tmpbuf, int n_read)
{
	int    pri_set = 0;
	char  *p = tmpbuf;

	/* SOCK_DGRAM messages do not have the terminating NUL,  add it */
	if (n_read > 0)
		tmpbuf[n_read] = '\0';

	while (p < tmpbuf + n_read) {

		int           pri = (LOG_USER | LOG_NOTICE);
		char          line[ MAXLINE + 1 ];
		char         *q = line;

		while (q < &line[ sizeof (line) - 1 ]) {
			if (!pri_set && *p == '<') {
			/* Parse the magic priority number. */
				pri = 0;
				p++;
				while (isdigit(*p)) {
					pri = 10 * pri + (*p - '0');
					p++;
				}
				if (pri & ~(LOG_FACMASK | LOG_PRIMASK)){
					pri = (LOG_USER | LOG_NOTICE);
				}
				pri_set = 1;
			} else if (*p == '\0') {
				pri_set = 0;
				*q = *p++;
				break;
			} else if (*p == '\n') {
				*q++ = ' ';
			} else if (iscntrl(*p) && (*p < 0177)) {
				*q++ = '^';
				*q++ = *p ^ 0100;
			} else {
				*q++ = *p;
			}
			p++;
		}
		*q = '\0';
		p++;
		/* Now log it */
		if (q > line)
			syslog_message (pri, line);
	}
	return n_read;
}

static void doSyslogd(int argc, char *argv[]) __attribute__ ((noreturn));
static void doSyslogd(int argc, char *argv[])
{
	struct sockaddr_un sunx;
	socklen_t addrLength;

	int sock_fd;
	fd_set fds;

	/* Set up signal handlers. */
	signal(SIGINT, quit_signal);
	signal(SIGTERM, quit_signal);
	signal(SIGQUIT, quit_signal);
	signal(SIGHUP, reload_signal);
	signal(SIGCHLD, SIG_IGN);
#ifdef SIGCLD
	signal(SIGCLD, SIG_IGN);
#endif

	debug_printf("init local");
	init_local_targets(syslogd_config);
#ifdef CONFIG_FEATURE_REMOTE_LOG
	debug_printf("init remote");
	init_remote_targets(syslogd_config);
#endif
#ifdef CONFIG_USER_SMTP_SMTPCLIENT
	debug_printf("init email");
	init_email_targets(syslogd_config);
#endif

	/* Create the syslog file so realpath() can work. */
	if (realpath(_PATH_LOG, lfile) != NULL) {
		unlink(lfile);
	}

	debug_printf("done realpath");

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	strncpy(sunx.sun_path, lfile, sizeof(sunx.sun_path));
	if ((sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		bb_perror_msg_and_die("Couldn't get file descriptor for socket "
						   _PATH_LOG);
	}

	debug_printf("created socket");

	addrLength = sizeof(sunx.sun_family) + strlen(sunx.sun_path);
	if (bind(sock_fd, (struct sockaddr *) &sunx, addrLength) < 0) {
		bb_perror_msg_and_die("Could not connect to socket " _PATH_LOG);
	}

	debug_printf("did bind");

	if (chmod(lfile, 0666) < 0) {
		bb_perror_msg_and_die("Could not set permission on " _PATH_LOG);
	}
#ifdef CONFIG_FEATURE_IPC_SYSLOG
	if (syslogd_config->local.circular_logging) {
		ipcsyslog_init();
		debug_printf("created circular log");
	}
#endif

	debug_printf("about to log startup message");

	//syslog_message(LOG_SYSLOG | LOG_INFO, "syslogd started: " BB_BANNER);

	for (;;) {
		debug_printf("wait for message");

		FD_ZERO(&fds);
		FD_SET(sock_fd, &fds);

		if (reload_config) {
			char *p;

			shutdown_local_targets(syslogd_config);
#ifdef CONFIG_FEATURE_REMOTE_LOG
			shutdown_remote_targets(syslogd_config);
#endif
#ifdef CONFIG_USER_SMTP_SMTPCLIENT
			shutdown_email_targets(syslogd_config);
#endif
			syslogd_discard_config(syslogd_config);
			load_config(syslogd_config, argc, argv);
			/*
			 * Get hostname again.
			 */
			gethostname(syslogd_config->local_hostname, sizeof(syslogd_config->local_hostname));
			if ((p = strchr(syslogd_config->local_hostname, '.'))) {
				*p = '\0';
			}

			reload_config = 0;
			//syslog_message(LOG_SYSLOG | LOG_INFO, "syslogd: configuration reloaded");
			debug_printf("syslogd: configuration reloaded");
		}

		if (select(sock_fd + 1, &fds, NULL, NULL, 0) < 0) {
			if (errno == EINTR) {
				/* alarm may have happened. */
				continue;
			}
			bb_perror_msg_and_die("select error");
		}

		if (FD_ISSET(sock_fd, &fds)) {
			int i;

			RESERVE_CONFIG_BUFFER(tmpbuf, MAXLINE + 1);

			memset(tmpbuf, '\0', MAXLINE + 1);
			if ((i = recv(sock_fd, tmpbuf, MAXLINE, 0)) > 0) {
				serveConnection(tmpbuf, i);
			} else {
				bb_perror_msg_and_die("UNIX socket error");
			}
			RELEASE_CONFIG_BUFFER(tmpbuf);
		}				/* FD_ISSET() */
	}					/* for main loop */
}

static int load_config(syslogd_config_t *config, int argc, char *argv[])
{
	int doFork = TRUE;
	int opt;

	debug_printf("Loading config from %s to initialise", DEFAULT_CONFIG_FILE);

	/* Load the default config file */
	syslogd_load_config(DEFAULT_CONFIG_FILE, config);

	debug_printf("Loaded from default config file, parsing args");

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "f:m:nO:s:b:R:LC::")) > 0) {

		debug_printf("opt=%c optarg=%s", opt, optarg);

		switch (opt) {
		case 'm':
			config->local.markinterval = atoi(optarg) * 60;
			break;
		case 'n':
			doFork = FALSE;
			break;
		case 'f':
			/* Note: All previous command line settings will be lost */
			debug_printf("loading config from %s", optarg);
			syslogd_discard_config(config);
			syslogd_load_config(optarg, config);
			break;
		case 'O':
			config->local.logfile = strdup(optarg);
			break;
#ifdef CONFIG_FEATURE_ROTATE_LOGFILE
		case 's':
			config->local.maxsize = atoi(optarg);
			break;
		case 'b':
			config->local.numfiles = atoi(optarg);
			if( config->local.numfiles > 99 ) config->local.numfiles = 99;
			break;
#endif
#ifdef CONFIG_FEATURE_REMOTE_LOG
		case 'R':
			{
				char *p;

				syslogd_remote_config_t *remote = malloc(sizeof(*remote));
				memset(remote, 0, sizeof(*remote));
				remote->common.target = SYSLOG_TARGET_REMOTE;
				remote->common.level = LOG_DEBUG;
				remote->port = 514;
				remote->common.next = config->local.common.next;
				config->local.common.next = &remote->common;

				remote->host = bb_xstrdup(optarg);
				if ((p = strchr(remote->host, ':'))) {
					remote->port = atoi(p + 1);
					*p = '\0';
				}
			}
			break;
#endif
#ifdef CONFIG_FEATURE_IPC_SYSLOG
		case 'C':
			if (optarg) {
				int buf_size = atoi(optarg);
				if (buf_size >= 4) {
					//shm_size = buf_size * 1024;
				}
			}
			syslogd_config->local.circular_logging = TRUE;
			break;
#endif
		default:
			bb_show_usage();
		}
	}

	return doFork;
}

static void update_tz(int signo);
static void update_tz(int signo)
{
    FILE *fp = NULL;
    fp = fopen("/tmp/timezone_diff", "r+");
    if(fp != NULL)
    {
        fscanf(fp, "%d", &timezone_diff);
        fclose(fp);
    }
    else
        timezone_diff = 0;
}

extern int syslogd_main(int argc, char **argv)
{
#ifdef CONFIG_SYS_SYSLOG
	int doFork;
	char *p;
	

	syslogd_config_t config;

	doFork = load_config(&config, argc, argv);

	/* And create a global to reference it */
	syslogd_config = &config;

	/* Store away localhost's name before the fork */
	gethostname(syslogd_config->local_hostname, sizeof(syslogd_config->local_hostname));
	if ((p = strchr(syslogd_config->local_hostname, '.'))) {
		*p = '\0';
	}

	umask(0);

	debug_printf("doFork=%d", doFork);

	if (doFork == TRUE) {
#if defined(__uClinux__)
		vfork_daemon_rexec(0, 1, argc, argv, "-n");
#else /* __uClinux__ */
		if(daemon(0, 1) < 0)
			bb_perror_msg_and_die("daemon");
#endif /* __uClinux__ */
	}

    signal(SIGUSR1, update_tz);
    update_tz(0);

	syslogd_write_pid(DEFAULT_PID_FILE);
	doSyslogd(argc, argv);
#endif
	return EXIT_SUCCESS;
}

#ifdef DEBUG_TO_FILE
void debug_printf(const char *format, ...)
{
	static FILE *fh;

	va_list args;
	va_start(args, format);

	if (!fh) {
		fh = fopen("/tmp/syslogd.out", "a");
		fprintf(fh, "--------------------------------\n");
	}
	vfprintf(fh, format, args);
	va_end(args);
	fputc('\n', fh);
	fflush(fh);
}
#endif

/*
Local Variables
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
