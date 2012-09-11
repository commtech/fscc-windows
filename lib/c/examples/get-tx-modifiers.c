#include <fscc.h>

int main(void)
{
	HANDLE h;
	/*! [Setup variables] */
	/* Declare our modifiers variable */
	unsigned modifiers;
	/*! [Setup variables] */
	
	fscc_connect(0, FALSE, &h);

	/*! [Get modifiers] */
    /* Get modifiers */
	fscc_get_tx_modifiers(h, &modifiers);
	/*! [Get modifiers] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}