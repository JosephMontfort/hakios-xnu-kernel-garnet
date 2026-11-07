/*
 * sleepd_stub: minimal daemon to satisfy launchd Mach service lookups for
 * com.apple.sleepd.sleepserver and related services.
 *
 * Prevents SpringBoard and widget clients from pinning CPU cores in an
 * aggressive reconnection loop when the full sleep daemon is absent.
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
		printf("sleepd_stub: checked in for %s (port %u)\n", name, port);
	} else {
		printf("sleepd_stub: check_in for %s returned %d\n", name, kr);
	}
}

int
main(int argc, char **argv)
{
	printf("sleepd_stub: started as pid %d\n", getpid());

	register_service("com.apple.sleepd.sleepserver");
	register_service("com.apple.sleepd");
	register_service("com.apple.sleepd.activitymanager");
	register_service("com.apple.navigationService");

	dispatch_main();
	return 0;
}
