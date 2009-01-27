#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <AT91SAM7.h>
#include <string.h>
#include <openpicc.h>
#include <USB-CDC.h>

#include "board.h"
#include "ssc.h"
#include "led.h"
#include "usb_print.h"
#include "pio_irq.h"

#include "cmd.h"

static const AT91PS_SSC ssc = AT91C_BASE_SSC;
static const AT91PS_PDC pdc = (AT91PS_PDC) &(AT91C_BASE_SSC->SSC_RPR);
#define SSC_BUFNUM 35
#define SSC_BUFSIZE (SSC_BUFNUM*1*1024)
#define SSC_DATALEN 32

#define DO_ZEROCOPY

struct ssc_buffer {
	enum { BUFFER_IDLE=0, BUFFER_FILLING, BUFFER_PROCESSING, BUFFER_PROCESSING_USB } state;
	unsigned char __attribute__((aligned(4))) data[SSC_BUFSIZE/SSC_BUFNUM];
};
static volatile struct ssc_buffer rx_buffer[SSC_BUFNUM];

static volatile struct ssc_buffer *primary_buffer, *secondary_buffer;

static xQueueHandle ssc_receive_queue;
static xTaskHandle ssc_receive_task_handle;

/* 0xAA is very unlikely to appear in real radio data, especially for long continued runs, since
 * the sampling frequency is not aligned to the carrier frequency. However, we'll add one irregularity
 * (0x64) to make sure that even a feedback of some kind can not accidentally result in this pattern
 * being actually observed.
 */
#define OVERFLOW_MAGIC 0xAA, 0xAA, 0x64, 0xAA
static const unsigned char __attribute__((aligned(4))) OVERFLOW_MARKER[] = {
		OVERFLOW_MAGIC,
		OVERFLOW_MAGIC,
		OVERFLOW_MAGIC,
		OVERFLOW_MAGIC
};

#define SSC_RX_IRQ_MASK	(AT91C_SSC_RXRDY | 	\
			 AT91C_SSC_OVRUN | 	\
			 AT91C_SSC_ENDRX |	\
			 AT91C_SSC_RXBUFF |	\
			 AT91C_SSC_RXSYN |	\
			 AT91C_SSC_CP0 |	\
			 AT91C_SSC_CP1)

/* Call with interrupts disabled (or otherwise locked) */
static int ssc_refill_rx(int secondary)
{
	int i;
	volatile struct ssc_buffer *buf = NULL;
	for(i=0; i<SSC_BUFNUM; i++) {
		if(rx_buffer[i].state == BUFFER_IDLE) {
			buf = &(rx_buffer[i]);
			break;
		}
	}

	if(buf == NULL) return 0;
	else {
		buf->state = BUFFER_FILLING;
		if(!secondary) {
			primary_buffer = buf;
			AT91F_PDC_SetRx(pdc, (unsigned char*)(buf->data), sizeof(buf->data)/(SSC_DATALEN/8));
		} else {
			secondary_buffer = buf;
			AT91F_PDC_SetNextRx(pdc, (unsigned char*)(buf->data), sizeof(buf->data)/(SSC_DATALEN/8));
		}
	}

	return 1;
}

static unsigned long ssc_irq_count = 0;
static portBASE_TYPE _ssc_irq(portBASE_TYPE task_woken)
{
	ssc_irq_count++;

	u_int32_t sr = ssc->SSC_SR;
	int i, count = 0;
	for(i=0; i<SSC_BUFNUM; i++) {
		if(rx_buffer[i].state == BUFFER_IDLE) {
			count++;
		}
	}
	vLedSetBrightness(LED_RED, 500-(count*500)/SSC_BUFNUM);

	if(sr & AT91C_SSC_ENDRX) {
		/* Process primary, refill secondary */
		primary_buffer->state = BUFFER_PROCESSING;
#ifdef DEBUG_MARKERS
		((unsigned int*)(primary_buffer->data))[0] = 0xff00ff00;
		((unsigned int*)(primary_buffer->data))[1] = (ssc_irq_count<<1) | 1 ;
		((unsigned int*)(primary_buffer->data))[2] = 0x00ff00ff;
#endif
		task_woken = xQueueSendFromISR(ssc_receive_queue, &primary_buffer, task_woken);

		/* Update buffer pointers */
		primary_buffer = secondary_buffer;
		secondary_buffer = NULL;

		/* Refill secondary */
		if(!ssc_refill_rx(1)) {
			/* No more buffers, disable IRQ, send NULL pointer to notify thread of overflow */
			ssc->SSC_IDR = SSC_RX_IRQ_MASK;
			struct ssc_buffer *tmp = NULL;
			task_woken = xQueueSendFromISR(ssc_receive_queue, &tmp, task_woken);
		}

	}

	if(sr & AT91C_SSC_RXBUFF) {
		/* Process primary, refill primary */
		primary_buffer->state = BUFFER_PROCESSING;
#ifdef DEBUG_MARKERS
		((unsigned int*)(primary_buffer->data))[0] = 0xff00ff00;
		((unsigned int*)(primary_buffer->data))[1] = (ssc_irq_count<<1) | 0 ;
		((unsigned int*)(primary_buffer->data))[2] = 0x00ff00ff;
#endif
		task_woken = xQueueSendFromISR(ssc_receive_queue, &primary_buffer, task_woken);

		/* Update buffer pointers */
		primary_buffer = NULL;

		/* Refill primary */
		ssc_refill_rx(0);
	}

	AT91F_AIC_ClearIt(AT91C_ID_SSC);
	AT91F_AIC_AcknowledgeIt();

	return task_woken;
}

static void __ramfunc ssc_irq(void) __attribute__ ((naked));
static void __ramfunc ssc_irq(void)
{
	portENTER_SWITCHING_ISR();
	portBASE_TYPE task_woken = pdFALSE;
	task_woken = _ssc_irq(task_woken);
	portEXIT_SWITCHING_ISR(task_woken);
}

void ssc_start(void)
{
	ssc->SSC_IER = AT91C_SSC_ENDRX | AT91C_SSC_RXBUFF;
	/* Enable DMA */
	AT91F_PDC_EnableRx(pdc);
	AT91F_SSC_EnableRx(ssc);

}

static int sending_buffers = 0;
static const unsigned char SSC_START_STRING[] = { '{', '{', '{', '{' };
static const unsigned char SSC_STOP_STRING[] = { '}', '}', '}', '}' };

void ssc_start_stop_sending_buffers(void) {
	vTaskDelay(50);
	if(!sending_buffers) {
		pio_irq_disable(OPENPICC_SSC_DATA);
		pio_irq_disable(OPENPICC->PLL_LOCK);
		vUSBSendBuffer_blocking(SSC_START_STRING, 0, sizeof(SSC_START_STRING), portMAX_DELAY);
	}
	sending_buffers ^= 1;
	if(!sending_buffers) {
		vUSBSendBuffer_blocking(SSC_STOP_STRING, 0, sizeof(SSC_STOP_STRING), portMAX_DELAY);
	}
}

static int __ramfunc __attribute__((unused)) check_nonempty(struct ssc_buffer *buf)
{
	unsigned int i;
#ifdef DEBUG_MARKERS
	u_int32_t *buf32 = ((u_int32_t*)&(buf->data))+3;
	for(i=3; i<(sizeof(buf->data)/4); i++) {
#else
		u_int32_t *buf32 = ((u_int32_t*)&(buf->data));
		for(i=0; i<(sizeof(buf->data)/4); i++) {
#endif
		if(*(buf32) != 0 && *(buf32++) != 0xffffffff)
			return 1;
	}
	return 0;
}

static unsigned long buffers_processed = 0;
static unsigned long buffers_empty = 0;
#if 0
static void process_buffer(struct ssc_buffer *buf)
{
	if(buffers_processed % 10 == 0) {
		compression_buffer.len = sizeof(compression_buffer.data);
		int r = lzo1x_1_compress(buf->data, sizeof(buf->data), compression_buffer.data,
				&compression_buffer.len, NULL);
		if(r == LZO_E_OK) {
			usb_print_string("Ok ");
			DumpUIntToUSB(compression_buffer.len);
			usb_print_string("\r\n");
		} else {
			usb_print_string("fail\r\n");
		}
	}
	buffers_processed++;
}
#else
unsigned int buffer_index = 0;

#ifdef DO_ZEROCOPY
static void zero_copy_cb(void *cookie)
{
	volatile struct ssc_buffer *buf = cookie;
	buf->state = BUFFER_IDLE;
}
#endif

static void process_buffer(volatile struct ssc_buffer *buf)
{
#ifndef DO_ZEROCOPY
	if(check_nonempty((struct ssc_buffer*)buf)) {
		vUSBSendBuffer_blocking((unsigned char*)(buf->data), 0,
				sizeof(buf->data), 200/portTICK_RATE_MS);
	} else buffers_empty++;
#else
	buf->state = BUFFER_PROCESSING_USB;
	if(!usb_send_buffer_zero_copy((void*)(buf->data), sizeof(buf->data), zero_copy_cb, (void*)buf, 200/portTICK_RATE_MS))
		buf->state = BUFFER_IDLE;
#endif
	buffers_processed++;
}
#endif

static void ssc_recover_from_overflow(void)
{
#ifndef DO_ZEROCOPY
#error Resume-from-overflow code is not capable of !DO_ZEROCOPY operation
#endif
	/* Step 1: Make sure that the SSC is disabled */
	ssc->SSC_IDR = SSC_RX_IRQ_MASK;

	/* Step 1.5: Change the LED brightness, since the SSC IRQ can't do that anymore */
	vLedSetBrightness(LED_RED, 10);

	/* Step 2: Flush the receive queue */
	volatile struct ssc_buffer *tmp;
	while(xQueueReceive(ssc_receive_queue, &tmp, 1) == pdTRUE)
		if(tmp != NULL) tmp->state = BUFFER_IDLE;

	/* Step 3: Wait for the USB-assigned buffers to be transferred (possibly a long time until a host
	 *         is reconnected) */
	int do_break=0, i;
	while(do_break == 0) {
		taskENTER_CRITICAL();
		do_break = 1;
		for(i=0; i<SSC_BUFNUM; i++) {
			if(rx_buffer[i].state == BUFFER_PROCESSING_USB) {
				do_break = 0;
			}
		}
		taskEXIT_CRITICAL();
		vTaskDelay(10/portTICK_RATE_MS);
	}

	/* Step 4: Mark all buffers as clear */
	taskENTER_CRITICAL();
	for(i=0; i<SSC_BUFNUM; i++) {
		rx_buffer[i].state == BUFFER_IDLE;
	}
	taskEXIT_CRITICAL();

	/* Step 5: Set up the SSC buffers again */
	ssc_refill_rx(0);
	ssc_refill_rx(1);

	/* Step 6: Send overflow marker */
	usb_send_buffer_zero_copy(OVERFLOW_MARKER, sizeof(OVERFLOW_MARKER), NULL, NULL, portMAX_DELAY);

	/* Step 7: Restart SSC operation */
	ssc_start();
}

static void ssc_receive_task(void *params)
{
	(void)params;

	ssc_start();

	volatile struct ssc_buffer *buf;
	while(1) {
		if(xQueueReceive(ssc_receive_queue, &buf, portMAX_DELAY) == pdTRUE) {
			if(buf != NULL) {
				/* Process */
				if(sending_buffers) {
					process_buffer(buf);
#ifndef DO_ZEROCOPY
					/* Free */
					buf->state = BUFFER_IDLE;
#endif
				} else {
					/* Free */
					buf->state = BUFFER_IDLE;
				}
			} else {
				/* Overflow happened, wait for all buffers that have been given to the USB to be clear again,
				 * then clear all buffers (even those that are in the SSC, just assume that they are free),
				 * send an overflow mark through the USB and restore SSC operation and interrupts. */
				ssc_recover_from_overflow();
			}
		}
	}
}

unsigned long ssc_get_buffers_processed(void) { return buffers_processed; }
unsigned long ssc_get_buffers_empty(void) { return buffers_empty; }
unsigned long ssc_get_irq_count(void) { return ssc_irq_count; }

int ssc_init(void)
{
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA,
			    OPENPICC_SSC_DATA,
			    0);
	AT91F_SSC_CfgPMC();

	/* Disable all interrupts */
	ssc->SSC_IDR = SSC_RX_IRQ_MASK;

	/* Set SSC clock divider to 7 resulting in an SSC clock of MCK/14 ~ 3.9613381MHz
	 * which gives slightly less than 4 times oversampling for the 847kHz carrier
	 * and approx 64ms of sampling time for a 32kB buffer */
	ssc->SSC_CMR = 8;

	unsigned int i;
	for(i=0; i<sizeof(rx_buffer)/sizeof(rx_buffer[0]); i++)
		memset((void*)&rx_buffer[i], 0, sizeof(rx_buffer[i]));

	ssc_receive_queue = xQueueCreate(SSC_BUFNUM, sizeof(struct ssc_buffer*));
	if(ssc_receive_queue == 0)
		return 0;
	if(xTaskCreate(ssc_receive_task, (void*)"SSC RECV", TASK_SSC_STACK,
			NULL, TASK_SSC_PRIORITY, &ssc_receive_task_handle) != pdPASS) {
		vQueueDelete(ssc_receive_queue);
		return 0;
	}

	AT91F_PDC_DisableRx(pdc);
	AT91F_SSC_DisableRx(ssc);

	int start_cond = AT91C_SSC_START_CONTINOUS;
	int data_len = SSC_DATALEN;
	int num_data = 16;
	int sync_len = 0;

	ssc->SSC_RFMR = ((data_len-1) & 0x1f) |
			(((num_data-1) & 0x0f) << 8) |
			(((sync_len-1) & 0x0f) << 16);
	ssc->SSC_RCMR = 0x00 | AT91C_SSC_CKO_NONE | start_cond;

	ssc_refill_rx(0);
	ssc_refill_rx(1);

	AT91F_AIC_ConfigureIt(AT91C_ID_SSC, OPENPICC_IRQ_PRIO_SSC,
			AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL, (THandler)&ssc_irq);
			//AT91C_AIC_SRCTYPE_INT_POSITIVE_EDGE, (THandler)&ssc_irq);
	AT91F_AIC_EnableIt(AT91C_ID_SSC);

	return 1;
}
