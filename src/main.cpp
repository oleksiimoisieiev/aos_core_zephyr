/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DEBUG

#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>

#include <aos/common/version.hpp>

#include "app/app.hpp"
#include "logger/logger.hpp"
#include "version.hpp"
#include <vch.h>

#if defined(DEBUG)
extern "C" {

#include <storage.h>
#include <xen_dom_mgmt.h>
#include <xrun.h>

char json_msg[] = "{"
	 "\"ociVersion\" : \"1.0.1\", "
		"\"vm\" : { "
			"\"hypervisor\": { "
			"\"path\": \"xen\", "
			"\"parameters\": [\"pvcalls=true\"] "
		"}, "
		"\"kernel\": { "
			"\"path\" : \"unikernel.bin\", "
			"\"parameters\" : [ \"port=8124\", \"hello world\" ]"
		"}, "
		"\"hwconfig\": { "
			"\"devicetree\": \"uni.dtb\" "
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
}

} /* extern "C" */
#endif /* DEBUG */
extern struct xen_domain_cfg domd_cfg;
int main(void)
{
       char tmp[64] = { 0 };
       struct vch_handle srv, cli;
       int rc;
       const char msg1[] = "sample dom0 message 1";
       const size_t msg1_sz = sizeof(msg1);
 
    printk("*** Aos zephyr application: %s ***\n", AOS_ZEPHYR_APP_VERSION);
    printk("*** Aos core library: %s ***\n", AOS_CORE_VERSION);

	prepare_configs();


    rc = domain_create(&domd_cfg, 1);
    if (rc) {
	printk("failed to create domain (%d)\n", rc);
	return rc;
    }

    rc = vch_open(1, "sample_vchan", 128, 256, &srv);
    printk("vch_open() = %d\n", rc);
    rc = vch_write(&srv, msg1, msg1_sz);
    printk("vch_write() = %d\n", rc);

//       rc = vch_connect(0, "sample_vchan", &cli);

//       printk("vch_connect() = %d\n", rc);
//       rc = vch_read(&cli, tmp, 4);
//       printk("vch_read() = %d\n", rc);
//       printk("tmp = '%s'\n", tmp);

//       vch_close(&cli);
	srv.blocking = true;
	for (int i = 0; i < 100; i++) {
		rc = snprintf(tmp, sizeof(tmp), "sample msg #%d !", i);
		rc = vch_write(&srv, tmp, rc);
		printk("vch_write() = %d\n", rc);
		//usleep(USEC_PER_SEC * 2);
		rc = vch_read(&srv, tmp, sizeof(tmp) - 1);
		if (rc > 0) {
			tmp[rc] = '\0';
			printk("[%s]\n", tmp);
		} else
			printk("vch_read() = %d\n", rc);
	}
    vch_close(&srv);
    return 0;
}
