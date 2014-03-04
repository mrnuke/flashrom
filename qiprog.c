#include <stdio.h>
#include <string.h>
#include "programmer.h"
#include "qiprog.h"

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
	static struct qiprog_device *dev = NULL;

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

	/* Choose the first device for now */
	dev = devs[0];

	if (qiprog_open_device(dev) != QIPROG_SUCCESS) {
		printf("Error opening device\n");
		return -1;
	}

	qi_pgm.data = dev;

	register_opaque_programmer(&qi_pgm);

	arg = extract_programmer_param("type");
	if (arg) {
		if (!strcasecmp(arg, "lpc")) {
			bus = QIPROG_BUS_LPC;
		}
	}

	(void) bus;
	printf("so far so good\n");
	return 0;
}

#define INIT_DEV_AND_CHECK_VALID(dev, flash)				\
	do {							\
		dev = (void *)flash->pgm->opaque.data;		\
		if (!dev) {					\
			printf("Motherfuckerd\n");		\
			return -1;				\
		}						\
	} while(0)

static int flashrom_qiprog_probe(struct flashctx *flash)
{
	struct qiprog_chip_id ids[9];
	struct qiprog_device *dev;

	INIT_DEV_AND_CHECK_VALID(dev, flash);

	if (qiprog_read_chip_id(dev, ids) != QIPROG_SUCCESS) {
		printf("Could not read IDs of connected chips\n");
		return -1;
	}

	if (ids[0].id_method == QIPROG_ID_INVALID) {
		printf("No connected chips found\n");
		return -1;
	}

	flash->chip->manufacture_id = ids[0].device_id;
	flash->chip->model_id = ids[0].device_id;

	printf("Proba dona\n");
	flash->chip->tested = TEST_OK_PREW;
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
