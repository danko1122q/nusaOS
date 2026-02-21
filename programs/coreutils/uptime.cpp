/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2024 Byteduck */


#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <libnusa/Args.h>

bool realtime = false;

// Baca uptime dalam detik dari /proc/uptime
// Format isi file: "<seconds>\n"
static long read_uptime_seconds() {
	FILE* f = fopen("/proc/uptime", "r");
	if (!f) {
		perror("uptime: cannot open /proc/uptime");
		return -1;
	}
	long seconds = -1;
	fscanf(f, "%ld", &seconds);
	fclose(f);
	return seconds;
}

// Format detik ke "X days, HH:MM:SS" atau "HH:MM:SS"
static void print_uptime(long seconds) {
	if (seconds < 0) {
		printf("up unknown\n");
		return;
	}

	long days    = seconds / 86400;
	long hours   = (seconds % 86400) / 3600;
	long minutes = (seconds % 3600) / 60;
	long secs    = seconds % 60;

	printf("up ");
	if (days > 0)
		printf("%ld day%s, ", days, days == 1 ? "" : "s");
	printf("%.2ld:%.2ld:%.2ld\n", hours, minutes, secs);
}

int main(int argc, char** argv) {
	Duck::Args args;
	args.add_flag(realtime, "r", "realtime", "Refresh uptime every second until Ctrl+C.");
	args.parse(argc, argv);

	if (!realtime) {
		long secs = read_uptime_seconds();
		print_uptime(secs);
		return secs < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	// Mode realtime: refresh setiap detik pakai \r overwrite baris
	printf("Press Ctrl+C to exit.\n");
	while (true) {
		long secs = read_uptime_seconds();
		printf("\r  ");
		print_uptime(secs);
		fflush(stdout);
		sleep(1);
	}

	return EXIT_SUCCESS;
}