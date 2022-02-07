/*
 * EFI-related functions to set up and run test cases in EFI
 *
 * Copyright (c) 2021, SUSE, Varad Gautam <varad.gautam@suse.com>
 * Copyright (c) 2021, Google Inc, Zixuan Wang <zixuanwang@google.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "efi.h"
#include "argv.h"
#include <libcflat.h>
#include <asm/setup.h>

/* From lib/argv.c */
extern int __argc, __envc;
extern const char *__args;
extern char *__argv[100];
extern char *__environ[200];

extern int main(int argc, char **argv, char **envp);

efi_system_table_t *efi_system_table = NULL;

static void efi_free_pool(void *ptr)
{
	efi_bs_call(free_pool, ptr);
}

efi_status_t efi_get_memory_map(struct efi_boot_memmap *map)
{
	efi_memory_desc_t *m = NULL;
	efi_status_t status;
	unsigned long key = 0, map_size = 0, desc_size = 0;
	u32 desc_ver;

	status = efi_bs_call(get_memory_map, &map_size,
			     NULL, &key, &desc_size, &desc_ver);
	if (status != EFI_BUFFER_TOO_SMALL || map_size == 0)
		goto out;

	/*
	 * Pad map_size with additional descriptors so we don't need to
	 * retry.
	 */
	map_size += 4 * desc_size;
	*map->buff_size = map_size;
	status = efi_bs_call(allocate_pool, EFI_LOADER_DATA,
			     map_size, (void **)&m);
	if (status != EFI_SUCCESS)
		goto out;

	/* Get the map. */
	status = efi_bs_call(get_memory_map, &map_size,
			     m, &key, &desc_size, &desc_ver);
	if (status != EFI_SUCCESS) {
		efi_free_pool(m);
		goto out;
	}

	*map->desc_ver = desc_ver;
	*map->desc_size = desc_size;
	*map->map_size = map_size;
	*map->key_ptr = key;
out:
	*map->map = m;
	return status;
}

efi_status_t efi_exit_boot_services(void *handle, struct efi_boot_memmap *map)
{
	return efi_bs_call(exit_boot_services, handle, *map->key_ptr);
}

efi_status_t efi_get_system_config_table(efi_guid_t table_guid, void **table)
{
	size_t i;
	efi_config_table_t *tables;

	tables = (efi_config_table_t *)efi_system_table->tables;
	for (i = 0; i < efi_system_table->nr_tables; i++) {
		if (!memcmp(&table_guid, &tables[i].guid, sizeof(efi_guid_t))) {
			*table = tables[i].table;
			return EFI_SUCCESS;
		}
	}
	return EFI_NOT_FOUND;
}

static void efi_exit(efi_status_t code)
{
	exit(code);

	/*
	 * Fallback to UEFI reset_system() service, in case testdev is
	 * missing and exit() does not properly exit.
	 */
	efi_rs_call(reset_system, EFI_RESET_SHUTDOWN, code, 0, NULL);
}

static efi_status_t efi_get_volume(efi_handle_t handle,
				   efi_file_protocol_t **volume)
{
	efi_guid_t loaded_image_protocol = LOADED_IMAGE_PROTOCOL_GUID;
	efi_guid_t file_system_protocol = EFI_FILE_SYSTEM_GUID;
	efi_loaded_image_t *loaded_image = NULL;
	efi_simple_file_system_protocol_t *io;
	efi_status_t status;

	status = efi_bs_call(handle_protocol, handle, &loaded_image_protocol,
			     (void **)&loaded_image);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed to handle loaded image");
		goto efi_get_volume_error;
	}

	status = efi_bs_call(handle_protocol, loaded_image->device_handle,
			     &file_system_protocol, (void **)&io);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed to handle file system protocol");
		goto efi_get_volume_error;
	}

	status = io->open_volume(io, volume);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed to open volume");
		goto efi_get_volume_error;
	}

	return EFI_SUCCESS;

efi_get_volume_error:
	printf(" error: 0x%lx\n", status);
	return EFI_ABORTED;
}

static efi_status_t efi_read_file(efi_file_protocol_t *volume,
				  efi_char16_t *file_name,
				  unsigned long *file_size, char **file_data)
{
	efi_guid_t file_info_guid = EFI_FILE_INFO_ID;
	efi_file_protocol_t *file_handle = NULL;
	struct finfo file_info;
	unsigned long file_info_size;
	efi_status_t status;

	status = volume->open(volume, &file_handle, file_name,
			      EFI_FILE_MODE_READ, 0);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed to open file");
		goto efi_read_file_error;
	}

	file_info_size = sizeof(file_info);
	status = file_handle->get_info(file_handle, &file_info_guid,
				       &file_info_size, &file_info);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed to get file info");
		goto efi_read_file_error;
	}

	*file_size = file_info.info.file_size;
	status = efi_bs_call(allocate_pool, EFI_LOADER_DATA, *file_size + 1,
			     (void **)file_data);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed allocate buffer");
		goto efi_read_file_error;
	}

	status = file_handle->read(file_handle, file_size, *file_data);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed to read file data");
		goto efi_read_file_error;
	}

	status = file_handle->close(file_handle);
	if (status != EFI_SUCCESS) {
		printf("ERROR: failed to close file");
		goto efi_read_file_error;
	}

	(*file_data)[*file_size] = '\0';

	return EFI_SUCCESS;

efi_read_file_error:
	/*
	 * TODO: Current printf does not support wide char (2nd byte of the each
	 * wide char is always a '\0'), thus only the 1st character is printed.
	 */
	printf(" file: %ls, error: 0x%lx\n", file_name, status);
	return EFI_ABORTED;
}

static efi_status_t efi_set_up_envs(efi_file_protocol_t *volume)
{
	efi_char16_t file_name[] = L"ENVS.TXT";
	unsigned long file_size;
	char *file_data = NULL;
	efi_status_t status;

	status = efi_read_file(volume, file_name, &file_size, &file_data);
	if (status != EFI_SUCCESS) {
		printf("Failed to read file\n");
		goto efi_set_up_envs_error;
	}

	setup_env(file_data, (int)file_size);

	return EFI_SUCCESS;

efi_set_up_envs_error:
	return EFI_ABORTED;
}

static efi_status_t efi_set_up_args(efi_file_protocol_t *volume)
{
	efi_char16_t file_name[] = L"ARGS.TXT";
	unsigned long file_size;
	char *file_data = NULL;
	efi_status_t status;

	status = efi_read_file(volume, file_name, &file_size, &file_data);
	if (status != EFI_SUCCESS) {
		printf("Failed to read file\n");
		goto efi_set_up_envs_error;
	}

	__args = file_data;
	__setup_args();

	return EFI_SUCCESS;

efi_set_up_envs_error:
	return EFI_ABORTED;
}

efi_status_t efi_main(efi_handle_t handle, efi_system_table_t *sys_tab)
{
	int ret;
	efi_status_t status;
	efi_bootinfo_t efi_bootinfo;

	efi_system_table = sys_tab;
	efi_file_protocol_t *volume = NULL;

	/* Memory map struct values */
	efi_memory_desc_t *map = NULL;
	unsigned long map_size = 0, desc_size = 0, key = 0, buff_size = 0;
	u32 desc_ver;

	/* Open env and args files */
	status = efi_get_volume(handle, &volume);
	if (status != EFI_SUCCESS) {
		printf("Failed to get volume\n");
		goto efi_main_error;
	}

	efi_set_up_envs(volume);
	efi_set_up_args(volume);

	/* Set up efi_bootinfo */
	efi_bootinfo.mem_map.map = &map;
	efi_bootinfo.mem_map.map_size = &map_size;
	efi_bootinfo.mem_map.desc_size = &desc_size;
	efi_bootinfo.mem_map.desc_ver = &desc_ver;
	efi_bootinfo.mem_map.key_ptr = &key;
	efi_bootinfo.mem_map.buff_size = &buff_size;

	/* Get EFI memory map */
	status = efi_get_memory_map(&efi_bootinfo.mem_map);
	if (status != EFI_SUCCESS) {
		printf("Failed to get memory map\n");
		goto efi_main_error;
	}

	/* 
	 * Exit EFI boot services, let kvm-unit-tests take full control of the
	 * guest
	 */
	status = efi_exit_boot_services(handle, &efi_bootinfo.mem_map);
	if (status != EFI_SUCCESS) {
		printf("Failed to exit boot services\n");
		goto efi_main_error;
	}

	/* Set up arch-specific resources */
	status = setup_efi(&efi_bootinfo);
	if (status != EFI_SUCCESS) {
		printf("Failed to set up arch-specific resources\n");
		goto efi_main_error;
	}

	/* Run the test case */
	ret = main(__argc, __argv, __environ);

	/* Shutdown the guest VM */
	efi_exit(ret);

	/* Unreachable */
	return EFI_UNSUPPORTED;

efi_main_error:
	/* Shutdown the guest with error EFI status */
	efi_exit(status);

	/* Unreachable */
	return EFI_UNSUPPORTED;
}
