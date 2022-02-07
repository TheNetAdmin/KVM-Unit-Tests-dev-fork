#include "libcflat.h"
#include "efi.h"

static void set_vars(void)
{
	// char initrd_val[] = "KEY1=VAL1\nKEY2=VAL2\n";
	char initrd_val[] = "KEY";
	efi_char16_t initrd_name[] = L"qemu_initrd";
	efi_status_t status;
	u32 attr = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
	status = efi_set_variable(initrd_name, QEMU_INITRD_GUID, attr, initrd_val);
	printf("status=0x%lx\n", status);
	assert(status == EFI_SUCCESS);
}

static void get_vars(void)
{
	char initrd_val[1024];
	unsigned long initrd_val_len = 1024;
	efi_char16_t initrd_name[] = L"qemu_initrd";
	efi_status_t status;
	status = efi_get_variable(initrd_name, QEMU_INITRD_GUID, &initrd_val_len, initrd_val);
	printf("initrd_len: %lu\n", initrd_val_len);
	printf("status=0x%lx\n", status);
	assert(status == EFI_SUCCESS);
	printf("initrd: %s\n", initrd_val);
}

int main(int argc, char **argv)
{
	int i;
	printf("Got %d args\n", argc);
	for (i = 0; i < argc; i++) {
		printf("%d: %s\n", i, argv[i]);
	}
	printf("Set vars\n");
	set_vars();
	printf("Get vars\n");
	get_vars();
	return 0;
}
