/*
** Copyright 2003 S. Varshavchik.
** See COPYING for distribution information.
*/

#include "config.h"
#include "maildirwatch.h"
#include "maildircreate.h"
#include "liblock/config.h"
#include "liblock/liblock.h"
#include "liblock/mail.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdexcept>
#include <string>

#ifndef LOCK_TIMEOUT
#define LOCK_TIMEOUT 120
#endif

/*
** Courier-IMAP compatible maildir lock.
*/

struct lock_result {
	std::string lockname;
	std::string filename;
	int error=0;
};

static lock_result do_lock(const std::string &dir,
			   maildir::watch *w)
{
	maildir::tmpcreate_info createInfo;

	createInfo.maildir=dir;
	createInfo.uniq="courierlock";

	const char *p=getenv("HOSTNAME");

	if (p)
		createInfo.hostname=p;

	int fd=createInfo.fd();

	if (fd < 0)
		return {"", createInfo.tmpname, errno};

	close(fd);

	/* HACK: newname now contains: ".../new/filename" */
	size_t l=createInfo.newname.rfind('/');

	createInfo.newname=createInfo.newname.substr(0, l-3);

	createInfo.newname += WATCHDOTLOCK;

	while (ll_dotlock(createInfo.newname.c_str(),
			  createInfo.tmpname.c_str(), LOCK_TIMEOUT) < 0)
	{
		if (errno == EEXIST)
		{
			if (w == NULL || !w->unlock(LOCK_TIMEOUT))
				sleep(1);
			continue;
		}

		if (errno == EAGAIN)
		{
			sleep(5);
			continue;
		}

		return {"", createInfo.newname, errno};
	}

	return {createInfo.newname, "", 0};
}

maildir::watch::lock::lock(watch &&w)
	: lock{ static_cast<watch &>(w)}
{
}

maildir::watch::lock::lock(watch &w)
{
	auto result=do_lock(w.maildir, &w);

	lockname=result.lockname;

	if (lockname.empty())
	{
		std::string errmsg="invalid maildir for a lock";

		if (!result.filename.empty())
		{
			errmsg += ": ";
			errmsg += result.filename;
		}

		if (result.error)
		{
			errmsg += ": ";
			errmsg += strerror(result.error);
		}

		throw std::runtime_error(errmsg);
	}
}

maildir::watch::lock::~lock()
{
	unlink(lockname.c_str());
}

char *maildir_lock(const char *dir, struct maildirwatch *w,
		   int *tryAnyway)
{
	if (tryAnyway)
		*tryAnyway=0;

	auto s=do_lock(dir, w ? static_cast<maildir::watch *>(w):nullptr)
		.lockname;

	if (s.empty())
		return nullptr;

	return strdup(s.c_str());
}
