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
#ifndef __BLUETOOTH_H__
#define __BLUETOOTH_H__
#include "stdint.h"
#include "btif_sys_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BOOL_DEFINED
typedef unsigned int BOOL;      /* IGNORESTYLE */
#endif

typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;

typedef int S32;
typedef short S16;
typedef char S8;

#ifndef U32_PTR_DEFINED
typedef U32 U32_PTR;

#define U32_PTR_DEFINED
#endif /* U32_PTR_DEFINED */

/* Variable sized integers. Used to optimize processor efficiency by
 * using the most efficient data size for counters, arithmatic, etc.
 */
typedef unsigned long I32;

#ifndef XA_INTEGER_SIZE
#define XA_INTEGER_SIZE    4
#endif

#if XA_INTEGER_SIZE == 4
typedef unsigned long I16;
typedef unsigned long I8;
#elif XA_INTEGER_SIZE == 2
typedef unsigned short I16;
typedef unsigned short I8;
#elif XA_INTEGER_SIZE == 1
typedef unsigned short I16;
typedef unsigned char I8;
#else
#error No XA_INTEGER_SIZE specified!
#endif

typedef void (*PFV) (void);

/* Boolean Definitions */
#ifndef TRUE
#define TRUE  (1==1)
#endif /* TRUE */

#ifndef FALSE
#define FALSE (0==1)
#endif /* FALSE */

#ifndef TimeT
typedef U32 TimeT;
#endif

#ifndef BtPriority
typedef U8 BtPriority;
#endif

/** Bluetooth Address */
typedef struct {
    uint8_t address[6];
} __attribute__ ((packed)) bt_bdaddr_t;

#define BTIF_BD_ADDR_SIZE    6

typedef struct _list_entr {
    struct _list_entr *Flink;
    struct _list_entr *Blink;
} list_entry_t;

/*---------------------------------------------------------------------------
 *
 *  Doubly-linked list manipulation routines.  Some are implemented as
 *  macros but logically are procedures.
 */
#ifndef BTIF_LIST_MACROS
//void InitializeListHead(ListEntry *head);
#define initialize_list_head(ListHead) (\
    (ListHead)->Flink = (ListHead)->Blink = (ListHead) )

#define initialize_list_entry(Entry) (\
    (Entry)->Flink = (Entry)->Blink = 0 )

#define is_entry_available(Entry) (\
    ((Entry)->Flink == 0))

#ifndef is_list_empty
//BOOL is_list_empty(ListEntry *head);
#define is_list_empty(ListHead) (\
    ((ListHead)->Flink == (ListHead)))
#endif

#define get_head_list(ListHead) (ListHead)->Flink

#define get_tail_list(ListHead) (ListHead)->Blink

#define get_next_node(Node)     (Node)->Flink

#define get_prior_node(Node)    (Node)->Blink

#define is_node_connected(n) (((n)->Blink->Flink == (n)) && ((n)->Flink->Blink == (n)))
BOOL is_list_circular(list_entry_t * list);

#define list_assert(exp) (ASSERT(exp, "%s %s, %d\n", #exp, __func__, __LINE__))

//void InsertTailList(ListEntry *head, ListEntry *entry);
void _insert_tail_list(list_entry_t * head, list_entry_t * entry);

#define insert_tail_list(a, b) (list_assert(is_list_circular(a)), \
                            _insert_tail_list(a, b), \
                            list_assert(is_list_circular(a)))

void insert_head_list(list_entry_t * head, list_entry_t * entry);
void _insert_head_list(list_entry_t * head, list_entry_t * entry);

#define insert_head_list(a, b) (list_assert(is_list_circular(a)), \
                            _insert_head_list(a, b), \
                            list_assert(is_list_circular(a)))

list_entry_t *remove_head_list(list_entry_t * head);
list_entry_t *_remove_head_list(list_entry_t * head);

#define remove_head_list(a) (list_assert(is_list_circular(a)), \
                            _remove_head_list(a))

void remove_entry_list(list_entry_t * entry);
BOOL is_node_on_list(list_entry_t * head, list_entry_t * node);
U8 get_list_number(list_entry_t * head);
BOOL is_list_circular(list_entry_t * list);
void move_list(list_entry_t * dest, list_entry_t * src);
#endif

#define iterate_list_safe(head, cur, next, type) \
    for ( (cur) = (type) get_head_list(head) ; \
          (next) = (type) get_next_node(&(cur)->node), \
            (cur) != (type) (head); \
          (cur) = (next))

/*---------------------------------------------------------------------------
 * IterateList()
 *
 *     Sets up ordinary traversal of a list. The current member must NOT
 *     be removed during iteration. Must be followed by a block of code
 *     containing the body of the iteration.
 *
 *     For example:
 *     BtSecurityRecord *record;
 *     IterateList(MEC(secList), record, BtSecurityRecord *) {
 *         [...do something with "record", but do not remove it!...]
 *     }
 *
 * Parameters:
 *     head - Head of list (address of ListEntry structure)
 *     cur - Variable to use for current list member
 *     type - Structure type of cur and next.
 */
#define iterate_list(head, cur, type) \
    for ( (cur) = (type) get_head_list(&(head)) ; \
          (cur) != (type) &(head); \
          (cur) = (type) get_next_node(&(cur)->node) )

enum _bt_status {
    BT_STS_SUCCESS = 0,
    BT_STS_FAILED = 1,
    BT_STS_PENDING = 2,
    BT_STS_BUSY = 11,
    BT_STS_NO_RESOURCES = 12,
    BT_STS_NOT_FOUND = 13,
    BT_STS_DEVICE_NOT_FOUND = 14,
    BT_STS_CONNECTION_FAILED = 15,
    BT_STS_TIMEOUT = 16,
    BT_STS_NO_CONNECTION = 17,
    BT_STS_INVALID_PARM = 18,
    BT_STS_IN_PROGRESS = 19,
    BT_STS_RESTRICTED = 20,
    BT_STS_INVALID_TYPE = 21,
    BT_STS_HCI_INIT_ERR = 22,
    BT_STS_NOT_SUPPORTED = 23,
    BT_STS_IN_USE = 5,
    BT_STS_SDP_CONT_STATE = 24,
    BT_STS_CONTINUE =24,
    BT_STS_CANCELLED = 25,

    /* The last defined status code */
    BT_STS_LAST_CODE = BT_STS_CANCELLED,
};

typedef uint32_t bt_status_t;

typedef struct _evm_timer evm_timer_t;
typedef void (*evm_timer_notify) (evm_timer_t *);

struct _evm_timer {
    list_entry_t node;          /* Used internally by the Event Manager */
    void *context;              /* Context area for use by callers */
    evm_timer_notify func;      /* Function to call when timer fires */

    /* === Internal use only === */
    TimeT time;                 /* Amount of time to wait */
    TimeT startTime;            /* System time when the timer started */
};

/*---------------------------------------------------------------------------
 *            le_to_host16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve a 16-bit number from the given buffer. The number
 *            is in Little-Endian format.
 *
 * Return:    16-bit number.
 */
U16 le_to_host16(const U8 * ptr);

/*---------------------------------------------------------------------------
 *            be_to_host16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve a 16-bit number from the given buffer. The number
 *            is in Big-Endian format.
 *
 * Return:    16-bit number.
 */
U16 be_to_host16(const U8* ptr);
/*---------------------------------------------------------------------------
 *            be_to_host32()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve a 32-bit number from the given buffer. The number
 *            is in Big-Endian format.
 *
 * Return:    32-bit number.
 */
U32 be_to_host32(const U8* ptr);
/*---------------------------------------------------------------------------
 *            store_le16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 16 bit value into a buffer in Little Endian format.
 *
 * Return:    void
 */
void store_le16(U8 *buff, U16 le_value);

/*---------------------------------------------------------------------------
 *            store_le32()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 32 bit value into a buffer in Little Endian format.
 *
 * Return:    void
 */
void store_le32(U8 *buff, U32 le_value);

/*---------------------------------------------------------------------------
 *            store_be16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 16 bit value into a buffer in Big Endian format.
 *
 * Return:    void
 */
void store_be16(U8 *buff, U16 be_value);

/*---------------------------------------------------------------------------
 *            store_be32()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 32 bit value into a buffer in Big Endian format.
 *
 * Return:    void
 */
void store_be32(U8 *buff, U32 be_value);

#ifdef __cplusplus
}
#endif                          /*  */
#endif /*__BLUETOOTH_H__*/
