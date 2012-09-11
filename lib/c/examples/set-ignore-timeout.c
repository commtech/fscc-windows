#include <fscc.h>

int main(void)
{
	HANDLE h;
	
	fscc_connect(0, FALSE, &h);

	/*! [Enable ignore timeout] */
	fscc_enable_ignore_timeout(h);
	/*! [Enable ignore timeout] */

	/*! [Disable ignore timeout] */
	fscc_disable_ignore_timeout(h);
	/*! [Disable ignore timeout] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}