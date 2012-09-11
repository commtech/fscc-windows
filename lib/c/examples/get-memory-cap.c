#include <fscc.h>

int main(void)
{
	HANDLE h;
	/*! [Setup variables] */
	/* Declare our memory cap variable */
	struct fscc_memory_cap m;
	/*! [Setup variables] */
	
	fscc_connect(0, FALSE, &h);
	
	/*! [Get memory cap] */	
	/* Set the memory cap values */
	fscc_get_memory_cap(h, &m);
	/*! [Get memory cap] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}