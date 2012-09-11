#include <fscc.h>

int main(void)
{
	HANDLE h;
	/*! [Setup variables] */
	/* Declare our registers variable */
	struct fscc_registers r;
	/*! [Setup variables] */
	
	fscc_connect(0, FALSE, &h);
	
	/*! [Set registers] */
	/* Initialize our registers structure */
	FSCC_REGISTERS_INIT(r);
	
	/* Change the CCR0 and BGR elements to our desired values */
	r.CCR0 = 0x0011201c;
	r.BGR = 10;
	
	/* Set the CCR0 and BGR register values */
	fscc_set_registers(h, &r);
	/*! [Set registers] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}