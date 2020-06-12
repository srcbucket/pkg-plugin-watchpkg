PREFIX?=	/usr/local
LIBDIR=		${PREFIX}/lib/pkg/
CONFDIR=	${PREFIX}/etc/pkg/
CONFNAME=	${PLUGIN_NAME}.conf.sample
SHLIB_DIR?=	${LIBDIR}/
SHLIB_NAME?=	${PLUGIN_NAME}.so
NO_OBJ=		yes

PLUGIN_NAME=	watchpkg
SRCS=		watchpkg.c

PKGFLAGS!=	pkgconf --cflags pkg
CFLAGS+=	${PKGFLAGS}

beforeinstall:
	${INSTALL} -d ${LIBDIR}
	${INSTALL} -d ${CONFDIR}
	${INSTALL} ${.CURDIR}/${CONFNAME} ${CONFDIR}/${CONFNAME}

.include <bsd.lib.mk>
