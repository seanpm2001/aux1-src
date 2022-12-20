#ifndef lint	/* .../sys/NFS/sys/nfs_server.c */
#define _AC_NAME nfs_server_c
#define _AC_NO_MAIN "@(#) Copyright (c) 1983-87 Sun Microsystems Inc., 1985-87 UniSoft Corporation, All Rights Reserved.  {Apple version 1.2 87/11/11 21:21:28}"
#include <apple_notice.h>

#ifdef _AC_HISTORY
  static char *sccsid = "@(#)Copyright Apple Computer 1987\tVersion 1.2 of nfs_server.c on 87/11/11 21:21:28";
#endif		/* _AC_HISTORY */
#endif		/* lint */

#define _AC_MODS
/*      @(#)nfs_server.c 1.1 85/05/30 SMI      */

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 * Copyright (c) 1985 by UniSoft, Inc.
 */

#include "sys/param.h"
#include "sys/signal.h"
#include "sys/types.h"
#include "sys/time.h"
#ifdef PAGING
#include "sys/mmu.h"
#include "sys/seg.h"
#include "sys/page.h"
#endif PAGING
#include "sys/systm.h"
#include "sys/errno.h"
#include "sys/user.h"
#include "sys/buf.h"
#include "sys/vfs.h"
#include "sys/vnode.h"
#include "sys/pathname.h"
#include "sys/uio.h"
#include "sys/file.h"
#include "sys/socketvar.h"
#include "sys/errno.h"
#include "sys/dir.h"
#include "netinet/in.h"
#include "rpc/types.h"
#include "rpc/auth.h"
#include "rpc/auth_unix.h"
#include "rpc/svc.h"
#include "rpc/xdr.h"
#include "nfs/nfs.h"
#include "sys/mbuf.h"
#undef DIRBLKSIZ
#include "svfs/fsdir.h"
 

/*
 * rpc service program version range supported
 */
#define	VERSIONMIN	2
#define	VERSIONMAX	2

struct vnode	*fhtovp();
struct file	*getsock();
void		svcerr_progvers();
void		rfs_dispatch();

#ifdef NFSDEBUG
extern int nfsdebug;
#endif

struct {
	int	ncalls;		/* number of calls received */
	int	nbadcalls;	/* calls that failed */
	int	reqs[32];	/* count for each request */
} svstat;

/*
 * NFS Server system call.
 * Does all of the work of running a NFS server.
 * sock is the fd of an open UDP socket.
 */
nfs_svc(uap)
	struct a {
		int     sock;
	} *uap;
{
	struct vnode	*rdir;
	struct vnode	*cdir;
	struct socket   *so;
	struct file	*fp;
	SVCXPRT *xprt;
	u_long vers;
 
	fp = getsock(uap->sock);
	if (fp == 0) {
		u.u_error = EBADF;
		return;
	}
	so = (struct socket *)fp->f_data;
	 
	/*
	 * Be sure that rdir (the server's root vnode) is set.
	 * Save the current directory and restore it again when
	 * the call terminates.  rfs_lookup uses u.u_cdir for lookupname.
	 */
	rdir = u.u_rdir;
	cdir = u.u_cdir;
	if (u.u_rdir == (struct vnode *)0) {
		u.u_rdir = u.u_cdir;
	}
	xprt = svckudp_create(so, NFS_PORT);
	for (vers = VERSIONMIN; vers <= VERSIONMAX; vers++) {
		(void) svc_register(xprt, NFS_PROGRAM, vers, rfs_dispatch,
		    FALSE);
	}
	if (save(u.u_qsav)) {
		svc_unregister(NFS_PROGRAM, NFS_VERSION);
		SVC_DESTROY(xprt);
		u.u_error = EINTR;
	} else {
		svc_run(xprt);  /* never returns */
	}
	u.u_rdir = rdir;
	u.u_cdir = cdir;
}


/*
 * Get file handle system call.
 * Takes open file descriptor and returns a file handle for it.
 */
nfs_getfh(uap)
	register struct a {
		int	fdes;
		fhandle_t	*fhp;
	} *uap;
{
	register struct file *fp;
	fhandle_t fh;
	struct vnode *vp;

	if (!suser()) {
		return;
	}
	fp = getf(uap->fdes);
	if (fp == NULL) {
		return;
	}
	vp = (struct vnode *)fp->f_data;
	u.u_error = makefh(&fh, vp);
	if (!u.u_error) {
		u.u_error =
		    copyout((caddr_t)&fh, (caddr_t)uap->fhp, sizeof(fh));
	}
	return;
}

	
/*
 * These are the interface routines for the server side of the
 * Networked File System.  See the NFS protocol specification
 * for a description of this interface.
 */


/*
 * Get file attributes.
 * Returns the current attributes of the file with the given fhandle.
 */
int
rfs_getattr(fhp, ns)
	fhandle_t *fhp;
	struct nfsattrstat *ns;
{
	int error;
	struct vnode *vp;
	struct vattr va;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_getattr fh %o %d\n",
	    fhp->fh_fsid, fhp->fh_fno);
#endif
	vp = fhtovp(fhp);
	if (vp == NULL) {
		ns->ns_status = NFSERR_STALE;
		return;
	}
	error = VOP_GETATTR(vp, &va, u.u_cred);
	if (!error) {
		vattr_to_nattr(&va, &ns->ns_attr);
	}
	ns->ns_status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_getattr: returning %d\n", error);
#endif
}

/*
 * Set file attributes.
 * Sets the attributes of the file with the given fhandle.  Returns
 * the new attributes.
 */
int
rfs_setattr(args, ns)
	struct nfssaargs *args;
	struct nfsattrstat *ns;
{
	int error;
	struct vnode *vp;
	struct vattr va;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_setattr fh %o %d\n",
	    args->saa_fh.fh_fsid, args->saa_fh.fh_fno);
#endif
	vp = fhtovp(&args->saa_fh);
	if (vp == NULL) {
		ns->ns_status = NFSERR_STALE;
		return;
	}
	sattr_to_vattr(&args->saa_sa, &va);
	error = VOP_SETATTR(vp, &va, u.u_cred);
	if (!error) {
		error = VOP_GETATTR(vp, &va, u.u_cred);
		if (!error) {
			vattr_to_nattr(&va, &ns->ns_attr);
		}
	}
	ns->ns_status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_setattr: returning %d\n", error);
#endif
}

/*
 * Directory lookup.
 * Returns an fhandle and file attributes for file name in a directory.
 */
int
rfs_lookup(da, dr)
	struct nfsdiropargs *da;
	struct  nfsdiropres *dr;
{
	int error;
	struct vnode *dvp;
	struct vnode *vp = (struct vnode *)0;
	struct vattr *va;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_lookup %s fh %o %d\n",
	    da->da_name, da->da_fhandle.fh_fsid, da->da_fhandle.fh_fno);
#endif
	dvp = fhtovp(&da->da_fhandle);
	if (dvp == NULL) {
		dr->dr_status = NFSERR_STALE;
		return;
	}
	va = (struct vattr *)kmem_alloc((u_int)sizeof(*va));

	/*
	 * do lookup.
	 */
	error = VOP_LOOKUP(dvp, da->da_name, &vp, u.u_cred);
	if (error) {
		if (error > EDQUOT) {
			panic("rfs_lookup");
		}
		goto bad;
	}
	error = VOP_GETATTR(vp, va, u.u_cred);
	if (!error) {
		vattr_to_nattr(va, &dr->dr_attr);
		error = makefh(&dr->dr_fhandle, vp);
	}
bad:
	kmem_free((caddr_t)va, (u_int)sizeof(*va));
	dr->dr_status = puterrno(error);
	if (vp) {
		VN_RELE(vp);
	}
	VN_RELE(dvp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_lookup: returning %d\n", error);
#endif
}

/*
 * Read symbolic link.
 * Returns the string in the symbolic link at the given fhandle.
 */
int
rfs_readlink(fhp, rl)
	fhandle_t *fhp;
	struct nfsrdlnres *rl;
{
	int error;
	struct iovec iov;
	struct uio uio;
	struct vnode *vp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_readlink fh %o %d\n",
	    fhp->fh_fsid, fhp->fh_fno);
#endif
	vp = fhtovp(fhp);
	if (vp == NULL) {
		rl->rl_status = NFSERR_STALE;
		return;
	}

	/*
	 * Allocate data for pathname.  This will be freed by rfs_rlfree.
	 */
	rl->rl_data = (char *)kmem_alloc((u_int)MAXPATHLEN);

	/*
	 * Set up io vector to read sym link data
	 */
	iov.iov_base = rl->rl_data;
	iov.iov_len = NFS_MAXPATHLEN;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_seg = UIOSEG_KERNEL;
	uio.uio_offset = 0;
	uio.uio_resid = NFS_MAXPATHLEN;

	/*
	 * read link
	 */
	error = VOP_READLINK(vp, &uio, u.u_cred);

	/*
	 * Clean up
	 */
	if (error) {	
		kmem_free((caddr_t)rl->rl_data, (u_int)NFS_MAXPATHLEN);
		rl->rl_count = 0;
		rl->rl_data = NULL;
	} else {
		rl->rl_count = NFS_MAXPATHLEN - uio.uio_resid;
	}
	rl->rl_status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_readlink: returning '%s' %d\n",
	    rl->rl_data, error);
#endif
}

/*
 * Free data allocated by rfs_readlink
 */
rfs_rlfree(rl)
	struct nfsrdlnres *rl;
{
	if (rl->rl_data) {
		kmem_free((caddr_t)rl->rl_data, (u_int)NFS_MAXPATHLEN); 
	}
}

/*
 * Read data.
 * Returns some data read from the file at the given fhandle.
 */
int
rfs_read(ra, rr)
	struct nfsreadargs *ra;
	struct nfsrdresult *rr;
{
	int error;
	struct vnode *vp;
	struct vattr va;
	struct iovec iov;
	struct uio uio;
	int offset, fsbsize;
	struct buf *bp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_read %d from fh %o %d\n",
	    ra->ra_count, ra->ra_fhandle.fh_fsid, ra->ra_fhandle.fh_fno);
#endif
	rr->rr_data = NULL;
	rr->rr_count = 0;
	vp = fhtovp(&ra->ra_fhandle);
	if (vp == NULL) {
		rr->rr_status = NFSERR_STALE;
		return;
	}
	error = VOP_GETATTR(vp, &va, u.u_cred);
	if (error) {
		goto bad;
	}
	/*
	 * This is a kludge to allow reading of files created
	 * with no read permission.  The owner of the file
	 * is always allowed to read it.
	 */
	if (u.u_uid != va.va_uid) {
		error = VOP_ACCESS(vp, VREAD, u.u_cred);
		if (error) {
			/*
			 * Exec is the same as read over the net because
			 * of demand loading.
			 */
			error = VOP_ACCESS(vp, VEXEC, u.u_cred);
		}
		if (error) {
			goto bad;
		}
	}

	/*
	 * Check whether we can do this with bread, which would
	 * save the copy through the uio.
	 */
	fsbsize = vp->v_vfsp->vfs_bsize;
	offset = ra->ra_offset % fsbsize;
	if (offset + ra->ra_count <= fsbsize) {
		if (ra->ra_offset >= va.va_size) {
			rr->rr_count = 0;
			vattr_to_nattr(&va, &rr->rr_attr);
			goto done;
		}
		error = VOP_BREAD(vp, ra->ra_offset / fsbsize, &bp);
		if (error == 0) {
			rr->rr_data = bp->b_un.b_addr + offset;
			rr->rr_count = MIN(
			    (u_int)(va.va_size - ra->ra_offset),
			    (u_int)ra->ra_count);
			rr->rr_bp = bp;
			rr->rr_vp = vp;
			VN_HOLD(vp);
			vattr_to_nattr(&va, &rr->rr_attr);
			goto done;
		} else {
			printf("nfs read: failed, errno %d\n", error);
		}
	}
	rr->rr_bp = (struct buf *) 0;
			
	/*
	 * Allocate space for data.  This will be freed by xdr_rdresult
	 * when it is called with x_op = XDR_FREE.
	 */
	rr->rr_data = (char *)kmem_alloc((u_int)ra->ra_count);

	/*
	 * Set up io vector
	 */
	iov.iov_base = rr->rr_data;
	iov.iov_len = ra->ra_count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_seg = UIOSEG_KERNEL;
	uio.uio_offset = ra->ra_offset;
	uio.uio_resid = ra->ra_count;
	/*
	 * for now we assume no append mode and ignore
	 * totcount (read ahead)
	 */
	error = VOP_RDWR(vp, &uio, UIO_READ, IO_SYNC, u.u_cred);
	if (error) {
		goto bad;
	}
	vattr_to_nattr(&va, &rr->rr_attr);
	rr->rr_count = ra->ra_count - uio.uio_resid;
	/*
	 * free the unused part of the data allocated
	 */
	if (uio.uio_resid) {
		kmem_free((caddr_t)(rr->rr_data + rr->rr_count),
		    (u_int)uio.uio_resid);
	}
bad:
	if (error && rr->rr_data != NULL) {
		kmem_free((caddr_t)rr->rr_data, (u_int)ra->ra_count);
		rr->rr_data = NULL;
		rr->rr_count = 0;
	}
done:
	rr->rr_status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_read returning %d, count = %d\n",
	    error, rr->rr_count);
#endif
}

/*
 * Free data allocated by rfs_read.
 */
rfs_rdfree(rr)
	struct nfsrdresult *rr;
{
	if (rr->rr_bp == 0 && rr->rr_data) {
		kmem_free((caddr_t)rr->rr_data, (u_int)rr->rr_count);
	}
}

/*
 * Write data to file.
 * Returns attributes of a file after writing some data to it.
 */
int
rfs_write(wa, ns)
	struct nfswriteargs *wa;
	struct nfsattrstat *ns;
{
	int error;
	struct vnode *vp;
	struct vattr va;
	struct iovec iov;
	struct uio uio;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_write: %d bytes fh %o %d\n",
	    wa->wa_count, wa->wa_fhandle.fh_fsid, wa->wa_fhandle.fh_fno);
#endif
	vp = fhtovp(&wa->wa_fhandle);
	if (vp == NULL) {
		ns->ns_status = NFSERR_STALE;
		return;
	}
	error = VOP_GETATTR(vp, &va, u.u_cred);
	if (!error) {
		if (u.u_uid != va.va_uid) {
			/*
			 * This is a kludge to allow writes of files created
			 * with read only permission.  The owner of the file
			 * is always allowed to write it.
			 */
			error = VOP_ACCESS(vp, VWRITE, u.u_cred);
		}
		if (!error) {
			iov.iov_base = wa->wa_data;
			iov.iov_len = wa->wa_count;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_seg = UIOSEG_KERNEL;
			uio.uio_offset = wa->wa_offset;
			uio.uio_resid = wa->wa_count;
			/*
			 * for now we assume no append mode
			 */
			error = VOP_RDWR(vp, &uio, UIO_WRITE, IO_SYNC, u.u_cred);
		}
	}
	if (error) {
		printf("nfs write: failed, errno %d fh %o %d\n",
		    error, wa->wa_fhandle.fh_fsid, wa->wa_fhandle.fh_fno);
	} else {
		/*
		 * Get attributes again so we send the latest mod
		 * time to the client side for his cache.
		 */
		error = VOP_GETATTR(vp, &va, u.u_cred);
	}
	ns->ns_status = puterrno(error);
	if (!error) {
		vattr_to_nattr(&va, &ns->ns_attr);
	}
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_write: returning %d\n", error);
#endif
}

/*
 * Create a file.
 * Creates a file with given attributes and returns those attributes
 * and an fhandle for the new file.
 */
int
rfs_create(args, dr)
	struct nfscreatargs *args;
	struct  nfsdiropres *dr;
{
	int error;
	struct vattr va;
	struct vnode *vp;
	struct vnode *dvp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_create: %s dfh %o %d\n",
	    args->ca_da.da_name, args->ca_da.da_fhandle.fh_fsid,
	    args->ca_da.da_fhandle.fh_fno);
#endif
	sattr_to_vattr(&args->ca_sa, &va);
	va.va_type = VREG;
	/*
	 * XXX Should get exclusive flag and use it.
	 */
	dvp = fhtovp(&args->ca_da.da_fhandle);
	if (dvp == NULL) {
		dr->dr_status = NFSERR_STALE;
		return;
	}
	error = VOP_CREATE(
	    dvp, args->ca_da.da_name, &va, NONEXCL, VWRITE, &vp, u.u_cred);
	if (!error) {
		error = VOP_GETATTR(vp, &va, u.u_cred);
		if (!error) {
			vattr_to_nattr(&va, &dr->dr_attr);
			error = makefh(&dr->dr_fhandle, vp);
		}
		VN_RELE(vp);
	}
	dr->dr_status = puterrno(error);
	VN_RELE(dvp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_create: returning %d\n", error);
#endif
}

/*
 * Remove a file.
 * Remove named file from parent directory.
 */
int
rfs_remove(da, status)
	struct nfsdiropargs *da;
	enum nfsstat *status;
{
	int error;
	struct vnode *vp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_remove %s dfh %o %d\n",
	    da->da_name, da->da_fhandle.fh_fsid, da->da_fhandle.fh_fno);
#endif
	vp = fhtovp(&da->da_fhandle);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		return;
	}
	error = VOP_REMOVE(vp, da->da_name, u.u_cred);
	*status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_remove: %s returning %d\n",
	    da->da_name, error);
#endif
}

/*
 * rename a file
 * Give a file (from) a new name (to).
 */
int
rfs_rename(args, status)
	struct nfsrnmargs *args;
	enum nfsstat *status; 
{
	int error;
	struct vnode *fromvp;
	struct vnode *tovp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_rename %s ffh %o %d -> %s tfh %o %d\n",
	    args->rna_from.da_name,
	    args->rna_from.da_fhandle.fh_fsid,
	    args->rna_from.da_fhandle.fh_fno,
	    args->rna_to.da_name,
	    args->rna_to.da_fhandle.fh_fsid,
	    args->rna_to.da_fhandle.fh_fno);
#endif
	fromvp = fhtovp(&args->rna_from.da_fhandle);
	if (fromvp == NULL) {
		*status = NFSERR_STALE;
		return;
	}
	tovp = fhtovp(&args->rna_to.da_fhandle);
	if (tovp == NULL) {
		*status = NFSERR_STALE;
		VN_RELE(fromvp);
		return;
	}
	error = VOP_RENAME(
	   fromvp, args->rna_from.da_name, tovp, args->rna_to.da_name, u.u_cred);
	*status = puterrno(error); 
	VN_RELE(fromvp);
	VN_RELE(tovp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_rename: returning %d\n", error);
#endif
} 

/*
 * Link to a file.
 * Create a file (to) which is a hard link to the given file (from).
 */
int
rfs_link(args, status) 
	struct nfslinkargs *args;
	enum nfsstat *status;  
{
	int error;
	struct vnode *fromvp;
	struct vnode *tovp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_link ffh %o %d -> %s tfh %o %d\n",
	    args->la_from.fh_fsid, args->la_from.fh_fno,
	    args->la_to.da_name,
	    args->la_to.da_fhandle.fh_fsid, args->la_to.da_fhandle.fh_fno);
#endif
	fromvp = fhtovp(&args->la_from);
	if (fromvp == NULL) {
		*status = NFSERR_STALE;
		return;
	}
	tovp = fhtovp(&args->la_to.da_fhandle);
	if (tovp == NULL) {
		*status = NFSERR_STALE;
		VN_RELE(fromvp);
		return;
	}
	error = VOP_LINK(fromvp, tovp, args->la_to.da_name, u.u_cred);
	*status = puterrno(error);
	VN_RELE(fromvp);
	VN_RELE(tovp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_link: returning %d\n", error);
#endif
} 
 
/*
 * Symbolicly link to a file.
 * Create a file (to) with the given attributes which is a symbolic link
 * to the given path name (to).
 */
int
rfs_symlink(args, status) 
	struct nfsslargs *args;
	enum nfsstat *status;   
{		  
	int error;
	struct vattr va;
	struct vnode *vp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_symlink %s ffh %o %d -> %s\n",
	    args->sla_from.da_name,
	    args->sla_from.da_fhandle.fh_fsid,
	    args->sla_from.da_fhandle.fh_fno,
	    args->sla_tnm);
#endif
	sattr_to_vattr(&args->sla_sa, &va);
	va.va_type = VLNK;
	vp = fhtovp(&args->sla_from.da_fhandle);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		return;
	}
	error = VOP_SYMLINK(
	    vp, args->sla_from.da_name, &va, args->sla_tnm, u.u_cred);
	*status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_symlink: returning %d\n", error);
#endif
}  
  
/*
 * Make a directory.
 * Create a directory with the given name, parent directory, and attributes.
 * Returns a file handle and attributes for the new directory.
 */
int
rfs_mkdir(args, dr)
	struct nfscreatargs *args;
	struct  nfsdiropres *dr;
{
	int error;
	struct vattr va;
	struct vnode *dvp;
	struct vnode *vp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_mkdir %s fh %o %d\n",
	    args->ca_da.da_name, args->ca_da.da_fhandle.fh_fsid,
	    args->ca_da.da_fhandle.fh_fno);
#endif
	sattr_to_vattr(&args->ca_sa, &va);
	va.va_type = VDIR;
	/*
	 * Should get exclusive flag and pass it on here
	 */
	vp = fhtovp(&args->ca_da.da_fhandle);
	if (vp == NULL) {
		dr->dr_status = NFSERR_STALE;
		return;
	}
	error = VOP_MKDIR(vp, args->ca_da.da_name, &va, &dvp, u.u_cred);
	if (!error) {
		vattr_to_nattr(&va, &dr->dr_attr);
		error = makefh(&dr->dr_fhandle, dvp);
		VN_RELE(dvp);
	}
	dr->dr_status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_mkdir: returning %d\n", error);
#endif
}

/*
 * Remove a directory.
 * Remove the given directory name from the given parent directory.
 */
int
rfs_rmdir(da, status)
	struct nfsdiropargs *da;
	enum nfsstat *status;
{
	int error;
	struct vnode *vp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_rmdir %s fh %o %d\n",
	    da->da_name, da->da_fhandle.fh_fsid, da->da_fhandle.fh_fno);
#endif
	vp = fhtovp(&da->da_fhandle);
	if (vp == NULL) {
		*status = NFSERR_STALE;
		return;
	}
	error = VOP_RMDIR(vp, da->da_name, u.u_cred);
	*status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "nfs_rmdir returning %d\n", error);
#endif
}

int
rfs_readdir(rda, rd)
	struct nfsrddirargs *rda;
	struct nfsrddirres  *rd;
{
	int error;
	u_long offset;
	u_long skipped;
	struct iovec iov;
	struct uio uio;
	struct vnode *vp;
	struct direct *dp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_readdir fh %o %d count %d\n",
	    rda->rda_fh.fh_fsid, rda->rda_fh.fh_fno, rda->rda_count);
#endif
	vp = fhtovp(&rda->rda_fh);
	if (vp == NULL) {
		rd->rd_status = NFSERR_STALE;
		return;
	}
	/*
	 * check cd access to dir.  we have to do this here because
	 * the opendir doesn't go over the wire.
	 */
	error = VOP_ACCESS(vp, VEXEC, u.u_cred);
	if (error) {
		goto bad;
	}

nxtblk:
	/*
	 * Allocate data for entries.  This will be freed by rfs_rdfree.
	 */
	rd->rd_entries = (struct direct *)kmem_alloc((u_int)rda->rda_count);
	rd->rd_bufsize = rda->rda_count;

#ifdef	SVFSDIRSIZ
	rd->rd_offset = offset = rda->rda_offset & ~(sizeof(struct svfsdirect) - 1);
#else
	rd->rd_offset = offset = rda->rda_offset & ~(DIRBLKSIZ -1);
#endif

	/*
	 * Set up io vector to read directory data
	 */
	iov.iov_base = (caddr_t)rd->rd_entries;
	iov.iov_len = rda->rda_count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_seg = UIOSEG_KERNEL;
	uio.uio_offset = offset;
	uio.uio_resid = rda->rda_count;

	/*
	 * read directory
	 */
	error = VOP_READDIR(vp, &uio, u.u_cred);

	/*
	 * Clean up
	 */
	if (error) {	
		kmem_free((caddr_t)rd->rd_entries, (u_int)rd->rd_bufsize);
		rd->rd_bufsize = 0;
		rd->rd_size = 0;
		rd->rd_entries = NULL;
	} else {
		/*
		 * set size and eof
		 */
		if (uio.uio_resid) {
			rd->rd_size = rda->rda_count - uio.uio_resid;
			rd->rd_eof = TRUE;
		} else {
			rd->rd_size = rda->rda_count;
			rd->rd_eof = FALSE;
		}

		/*
		 * if client request was in the middle of a block
		 * or block begins with null entries skip entries
		 * til we are on a valid entry >= client's requested
		 * offset.
		 */
		dp = rd->rd_entries;
		skipped = 0;
#ifdef	SVFSDIRSIZ
		while (((offset + sizeof(struct svfsdirect) <= rda->rda_offset) ||
			(dp->d_ino == 0))
		      && (skipped < rd->rd_size)) {
			skipped += dp->d_reclen;
			offset += sizeof(struct svfsdirect);
			dp = (struct direct *)((int)dp + dp->d_reclen);
		}
#else
		while (((offset + dp->d_reclen <= rda->rda_offset) ||
			(dp->d_ino == 0))
		      && (skipped < rd->rd_size)) {
			skipped += dp->d_reclen;
			offset += dp->d_reclen;
			dp = (struct direct *)((int)dp + dp->d_reclen);
		}
#endif

		/*
		 * Reset entries pointer and free space we are skipping
		 */
		if (skipped) {
			rd->rd_size -= skipped;
			rd->rd_bufsize -= skipped;
			rd->rd_offset = offset;
			kmem_free((caddr_t)rd->rd_entries, (u_int)skipped);
			rd->rd_entries = (struct direct *)
			    ((int)rd->rd_entries + skipped);
			if (rd->rd_size == 0 && !rd->rd_eof) {
				/*
				 * we have skipped a whole block, reset offset
				 * and read another block (unless eof)
				 */
				rda->rda_offset = rd->rd_offset;
				goto nxtblk;
			}
		}
	}
bad:
	rd->rd_status = puterrno(error);
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_readdir: returning %d\n", error);
#endif
}

rfs_rddirfree(rd)
	struct nfsrddirres *rd;
{

	kmem_free((caddr_t)rd->rd_entries, (u_int)rd->rd_bufsize);
}

rfs_statfs(fh, fs)
	fhandle_t *fh;
	struct nfsstatfs *fs;
{
	int error;
	struct statfs sb;
	struct vnode *vp;

#ifdef NFSDEBUG
	dprint(nfsdebug, 4, "rfs_statfs fh %o %d\n", fh->fh_fsid, fh->fh_fno);
#endif
	vp = fhtovp(fh);
	if (vp == NULL) {
		fs->fs_status = NFSERR_STALE;
		return;
	}
	error = VFS_STATFS(vp->v_vfsp, &sb);
	fs->fs_status = puterrno(error);
	if (!error) {
		fs->fs_tsize = NFS_MAXDATA;      /* XXX should use real size */
		fs->fs_bsize = sb.f_bsize;
		fs->fs_blocks = sb.f_blocks;
		fs->fs_bfree = sb.f_bfree;
		fs->fs_bavail = sb.f_bavail;
	}
	VN_RELE(vp);
#ifdef NFSDEBUG
	dprint(nfsdebug, 5, "rfs_statfs returning %d\n", error);
#endif
}

/*ARGSUSED*/
rfs_null(argp, resp)
	caddr_t *argp;
	caddr_t *resp;
{
	/* do nothing */
	return (0);
}

/*ARGSUSED*/
rfs_error(argp, resp)
	caddr_t *argp;
	caddr_t *resp;
{
	return (EOPNOTSUPP);
}

int
nullfree()
{
}

/*
 * rfs dispatch table
 * Indexed by version,proc
 */

struct rfsdisp {
	int	  (*dis_proc)();	/* proc to call */
	xdrproc_t dis_xdrargs;		/* xdr routine to get args */
	int	  dis_argsz;		/* sizeof args */
	xdrproc_t dis_xdrres;		/* xdr routine to put results */
	int	  dis_ressz;		/* size of results */
	int	  (*dis_resfree)();	/* frees space allocated by proc */
} rfsdisptab[][RFS_NPROC]  = {
	{
	/*
	 * VERSION 2
	 * Changed rddirres to have eof at end instead of beginning
	 */
	/* RFS_NULL = 0 */
	{rfs_null, xdr_void, 0,
	    xdr_void, 0, nullfree},
	/* RFS_GETATTR = 1 */
	{rfs_getattr, xdr_fhandle, sizeof(fhandle_t),
	    xdr_attrstat, sizeof(struct nfsattrstat), nullfree},
	/* RFS_SETATTR = 2 */
	{rfs_setattr, xdr_saargs, sizeof(struct nfssaargs),
	    xdr_attrstat, sizeof(struct nfsattrstat), nullfree},
	/* RFS_ROOT = 3 *** NO LONGER SUPPORTED *** */
	{rfs_error, xdr_void, 0,
	    xdr_void, 0, nullfree},
	/* RFS_LOOKUP = 4 */
	{rfs_lookup, xdr_diropargs, sizeof(struct nfsdiropargs),
	    xdr_diropres, sizeof(struct nfsdiropres), nullfree},
	/* RFS_READLINK = 5 */
	{rfs_readlink, xdr_fhandle, sizeof(fhandle_t),
	    xdr_rdlnres, sizeof(struct nfsrdlnres), rfs_rlfree},
	/* RFS_READ = 6 */
	{rfs_read, xdr_readargs, sizeof(struct nfsreadargs),
	    xdr_rdresult, sizeof(struct nfsrdresult), rfs_rdfree},
	/* RFS_WRITECACHE = 7 *** NO LONGER SUPPORTED *** */
	{rfs_error, xdr_void, 0,
	    xdr_void, 0, nullfree},
	/* RFS_WRITE = 8 */
	{rfs_write, xdr_writeargs, sizeof(struct nfswriteargs),
	    xdr_attrstat, sizeof(struct nfsattrstat), nullfree},
	/* RFS_CREATE = 9 */
	{rfs_create, xdr_creatargs, sizeof(struct nfscreatargs),
	    xdr_diropres, sizeof(struct nfsdiropres), nullfree},
	/* RFS_REMOVE = 10 */
	{rfs_remove, xdr_diropargs, sizeof(struct nfsdiropargs), 
	    xdr_enum, sizeof(enum nfsstat), nullfree},
	/* RFS_RENAME = 11 */
	{rfs_rename, xdr_rnmargs, sizeof(struct nfsrnmargs), 
	    xdr_enum, sizeof(enum nfsstat), nullfree},
	/* RFS_LINK = 12 */
	{rfs_link, xdr_linkargs, sizeof(struct nfslinkargs), 
	    xdr_enum, sizeof(enum nfsstat), nullfree},
	/* RFS_SYMLINK = 13 */
	{rfs_symlink, xdr_slargs, sizeof(struct nfsslargs), 
	    xdr_enum, sizeof(enum nfsstat), nullfree},
	/* RFS_MKDIR = 14 */
	{rfs_mkdir, xdr_creatargs, sizeof(struct nfscreatargs),
	    xdr_diropres, sizeof(struct nfsdiropres), nullfree},
	/* RFS_RMDIR = 15 */
	{rfs_rmdir, xdr_diropargs, sizeof(struct nfsdiropargs), 
	    xdr_enum, sizeof(enum nfsstat), nullfree},
	/* RFS_READDIR = 16 */
	{rfs_readdir, xdr_rddirargs, sizeof(struct nfsrddirargs),
	    xdr_putrddirres, sizeof(struct nfsrddirres), rfs_rddirfree},
	/* RFS_STATFS = 17 */
	{rfs_statfs, xdr_fhandle, sizeof(fhandle_t),
	    xdr_statfs, sizeof(struct nfsstatfs), nullfree},
	}
};

int nobody = -2;

void
rfs_dispatch(req, xprt)
	struct svc_req *req;
	register SVCXPRT *xprt;
{
	int which;
	int vers;
	caddr_t	*args = NULL;
	caddr_t	*res = NULL;
	struct rfsdisp *disp;
	struct authunix_parms *aup;
	int *gp;
	struct ucred *tmpcr;
	struct ucred *newcr = NULL;

	svstat.ncalls++;
	which = req->rq_proc;
	vers = req->rq_vers;
	if (vers < VERSIONMIN || vers > VERSIONMAX) {
#ifdef NFSDEBUG
		dprint(nfsdebug, 2,
		    "rfs_dispatch: bad vers %d low %d high %d\n",
		    vers, VERSIONMIN, VERSIONMAX);
#endif
		svcerr_progvers(req->rq_xprt, (u_long)VERSIONMIN,
		    (u_long)VERSIONMAX);
		svstat.nbadcalls++;
		goto error;
	}
	vers -= VERSIONMIN;
	disp = &rfsdisptab[vers][which];
	svstat.reqs[which]++;

	/*
	 * Clean up as if a system call just started
	 */
	u.u_error = 0;

	/*
	 * Allocate args struct and deserialize into it.
	 */
	if (disp->dis_argsz) {
		args = (caddr_t *)kmem_alloc((u_int)disp->dis_argsz);
		bzero((caddr_t)args, (u_int)disp->dis_argsz);
	}
	if ( ! SVC_GETARGS(xprt, disp->dis_xdrargs, args)) {
		svcerr_decode(xprt);
		svstat.nbadcalls++;
		goto error;
	}

	/*
	 * Check for unix style credentials
	 */
	if (req->rq_cred.oa_flavor != AUTH_UNIX && which != RFS_NULL) {
		svcerr_weakauth(xprt);
		goto error;
	}

	/*
	 * Set uid, gid, and gids to auth params
	 */
	if (which != RFS_NULL) {
		aup = (struct authunix_parms *)req->rq_clntcred;
		newcr = crget();
		if (aup->aup_uid == 0) {
			/*
			 * root over the net becomes other on the server (uid -2)
			 */
			newcr->cr_uid = nobody;
		} else {
			newcr->cr_uid = aup->aup_uid;
		}
		newcr->cr_gid = aup->aup_gid;
		bcopy((caddr_t)aup->aup_gids, (caddr_t)newcr->cr_groups,
		    aup->aup_len * sizeof(newcr->cr_groups[0]));
		for (gp = &newcr->cr_groups[aup->aup_len];
		     gp < &newcr->cr_groups[NGROUPS];
		     gp++) {
			*gp = NOGROUP;
		}
		tmpcr = u.u_cred;
		u.u_cred = newcr;
	}

	/*
	 * Allocate results struct.
	 */
	if (disp->dis_ressz) {
		res = (caddr_t *)kmem_alloc((u_int)disp->dis_ressz);
		bzero((caddr_t)res, (u_int)disp->dis_ressz);
	}

	/*
	 * Call service routine with arg struct and results struct
	 */
	(*disp->dis_proc)(args, res);

	/*
	 * Serialize results struct
	 */
	if ( ! svc_sendreply(xprt, disp->dis_xdrres, (caddr_t)res) ) {
#ifdef NFSDEBUG
		dprint(nfsdebug, 1, "nfs: can't encode reply\n");
#endif
	}

error:
	/*
	 * Free arguments struct
	 */
	if (args != NULL) {
		if ( ! SVC_FREEARGS(xprt, disp->dis_xdrargs, args) ) {
#ifdef NFSDEBUG
			dprint(nfsdebug, 1, "nfs: can't free arguments\n");
#endif
		}
		kmem_free((caddr_t)args, (u_int)disp->dis_argsz);
	}
	/*
	 * Free results struct
	 */
	if (res != NULL) {
		if ( disp->dis_resfree != nullfree ) {
			(*disp->dis_resfree)(res);
		}
		kmem_free((caddr_t)res, (u_int)disp->dis_ressz);
	}
	/*
	 * restore original credentials
	 */
	if (newcr) {
		u.u_cred = tmpcr;
		crfree(newcr);
	}
}

geterrno(nfserr)
enum nfsstat nfserr;
{
	switch ((int) nfserr) {
		case NFS_OK:
			return (0);

		case NFSERR_PERM:
		case NFSERR_NOENT:
		case NFSERR_IO:
		case NFSERR_NXIO:
		case NFSERR_ACCES:
		case NFSERR_EXIST:
		case NFSERR_NODEV:
		case NFSERR_NOTDIR:
		case NFSERR_ISDIR:
		case NFSERR_FBIG:
		case NFSERR_NOSPC:
		case NFSERR_ROFS:
			return ((int) nfserr);

		/*
		 *	N.B.:	by coincidence, NFSERR_WFLUSH == 71 == Sun's
		 *		error number for EREMOTE.  Thus EREMOTE does
		 *		not appear in the Sun -> UniPlus+ error number
		 *		mapping below.
		 */
		case NFSERR_WFLUSH:
			return ((int) nfserr);

		case NFSERR_NAMETOOLONG:
			return (ENAMETOOLONG);

		case NFSERR_NOTEMPTY:
			return (ENOTEMPTY);

		case NFSERR_DQUOT:
			return (EDQUOT);

		case NFSERR_STALE:
			return (ESTALE);

		/*
		 *	This shouldn't be necessary, but Sun hasn't really
		 *	defined which errors may and may not be returned as
		 *	a result of a remote operation.  This makes it
		 *	necessary for us to map 4.2BSD error numbers into
		 *	our error numbers.
		 */
		case 3:
			return(ESRCH);

		case 4:
			return(EINTR);

		case 7:
			return(E2BIG);

		case 8:
			return(ENOEXEC);

		case 9:
			return(EBADF);

		case 10:
			return(ECHILD);

		case 11:
			return(EAGAIN);

		case 12:
			return(ENOMEM);

		case 14:
			return(EFAULT);

		case 15:
			return(ENOTBLK);

		case 16:
			return(EBUSY);

		case 18:
			return(EXDEV);

		case 22:
			return(EINVAL);

		case 23:
			return(ENFILE);

		case 24:
			return(EMFILE);

		case 25:
			return(ENOTTY);

		case 26:
			return(ETXTBSY);

		case 29:
			return(ESPIPE);

		case 31:
			return(EMLINK);

		case 32:
			return(EPIPE);

		case 33:
			return(EDOM);

		case 34:
			return(ERANGE);

		case 35:
			return(EWOULDBLOCK);

		case 36:
			return(EINPROGRESS);

		case 37:
			return(EALREADY);

		case 38:
			return(ENOTSOCK);

		case 39:
			return(EDESTADDRREQ);

		case 40:
			return(EMSGSIZE);

		case 41:
			return(EPROTOTYPE);

		case 42:
			return(ENOPROTOOPT);

		case 43:
			return(EPROTONOSUPPORT);

		case 44:
			return(ESOCKTNOSUPPORT);

		case 45:
			return(EOPNOTSUPP);

		case 46:
			return(EPFNOSUPPORT);

		case 47:
			return(EAFNOSUPPORT);

		case 48:
			return(EADDRINUSE);

		case 49:
			return(EADDRNOTAVAIL);

		case 50:
			return(ENETDOWN);

		case 51:
			return(ENETUNREACH);

		case 52:
			return(ENETRESET);

		case 53:
			return(ECONNABORTED);

		case 54:
			return(ECONNRESET);

		case 55:
			return(ENOBUFS);

		case 56:
			return(EISCONN);

		case 57:
			return(ENOTCONN);

		case 58:
			return(ESHUTDOWN);

		case 59:
			return(ETOOMANYREFS);

		case 60:
			return(ETIMEDOUT);

		case 61:
			return(ECONNREFUSED);

		case 62:
			return(ELOOP);

		case 64:
			return(EHOSTDOWN);

		case 65:
			return(EHOSTUNREACH);

		case 67:
			return(EPROCLIM);

		case 68:
			return(EUSERS);

		default:
			/*
			 *	The nfs_*() routines will call geterrno() if
			 *	rfscall() returns 0.  Rfscall() returns the
			 *	re_errno field from the rpc_err structure
			 *	after calling CLNT_GETERR() (clntkudp_error()
			 *	in our case) to read it out.  CLNT_CALL()
			 *	(clntkudp_callit() in our case) always sets
			 *	the re_status field, but for some errors
			 *	(specifically RPC_CANTENCODEARGS, RPC_CANTSEND,
			 *	and RPC_CANTRECV) the re_errno field is not set
			 *	and the results structure is not filled in.
			 *
			 *	The net result is that although the RPC call
			 *	failed, the higher level routines don't realize
			 *	that a failure has occurred, and call geterrno()
			 *	with garbage in nfserr.  We faithfully return
			 *	said garbage.  Sigh.
			 *
			 */
			return ((int) nfserr);
	}
}

enum nfsstat
puterrno(unixerr)
int unixerr;
{
	switch (unixerr) {
		case 0:
			return (NFS_OK);

		case EPERM:
		case ENOENT:
		case EIO:
		case ENXIO:
		case EACCES:
		case EEXIST:
		case ENODEV:
		case ENOTDIR:
		case EISDIR:
		case EFBIG:
		case ENOSPC:
		case EROFS:
			return ((enum nfsstat) unixerr);

		case ENAMETOOLONG:
			return (NFSERR_NAMETOOLONG);

		case ENOTEMPTY:
			return (NFSERR_NOTEMPTY);

		case EDQUOT:
			return (NFSERR_DQUOT);

		case ESTALE:
			return (NFSERR_STALE);

		default:
			printf("puterrno:  unmapped UNIX error %d\n", unixerr);
			return ((enum nfsstat) unixerr);
	}
}
