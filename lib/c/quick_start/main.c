#include <stdio.h>

#include <fscc.h>

int main(void)
{
	HANDLE h;
	int e = 0;
	char odata[] = "Hello world!";
	char idata[20] = {0};
	unsigned tmp;

	/* Open FSCC0 in a blockin IO mode */
	e = fscc_connect(3, FALSE, &h);
	if (e != 0) {
		fprintf(stderr, "fscc_connect failed with %d\n", e);
		return EXIT_FAILURE;
	}

	/* Send our "Hello world!" text */
	fscc_write(h, odata, sizeof(odata), &tmp, NULL);

	/* Read the data back in (with our loopback connector) */
	fscc_read(h, idata, sizeof(idata), &tmp, NULL);

	fprintf(stdout, idata);

	fscc_disconnect(h);

	return EXIT_SUCCESS;
}