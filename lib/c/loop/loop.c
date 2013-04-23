#include <conio.h>
#include <stdio.h>

#include <Windows.h>

#include <fscc.h>

#define DATA_LENGTH 20

int init(HANDLE h);
int loop(HANDLE h1, HANDLE h2);

int main(int argc, char *argv[])
{
	HANDLE h1, h2;
	int e = 0;
	unsigned port_num_1, port_num_2;
	BOOL reset;
	unsigned iterations = 0;
	unsigned mismatched = 0;

	if (argc < 3 || argc > 4) {
		fprintf(stdout, "%s PORT_NUM PORT_NUM [RESET_REGISTER=1]", argv[0]);
		return EXIT_FAILURE;
	}

	port_num_1 = atoi(argv[1]);
	port_num_2 = atoi(argv[2]);
	reset = (argc == 4) ? atoi(argv[3]) : 1;

	e = fscc_connect(port_num_1, TRUE, &h1);
	if (e != 0) {
		fprintf(stderr, "fscc_connect failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = fscc_connect(port_num_2, TRUE, &h2);
	if (e != 0) {
		fprintf(stderr, "fscc_connect failed with %d\n", e);
		fscc_disconnect(h2);
		return EXIT_FAILURE;
	}

	if (reset) {
		e = init(h1);
		if (e != 0) {
			fscc_disconnect(h1);
			fscc_disconnect(h2);
			return EXIT_FAILURE;
		}
		
		e = init(h2);
		if (e != 0) {
			fscc_disconnect(h1);
			fscc_disconnect(h2);
			return EXIT_FAILURE;
		}
	}

	fprintf(stdout, "Data looping, press any key to stop...\n");

	while (_kbhit() == 0) {
		e = loop(h1, h2);
		if (e != 0) {
			if (e == ERROR_INVALID_DATA) {
				mismatched++;
			}
			else {
				fscc_disconnect(h1);
				fscc_disconnect(h2);
				return EXIT_FAILURE;
			}
		}

		iterations++;
	}

	if (mismatched == 0)
		fprintf(stdout, "Passed (%d iterations).", iterations);
	else
		fprintf(stderr, "Failed (%d out of %d iterations).", 
		        mismatched, iterations);

	fscc_disconnect(h1);
	fscc_disconnect(h2);

	return EXIT_SUCCESS;
}

int init(HANDLE h)
{
	struct fscc_registers r;
	int e = 0;

	fprintf(stdout, "Restoring to default settings.\n");

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

	e = fscc_set_clock_frequency(h, 1000000, 2);
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

int loop(HANDLE h1, HANDLE h2)
{
	unsigned bytes_written = 0, bytes_read = 0;
	char odata[DATA_LENGTH];
	char idata[100];
	int e = 0;
	OVERLAPPED o;

	memset(&o, 0, sizeof(o));
	memset(odata, 0x01, sizeof(odata));
	memset(&idata, 0, sizeof(idata));

	e = fscc_write(h1, odata, sizeof(odata), &bytes_written, &o);
	if (e != 0 && e != ERROR_IO_PENDING) {
		fprintf(stderr, "fscc_write failed with %d\n", e);
		return EXIT_FAILURE;
	}

	e = GetOverlappedResult(h1, &o, (DWORD*)&bytes_written, TRUE);
	if (e == FALSE)
		return EXIT_FAILURE;

	e = fscc_read_with_timeout(h2, idata, sizeof(idata), &bytes_read, 1000);
	if (e != 0) {
		fprintf(stderr, "fscc_read_with_timeout failed with %d\n", e);
		return EXIT_FAILURE;
	}
	
	if (bytes_read == 0 || memcmp(odata, idata, sizeof(odata)) != 0)
		return ERROR_INVALID_DATA;

	return ERROR_SUCCESS;
}