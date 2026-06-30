// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023 KBEmbedded

#include <furi.h>
#include <furi_hal.h>
#include <stm32wbxx_ll_exti.h>
#include <stm32wbxx_ll_system.h>

#include <stdint.h>

struct exti_workaround {
	uint32_t* ivt_mirror;
	uint32_t ivt_mirror_offs;
	bool exti3_rise_enable;
	bool exti3_fall_enable;
	bool exti3_event_enable;
	const GpioPin *clk;
};

/* NOTE WELL! This function is absurdly hacky and a stupid workaround to a
 * stupid issue that doesn't really have any other solution in the current
 * Flipper/FURI API. I'm over-commenting this so we know exactly what is going
 * on if we ever have to re-visit this mess.
 *
 * This block of text below describes the overall idea, more specific comments
 * in the function body.
 *
 * TODO: make this more generic for any other GPIOs that might conflict with
 * exti interrupts. PA6, PB3, PC3, PB2? (NFC), PA13, PB6
 * NOTE: This is only set up at the moment for PB3, hardcoded
 *
 * There are multiple problems that this workaround is handling. EXTI interrupts
 * are shared among multiple pins. The FURI core maintains per-pin ISRs in a
 * private struct that has no way to read, save, or otherwise be able to put
 * back the ISR that would service a conflicting EXTI. e.g. PB3 and PH3
 * (the OK button) both share EXTI3. Setting an interrupt on PB3 will clobber
 * the FURI ISR callback/context pair as well as change EXTI3 to use PB3 as
 * the interrupt source.
 *
 * To make an interrupt work correctly on PB3 and not break the OK button
 * we need a way to set an interrupt for PB3 in a way that doesn't clobber the
 * private FURI GPIO ISR handles and can let the interrupt for the OK button
 * work again when we're done.
 *
 * The general concept of this workaround is to modify the IVT to create our
 * own handler for EXTI3 interrupts. Doing this leaves the aforementioned private
 * GPIO struct unmodified and disables the OK button from triggering an interrupt.
 * The IVT is normally located at the lowest addresses of flash (which is located
 * at 0x08000000 and mapped at runtime to 0x00000000); this means the IVT cannot
 * be changed at runtime.
 *
 * To make this work, we use the Vector Table Offset Register (VTOR) in the
 * System Control Block (SCB). The VTOR allows for changing the location of the
 * IVT. We copy the IVT to a location in memory, and then do a dance to safely
 * set up the GPIO interrupt to PB3, and swap in our IVT with the modified EXTI3
 * handler.
 *
 * When undoing this, the process is not quite in reverse as we have to put back
 * specific interrupt settings that we very likely would have clobbered but have
 * the ability to save beforehand.
 *
 * Wrapping the steps in disabling the EXTI3 interrupt is probably not needed,
 * but is a precaution since we are changing the interrupt sources in weird ways.
 */
/* Used to map our callback context in a way the handler can access */
static void *exti3_cb_context;
static void (*callback)(void *context);
static void gblink_exti3_IRQHandler(void) {
	if(LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_3)) {
		callback(exti3_cb_context);
		LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_3);
	}
}

void *exti_workaround(const GpioPin *clk, void (*isr_callback)(void *context), void *context)
{
	struct exti_workaround *work = NULL;

	/* This process makes a number of assumptions, including that the IVT
	 * is located at 0x00000000, that the lowest flash page is mapped to
	 * that base address, and that the VTOR points to 0x00000000.
	 * There are runtime protections in place to prevent reading from the
	 * first 1 MB of addresses. So we have to always assume that the lowest
	 * page of flash is mapped to 0x00000000 and read the IVT from the that
	 * page in flash directly.
	 * The only check we can really do here is ensuring VTOR is 0 and that
	 * Main memory is mapped to 0x00000000. If either of those are not true,
	 * then we can't continue.
	 */
	furi_check(SCB->VTOR == 0x0);
	furi_check(LL_SYSCFG_GetRemapMemory() == LL_SYSCFG_REMAP_FLASH);

	/* Create a mirror of the existing IVT from CPU 1
	 * The IVT on this platform has 79 entries; 63 maskable, 10 non-maskable,
	 * 6 reserved. The maskable interrupts start at offset 16.
	 * CMSIS documentation says that the boundary for IVT must be aligned to
	 * the number of interrupts, rounded up to the nearest power of two, and
	 * then multiplied by the word width of the CPU. 79 rounds up to 128
	 * with a word width of 4, this is 512/0x200 bytes.
	 * As there is no good way with FreeRTOS to request an alloc at an
	 * aligned boundary, allocate the amount of data we need, plus 0x200
	 * bytes, to guarantee that we can put the table in a location that is
	 * properly aligned. Once we find a suitable base address, this offset
	 * is saved for later.
	 */
	work = malloc(sizeof(struct exti_workaround));
	work->ivt_mirror = malloc((79 * sizeof(uint32_t)) + 0x200);
	work->ivt_mirror_offs = (uint32_t)work->ivt_mirror;
	while (work->ivt_mirror_offs & 0x1FF)
		work->ivt_mirror_offs++;
	/* 0x08000000 is used instead of 0x00000000 because everything complains
	 * using a NULL pointer.
	 */
	memcpy((uint32_t *)work->ivt_mirror_offs, ((uint32_t *)0x08000000), 79 * sizeof(uint32_t));

	/* Point our IVT's EXTI3 interrupt to our desired interrupt handler.
	 * Also copy the gblink struct to the global var that the interrupt
	 * handler will use to make further calls.
	 */
	((uint32_t *)work->ivt_mirror_offs)[25] = (uint32_t)gblink_exti3_IRQHandler; // 16 NMI + offset of 9 for EXTI3
	callback = isr_callback;
	exti3_cb_context = context;

	/* Disable the EXTI3 interrupt. This lets us do bad things without
	 * fear of an IRQ hitting in the middle.
	 */
	LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_3);

	/* Save the existing rise/fall trigger settings. In theory, these should
	 * really never change through the life of the flipper OS. But for safety
	 * we always save them rather than just blindly restoring the same settings
	 * back when we undo this later.
	 *
	 * Note that these are clobbered by calls to furi_hal_gpio_init() which
	 * can and should happen after this workaround function exits. They can
	 * be set and unset without worry of clobbering the FURI private IRQ info.
	 */
	work->exti3_rise_enable = LL_EXTI_IsEnabledRisingTrig_0_31(LL_EXTI_LINE_3);
	work->exti3_fall_enable = LL_EXTI_IsEnabledFallingTrig_0_31(LL_EXTI_LINE_3);
	work->exti3_event_enable = LL_EXTI_IsEnabledEvent_0_31(LL_EXTI_LINE_3);
	work->clk = clk;

	/* Update the NVIC table to point at our desired table.
	 * Out of safety, stop the world around changing the VTOR reg.
	 */
	FURI_CRITICAL_ENTER();
	SCB->VTOR = work->ivt_mirror_offs;
	FURI_CRITICAL_EXIT();

	/* Last, enable the interrupts and hope everything works. */
	LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_3);

	return work;
}

void exti_workaround_undo(void *handle)
{
	struct exti_workaround *work = handle;
	/* First, disable the EXTI3 interrupt. This lets us do bad things without
	 * fear of an IRQ hitting in the middle.
	 */
	LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_3);

	/* Set the correct input source, PH3/OK button, to EXTI3. It is important
	 * to do this before calling furi_hal_gpio_init() on PB3. When that func
	 * is called with no interrupt settings enabled, if the EXTI source
	 * matches the pin, and the interrupt is enabled, interrupts will be
	 * disabled. By manually setting the EXTI3 source here, it no longer
	 * matches the PB3 pin, and our changing of IO settings on our GPIO pin
	 * to no longer have interrupts will not affect the shared IRQ.
	 */
	LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTH, LL_SYSCFG_EXTI_LINE3);

	/* Set the correct rise/fall/event settings back */
	if (work->exti3_rise_enable)
		LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_3);
	else
		LL_EXTI_DisableRisingTrig_0_31(LL_EXTI_LINE_3);

	if (work->exti3_fall_enable)
		LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_3);
	else
		LL_EXTI_DisableFallingTrig_0_31(LL_EXTI_LINE_3);

	if (work->exti3_event_enable)
		LL_EXTI_EnableEvent_0_31(LL_EXTI_LINE_3);
	else
		LL_EXTI_DisableEvent_0_31(LL_EXTI_LINE_3);


	/* Set the IVT back to the normal, in-flash table. Stopping the world
	 * while we do so.
	 * NOTE: This just assumes the VTOR is always at 0x0 by default, if this
	 * ever changes in the Flipper OS, then that will be a problem.
	 */
	FURI_CRITICAL_ENTER();
	SCB->VTOR = 0x0;
	FURI_CRITICAL_EXIT();

	/* Re-enable the interrupt, OK button should work again. */
	LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_3);

	/* Free the alloc()ed mirror space */
	free(work->ivt_mirror);
	free(work);
}
