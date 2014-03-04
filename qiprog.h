/*
 * qiprog - Reference implementation of the QiProg protocol
 *
 * Copyright (C) 2013 Alexandru Gagniuc <mr.nuke.me@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __QIPROG_H
#define __QIPROG_H

#include <stdint.h>
#include <stddef.h>

/* Shortcut for enforcing C linkage in C++ code */
#ifdef __cplusplus
#define QIPROG_BEGIN_DECLS extern "C" {
#define QIPROG_END_DECLS }
#else
#define QIPROG_BEGIN_DECLS
#define QIPROG_END_DECLS
#endif

/**
 * @brief QiProg logging verbosity
 */
enum qiprog_log_level {
	QIPROG_LOG_NONE = 0, /**< Do not print messages */
	QIPROG_LOG_ERR = 1,  /**< Print error conditions. */
	QIPROG_LOG_WARN = 2, /**< Print warnings. */
	QIPROG_LOG_INFO = 3, /**< Print informational messages. */
	QIPROG_LOG_DBG = 4,  /**< Print debug messages. */
	QIPROG_LOG_SPEW = 5, /**< Print way too many messages. */
};

/**
 * @brief Specify different bus types supported by QiProg devices.
 *
 * These values may be OR'ed together to specify more than one bus
 */
enum qiprog_bus {
	QIPROG_BUS_ISA = (1 << 0),
	QIPROG_BUS_LPC = (1 << 1),
	QIPROG_BUS_FWH = (1 << 2),
	QIPROG_BUS_SPI = (1 << 3),
	QIPROG_BUS_BDM17 = (1 << 4),
	QIPROG_BUS_BDM35 = (1 << 5),
	QIPROG_BUS_AUD = (1 << 6),
};

/**
 * @brief QiProg error codes
 */
typedef enum qiprog_error {
	QIPROG_SUCCESS = 0,		/**< No error */
	QIPROG_ERR = -1,		/**< Generic error */
	QIPROG_ERR_MALLOC = -2,		/**< Insufficient memory */
	QIPROG_ERR_ARG = -3,		/**< Illegal argument passed */
	QIPROG_ERR_TIMEOUT = -4,	/**< Programmer operation timed out */
	QIPROG_ERR_LARGE_ARG = -5,	/**< Argument too large */

	QIPROG_ERR_CHIP_TIMEOUT = -20,	/**< Flash chip operation timed out */
	QIPROG_ERR_NO_RESPONSE = -21,	/**< Flash chip did not respond */
} qiprog_err;

/**
 * @brief QiProg device capabilities
 */
struct qiprog_capabilities {
	/** bitwise OR of supported QIPROG_LANG_ bits */
	uint16_t instruction_set;
	/** bitwise OR of supported QIPROG_BUS_ bits */
	uint32_t bus_master;
	/**
	 * capabilities.max_direct_data contains the maximum number of bytes
	 * that can be stored by a QiProg device using the instruction set
	 * feature.
	 *
	 * Note that a QiProg device may not support any instruction set, in which case
	 * capabilities.instruction_set = 0.
	 *
	 * This feature is in the TODO stage and should not be relied upon.
	 */
	uint32_t max_direct_data;
	/**
	 * The capabilities.voltages array indicates which voltages the QiProg
	 * device can supply, in millivolt (mV) units. the array ends on the
	 * first 0-value, or if no 0-value is present then the array contains
	 * exactly 10 voltages.
	 */
	uint16_t voltages[10];
};

/**
 * @brief Possible identification methods
 */
enum qiprog_id_method {
	/** No flash chip was identified */
	QIPROG_ID_INVALID = 0,
	/** JEDEC-compliant sequence (not CFI) */
	QIPROG_ID_METH_JEDEC = 0x01,
	/** Common Flash Interface (CFI) */
	QIPROG_ID_METH_CFI = 0x02,
};

/**
 * @brief Flash chip identification
 */
struct qiprog_chip_id {
	/** Method used to identify the chip */
	uint8_t id_method;
	/** Manufacturer's or Vendor's ID */
	uint16_t vendor_id;
	/** The product ID */
	uint32_t device_id;
};

/**
 * @brief Possible erase types for set_erase_size
 */
enum qiprog_erase_type {
	QIPROG_ERASE_TYPE_INVALID = 0,
	QIPROG_ERASE_TYPE_CHIP = 0x01,
	QIPROG_ERASE_TYPE_BLOCK = 0x02,
	QIPROG_ERASE_TYPE_SECTOR = 0x03,
};

enum qiprog_erase_cmd {
	QIPROG_ERASE_CMD_INVALID = 0,
	QIPROG_ERASE_CMD_JEDEC_ISA = 0x01,
	QIPROG_ERASE_CMD_CUSTOM = 0xff,
};

enum qiprog_erase_subcmd {
	QIPROG_ERASE_SUBCMD_DEFAULT = 0,
	QIPROG_ERASE_SUBCMD_CUSTOM = 0xff,
};

enum qiprog_erase_flags {
	QIPROG_ERASE_BEFORE_WRITE = (1 << 0),
};

enum qiprog_write_cmd {
	QIPROG_WRITE_CMD_INVALID = 0,
	QIPROG_WRITE_CMD_JEDEC_ISA = 0x01,
	QIPROG_WRITE_CMD_CUSTOM = 0xff,
};

enum qiprog_write_subcmd {
	QIPROG_WRITE_SUBCMD_DEFAULT = 0,
	QIPROG_WRITE_SUBCMD_CUSTOM = 0xff
};

/** Opaque QiProg context */
struct qiprog_context;
/** Opaque QiProg device */
struct qiprog_device;

QIPROG_BEGIN_DECLS

qiprog_err qiprog_init(struct qiprog_context **ctx);
void qiprog_set_loglevel(enum qiprog_log_level level);
qiprog_err qiprog_exit(struct qiprog_context *ctx);
size_t qiprog_get_device_list(struct qiprog_context *ctx,
			      struct qiprog_device ***list);
qiprog_err qiprog_open_device(struct qiprog_device *dev);
qiprog_err qiprog_get_capabilities(struct qiprog_device *dev,
				   struct qiprog_capabilities *caps);
qiprog_err qiprog_set_bus(struct qiprog_device *dev, enum qiprog_bus bus);
qiprog_err qiprog_set_clock(struct qiprog_device *dev, uint32_t *clock_khz);
qiprog_err qiprog_read_chip_id(struct qiprog_device *dev,
			       struct qiprog_chip_id ids[9]);
qiprog_err qiprog_read(struct qiprog_device *dev, uint32_t where, void *dest,
		       uint32_t n);
qiprog_err qiprog_write(struct qiprog_device *dev, uint32_t where, void *src,
			uint32_t n);
qiprog_err qiprog_set_erase_size(struct qiprog_device *dev, uint8_t chip_idx,
				 enum qiprog_erase_type *types, uint32_t *sizes,
				 size_t num_sizes);
qiprog_err qiprog_set_erase_command(struct qiprog_device *dev, uint8_t chip_idx,
				    enum qiprog_erase_cmd cmd,
				    enum qiprog_erase_subcmd subcmd,
				    uint16_t flags);
qiprog_err qiprog_set_custom_erase_command(struct qiprog_device *dev,
					   uint8_t chip_idx,
					   uint32_t *addr, uint8_t *data,
					   size_t num_bytes);
qiprog_err qiprog_set_write_command(struct qiprog_device *dev, uint8_t chip_idx,
				    enum qiprog_write_cmd cmd,
				    enum qiprog_write_subcmd subcmd);
qiprog_err qiprog_set_custom_write_command(struct qiprog_device *dev,
					   uint8_t chip_idx,
					   uint32_t *addr, uint8_t *data,
					   size_t num_bytes);
qiprog_err qiprog_set_chip_size(struct qiprog_device *dev, uint8_t chip_idx,
				uint32_t size);
qiprog_err qiprog_set_spi_timing(struct qiprog_device *dev,
				 uint16_t tpu_read_us, uint32_t tces_ns);
qiprog_err qiprog_read8(struct qiprog_device *dev, uint32_t addr,
			uint8_t *data);
qiprog_err qiprog_read16(struct qiprog_device *dev, uint32_t addr,
			 uint16_t *data);
qiprog_err qiprog_read32(struct qiprog_device *dev, uint32_t addr,
			 uint32_t *data);
qiprog_err qiprog_write8(struct qiprog_device *dev, uint32_t addr,
			 uint8_t data);
qiprog_err qiprog_write16(struct qiprog_device *dev, uint32_t addr,
			  uint16_t data);
qiprog_err qiprog_write32(struct qiprog_device *dev, uint32_t addr,
			  uint32_t data);
qiprog_err qiprog_set_vdd(struct qiprog_device *dev, uint16_t vdd_mv);

QIPROG_END_DECLS
#endif				/* __QIPROG_H */
