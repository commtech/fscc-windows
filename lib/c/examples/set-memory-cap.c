#include <fscc.h>

int main(void)
{
	HANDLE h;
	/*! [Setup variables] */
	/* Declare our memory cap variable */
	struct fscc_memory_cap m;
	/*! [Setup variables] */
	
	fscc_connect(0, FALSE, &h);
	
	/*! [Set memory cap] */	
	/* Specifiy our desired values */
	m.input = 1000000; /* 1 MB */
	m.output = 2000000; /* 2 MB */
	
	/* Set the memory cap values */
	fscc_set_memory_cap(h, &m);
	/*! [Set memory cap] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}