
#define NR_PIO 32

static u_int8_t ffs(u_int32_t in)
{
	int i;

	for (i = sizeof(in)*8; i > 0; i++) {
		if (in & (1 << i-1))
			return i;
	}

	return 0;
}

static void pio_irq_demux(void)
{
	u_int32_t pio = AT91F_PIO_GetInterruptStatus(AT91C_BASE_PIOA);

	for (i = 0; i < NR_PIO; i++) {
		if (pio & (1 << i) && pio_handlers[i])
			pio_handlers[i](i);
	}
	return;
}

void pio_irq_enable(u_int32_t pio)
{
	AT91F_PIO_InterruptEnable(AT91C_BASE_PIOA, pio);
}

void pio_irq_disable(u_int32_t pio)
{
	AT91F_PIO_InterruptDisable(AT91C_BASE_PIOA, pio);
}

int pio_irq_register(u_int32_t pio, void (*handler)(void))
{
	u_int8_t num = ffs(pio);

	if (num == 0)
		return -EINVAL;
	num--;

	if (pio_handlers[num])
		return -EBUSY;

	pio_irq_disable(pio);
	AT91F_PIO_CfgInput(AT91C_BASE_PIOA, pio);
	pio_handlers[num] = handler;

	return 0;
}

void pio_irq_unregister(u_int32_t pio)
{
	u_int8_t num = ffs(pio);

	if (num == 0)
		return;
	num--;

	pio_irq_disable(pio);
	pio_handlers[num] = NULL;
}
