# pkg-plugins-watchpkg

Plugin for FreeBSD pkg(8) that notifies a list of scripts about package changes.

## General Information

The _watchpkg_ plugin watches packages for `INSTALL`, `DEINSTALL` and `UPGRADE` events
and executes a set of scripts after `pkg install`, `pkg upgrade`, `pkg remove` or
`pkg autoremove` have finished.

The scripts will be called as:

	/path/to/script <pkg-name> <pkg-origin>

Example:

	/home/jack/bin/recompile-project.sh gcc9 lang/gcc9

## Configuring the plugin

The configuration file must be placed in _PLUGINS\_CONF\_DIR_ (see _pkg.conf(5)_), which
defaults to `${PREFIX}/etc/pkg`. The shipped `watchpkg.conf.sample` can be used as a starting
point:

	$ cp /path/to/pkg-plugins-watchpkg/watchpkg.conf.sample /usr/local/etc/pkg/watchpkg.conf

There are two options to be set:

Option    | Description
--------- | -----------
`SCRIPTS` | A list of scripts to be executed in the given order for changes in watched packages. Use of full qualified paths is recommended.
`PKGS`    | A list of package names or origins to watch. If the list is empty, all packages will be watched.

Example:

	SCRIPTS [
		"/home/jack/bin/recompile-project.sh",
	]
	PKGS [
		"gcc8",
		"lang/gcc9",
	]

## How to build the plugin?

In order to build the plugin enter into the plugin's directory and run make(1), e.g.:

	$ cd /path/to/pkg-plugins-watchpkg
	$ make
	
Once the plugin is built you can install it using the following command:

	$ make install 
	
The plugin will be installed as a shared library in `${PREFIX}/lib/pkg/template.so`. If _PKG\_PLUGINS\_DIR_
 (see _pkg.conf(5)_) points to a different location, adjust the Makefile or copy the shared library manually.

## Enabling the plugin

To use the plugin, tell pkgng about it by adding it to the _PLUGINS_ option in `${PREFIX}/etc/pkg.conf`
(_pkg.conf(5)_). Also make sure _PKG\_ENABLE\_PLUGINS_ is set to true:

	PKG_ENABLE_PLUGINS = true;
	PLUGINS [
		"watchpkg",
	]

## Testing the plugin

To test the plugin, first check that it is recongnized and
loaded by pkgng by executing the `pkg plugins` command:

	$ pkg plugins
	NAME       DESC                                          VERSION   
	watchpkg   Watch for package changes                     1.0.0     

If the plugin shows up correctly then you are good to go! :)

