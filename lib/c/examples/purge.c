#include <fscc.h>

int main(void)
{
	HANDLE h;
	
	fscc_connect(0, FALSE, &h);

	/*! [Purge TX] */
	/* Purge TX */
	fscc_purge(TRUE, FALSE);
	/*! [Purge TX] */

	/*! [Purge RX] */
	/* Purge RX */
	fscc_purge(FALSE, TRUE);
	/*! [Purge RX] */

	/*! [Purge both TX & RX] */
	/* Purge both TX & RX */
	fscc_purge(TRUE, TRUE);
	/*! [Purge both TX & RX] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}