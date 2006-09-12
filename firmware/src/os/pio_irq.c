#include <errno.h>
#include <sys/types.h>
#include <lib_AT91SAM7.h>
#include <os/pio_irq.h>
#include <os/dbgu.h>
#include <os/req_ctx.h>
#include <openpcd.h>

struct pioirq_state {
	irq_handler_t *handlers[NR_PIO];
	u_int32_t usbmask;
	u_int32_t usb_throttled; /* atomic? */
};

static struct pioirq_state pirqs;

static void pio_irq_demux(void)
{
	u_int32_t pio = AT91F_PIO_GetInterruptStatus(AT91C_BASE_PIOA);
	u_int8_t send_usb = 0;
	int i;

	DEBUGPCRF("PIO_ISR_STATUS = 0x%08x", pio);

	for (i = 0; i < NR_PIO; i++) {
		if (pio & (1 << i) && pirqs.handlers[i])
			pirqs.handlers[i](i);
		if (pirqs.usbmask & (1 << i))
			send_usb = 1;
	}

	if (send_usb && !pirqs.usb_throttled) {
		struct req_ctx *irq_rctx;
		irq_rctx = req_ctx_find_get(RCTX_STATE_FREE,
					    RCTX_STATE_PIOIRQ_BUSY);
		if (!irq_rctx) {
			/* we cannot disable the interrupt, since we have
			 * non-usb listeners */
			pirqs.usb_throttled = 1;
		} else {
			struct openpcd_hdr *opcdh;
			u_int32_t *regmask;
			opcdh = (struct openpcd_hdr *) &irq_rctx->tx.data[0];
			regmask = (u_int32_t *) (&irq_rctx->tx.data[0] + sizeof(*opcdh));
			opcdh->cmd = OPENPCD_CMD_PIO_IRQ;
			opcdh->reg = 0x00;
			opcdh->flags = 0x00;
			opcdh->val = 0x00;

			irq_rctx->tx.tot_len = sizeof(*opcdh) + sizeof(u_int32_t);
			req_ctx_set_state(irq_rctx, RCTX_STATE_UDP_EP3_PENDING);
		}
	}

	AT91F_AIC_ClearIt(AT91C_BASE_AIC, AT91C_ID_PIOA);
}

void pio_irq_enable(u_int32_t pio)
{
	AT91F_PIO_InterruptEnable(AT91C_BASE_PIOA, pio);
}

void pio_irq_disable(u_int32_t pio)
{
	AT91F_PIO_InterruptDisable(AT91C_BASE_PIOA, pio);
}

int pio_irq_register(u_int32_t pio, irq_handler_t *handler)
{
	u_int8_t num = ffs(pio);

	if (num == 0)
		return -EINVAL;
	num--;

	if (pirqs.handlers[num])
		return -EBUSY;

	pio_irq_disable(pio);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, pio);
	pirqs.handlers[num] = handler;

	return 0;
}

void pio_irq_unregister(u_int32_t pio)
{
	u_int8_t num = ffs(pio);

	if (num == 0)
		return;
	num--;

	pio_irq_disable(pio);
	pirqs.handlers[num] = NULL;
}

static int pio_irq_usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];

	switch (poh->cmd) {
	case OPENPCD_CMD_PIO_IRQ:
		pirqs.usbmask = poh->val;
		break;
	default:
		DEBUGP("UNKNOWN ");
		return -EINVAL;
	}

	return 0;
}

void pio_irq_init(void)
{
	AT91F_PIOA_CfgPMC();
	AT91F_AIC_ConfigureIt(AT91C_BASE_AIC, AT91C_ID_PIOA,
			      AT91C_AIC_PRIOR_LOWEST,
			      AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, &pio_irq_demux);
	AT91F_AIC_EnableIt(AT91C_BASE_AIC, AT91C_ID_PIOA);
}
