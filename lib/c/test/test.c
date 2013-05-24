#include <conio.h>
#include <stdio.h>

#include <Windows.h>

#include <fscc.h>

#define DATA_LENGTH 20
#define NUM_ITERATIONS 100

int init(HANDLE h);
int loop(HANDLE h);

int main(int argc, char *argv[])
{
	HANDLE h;
	int e = 0;
	unsigned port_num;
	unsigned i = 0;

	if (argc != 2) {
		fprintf(stdout, "%s PORT_NUM", argv[0]);
		return EXIT_FAILURE;
	}

	port_num = atoi(argv[1]);

	e = fscc_connect(port_num, TRUE, &h);
	if (e != 0) {
		fprintf(stderr, "fscc_connect failed with %d\n", e);
		return EXIT_FAILURE;
	}

	fprintf(stdout, "This is a very simple test to verify your card is\n");
	fprintf(stdout, "communicating correctly.\n\n");

	fprintf(stdout, "NOTE: This will change your registers to defaults.\n\n");

	fprintf(stdout, "1) Connect your included loopback plug.\n");
	fprintf(stdout, "2) Press any key to start the test.\n\n");


	_getch();

	e = init(h);
	if (e != 0) {
		fscc_disconnect(h);
		return EXIT_FAILURE;
	}

	for (i = 0; i < NUM_ITERATIONS; i++) {
		e = loop(h);
		if (e != 0) {
			if (e == ERROR_INVALID_DATA) {
				break;
			}
			else {
				fscc_disconnect(h);
				return EXIT_FAILURE;
			}
		}
	}

	if (e != ERROR_INVALID_DATA)
		fprintf(stdout, "Passed, you can begin development.");
	else
		fprintf(stderr, "Failed, contact technical support.");

	fscc_disconnect(h);

	return EXIT_SUCCESS;
}

int init(HANDLE h)
{
	struct fscc_registers r;
	struct fscc_memory_cap m;
	int e = 0;

	m.input = 1000000;
	m.output = 1000000;

	e = fscc_set_memory_cap(h, &m);
	if (e != 0) {
		fprintf(stderr, "fscc_disable_append_status failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = fscc_disable_ignore_timeout(h);
	if (e != 0) {
		fprintf(stderr, "fscc_disable_append_status failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = fscc_disable_append_status(h);
	if (e != 0) {
		fprintf(stderr, "fscc_disable_append_status failed with %d\n", e);
		return EXIT_FAILURE;
	}
	
	e = fscc_set_tx_modifiers(h, XF);
	if (e != 0) {
		fprintf(stderr, "fscc_set_tx_modifiers failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = fscc_disable_ignore_timeout(h);
	if (e != 0) {
		fprintf(stderr, "fscc_disable_ignore_timeout failed with %d\n", e);
		return EXIT_FAILURE;
	}

	FSCC_REGISTERS_INIT(r);

	r.FIFOT = 0x08001000;
	r.CCR0 = 0x0011201c;
	r.CCR1 = 0x00000018;
	r.CCR2 = 0x00000000;
	r.BGR = 0x00000000;
	r.SSR = 0x0000007e;
	r.SMR = 0x00000000;
	r.TSR = 0x0000007e;
	r.TMR = 0x00000000;
	r.RAR = 0x00000000;
	r.RAMR = 0x00000000;
	r.PPR = 0x00000000;
	r.TCR = 0x00000000;
	r.IMR = 0x0f000000;
	r.DPLLR = 0x00000004;
	r.FCR = 0x00000000;

	e = fscc_set_registers(h, &r);
	if (e != 0) {
		fprintf(stderr, "fscc_set_registers failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = fscc_set_clock_frequency(h, 18432000, 2);
	if (e != 0) {
		fprintf(stderr, "fscc_set_clock_frequency failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = fscc_purge(h, TRUE, TRUE);
	if (e != 0) {
		fprintf(stderr, "fscc_purge failed with %d\n", e);
		return EXIT_FAILURE;
	}

	return ERROR_SUCCESS;
}

int loop(HANDLE h)
{
	unsigned bytes_written = 0, bytes_read = 0;
	char odata[DATA_LENGTH];
	char idata[100];
	int e = 0;
	OVERLAPPED o;

	memset(&o, 0, sizeof(o));
	memset(odata, 0x01, sizeof(odata));
	memset(&idata, 0, sizeof(idata));

	e = fscc_write(h, odata, sizeof(odata), &bytes_written, &o);
	if (e != 0 && e != ERROR_IO_PENDING) {
		fprintf(stderr, "fscc_write failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = GetOverlappedResult(h, &o, (DWORD*)&bytes_written, TRUE);
	if (e == FALSE)
		return EXIT_FAILURE;

	e = fscc_read_with_timeout(h, idata, sizeof(idata), &bytes_read, 1000);
	if (e != 0) {
		fprintf(stderr, "fscc_read_with_timeout failed with %d\n", e);
		return EXIT_FAILURE;
	}
	
	if (bytes_read == 0 || memcmp(odata, idata, sizeof(odata)) != 0)
		return ERROR_INVALID_DATA;

	return ERROR_SUCCESS;
}