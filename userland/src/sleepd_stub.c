/*
 * sleepd_stub: minimal daemon to satisfy launchd Mach service lookups for
 * com.apple.sleepd.sleepserver and related services.
 *
 * Prevents SpringBoard and widget clients from pinning CPU cores in an
 * aggressive reconnection loop when the full sleep daemon is absent.
 *
 * Run by launchd (com.redwood.svcstub.plist) with the services to hold as
 * arguments, each also in the plist's MachServices. garnet's boot log of
 * 13 November: the Maps widget looked up com.apple.navigationService and
 * com.apple.geod some 35,000 times in two bursts (launchd's "failed lookup",
 * error 3), the Sleep widget com.apple.sleepd.sleepserver. A port that exists
 * but never answers leaves the client waiting for its reply instead of
 * retrying at once. Messages are never received: the queue is made as long
 * as a port's can be so a sender is not stopped by a full one.
 */
#include <mach/mach.h>
#include <stdio.h>
#include <unistd.h>
#include <dispatch/dispatch.h>

extern mach_port_t bootstrap_port;
extern kern_return_t bootstrap_check_in(mach_port_t bp, const char *service_name, mach_port_t *sp);

static void
register_service(const char *name)
{
	mach_port_t port = MACH_PORT_NULL;
	kern_return_t kr = bootstrap_check_in(bootstrap_port, name, &port);
	if (kr == KERN_SUCCESS) {
		mach_port_limits_t limits = { MACH_PORT_QLIMIT_MAX };
		mach_port_set_attributes(mach_task_self(), port, MACH_PORT_LIMITS_INFO,
		    (mach_port_info_t)&limits, MACH_PORT_LIMITS_INFO_COUNT);
		printf("sleepd_stub: checked in for %s (port %u)\n", name, port);
	} else {
		printf("sleepd_stub: check_in for %s returned %d\n", name, kr);
	}
}

int
main(int argc, char **argv)
{
	printf("sleepd_stub: started as pid %d\n", getpid());

	if (argc > 1) {
		for (int i = 1; i < argc; i++)
			register_service(argv[i]);
	} else {
		register_service("com.apple.sleepd.sleepserver");
		register_service("com.apple.sleepd");
		register_service("com.apple.sleepd.activitymanager");
		register_service("com.apple.navigationService");
	}

	dispatch_main();
	return 0;
}
