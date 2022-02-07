save_efi_val_nvram() {
	# $1 NVRAM path
	# $2 QEMU initrd
	# $3 QEMU append
	if [ $# -ne 3 ]; then
		echo "save_efi_val_nvram expects 3 args but get $#"
		exit 1
	fi
	[[ -z "$EFI_TEST" ]] && echo "EFI_TEST var is not set" && exit 1
	[[ -z "$EFI_CASE" ]] && echo "EFI_CASE var is not set" && exit 1
	[[ -z "$EFI_SRC" ]] && echo "EFI_SRC var is not set" && exit 1

	nvram_path="$1"
	qemu_initrd="$2"
	qemu_append="$3"

	mkdir -p "$EFI_TEST/setvar/$EFI_CASE"

	cat <<-EOF > "$EFI_TEST/setvar/$EFI_CASE/setvar.c"
	#include "libcflat.h"
	EOF
}
