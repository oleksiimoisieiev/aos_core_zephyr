/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>

#include <aos/common/version.hpp>

#include "app/app.hpp"
#include "logger/logger.hpp"
#include "version.hpp"
#define DEBUG
#if defined(DEBUG)
extern "C" {
#include <zephyr/storage/flash_map.h>

#include <xen_dom_mgmt.h>
#include <xrun.h>
#define LFS_NAME_MAX 255
#define PARTITION_NODE \
	DT_NODELABEL(storage)

int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	if (!path) {
		printk("FAIL: Invalid input parameter");
		return -EINVAL;
	}

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		printk("FAIL: Error opening dir %s [%d]", path, res);
		return res;
	}

	printk("Listing dir %s ...", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			if (res < 0) {
				printk("FAIL: Error reading dir [%d]", res);
			}
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			printk("[DIR ] %s", entry.name);
		} else {
			printk("[FILE] %s (size = %zu)",
					entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

int write_file(const char *path, const char *name,
	       char *buf, size_t size)
{
	struct fs_file_t file;
	int rc, ret;
	char fname[LFS_NAME_MAX];

	if (!path || !name || !buf) {
		printk("FAIL: Invalid input parameters");
		return -EINVAL;
	}

        rc = snprintf(fname, LFS_NAME_MAX, "%s/%s", path, name);
	if (rc <= 0) {
		printk("FAIL: Unable to form file path: %d", rc);
		return rc;
	}

	fs_file_t_init(&file);
	rc = fs_open(&file, fname, FS_O_CREATE | FS_O_WRITE);
	if (rc < 0) {
		printk("FAIL: open %s: %d", fname, rc);
		return rc;
	}

	rc = fs_write(&file, buf, size);
	if (rc < 0) {
		printk("FAIL: write %s: %d", fname, rc);
		goto out;
	}

	printk("%s write file size %lu: [wr:%d]", fname, size, rc);

 out:
	ret = fs_close(&file);
	if (ret < 0) {
		printk("FAIL: close %s: %d", fname, ret);
		return ret;
	}

	return (rc < 0 ? rc : 0);
}

static int littlefs_flash_erase(unsigned int id)
{
	const struct flash_area *pfa;
	int rc;

	rc = flash_area_open(id, &pfa);
	if (rc < 0) {
		printk("FAIL: unable to find flash area %u: %d\n",
			id, rc);
		return rc;
	}

	printk("Area %u at 0x%x on %s for %u bytes\n",
		   id, (unsigned int)pfa->fa_off, pfa->fa_dev->name,
		   (unsigned int)pfa->fa_size);

	flash_area_close(pfa);
	return rc;
}

#if DT_NODE_EXISTS(PARTITION_NODE)
FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);
#else /* PARTITION_NODE */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);
static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
	.mnt_point = "/lfs",
};
#endif /* PARTITION_NODE */

	struct fs_mount_t *mp =
#if DT_NODE_EXISTS(PARTITION_NODE)
		&FS_FSTAB_ENTRY(PARTITION_NODE)
#else
		&lfs_storage_mnt
#endif
		;

static int littlefs_mount(struct fs_mount_t *mp)
{
	int rc;

	rc = littlefs_flash_erase((uintptr_t)mp->storage_dev);
	if (rc < 0) {
		return rc;
	}

	/* Do not mount if auto-mount has been enabled */
#if !DT_NODE_EXISTS(PARTITION_NODE) || \
	!(FSTAB_ENTRY_DT_MOUNT_FLAGS(PARTITION_NODE) & FS_MOUNT_FLAG_AUTOMOUNT)
	rc = fs_mount(mp);
	if (rc < 0) {
		printk("Mount id %" PRIuPTR " at %s: %d\n",
			(uintptr_t)mp->storage_dev, mp->mnt_point, rc);
		return rc;
	}
	printk("%s mount: %d\n", mp->mnt_point, rc);
#else
	printk("%s automounted\n", mp->mnt_point);
#endif

	return 0;
}

static int littlefs_umount(struct fs_mount_t *mp)
{
	int rc;

	/* Do not mount if auto-mount has been enabled */
#if !DT_NODE_EXISTS(PARTITION_NODE) ||									\
    !(FSTAB_ENTRY_DT_MOUNT_FLAGS(PARTITION_NODE) & FS_MOUNT_FLAG_AUTOMOUNT)
	rc = fs_umount(mp);
	if (rc < 0) {
		printk("Fail to umount id %" PRIuPTR " at %s: %d\n", (uintptr_t)mp->storage_dev,
				mp->mnt_point, rc);
		return rc;
	}
	printk("%s unmounted: %d\n", mp->mnt_point, rc);
#else
	printk("%s automounted, ignore\n", mp->mnt_point);
#endif

	return 0;
}

char json_msg[] = "{"
	 "\"ociVersion\" : \"1.0.1\", "
		"\"vm\" : { "
			"\"hypervisor\": { "
			"\"path\": \"xen\", "
			"\"parameters\": [\"pvcalls=true\"] "
		"}, "
		"\"kernel\": { "
			"\"path\" : \"/lfs/unikernel.bin\", "
			"\"parameters\" : [ \"port=8124\", \"hello world\" ]"
		"}, "
		"\"hwconfig\": { "
			"\"devicetree\": \"/lfs/uni.dtb\" "
		"} "
	"} "
"}";

extern char __img_unikraft_start[];
extern char __img_unikraft_end[];
extern char __dtb_unikraft_start[];
extern char __dtb_unikraft_end[];

extern void init_root();

static void prepare_configs()
{
	int ret;

	init_root();

	ret = littlefs_mount(mp);
	if (ret) {
		printk("Unable to mount: %d\n", ret);
		return;
	}

	printk("dtb = %p\n", (void *)__dtb_unikraft_start);
	ret = write_file("/lfs", "config.json", json_msg, sizeof(json_msg));
	printk("Write config.json file ret = %d\n", ret);
	ret = write_file("/lfs", "unikernel.bin", __img_unikraft_start,
			__img_unikraft_end - __img_unikraft_start);
	printk("Write unikraft.bim file ret = %d\n", ret);
	ret = write_file("/lfs", "uni.dtb", __dtb_unikraft_start,
			__dtb_unikraft_end - __dtb_unikraft_start);
	printk("Write unikraft.dtb file ret = %d\n", ret);
	ret = lsdir("/lfs");
	printk("lsdir result = %d\n", ret);

	ret = littlefs_umount(mp);
	if (ret)
		printk("Error, unable to unmount %d\n", ret);
}

} /* extern "C" */
#endif /* CONFIG_DEBUG */

int main(void)
{
    printk("*** Aos zephyr application: %s ***\n", AOS_ZEPHYR_APP_VERSION);
    printk("*** Aos core library: %s ***\n", AOS_CORE_VERSION);

	prepare_configs();

    Logger::Init();

    auto& app = App::Get();

    auto err = app.Init();
    __ASSERT(err.IsNone(), "Error initializing application: %s", err.ToString());

    return 0;
}
