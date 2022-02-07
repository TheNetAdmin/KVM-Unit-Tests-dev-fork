#ifndef _EFI_H_
#define _EFI_H_

/*
 * EFI-related functions.
 *
 * Copyright (c) 2021, Google Inc, Zixuan Wang <zixuanwang@google.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "linux/efi.h"
#include <elf.h>

/*
 * efi_bootinfo_t: stores EFI-related machine info retrieved before exiting EFI
 * boot services, and is then used by setup_efi(). setup_efi() cannot retrieve
 * this info as it is called after ExitBootServices and thus some EFI resources
 * and functions are not available.
 */
typedef struct {
	struct efi_boot_memmap mem_map;
} efi_bootinfo_t;

/* 
 * GUIDs for QEMU arguments '-initrd' and '-append'
 */
#define QEMU_INITRD_GUID EFI_GUID(0x67af0b54, 0xb4a4, 0x4616, 0xbe, 0x95, 0xc7, 0xf6, 0x13, 0x62, 0x58, 0xdc)
#define QEMU_APPEND_GUID EFI_GUID(0x567889b4, 0xac67, 0x4134, 0xb4, 0x60, 0x72, 0xc6, 0x8b, 0x36, 0x79, 0xde)
#define EFI_GLOBAL_VARIABLE_GUID EFI_GUID(0x8be4df61, 0x93ca, 0x11d2,  0xaa, 0x0d, 0x00, 0xe0, 0x98, 0x03, 0x2b, 0x8c)


#define EFI_VARIABLE_NON_VOLATILE       0x0000000000000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x0000000000000002
#define EFI_VARIABLE_RUNTIME_ACCESS     0x0000000000000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD 0x0000000000000008
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS 0x0000000000000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x0000000000000020
#define EFI_VARIABLE_APPEND_WRITE	0x0000000000000040

efi_status_t _relocate(long ldbase, Elf64_Dyn *dyn, efi_handle_t handle,
		       efi_system_table_t *sys_tab);
efi_status_t efi_get_memory_map(struct efi_boot_memmap *map);
efi_status_t efi_exit_boot_services(void *handle, struct efi_boot_memmap *map);
efi_status_t efi_get_system_config_table(efi_guid_t table_guid, void **table);
efi_status_t efi_main(efi_handle_t handle, efi_system_table_t *sys_tab);
efi_status_t efi_set_variable(efi_char16_t *name, efi_guid_t guid, u32 attr, char *data);
efi_status_t efi_get_variable(efi_char16_t *name, efi_guid_t guid, unsigned long *data_size, void *data);

#endif /* _EFI_H_ */
