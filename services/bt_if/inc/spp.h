/***************************************************************************
 *
 * Copyright 2015-2019 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/

#ifndef __SPP_H__
#define __SPP_H__

#include "sdp_api.h"
#include "cmsis_os.h"
#include "cqueue.h"
#include "rfcomm_api.h"

/*---------------------------------------------------------------------------
 * Serial Port Profile (SPP) layer
 *
 *     The Serial Port Profile (SPP) specification defines procedures
 *     and protocols for Bluetooth devices emulating RS232 or other serial
 *     connections.
 */


/****************************************************************************
 *
 * Section: Configuration Constants
 *
 * The following defines are configuration constants that allow
 * an implementer to include/exclude functionality from SPP.
 *
 ****************************************************************************/

/*---------------------------------------------------------------------------
 * BTIF_SPP_SERVER constant
 *      Configuration constant to enable code for Serial Port Profile
 *      server. If the device is client-only, this should be defined as
 *      BTIF_DISABLED in overide.h.
 */
#ifndef BTIF_SPP_SERVER
#define BTIF_SPP_SERVER      BTIF_ENABLED
#endif

/*---------------------------------------------------------------------------
 * BTIF_SPP_CLIENT constant
 *      Configuration constant to enable code for Serial Port Profile
 *      client. If the device is server-only, this should be defined as
 *      BTIF_DISABLED in overide.h.
 */
#ifndef BTIF_SPP_CLIENT
#define BTIF_SPP_CLIENT      BTIF_DISABLED
#endif

#define SPP_RX_SIGNAL_ID         0x04

typedef struct	{
    osMutexId _osMutexId;
    osMutexDef_t _osMutexDef;
#ifdef CMSIS_OS_RTX
    int32_t _mutex_data[3];
#endif

} spp_mutex_t;

/****************************************************************************
 *
 * Types
 *
 ****************************************************************************/


/*---------------------------------------------------------------------------
 * spp_event_t type
 *
 *     The application is notified of various indications and confirmations
 *     through a callback function.  Depending on the event, different
 *     elements of the spp_callback_parms "spp_callback_parms.p" union will be
 *     valid.
 */
typedef uint16_t spp_event_t;

/** A connection has been established with a remote device.
 *
 *  When this callback is received, the "spp_callback_parms.p.remDev" field
 *  contains a pointer to the remote device context.
 */
#define SPP_EVENT_REMDEV_CONNECTED    	0

/** A connection has been terminated for a remote device.
 *
 *  When this callback is received, the "spp_callback_parms.p.other" field
 *  contains a 0.
 */
#define SPP_EVENT_REMDEV_DISCONNECTED 	1

/** The data has been sent out. At this time the tx buffer can be released.
 *  When this callback is received, the "spp_callback_parms.p.other" field
 *  is from data structure SppTxDone_t.
 *
 */
#define SPP_EVENT_DATA_SENT 			2

/** A request to close a channel was received.
 *
 *  When this callback is received, the "spp_callback_parms.p.other" field
 *  contains a 0.
 */
#define SPP_EVENT_REMDEV_DISCONNECTED_IND       3

/** A request to open a channel was received.
 *
 *  When this callback is received, the "spp_callback_parms.p.remDev" field
 *  contains a pointer to the remote device context.
 *
 *  This is an opportunitity for the server to reject the incoming request. To
 *  reject the request modify spp_callback_parms.status to be BT_STATUS_CANCELLED.
 */
#define SPP_EVENT_REMDEV_CONNECTED_IND          4

//min spp number 2
#define SPP_SERVICE_INDEX (0)
#define SPP_CLIENT_INDEX (1)
#define SPP_SERVICE_PORT_NUM (2)
/* End of spp_event_t */


/*---------------------------------------------------------------------------
 * spp_port_t type
 *
 *     Identifies the serial port as a client (outgoing) or server (incoming).
 */
typedef uint8_t spp_port_t;

#define SPP_SERVER_PORT      		0x01
#define SPP_CLIENT_PORT      		0x02

struct spp_device;

struct spp_tx_done {
	uint8_t         *tx_buf;
	uint16_t        tx_data_length;
};

/* End of spp_port_t */

#if BTIF_SPP_SERVER == BTIF_ENABLED
/*---------------------------------------------------------------------------
 * struct spp_service structure
 *
 *      Servers need this data type for registering with RFCOMM. A particular
 *      service may have one or more ports available. For example, a device
 *      might provide access to 2 modems. It is providing a single service
 *      (modem) via 2 serial ports.
 */
struct spp_service {
    struct rf_service    rf_service;       /* Required for registering service w/RFCOMM */
    const uint8_t             *name;         /* name of service */
    uint16_t             nameLen;       /* Length of name */
    uint16_t             numPorts;      /* Number of ports in this service */
    sdp_record_t         *sdpRecord;    /* Description of the service */
};

#endif /* BTIF_SPP_SERVER == BTIF_ENABLED */

#if BTIF_SPP_CLIENT == BTIF_ENABLED
/*---------------------------------------------------------------------------
 * spp_client structure
 *      Contains all fields unique to clients. spp_client is a data type in
 *      device structures (struct _spp_dev) for clients.
 */
struct spp_client {
    /* === Internal use only === */
    btif_remote_device_t *remDev;
    uint8_t                  serverId;
    sdp_query_token       *sdpToken;
};
#endif

/*---------------------------------------------------------------------------
 * spp_callback_parms structure
 *
 * A pointer to this structure is sent to the application's callback function
 * notifying the application of state changes or important events.
 */
struct spp_callback_parms {
    spp_event_t            event;   /* Event associated with the callback */

    int status;  /* Status of the callback event       */

    /* For certain events, a single member of this union will be valid.
     * See spp_event_t documentation for more information.
     */
    union {
        void           *other;
        btif_remote_device_t *remDev;
    } p;
};


/*---------------------------------------------------------------------------
 * spp_callback_t type
 *
 * A function of this type is called to indicate events to the application.
 */
typedef void (*spp_callback_t)(struct spp_device *locDev,
                                        struct spp_callback_parms *Info);

/* End of spp_callback_t */

/*---------------------------------------------------------------------------
 * _spp_dev structure
 *      This structure defines an SPP device.  Members should not be accessed
 *      directly. and this structure only used internally;
 */
struct _spp_dev {

    /* Fields specific to clients and servers */
    union {

#if BTIF_SPP_CLIENT == BTIF_ENABLED

        struct spp_client   client;

#endif

#if BTIF_SPP_SERVER == BTIF_ENABLED

        struct spp_service  *sppService;

#endif

    } type;

    /* === Internal use only === */

    spp_callback_t      callback;       /* application callback function */

    /* Server / Client elements */
    uint8_t             state;          /* device state */
    rf_channel_t        channel;
    struct rf_modem_status   rModemStatus;   /* remote modem status */
    struct rf_modem_status   lModemStatus;   /* local modem status */
    rf_line_status    lineStatus;
    rf_port_settings  portSettings;

    uint8_t             xonChar;
    uint8_t             xoffChar;
    int16_t             highWater;      /* when rRing.dataLen >= highWater,
                                       flow off rx */
    uint8_t             credit;         /* rx credits outstanding */
    uint8_t             maxCredits;     /* maximum rx credits to issue */
    uint8_t             msr;            /* Modem Status Register */
    int16_t             breakLen;

    /* Transmit packet */
    list_entry_t        freeTxPacketList;

    list_entry_t        pendingTxPacketList;
};

struct spp_device {
    spp_port_t      portType;       /* SPP_SERVER_PORT or SPP_CLIENT_PORT */
	osThreadId      reader_thread_id;
	spp_mutex_t     spp_mutex;
	CQueue          rx_queue;
	uint8_t         *rx_buffer;
	struct _spp_dev sppDev;
    void            *priv;   /*used by the application code for context use*/
};

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})


#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 *
 * Section: Driver Initialization Functions
 *
 * These APIs are called typically when the driver is initialized or loaded.
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 * spp_init_device()
 *
 *      Initializes a Serial Port Profile device structure. Call this API
 *      for each SPP device before using it for any other SPP API. In most
 *      cases this would be during driver initialization.
 *
 * Parameters:
 *      dev -   pointer to existing device structure
 *      packets - pointer to an array of TX packets used when queuing data to
 *          RFCOMM.
 *      numPackets - number of packets pointed to by 'packets'. This number
 *          can be 1 or more. If too few packets are provided, SPP may
 *          run out and be unable to queue more data to RFCOMM until packets
 *          in use are returned to SPP.
 */
void spp_init_device(struct spp_device *osDev, uint16_t numPackets);

/****************************************************************************
 *
 * Section: Serial Port Profile I/O Functions
 *
 * These APIs are provided to upper layers. Normally, they are called
 * as the result of a corresponding call to the system API. For example,
 * in a POSIX interface open() would call spp_open().
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 * spp_open()
 *
 *      Opens a serial device for reading and writing. Requires an
 *      existing ACL connection with the remote device for client devices.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      btDevice - for client port this is a pointer to an existing
 *               RemoteDevice structure describing the remote device to
 *               which a service connection will be made.  Server ports
 *               ignore this field.
 *      callback - The callback function that should be invoked to inform the
 *               application of events of interest.
 *
 * Returns:
 *      BT_STATUS_SUCCESS - device opened successfully
 *      BT_STATUS_INVALID_PARM - invalid parameter; usually this is because
 *                               btDevice is not valid or no ACL exists to
 *                               the device
 *      BT_STATUS_FAILED - device is not in a closed state
bt_status_t spp_open(SppDev         *dev,
                  btif_remote_device_t  *btDevice,
                  spp_callback_t     callback);
 */
bt_status_t spp_open(struct spp_device *osDev,
                        btif_remote_device_t  *btDevice,
                        spp_callback_t callback);

/*---------------------------------------------------------------------------
 * spp_close()
 *
 *     Close the serial device. Requires the device to have been opened
 *     previously by spp_open(). This function does not verify that the
 *     calling thread or process is the same one that opened it. The calling
 *     layer may optionally do any such checks.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      BT_STATUS_SUCCESS - closed successfully
 *      BT_STATUS_FAILED - device was not opened
 */
bt_status_t spp_close(struct spp_device *osDev);

/*---------------------------------------------------------------------------
 * spp_read()
 *      Read from the serial device. Requires the device to have been
 *      successfully opened by spp_open().
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      buffer - allocated buffer to receive bytes
 *      maxBytes - on input: maximum bytes to read; on successful completion:
 *                 number of bytes actually read
 *
 * Returns:
 *      BT_STATUS_SUCCESS - read was successful
 *      BT_STATUS_FAILED - device is not opened.
 */
bt_status_t spp_read(struct spp_device *osDev, char *buffer, uint16_t *maxBytes);

/*---------------------------------------------------------------------------
 * spp_write()
 *      Write to the serial device. Requires the device to have been
 *      successfully opened by spp_open().
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      buffer - buffer containing characters to write
 *      nBytes - on input: pointer to number of bytes in buffer; actual
 *               bytes written are returned in nBytes if it returns success
 *
 * Returns:
 *      BT_STATUS_SUCCESS - write was successful
 *      BT_STATUS_FAILED - device is not open
 */
bt_status_t spp_write(struct spp_device *osDev, char *buffer, uint16_t *nBytes);

/*---------------------------------------------------------------------------
 * spp_ioctl()
 *      Perform an I/O control function on the serial device.
 *      Requires the device to have been successfully opened by the caller.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      function - I/O control function as allowed by OS
 *      arg - argument peculiar to function; many ioctl functions alter
 *            the contents of *arg upon success (this is totally platform
 *            dependent and under the control of SPPOS_Ioctl)
 *
 * Returns:
 *      This function returns the status of SPPOS_Ioctl.
 */
bt_status_t spp_ioctl(struct spp_device *osDev, uint16_t function, void *arg);

/****************************************************************************
 * Section: SPP Lower-layer API's
 *
 * The following APIs are primarily for I/O control operations. That is,
 * the serial driver could call these functions for assistance in carrying
 * out a particular I/O request. For example, SPPOS_Ioctl() might use
 * these functions to obtain information about the device.
 *
 * Error checking (for example, making sure the SppDev is open and in the
 * right state, protecting from race conditions, etc.) is assumed handled
 * prior to making these calls.
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 * spp_get_data_format()
 *      Retrieve the current data format for the specified device.
 *
 * Parameters:
 *      dev - pointer to existing device structure
 *
 * Returns:
 *      Current data format (data bits, stop bits and parity). The value
 *      returned is a superset of that for each individual setting (data bits,
 *      stop bits and parity - see APIs below).
 */
rf_data_format spp_get_data_format(struct spp_device *dev);
#define spp_get_data_format(dev)        ((dev->sppDev)->portSettings.dataFormat)

/*---------------------------------------------------------------------------
 * spp_get_data_bits()
 *      Get the number of data bits in effect for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      Current setting for number of data bits as defined by rf_data_format.
 */

rf_data_format spp_get_data_bits(struct spp_device *dev);
#define spp_get_data_bits(dev)        (rf_data_format)((dev->sppDev)->portSettings.dataFormat \
                                                    & RF_DATA_BITS_MASK)

/*---------------------------------------------------------------------------
 * spp_get_parity()
 *      Get the current parity setting for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      Current parity setting (see rf_data_format).
 */
rf_data_format spp_get_parity(struct spp_device *dev);
#define spp_get_parity(dev)        (rf_data_format)((dev->sppDev)->portSettings.dataFormat \
                                   & (RF_PARITY_TYPE_MASK | RF_PARITY_MASK))

/*---------------------------------------------------------------------------
 * spp_get_stop_bits()
 *      Retrieve the number of stop bits for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      Current setting for number of stop bits as defined by rf_data_format.
 */
rf_data_format spp_get_stop_bits(struct spp_device *dev);
#define spp_get_stop_bits(dev)   (rf_data_format)((dev->sppDev)->portSettings.dataFormat \
                                     & RF_STOP_BITS_MASK)

/*---------------------------------------------------------------------------
 * spp_get_flow_control()
 *      Retrieve the current flow control setting for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      Flow control setting (see rf_flow_control).
 */
rf_flow_control spp_get_flow_control(struct spp_device *dev);
#define spp_get_flow_control(dev)   ((dev->sppDev)->portSettings.flowControl)

/*---------------------------------------------------------------------------
 * spp_get_modem_status()
 *      Retrieve the current modem status signals for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      Modem status signals (see rf_signals).
 */
rf_signals spp_get_modem_status(struct spp_device *dev);
#define spp_get_modem_status(dev)        ((dev->sppDev)->rModemStatus.signals)


/*---------------------------------------------------------------------------
 * spp_get_baud()
 *      Retrieve the current baud rate setting for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      Baud rate (see rf_baud_rate).
 */
rf_baud_rate spp_get_baud(struct spp_device *dev);
#define spp_get_baud(dev)        ((dev->sppDev)->portSettings.baudRate)

/*---------------------------------------------------------------------------
 * spp_send_port_settings()
 *      Transmit the port settings (RfPortSettings) to the remote device.
 *      When a port setting changes on the local system, that setting should
 *      be communicated to the remote device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *
 * Returns:
 *      BT_STATUS_SUCCESS - The processing of sending to the remote device
 *               has been started or BTIF_RF_SEND_CONTROL is disabled
 *      BT_STATUS_FAILED
 */
bt_status_t spp_send_port_settings(struct spp_device *osDev);

/*---------------------------------------------------------------------------
 * spp_set_modem_control()
 *      Set the modem control signals for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      signals - modem control signals to be set
 *
 * Returns:
 *      The modem control signals are set in the device structure. If
 *      BTIF_RF_SEND_CONTROL is enabled, the settings are sent to the remote
 *      device.
 *
 *      BT_STATUS_SUCCESS - BTIF_RF_SEND_CONTROL is disabled or
 *               BTIF_RF_SEND_CONTROL is enabled and the transmission
 *               was begun.
 *
 *      Other returns - See RF_SetModemStatus().
 */
bt_status_t spp_set_modem_control(struct spp_device *osDev, rf_signals signals);

/*---------------------------------------------------------------------------
 * spp_set_baud()
 *      Set the baud rate for the specified device. Baud rate for the
 *      device is informational only.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      baud -   current baud rate
 *
 * Returns:
 *      No error checking is performed. The device structure is updated
 *      with 'baud'. Note that spp_send_port_settings() should be called
 *      subsequently to send the setting to the remote device. It is not
 *      performed automatically in case other port settings are also to
 *      be set.
 */
void spp_set_baud(struct spp_device *osDev, uint8_t baud);

/*---------------------------------------------------------------------------
 * spp_set_data_format()
 *      Set the data format values for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      value -    data format (see rf_data_format)
 *
 * Returns:
 *      No error checking is performed. The device structure is updated
 *      with 'value'. Note that spp_send_port_settings() should be called
 *      subsequently to send the setting to the remote device. It is not
 *      performed automatically in case other port settings are also to
 *      be set.
 */
void spp_set_data_format(struct spp_device *osDev, uint8_t val);

/*---------------------------------------------------------------------------
 * spp_set_flow_control()
 *      Set the flow control value for the specified device.
 *
 * Parameters:
 *      dev -    pointer to existing device structure
 *      value -    Flow control value (see rf_flow_control).
 *
 * Returns:
 *      No error checking is performed. The device structure is updated
 *      with 'value'. Note that spp_send_port_settings() should be called
 *      subsequently to send the setting to the remote device. It is not
 *      performed automatically in case other port settings are also to
 *      be set.
 */
void spp_set_flow_control(struct spp_device *osDev, uint8_t val);

bool sppos_is_txpacket_available(struct spp_device *osDev);

void spp_init_rx_buf(struct spp_device *osDev, uint8_t *rx_buf, uint32_t size);

bt_status_t spp_service_setup(struct spp_device *osDev,
                                struct spp_service *service,
                                sdp_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* __SPP_H__ */
