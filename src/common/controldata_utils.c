/*-------------------------------------------------------------------------
 *
 * controldata_utils.c
 *		Common code for control data file output.
 *
 *
 * Portions Copyright (c) 1996-2022, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/common/controldata_utils.c
 *
 *-------------------------------------------------------------------------
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "access/xlog_internal.h"
#include "catalog/pg_control.h"
#include "common/controldata_utils.h"
#include "common/file_perm.h"
#ifdef FRONTEND
#include "common/logging.h"
#endif
#include "port/pg_crc32c.h"

#ifndef FRONTEND
#include "pgstat.h"
#include "storage/fd.h"
#endif

/* POLAR */
#ifdef FRONTEND
#include "polar_vfs/polar_vfs_fe.h"
#else
#include "access/xlog.h"
#endif
#include "storage/polar_fd.h"
#include "utils/elog.h"
/* POLAR end */

/*
 * get_controlfile()
 *
 * Get controlfile values.  The result is returned as a palloc'd copy of the
 * control file data.
 *
 * crc_ok_p can be used by the caller to see whether the CRC of the control
 * file data is correct.
 */
ControlFileData *
get_controlfile(const char *DataDir, bool *crc_ok_p)
{
	ControlFileData *ControlFile;
	int			fd;
	char		ControlFilePath[MAXPGPATH];
	pg_crc32c	crc;
	int			r;
#ifdef FRONTEND
	pg_crc32c	last_crc;
	int			retries = 0;
#endif

	AssertArg(crc_ok_p);

	ControlFile = palloc(sizeof(ControlFileData));
#ifndef FRONTEND
	if (polar_enable_shared_storage_mode && !polar_is_replica())
#else
	if (polar_enable_shared_storage_mode && !polar_in_replica_mode_fe(DataDir))
#endif
		snprintf(ControlFilePath, MAXPGPATH, "%s/global/pg_control", polar_datadir);
	else
		snprintf(ControlFilePath, MAXPGPATH, "%s/global/pg_control", DataDir);

#ifdef FRONTEND
	INIT_CRC32C(last_crc);

retry:
#endif

#ifndef FRONTEND
	if ((fd = polar_open(ControlFilePath, O_RDONLY | PG_BINARY, 0)) == -1)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for reading: %m",
						ControlFilePath)));
#else
	if ((fd = polar_open(ControlFilePath, O_RDONLY | PG_BINARY, 0)) == -1)
		pg_fatal("could not open file \"%s\" for reading: %m",
				 ControlFilePath);
#endif

	r = polar_read(fd, ControlFile, sizeof(ControlFileData));
	if (r != sizeof(ControlFileData))
	{
		if (r < 0)
#ifndef FRONTEND
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read file \"%s\": %m", ControlFilePath)));
#else
			pg_fatal("could not read file \"%s\": %m", ControlFilePath);
#endif
		else
#ifndef FRONTEND
			ereport(ERROR,
					(errcode(ERRCODE_DATA_CORRUPTED),
					 errmsg("could not read file \"%s\": read %d of %zu",
							ControlFilePath, r, sizeof(ControlFileData))));
#else
			pg_fatal("could not read file \"%s\": read %d of %zu",
					 ControlFilePath, r, sizeof(ControlFileData));
#endif
	}

#ifndef FRONTEND
	if (polar_close(fd) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not close file \"%s\": %m",
						ControlFilePath)));
#else
	if (polar_close(fd) != 0)
		pg_fatal("could not close file \"%s\": %m", ControlFilePath);
#endif

	/* Check the CRC. */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc,
				(char *) ControlFile,
				offsetof(ControlFileData, crc));
	FIN_CRC32C(crc);

	*crc_ok_p = EQ_CRC32C(crc, ControlFile->crc);

#ifdef FRONTEND

	/*
	 * If the server was writing at the same time, it is possible that we read
	 * partially updated contents on some systems.  If the CRC doesn't match,
	 * retry a limited number of times until we compute the same bad CRC twice
	 * in a row with a short sleep in between.  Then the failure is unlikely
	 * to be due to a concurrent write.
	 */
	if (!*crc_ok_p &&
		(retries == 0 || !EQ_CRC32C(crc, last_crc)) &&
		retries < 10)
	{
		retries++;
		last_crc = crc;
		pg_usleep(10000);
		goto retry;
	}
#endif

	/* Make sure the control file is valid byte order. */
	if (ControlFile->pg_control_version % 65536 == 0 &&
		ControlFile->pg_control_version / 65536 != 0)
#ifndef FRONTEND
		elog(ERROR, _("byte ordering mismatch"));
#else
		pg_log_warning("possible byte ordering mismatch\n"
					   "The byte ordering used to store the pg_control file might not match the one\n"
					   "used by this program.  In that case the results below would be incorrect, and\n"
					   "the PostgreSQL installation would be incompatible with this data directory.");
#endif

	return ControlFile;
}

/*
 * update_controlfile()
 *
 * Update controlfile values with the contents given by caller.  The
 * contents to write are included in "ControlFile". "do_sync" can be
 * optionally used to flush the updated control file.  Note that it is up
 * to the caller to properly lock ControlFileLock when calling this
 * routine in the backend.
 */
void
update_controlfile(const char *DataDir,
				   ControlFileData *ControlFile, bool do_sync)
{
	int			fd;
	char		buffer[PG_CONTROL_FILE_SIZE];
	char		ControlFilePath[MAXPGPATH];

	/*
	 * Apply the same static assertions as in backend's WriteControlFile().
	 */
	StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_MAX_SAFE_SIZE,
					 "pg_control is too large for atomic disk writes");
	StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_FILE_SIZE,
					 "sizeof(ControlFileData) exceeds PG_CONTROL_FILE_SIZE");

	/* Update timestamp  */
	ControlFile->time = (pg_time_t) time(NULL);

	/* Recalculate CRC of control file */
	INIT_CRC32C(ControlFile->crc);
	COMP_CRC32C(ControlFile->crc,
				(char *) ControlFile,
				offsetof(ControlFileData, crc));
	FIN_CRC32C(ControlFile->crc);

	/*
	 * Write out PG_CONTROL_FILE_SIZE bytes into pg_control by zero-padding
	 * the excess over sizeof(ControlFileData), to avoid premature EOF related
	 * errors when reading it.
	 */
	memset(buffer, 0, PG_CONTROL_FILE_SIZE);
	memcpy(buffer, ControlFile, sizeof(ControlFileData));

#ifndef FRONTEND
	if (polar_enable_shared_storage_mode && !polar_is_replica())
#else
	if (polar_enable_shared_storage_mode && !polar_in_replica_mode_fe(DataDir))
#endif
		snprintf(ControlFilePath, MAXPGPATH, "%s/%s", polar_datadir, XLOG_CONTROL_FILE);
	else
		snprintf(ControlFilePath, MAXPGPATH, "%s/%s", DataDir, XLOG_CONTROL_FILE);


#ifndef FRONTEND

	/*
	 * All errors issue a PANIC, so no need to use OpenTransientFile() and to
	 * worry about file descriptor leaks.
	 */
	if ((fd = BasicOpenFile(ControlFilePath, O_RDWR | PG_BINARY)) < 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %m",
						ControlFilePath)));
#else
	if ((fd = polar_open(ControlFilePath, O_WRONLY | PG_BINARY,
						 pg_file_create_mode)) == -1)
		pg_fatal("could not open file \"%s\": %m", ControlFilePath);
#endif

	errno = 0;
#ifndef FRONTEND
	pgstat_report_wait_start(WAIT_EVENT_CONTROL_FILE_WRITE_UPDATE);
#endif
	if (polar_write(fd, buffer, PG_CONTROL_FILE_SIZE) != PG_CONTROL_FILE_SIZE)
	{
		/* if write didn't set errno, assume problem is no disk space */
		if (errno == 0)
			errno = ENOSPC;

#ifndef FRONTEND
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not write file \"%s\": %m",
						ControlFilePath)));
#else
		pg_fatal("could not write file \"%s\": %m", ControlFilePath);
#endif
	}
#ifndef FRONTEND
	pgstat_report_wait_end();
#endif

	if (do_sync)
	{
#ifndef FRONTEND
		pgstat_report_wait_start(WAIT_EVENT_CONTROL_FILE_SYNC_UPDATE);
		if (polar_fsync(fd) != 0)
			ereport(PANIC,
					(errcode_for_file_access(),
					 errmsg("could not fsync file \"%s\": %m",
							ControlFilePath)));
		pgstat_report_wait_end();
#else
		if (polar_fsync(fd) != 0)
			pg_fatal("could not fsync file \"%s\": %m", ControlFilePath);
#endif
	}

	if (polar_close(fd) != 0)
	{
#ifndef FRONTEND
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not close file \"%s\": %m",
						ControlFilePath)));
#else
		pg_fatal("could not close file \"%s\": %m", ControlFilePath);
#endif
	}
}
