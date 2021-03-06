/****************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <tinyara/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <tc_common.h>
#include <tinyara/kernel_test_drv.h>
#include "tc_internal.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: tc_compress_decompress
 *
 * Description:
 *   Block-wise compression and decompression of a file.
 *
 * Returned Value:
 *   None
 ****************************************************************************/
static void tc_compress_decompress(void)
{
	int fd;
	int ret_chk;
	fd = tc2_get_drvfd();

	/*If FAIL : Failed to open testcase driver*/
	TC_ASSERT_GEQ("tc2_get_drvfd", fd, OK);

	ret_chk = ioctl(fd, TESTIOC_COMPRESSION_TEST, 0);
	TC_ASSERT_EQ("Compression_read", ret_chk, OK);

	TC_SUCCESS_RESULT();
}

/****************************************************************************
 * Public Function
 ****************************************************************************/
int tc_compress_read_main(void)
{
	tc_compress_decompress();

	return 0;
}
