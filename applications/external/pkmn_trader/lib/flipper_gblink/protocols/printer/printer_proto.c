// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <furi.h>

#include <gblink/include/gblink.h>
#include <gblink/include/gblink_pinconf.h>
#include <protocols/printer/include/printer_proto.h>
#include "printer_i.h"

void *printer_alloc(void)
{
	struct printer_proto *printer = NULL;

	printer = malloc(sizeof(struct printer_proto));

	printer->packet = malloc(sizeof(struct packet));
	printer->image = malloc(sizeof(struct gb_image));

	printer->gblink_handle = gblink_alloc();

	/* Set up some settings for the print protocol. The final send/receive() calls
	 * may clobber some of these, but that is intentional and they don't need to
	 * care about some of the other details that are specified here.
	 */
	/* Reported 1.49 ms timeout between bytes, need confirmation */
	gblink_timeout_set(printer->gblink_handle, 1490);
	gblink_nobyte_set(printer->gblink_handle, 0x00);

	return printer;
}

/* TODO: Allow free() without stop, add a way to check if printer_stop has not
 * yet been called.
 */
void printer_free(void *printer_handle)
{
	struct printer_proto *printer = printer_handle;

	gblink_free(printer->gblink_handle);
	free(printer->packet);
	free(printer->image);
	free(printer);
}

void printer_callback_context_set(void *printer_handle, void *context)
{
	struct printer_proto *printer = printer_handle;

	printer->cb_context = context;
}

void printer_callback_set(void *printer_handle, void (*callback)(void *context, struct gb_image *image, enum cb_reason reason))
{
	struct printer_proto *printer = printer_handle;

	printer->callback = callback;
}

void *printer_gblink_handle_get(void *printer_handle)
{
	struct printer_proto *printer = printer_handle;

	return printer->gblink_handle;
}

void printer_stop(void *printer_handle)
{
	struct printer_proto *printer = printer_handle;

	gblink_stop(printer->gblink_handle);
	/* TODO: Call the callback one last time with a flag to indicate that the transfer has completely
	 * ended.
	 * Receive/send should also have a separate timeout, doesn't need to call stop, but, will
	 * also retrigger the complete callback. This allows for both the actual process to signal
	 * there was a gap (I think the gameboy print normally has a "I'm done" marker as well),
	 * and then the actual application that started the send/receive, can catch a back or other
	 * nav event, call stop itself, which will then call the callback again with a "we're done here"
	 * message as well.
	 */
	 
	/* TODO: Figure out what mode we're in, and run stop. Though, it might
	 * not be necessary to actually to know the mode. We should be able to
	 * just stop?
	 */
}
