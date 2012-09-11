#include <fscc.h>

int main(void)
{
	HANDLE h;
	
	fscc_connect(0, FALSE, &h);

	/*! [Set TXT | XREP] */
    /* Transmit repeat & transmit on timer */
	fscc_set_tx_modifiers(h, TXT | XREP);
	/*! [Set TXT | XREP] */
	
	/*! [Set XF] */
	/* Disable modifiers */
	fscc_set_tx_modifiers(h, XF);
	/*! [Set XF] */
	
	fscc_disconnect(h);
	
	return EXIT_SUCCESS;
}