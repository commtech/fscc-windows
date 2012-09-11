#include <fscc.h>

int main(void)
{
	HANDLE h;
	
	fscc_connect(0, FALSE, &h);

	/*! [Enable append status] */
	fscc_enable_append_status(h);
	/*! [Enable append status] */

	/*! [Disable append status] */
	fscc_disable_append_status(h);
	/*! [Disable append status] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}