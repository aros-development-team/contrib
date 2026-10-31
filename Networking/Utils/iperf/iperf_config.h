/* iperf_config.h — static configuration for AROS build */

#ifndef IPERF_CONFIG_H
#define IPERF_CONFIG_H

/* AROS has clock_gettime(CLOCK_MONOTONIC) in posixc */
#define HAVE_CLOCK_GETTIME 1

/* AROS does not have clock_nanosleep */
/* #undef HAVE_CLOCK_NANOSLEEP */

/* CPU affinity: AROS has pthread_setaffinity_np(), not the calls below */
/* #undef HAVE_CPUSET_SETAFFINITY */
#define HAVE_CPU_AFFINITY 1
/* #undef HAVE_SCHED_SETAFFINITY */
/* #undef HAVE_SETPROCESSAFFINITYMASK */

/* AROS: thread affinity through pthread_setaffinity_np() */
#define HAVE_PTHREAD_SETAFFINITY_NP 1

/* AROS does not have fork(); provide a no-op daemon() stub */
#define HAVE_DAEMON 1

/* AROS does not have dlfcn.h */
/* #undef HAVE_DLFCN_H */

/* AROS does not support IP_MTU_DISCOVER/IP_DONTFRAG */
/* #undef HAVE_DONT_FRAGMENT */
/* #undef HAVE_IP_DONTFRAG */
/* #undef HAVE_IP_DONTFRAGMENT */
/* #undef HAVE_IP_MTU_DISCOVER */

/* AROS has <endian.h> but it only provides byte-order constants, not
   byte-swap functions (be64toh, htobe64, etc.). Do NOT define HAVE_ENDIAN_H
   so portable_endian.h falls through to its own implementations. */
/* #undef HAVE_ENDIAN_H */
/* #undef HAVE_SYS_ENDIAN_H */

/* AROS does not have IPv6 flow labels */
/* #undef HAVE_FLOWLABEL */

/* AROS does not have getline(); iperf.h provides an inline fallback */
#define HAVE_GETLINE 1

/* Standard headers */
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define STDC_HEADERS 1

/* AROS does not have MPTCP */
/* #undef HAVE_IPPROTO_MPTCP */

/* AROS does not have linux/tcp.h */
/* #undef HAVE_LINUX_TCP_H */

/* AROS does not have MSG_TRUNC */
/* #undef HAVE_MSG_TRUNC */

/* AROS has nanosleep in posixc */
#define HAVE_NANOSLEEP 1

/* AROS does not have SCTP */
/* #undef HAVE_NETINET_SCTP_H */
/* #undef HAVE_SCTP_H */
/* #undef HAVE_STRUCT_SCTP_ASSOC_VALUE */

/* AROS does not have poll.h */
/* #undef HAVE_POLL_H */

/* AROS has pthreads */
#define HAVE_PTHREAD 1
/* #undef HAVE_PTHREAD_PRIO_INHERIT */
#define PTHREAD_CREATE_JOINABLE 0

/* AROS does not have sendfile */
/* #undef HAVE_SENDFILE */

/* AROS may have SHUT_WR */
#define HAVE_SOCKET_SHUTDOWN_SHUT_WR 1

/* AROS does not have SO_BINDTODEVICE */
/* #undef HAVE_SO_BINDTODEVICE */

/* AROS does not have SO_MAX_PACING_RATE */
/* #undef HAVE_SO_MAX_PACING_RATE */

/* No OpenSSL on AROS */
/* #undef HAVE_SSL */

/* AROS has <stdatomic.h> */
/* #undef HAVE_STDATOMIC_H */

/* AROS does not have TCP_CONGESTION sockopt */
/* #undef HAVE_TCP_CONGESTION */

/* AROS does not have TCP_INFO snd_wnd field */
/* #undef HAVE_TCP_INFO_SND_WND */

/* AROS does not have TCP_KEEPIDLE */
/* #undef HAVE_TCP_KEEPALIVE */

/* AROS does not have TCP_USER_TIMEOUT */
/* #undef HAVE_TCP_USER_TIMEOUT */

/* AROS does not have UDP_GRO/UDP_SEGMENT */
/* #undef HAVE_UDP_GRO */
/* #undef HAVE_UDP_SEGMENT */

/* Package info */
#define PACKAGE "iperf"
#define PACKAGE_BUGREPORT "https://github.com/esnet/iperf"
#define PACKAGE_NAME "iperf"
#define PACKAGE_STRING "iperf 3.20+"
#define PACKAGE_TARNAME "iperf"
#define PACKAGE_URL "https://software.es.net/iperf/"
#define PACKAGE_VERSION "3.20+"
#define VERSION "3.20+"

#endif /* IPERF_CONFIG_H */
