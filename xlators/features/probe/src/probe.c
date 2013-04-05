/*
   Copyright (c) 2006-2012 Red Hat, Inc. <http://www.redhat.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/
#include <ctype.h>
#include <sys/uio.h>

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "glusterfs.h"
#include "xlator.h"
#include "logging.h"

#include "probe_time.h"
#include "probe_stats.h"
#include "probe.h"

/*
 * This is a probe ``encryption'' xlator. It probe's data when
 * writing to disk and probe's it back when reading it.
 * This xlator is meant as an example, NOT FOR PRODUCTION
 * USE ;) (hence no error-checking)
 */

void
probe (char *buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (buf[i] >= 'a' && buf[i] <= 'z')
			buf[i] = 'a' + ((buf[i] - 'a' + 13) % 26);
		else if (buf[i] >= 'A' && buf[i] <= 'Z')
			buf[i] = 'A' + ((buf[i] - 'A' + 13) % 26);
	}
}

void
probe_iovec (struct iovec *vector, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		probe (vector[i].iov_base, vector[i].iov_len);
	}
}

int32_t
probe_readv_cbk (call_frame_t *frame,
                 void *cookie,
                 xlator_t *this,
                 int32_t op_ret,
                 int32_t op_errno,
                 struct iovec *vector,
                 int32_t count,
		 struct iatt *stbuf,
                 struct iobref *iobref, dict_t *xdata)
{
	probe_private_t *priv = (probe_private_t *)this->private;

	if (priv->decrypt_read) {
		probe_iovec (vector, count);
	}

	STACK_UNWIND_STRICT (readv, frame, op_ret, op_errno, vector, count,
                             stbuf, iobref, xdata);
	return 0;
}

int32_t
probe_readv (call_frame_t *frame,
             xlator_t *this,
             fd_t *fd,
             size_t size,
             off_t offset, uint32_t flags, dict_t *xdata)
{
	STACK_WIND (frame,
		    probe_readv_cbk,
		    FIRST_CHILD (this),
		    FIRST_CHILD (this)->fops->readv,
		    fd, size, offset, flags, xdata);
	return 0;
}

int32_t
probe_writev_cbk (call_frame_t *frame,
                  void *cookie,
                  xlator_t *this,
                  int32_t op_ret,
                  int32_t op_errno,
                  struct iatt *prebuf,
		  struct iatt *postbuf, dict_t *xdata)
{
	STACK_UNWIND_STRICT (writev, frame, op_ret, op_errno, prebuf, postbuf,
                             xdata);
	return 0;
}

int32_t
probe_writev (call_frame_t *frame,
              xlator_t *this,
              fd_t *fd,
              struct iovec *vector,
              int32_t count,
              off_t offset, uint32_t flags,
              struct iobref *iobref, dict_t *xdata)
{
	probe_private_t *priv = (probe_private_t *)this->private;
	if (priv->encrypt_write)
		probe_iovec (vector, count);

	STACK_WIND (frame,
		    probe_writev_cbk,
		    FIRST_CHILD (this),
		    FIRST_CHILD (this)->fops->writev,
		    fd, vector, count, offset, flags,
                    iobref, xdata);
	return 0;
}

int32_t
init (xlator_t *this)
{
	data_t *data = NULL;
	probe_private_t *priv = NULL;

	/*
	 * No need to check for children or parents
	 */

	/*
	 * Allocate private data for this object
	 */
	priv = GF_CALLOC (sizeof (probe_private_t), 1, 0);
        if (!priv) {
                return -1;
	}

	/*
	 * Initialize private data to default values
	 */
	priv->decrypt_read = 1;
	priv->encrypt_write = 1;
	priv->probe_start = _gf_false;
	priv->probe_end = _gf_false;
	probe_stats_init(&priv->probe_stats);

	/*
	 * Get options from volfile
	 */
	data = dict_get (this->options, "encrypt-write");
	if (data) {
		if (gf_string2boolean (data->data, &priv->encrypt_write) == -1) {
			gf_log (this->name, GF_LOG_ERROR,
				"encrypt-write takes only boolean options");
			return -1;
		}
	}

	data = dict_get (this->options, "decrypt-read");
	if (data) {
		if (gf_string2boolean (data->data, &priv->decrypt_read) == -1) {
			gf_log (this->name, GF_LOG_ERROR,
				"decrypt-read takes only boolean options");
			return -1;
		}
	}

	data = dict_get (this->options, "probe-start");
	if (data) {
		if (gf_string2boolean (data->data, &priv->probe_start) == -1) {
			gf_log (this->name, GF_LOG_ERROR,
				"probe_start takes only boolean options");
			return -1;
		}
	}

	data = dict_get (this->options, "probe-end");
	if (data) {
		if (gf_string2boolean (data->data, &priv->probe_end) == -1) {
			gf_log (this->name, GF_LOG_ERROR,
				"probe_end takes only boolean options");
			return -1;
		}
	}

	data = dict_get (this->options, "interval-secs");
	if (data) {
		if (gf_string2int(data->data, &priv->interval_secs) == -1) {
			gf_log (this->name, GF_LOG_ERROR,
				"interval_secs takes only int options");
			return -1;
		}
	}

        if (0 != dict_get_str (this->options, "probe-name", &priv->probe_name)) {
		priv->probe_name = "NONAME";
        }
	gf_log ("probe", GF_LOG_DEBUG, "probe-name is %s", priv->probe_name);

        if (0 != dict_get_str (this->options, "probe-group", &priv->probe_group)){
		priv->probe_group = "NOGROUP";
        }
	gf_log ("probe", GF_LOG_DEBUG, "probe-group is %s", priv->probe_group);

        if (0 != dict_get_str (this->options, "directory", &priv->directory)) {
		priv->directory = "/tmp";
        }
	gf_log ("probe", GF_LOG_DEBUG, "directory is %s", priv->directory);

	/*
	 * Save private state 
	 */
	this->private = priv;

	/*
	 * Debug
	 */
	gf_log ("probe", GF_LOG_DEBUG, "probe xlator loaded");
	return 0;
}

void
fini (xlator_t *this)
{
	probe_private_t *priv = this->private;

	/*
	 * Destroy stats object first
	 */
	probe_stats_destroy(&priv->probe_stats);

	/*
	 * Destroy private state
	 */
        if (NULL != priv) {
		this->private = NULL;
		GF_FREE (priv);
	}

	return;
}

/*
 * Set methods handled by this translator
 */
struct xlator_fops fops = {
	.readv        = probe_readv,
	.writev       = probe_writev
};

/*
 * Still a mistery 
 */
struct xlator_cbks cbks;

/*
 * Volfile options
 */
struct volume_options options[] = {
	{ .key  = {"encrypt-write"},
	  .type = GF_OPTION_TYPE_BOOL
	},
	{ .key  = {"decrypt-read"},
	  .type = GF_OPTION_TYPE_BOOL
	},
	{ .key  = {"probe-start"},
	  .type = GF_OPTION_TYPE_BOOL
	},
	{ .key  = {"probe-end"},
	  .type = GF_OPTION_TYPE_BOOL
	},
	{ .key = {"probe-name"},
	  .type = GF_OPTION_TYPE_STR
	},
	{ .key = {"probe-group"},
	  .type = GF_OPTION_TYPE_STR
	},
	{ .key = {"directory"},
	  .type = GF_OPTION_TYPE_STR
	},
	{ .key = {"interval-secs"},
	  .type = GF_OPTION_TYPE_INT
	},
	{ .key  = {NULL} },
};