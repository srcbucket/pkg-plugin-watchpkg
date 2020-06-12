/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2020, Markus Stoff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Include other headers if needed */
#include <sys/types.h>
#include <sys/wait.h>

#include <spawn.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>

/* Include pkg */
#include <pkg.h>

extern char **environ;


/* Define plugin name and configuration settings */
static const char PLUGIN_NAME[] = "watchpkg";
static const char PLUGIN_DESCRIPTION[] = "Watch for package changes";
static const char PLUGIN_VERSION[] = "1.0.1";

static const char CFG_SCRIPTS[] = "SCRIPTS";
static const char CFG_PKGS[] = "PKGS";


/* Linked list of strings */
typedef struct list {
	char *value;
	struct list *next;
} list_t;

list_t * list_append(list_t *next, const char *value);
void list_free(list_t *list);
bool list_contains(list_t *list, const char *value);


/* Linked list of notifications */
typedef struct notification {
	char *name;
	char *origin;
	struct notification *next;
} notification_t;

notification_t * notifications_insert(notification_t *next, const char *name, const char *origin);
void notifications_free(notification_t *notification);


/* Maintain a reference to ourself */
static struct pkg_plugin *self = NULL;

/* Global Objects (required to maintain state across callbacks) */
static list_t *cfg_scripts = NULL;
static list_t *cfg_pkgs = NULL;
static notification_t *pkg_notifications = NULL;

/* Callbacks */
int notify_package_changes(void *data, struct pkgdb *db);
int collect_package_changes(void *data, struct pkgdb *db);


/* Helper functions */
bool call_script(char *script, const char *pkg_name, const char *pkg_origin);
list_t * read_list_from_config (const pkg_object *cfg, const char *key);
char * strclone(const char *src);


/*
 * Initialize plugin
 *
 * Called by pkg(8).
 *
 * Register plugin metadata, parse configuration and register plugin callbacks.
 *
 * Returns EPKG_OK on success or EPKG_FATAL if errors occured.
 */
int
pkg_plugin_init(struct pkg_plugin *p)
{
	self = p;

	pkg_plugin_set(p, PKG_PLUGIN_NAME, PLUGIN_NAME);
	pkg_plugin_set(p, PKG_PLUGIN_DESC, PLUGIN_DESCRIPTION);
	pkg_plugin_set(p, PKG_PLUGIN_VERSION, PLUGIN_VERSION);

	pkg_plugin_conf_add(p, PKG_ARRAY, CFG_SCRIPTS, "");
	pkg_plugin_conf_add(p, PKG_ARRAY, CFG_PKGS, "");

	/* Parse configuration */
	pkg_plugin_parse(p);

	const pkg_object *scripts_list = NULL;
	const pkg_object *script_path = NULL;
	const pkg_object *pkgs_list= NULL;
	const pkg_object *pkg_name_or_origin = NULL;
	const pkg_object *cfg = NULL;
	pkg_iter it = NULL;

	cfg = pkg_plugin_conf(self);

	/*
	 * pkg(8) provides no access to configuration options.
	 *
	 * To avoid confusing output on periodic(8) output, we remain silent but
	 * return with error.
	 *
	 * May be removed once a fixed version of pkg(8) is available.
	 */
	if (pkg_object_type(cfg) != PKG_OBJECT) {
		return (EPKG_FATAL);
	}

	/* Read list of SCRIPTS to be called for changes in the given PKGS */
	cfg_scripts = read_list_from_config(cfg, CFG_SCRIPTS);
	/* Read list of PKGS to watch for changes */
	cfg_pkgs = read_list_from_config(cfg, CFG_PKGS);

	/* Without SCRIPTS, there is nothing to do */
	if (cfg_scripts == NULL) {
		pkg_plugin_info(p, "WARNING: No scripts configured. Nothing to do.");
		return (EPKG_OK);
	}

	/* Register callbacks */
	if (	   (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_EVENT,           &collect_package_changes) != EPKG_OK)
		|| (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_POST_INSTALL,    &notify_package_changes ) != EPKG_OK)
		|| (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_POST_DEINSTALL,  &notify_package_changes ) != EPKG_OK)
		|| (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_POST_UPGRADE,    &notify_package_changes ) != EPKG_OK)
		|| (pkg_plugin_hook_register(p, PKG_PLUGIN_HOOK_POST_AUTOREMOVE, &notify_package_changes ) != EPKG_OK)
	) {
		pkg_plugin_error(p, "failed to hook into the library");
		return (EPKG_FATAL);
	}
	
	return (EPKG_OK);
}

/*
 * Release allocated resources
 *
 * Called by pkg(8).
 *
 * Returns EPKG_OK
 */
int
pkg_plugin_shutdown(struct pkg_plugin *p __unused)
{
	notifications_free(pkg_notifications);
	list_free(cfg_pkgs);
	list_free(cfg_scripts);

	return (EPKG_OK);
}

/*
 * PKG_PLUGIN_HOOK_POST_INSTALL, PKG_PLUGIN_HOOK_POST_DEINSTALL,
 * PKG_PLUGIN_HOOK_POST_UPGRADE and PKG_PLUGIN_HOOK_POST_AUTOREMOVE callback
 * function
 *
 * Invoke every script in cfg_scripts for every package change registered in
 * pkg_notifications. The invokation order is the same as the iteration order of
 * cfg_scripts.
 *
 * Returns EPKG_FATAL if any script returned with an return code other than
 * zero. Otherwise EPKG_OK is returned.
 */
int
notify_package_changes(void *data, struct pkgdb *db)
{
	int rc = EPKG_OK;

	for (list_t *cur_script = cfg_scripts; cur_script != NULL; cur_script = cur_script->next) {
		for (notification_t *cur_pkg = pkg_notifications; cur_pkg != NULL; cur_pkg = cur_pkg->next) {
			if (! call_script(cur_script->value, cur_pkg->name, cur_pkg->origin)) {
				/* Don't return immediately, process as many change notifications as possible */
				rc = EPKG_FATAL;
			}
		}
	}

	return rc;
}

/*
 * PKG_PLUGIN_HOOK_EVENT callback function
 *
 * Register package name and origin of PKG_EVENT_INSTALL_FINISHED,
 * PKG_EVENT_DEINSTALL_FINISHED and PKG_EVENT_UPGRADE_FINISHED events in
 * pkg_notifications. For upgrade events, the old package name and origin are
 * registered.
 *
 * Returns EPKG_OK
 */
int
collect_package_changes(void *data, struct pkgdb *db)
{
	const struct pkg_event *evt = (const struct pkg_event *)data;
	const struct pkg *pkg = NULL;
	const char *name = NULL;
	const char *origin = NULL;

	if (evt != NULL) {
		switch (evt->type) {
			case PKG_EVENT_INSTALL_FINISHED:
				pkg = evt->e_install_finished.pkg;
				break;
			case PKG_EVENT_DEINSTALL_FINISHED:
				pkg = evt->e_deinstall_finished.pkg;
				break;
			case PKG_EVENT_UPGRADE_FINISHED:
				pkg = evt->e_upgrade_finished.o;
				break;
			default:
				break;
		}

		if (pkg != NULL) {
			if (pkg_get(pkg, PKG_NAME, &name, PKG_ORIGIN, &origin) == EPKG_OK) {
				/*
				 * If PKGS is empty, notify about all package changes,
				 * otherwise notify only about changes in given packages
				 */
				if ((cfg_pkgs == NULL)
						|| list_contains(cfg_pkgs, name)
						|| list_contains(cfg_pkgs, origin))
				{
					pkg_notifications = notifications_insert(pkg_notifications, name, origin);
				}
			}
		}
	}

	return (EPKG_OK);
}


/*
 * Run script via posix_spawn and append pkg_name and pkg_origin as arguments
 *
 * Returns true on success and false if errors occured.
 */
bool call_script(char *script, const char *pkg_name, const char *pkg_origin) {
	int error = 0;
	int pstat = 0;
	pid_t pid;

	char msg[MAXPATHLEN];
	snprintf(msg, sizeof(msg), "\"%s\", \"%s\"", pkg_name, pkg_origin);

	const char *argv[] = {
		script,
		pkg_name,
		pkg_origin,
		NULL
	};

	if ((error = posix_spawn(&pid, script, NULL, NULL,
	    __DECONST(char **, argv), environ)) != 0) {
		errno = error;
		pkg_plugin_errno(self, script, msg);
		return false;
	}
	while (waitpid(pid, &pstat, 0) == -1) {
		if (errno != EINTR)
			return false;
	}

	if (WEXITSTATUS(pstat) != 0) {
		pkg_plugin_error(self, "\"%s\" returned with error for: %s", script, msg);
		return false;
	}

	return true;
}

/*
 * Read key as PKG_ARRAY from cfg into a linked list of unique strings
 *
 * Returns a linked list of unique strings. The iteration order of the list
 * matches the sequence in the configuration file.
 */
list_t * read_list_from_config (const pkg_object *cfg, const char *key) {
	list_t *result = NULL;
	const pkg_object *list = NULL;
	const pkg_object *item = NULL;
	const char *item_v = NULL;
	pkg_iter it = NULL;

	list = pkg_object_find(cfg, key);
	while ((item = pkg_object_iterate(list, &it)) != NULL) {
		/*
		 * The empty string ("") is parsed as NULL by UCL
		 * Empty strings are silently ignored
		 */
		if ((item_v = pkg_object_string(item)) != NULL) {
			/* Don't add duplicates */
			if (!list_contains(result, item_v))
				result = list_append(result, item_v);
		}
	}

	return result;
}


/*
 * Clone given string
 *
 * If src is NULL, NULL is returned. Otherwise strlen(src) + 1 char of memory
 * are allocated via malloc(), src is copied into the new string and the new
 * string is returned.
 */
char * strclone(const char *src) {
	char *dst = NULL;
	if (src != NULL) {
		size_t size = strlen(src) + 1;
		dst = (char *) malloc(size * sizeof(char));
		memcpy(dst, src, size);
	}
	return dst;
}

/*
 * Append a value to a linked list
 *
 * If list is NULL, a new list is created and a pointer to it is returned.
 * Otherwise the head remains unchanged and list is returned.
 *
 * While this is not the most efficient implementation, it has the fewest
 * moving parts. No pointer to the tail of the list needs to be maintained.
 * Performance is not important.
 */
list_t * list_append(list_t *list, const char *value) {
	/* Create new list element */
	list_t *new_element = (list_t *) malloc(sizeof(list_t));
	new_element->value = strclone(value);
	new_element->next = NULL;

	/* Return element as new head of list */
	if (list == NULL)
		return new_element;

	/* Get the last list element and append the new value */
	list_t *last = list;
	for (last = list; last->next != NULL; last = last->next) ;
	last->next = new_element;

	/* The head of an existing list remains unchanged on append operations */
	return list;
}

/*
 * free() all elements of given list
 */
void list_free(list_t *list) {
	list_t *next = list;
	list_t *cur = NULL;
	while ((cur = next) != NULL) {
		next = next->next;
		free(cur->value);
		free(cur);
	}
}

/*
 * Return true if list contains value
 */
bool list_contains(list_t *list, const char *value) {
	for (list_t *cur = list; cur != NULL; cur = cur->next) {
		if (cur->value != NULL) {
			if (strcmp(cur->value, value) == 0)
				return true;
		}
		else if (value == NULL) {
			return true;
		}
	}
	return false;
}

/*
 * Add new package name and origin to pkg_notifications
 *
 * A new notification is allocated via malloc() and returned as the new head
 * of the linked list.
 */
notification_t * notifications_insert(notification_t *notifications, const char *name, const char *origin) {
	notification_t *new_notification = NULL;
	new_notification = (notification_t *) malloc(sizeof(notification_t));
	new_notification->name = strclone(name);
	new_notification->origin = strclone(origin);
	new_notification->next = notifications;

	return new_notification;
}

/*
 * free() all notifications
 */
void notifications_free(notification_t *notification) {
	notification_t *next = notification;
	notification_t *cur = NULL;
	while ((cur = next) != NULL) {
		next = next->next;
		free(cur->name);
		free(cur->origin);
		free(cur);
	}
}

