#include <sys/prctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/audit.h>
#include <linux/seccomp.h>
#include <linux/limits.h>
#include <linux/filter.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <stddef.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <attr/xattr.h>

#include "seccomp_fd_notify.h"

#define PRINT_WARNING(...)  { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
#define PRINT_INFO(...)  { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }

static int send_fds(int sock, int *fds_to_send, int num_fds)
{
	struct msghdr msg = {0};
	struct cmsghdr *cmsg = NULL;
	memset(&msg, 0, sizeof(msg));

	size_t cmsgbufsize = CMSG_SPACE(num_fds * sizeof(int));
	char *cmsgbuf = NULL;
	cmsgbuf = malloc(cmsgbufsize);

	struct iovec iov;
	char buf[1] = {0};
	memset(&iov, 0, sizeof(iov));
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = cmsgbufsize;

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(num_fds * sizeof(int));

	memcpy(CMSG_DATA(cmsg), fds_to_send, num_fds * sizeof(int));

	msg.msg_controllen = cmsg->cmsg_len;

	int sendmsg_return = sendmsg(sock, &msg, 0);
	if (sendmsg_return < 0) {
		PRINT_WARNING("sendmsg failed with return %d", sendmsg_return);
		return -1;
	}
	return 0;
}

static int seccomp(unsigned int operation, unsigned int flags, void *args)
{
	return syscall(__NR_seccomp, operation, flags, args);
}


/* For the x32 ABI, all system call numbers have bit 30 set */
#define X32_SYSCALL_BIT         0x40000000

/* install_notify_filter() install_notify_filter a seccomp filter that generates user-space
   notifications (SECCOMP_RET_USER_NOTIF) when the process calls mkdir(2); the
   filter allows all other system calls.

   The function return value is a file descriptor from which the user-space
   notifications can be fetched. */
static int install_notify_filter(void) {
	struct sock_filter filter[] = {
		/* X86_64_CHECK_ARCH_AND_LOAD_SYSCALL_NR */
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, arch))),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 0, 2),
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(struct seccomp_data, nr))),
		BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, X32_SYSCALL_BIT, 0, 1),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

		/* Trap perf-related syscalls */
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_bpf, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_USER_NOTIF),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_perf_event_open, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_USER_NOTIF),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_ioctl, 0, 1),
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_USER_NOTIF),

		/* Every other system call is allowed */
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
	};

	struct sock_fprog prog = {
		.len = (unsigned short) (sizeof(filter) / sizeof(filter[0])),
		.filter = filter,
	};

	/* Install the filter with the SECCOMP_FILTER_FLAG_NEW_LISTENER flag; as
	   a result, seccomp() returns a notification file descriptor. */

	/* Only one listening file descriptor can be established. An attempt to
	   establish a second listener yields an EBUSY error. */

	int notify_fd = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_NEW_LISTENER, &prog);
	if (notify_fd == -1) {
		PRINT_WARNING("seccomp install_notify_filter failed: %s", strerror(errno));
		return -1;
	}
	return notify_fd;
}

int get_mem_fd(int pid) {
	char mem_path[PATH_MAX];
	snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", pid);
	int mem_fd = open(mem_path, O_RDWR | O_CLOEXEC);
	return mem_fd;
}

int get_pid_fd(int pid) {
	char pid_path[PATH_MAX];
	snprintf(pid_path, sizeof(pid_path), "/proc/%d", pid);
	int pid_fd = open(pid_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	return pid_fd;
}

void maybe_setup_seccomp_notifer() {
	char *socket_path;
	socket_path = getenv(TITUS_SECCOMP_NOTIFY_SOCK_PATH);
	if (socket_path) {

		int sock_fd = -1;
		sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock_fd == -1) {
			PRINT_WARNING("Unable to open unix socket for seccomp handoff: %s", strerror(errno));
			return;
		}

		struct sockaddr_un addr = {0};
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
		if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			PRINT_WARNING("Unable to connect on unix socket (%s) for seccomp handoff: %s", socket_path, strerror(errno));
			return;
		}

		if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
			PRINT_WARNING("Couldn't prctl to no new privs: %s", strerror(errno));
			return;
		}

		int pid = getpid();
		int notify_fd, pid_fd, mem_fd = -1;
		notify_fd = install_notify_filter();
		pid_fd = get_pid_fd(pid);
		mem_fd = get_mem_fd(pid);

		int send_fd_list[3];
		send_fd_list[0] = pid_fd;
		send_fd_list[1] = mem_fd;
		send_fd_list[2] = notify_fd;

		if (send_fds(sock_fd, send_fd_list, 3) == -1) {
			PRINT_WARNING("Couldn't send fds to the socket at %s: %s", socket_path, strerror(errno));
			return;
		}
	}
	return;
}
