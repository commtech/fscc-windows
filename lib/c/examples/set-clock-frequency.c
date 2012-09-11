#include <fscc.h>

int main(void)
{
	HANDLE h;
	
	fscc_connect(0, FALSE, &h);
	
	/*! [Set clock frequency] */
	/* Set the clock frequency to 1 Mhz */
	fscc_set_clock_frequency(h, 1000000, 2);
	/*! [Set clock frequency] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}