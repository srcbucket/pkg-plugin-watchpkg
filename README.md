# pkg-plugin-watchpkg

Plugin for pkg(8) to notify a list of scripts about package changes

## Update port for a new release

Edit the `Makefile` and adjust the following variables:

Variable      | Example   | Description
------------- | --------- | -----------
`DISTVERSION` | `1.0.0`   | Must correspond with the actual plugin version as shown in `pkg plugins`.
`GH_TAGNAME`  | `b6e7315` | It will usually be `${DISTVERSION}`, but if necessary it may refer to a GIT commit ID like in the example.

Change into the ports directory and run:

	$ rm -f distinfo
	# make makesum

This will (re)create the port's `distinfo` file.

## Test the port

See the [FreeBSD Porter's Handbook](https://www.freebsd.org/doc/en_US.ISO8859-1/books/porters-handbook/) for detailed information.

At least run:

	$ make package
	# make install

Also perform static analysis of the port:

	$ portlint

## Submit the changes

First, checkout the ports tree using subversion:

	$ svn co http://svn.freebsd.org/ports/head /tmp/ports

Next, copy your changed files into the temporary working copy:

	$ cd /path/to/pkg-plugin-watchpkg
	$ git checkout freebsd-port
	$ cp * /tmp/ports/ports-mgmt/pkg-plugin-watchpkg/

Then create a subversion diff of your changes:

	$ cd /tmp/ports
	$ svn diff ports-mgmt/pkg-plugin-watchpkg >/tmp/pkg-plugin-watchpkg.diff

At last, submit the new diff `/tmp/pkg-plugin-watchpkg.diff` using the [FreeBSD bug submission form](https://bugs.freebsd.org/submit/), as
outlined [here](https://www.freebsd.org/doc/en_US.ISO8859-1/books/porters-handbook/porting-submitting.html) in the FreeBSD Porter's Handbook.

## Known Issues

`fetch: ...: size of remote file is not known` messages appear when performing `make fetch`:

	=> srcbucket-pkg-plugin-watchpkg-1.0.0_GH0.tar.gz doesn't seem to exist in /usr/home/ms/ports/distfiles/.
	=> Attempting to fetch https://codeload.github.com/srcbucket/pkg-plugin-watchpkg/tar.gz/1.0.0?dummy=/srcbucket-pkg-plugin-watchpkg-1.0.0_GH0.tar.gz
	fetch: https://codeload.github.com/srcbucket/pkg-plugin-watchpkg/tar.gz/1.0.0?dummy=/srcbucket-pkg-plugin-watchpkg-1.0.0_GH0.tar.gz: size unknown
	fetch: https://codeload.github.com/srcbucket/pkg-plugin-watchpkg/tar.gz/1.0.0?dummy=/srcbucket-pkg-plugin-watchpkg-1.0.0_GH0.tar.gz: size of remote file is not known
	srcbucket-pkg-plugin-watchpkg-1.0.0_GH0.tar.gz        6245  B  141 MBps    00s

These seem to be normal with GitHub and can safely be ignored. There is no way to avoid them.

