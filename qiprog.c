#include <stdio.h>
#include <string.h>
#include "programmer.h"
#include "qiprog.h"
#include "chipdrivers.h"

/*
 * We prefix our programmer functions with "flashrom_" because some would
 * collide with libqiprog functions. Since we have to prefix a few, we prefix
 * all of them for consistency.
 *
 * TODO:
 *   * Too many items to list
 */
static struct qiprog_context *ctx = NULL;
static struct qiprog_device **devs = NULL;

static struct opaque_programmer qi_pgm;

int flashrom_qiprog_init(void)
{
	const char *arg;
	size_t ndevs;
	enum qiprog_bus bus = 0;
	struct qiprog_device *dev = NULL;
	struct qiprog_capabilities caps;

	/* Debug _everything_ */
	qiprog_set_loglevel(QIPROG_LOG_SPEW);

	if (qiprog_init(&ctx) != QIPROG_SUCCESS) {
		printf("libqiprog initialization failure\n");
		return -1;
	}

	ndevs = qiprog_get_device_list(ctx, &devs);
	if (!ndevs) {
		printf("No device found\n");
		return -1;
	}

	/*
	 * TODO: We choose the first device for now
	 *   - Once libqiprog supports it, we might want to allow selecting the
	 *     device based on, for example, its serial number.
	 *   - We could also provide a programmer option to print details about
	 *     connected devices
	 *   - provide programmer option for setting bus voltage.
	 */
	dev = devs[0];

	if (qiprog_open_device(dev) != QIPROG_SUCCESS) {
		printf("Error opening device\n");
		return -1;
	}

	/* We only need a qiprog device pointer as a context */
	qi_pgm.data = dev;
	register_opaque_programmer(&qi_pgm);

	/* FIXME: What is this about? */
	arg = extract_programmer_param("type");
	if (arg) {
		if (!strcasecmp(arg, "lpc")) {
			bus = QIPROG_BUS_LPC;
		}
	}


	if (qiprog_get_capabilities(dev, &caps) != QIPROG_SUCCESS) {
		msg_perr("Could not get programmer's capabilities.\n");
		return -1;
	}

	if (!(caps.bus_master & bus)) {
		msg_perr("Programmer does not support requested bus type\n");
		return -1;
	}

	/*
	 * Operating the programmer without setting the bus could work on some
	 * programmers, but is not guaranteed, and on multi-bus programmers, we
	 * could end up running on a different bus than the one we expect.
	 */
	if (qiprog_set_bus(dev, bus) != QIPROG_SUCCESS) {
		msg_perr("Could not set bus\n");
		return -1;

	}

	printf("so far so good\n");
	return 0;
}

#define INIT_DEV_AND_CHECK_VALID(dev, flash)			\
	do {							\
		dev = (void *)flash->pgm->opaque.data;		\
		if (!dev) {					\
			msg_gerr("Motherfuckerd. BUG!\n");	\
			return -1;				\
		}						\
	} while(0)

static int flashrom_qiprog_probe(struct flashctx *flash)
{
	struct qiprog_chip_id ids[9];
	struct qiprog_device *dev;
	const struct flashchip *db_chip;

	INIT_DEV_AND_CHECK_VALID(dev, flash);

	if (qiprog_read_chip_id(dev, ids) != QIPROG_SUCCESS) {
		printf("Could not read IDs of connected chips\n");
		return -1;
	}

	if (ids[0].id_method == QIPROG_ID_INVALID) {
		printf("No connected chips found\n");
		return -1;
	}

	/*
	 * The great marriage of qiprog and flashrom:
	 *   - qiprog gets the chip identifiers efficiently
	 *   - flashrom knows everything else about the chip
	 */
	db_chip = get_chip_from_ids(ids[0].vendor_id, ids[0].device_id);
	if (!db_chip) {
		msg_gerr("No chip found matching ID %x:%x\n",
			 ids[0].vendor_id, ids[0].device_id);
		return -1;
	}

	*flash->chip = *db_chip;

	printf("Proba dona\n");
	return 1;
}

static int flashrom_qiprog_read(struct flashctx *flash, uint8_t *buf,
				 unsigned int start, unsigned int len)
{
	struct qiprog_device *dev = (void *)flash->pgm->opaque.data;

	if (!dev) {
		printf("Motherfucker\n");
		return -1;
	}

	printf("reada fucka\n");
	return -1;
}

static int flashrom_qiprog_write(struct flashctx *flash, uint8_t *buf,
				  unsigned int start, unsigned int len)
{
	struct qiprog_device *dev = (void *)flash->pgm->opaque.data;

	if (!dev) {
		printf("Motherfucker\n");
		return -1;
	}

	printf("writa fucka\n");
	return -1;
}

static int flashrom_qiprog_erase(struct flashctx *flash, unsigned int blockaddr,
				 unsigned int blocklen)
{
	struct qiprog_device *dev = (void *)flash->pgm->opaque.data;

	if (!dev) {
		printf("Motherfucker\n");
		return -1;
	}

	printf("erasa fucka\n");
	return -1;
}


static struct opaque_programmer qi_pgm = {
	.probe = flashrom_qiprog_probe,
	.read = flashrom_qiprog_read,
	.write = flashrom_qiprog_write,
	.erase = flashrom_qiprog_erase,
};
