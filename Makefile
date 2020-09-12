# $FreeBSD$

PORTNAME=	pkg-plugin-watchpkg
DISTVERSION=	1.0.0
CATEGORIES=	ports-mgmt

MAINTAINER=	ms-freebsd-ports@stoffnet.at
COMMENT=	Plugin for pkg(8) to notify a list of scripts about package changes

LICENSE=	BSD3CLAUSE

BUILD_DEPENDS=	pkg>0:ports-mgmt/pkg \
		pkgconf>0:devel/pkgconf
RUN_DEPENDS=	pkg>0:ports-mgmt/pkg

PLIST_FILES=	lib/pkg/watchpkg.so etc/pkg/watchpkg.conf.sample

USE_GITHUB=	yes
GH_ACCOUNT=	srcbucket
GH_TAGNAME=	${DISTVERSION}

do-install:
	${MKDIR} ${STAGEDIR}${LOCALBASE}/lib/pkg/
	${MKDIR} ${STAGEDIR}${LOCALBASE}/etc/pkg/
	${INSTALL_LIB} ${WRKSRC}/watchpkg.so ${STAGEDIR}${LOCALBASE}/lib/pkg/
	${INSTALL_MAN} ${WRKSRC}/watchpkg.conf.sample ${STAGEDIR}${PREFIX}/etc/pkg/

.include <bsd.port.mk>
