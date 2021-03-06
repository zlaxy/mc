#include <config.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/fileloc.h"
#include "lib/tty/color.h"
#include "lib/util.h"
#include "mountlist.h"
#include "panel.h"
#include "midnight.h"
#include "lib/widget/dialog.h"
#include "lib/mcconfig.h"

#include "lib/widget/wtools.h"
#include "lib/widget.h"
#include "lib/search.h"

#include "cmd.h"
#include "lib/tty/tty.h"

#include "lib/vfs/path.h"

#include "selmnt.h"
#include "hotlist.h"
#include "../setup.h"

gboolean mc_cd_mountpoint_and_dir = TRUE;
gboolean mc_cd_mountpoint_and_dir_align = TRUE;

#define SELMNT_HOTKEY_SYSMBOLS "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

char *selmnt_filter;
char hotkeys_list[] = SELMNT_HOTKEY_SYSMBOLS;

#ifdef __BEOS__
struct mount_entry *read_file_system_list (int need_fs_type, int all_fs);

struct mount_entry *read_file_system_list (int need_fs_type, int all_fs)
{
//	int				i, fd;
//	char				*tp, dev[_POSIX_NAME_MAX], dir[_POSIX_PATH_MAX];

    static struct mount_entry	*me = NULL;

    if (me)
    {
	if (me->me_devname) free(me->me_devname);
	if (me->me_mountdir) free(me->me_mountdir);
	if (me->me_type) free(me->me_type);
	return (NULL);
    }
    else
	me = (struct mount_entry *)malloc(sizeof(struct mount_entry));

    me->me_devname = strdup("//");
    me->me_mountdir = strdup("//");
    me->me_type = strdup("unknown");

    return (me);
}
#endif /*__BEOS__*/

//------------------ functions

static unsigned char
get_hotkey (int n)
{
    int c = ' ', i, pos = -1;

    switch (n)
    {
	case -1:
	    break;
	case 0:
	    for (i = strlen (hotkeys_list); i >= 0; i--)
		if (hotkeys_list[i] != ' ')
		    pos = i;
	    if (pos < strlen (hotkeys_list))
	    {
		c = hotkeys_list[pos];
		hotkeys_list[pos] = ' ';
	    }
	    break;
	default:
	    for (i = 0; ((i < strlen (hotkeys_list)) && (hotkeys_list[i] != n)); i++);
	    if (i < strlen (hotkeys_list))
	    {
		c = n;
		hotkeys_list[i] = ' ';
	    }
	    else
		c = get_hotkey(0);
    }

    return c;
}

//------------------ show mounts list
static int show_mnt (Mountp *mntpoints, WPanel *panel)
{
    Mountp *mz, *mhi;
    int menu_lines, i, maxlen=5, count = 0;
    Listbox* listbox;
    mz = mntpoints;

    if (!mz)
	return 0;

    while (mz->prev)           /* goto first */
	mz = mz->prev;

    mhi = mz;

    while (mhi)
    {
	if ((i = ((mhi->mpoint) ? g_utf8_strlen (mhi->mpoint, 255) + ((mhi->mpoint[0] == '~') ? -1 : 1) : 0) +
		 (mc_cd_mountpoint_and_dir ? 5 : 3) +
		 ((mhi->path && mc_cd_mountpoint_and_dir) ? g_utf8_strlen(mhi->path, 255) + 1 : -2)) > maxlen)
	    maxlen = i;
	if (maxlen > panel->widget.cols-8)
	    maxlen = panel->widget.cols-8;
	count++;
	mhi = mhi->next;
    }

// count = number of elements
// maxlen = maximum long of mountpoint menu item name

    menu_lines = count;
    strcpy(hotkeys_list, SELMNT_HOTKEY_SYSMBOLS);

    /* Create listbox */
    listbox = create_listbox_window_centered (LINES/2, panel->widget.x + panel->widget.cols/2 - 2,
					      menu_lines, maxlen, " Mountpoints ", "Mountpoints");
    i=0;

    while (mz) {
	static char buffer[256], b1[256], b2[256];
	static char hotkey;
//	sprintf( buffer, "%s (%d/%d)", mz->mpoint, mz->total/1024, mz->avail/1024 ); //for example!
//	sprintf( buffer, "%s", mz->mpoint);

    if ((mz->mpoint[0] == '~') || (g_utf8_strlen(mz->mpoint, 255) == 0))
	if (g_utf8_strlen(mz->mpoint, 255) > 0)
	    {
	    mz->mpoint++[0];
	    hotkey = get_hotkey(mz->mpoint++[0]);
	    }
	else
	    hotkey = get_hotkey(-1);
    else
	hotkey = get_hotkey(0);

    if (mc_cd_mountpoint_and_dir && mz->path)
	if (mc_cd_mountpoint_and_dir_align)
	{
	    g_snprintf( b1, sizeof (b1), "%c %s", hotkey, mz->mpoint);
	    g_snprintf( b2, sizeof (b2), "%s", mz->path);
	    if ((int) (maxlen - g_utf8_strlen(b1, 255) - g_utf8_strlen(b2, 255) - 4) < 0)
		g_snprintf( buffer, sizeof (buffer), "%s [%s]", b1, b2);
	    else 
	    {
		static char spaces[256];
		memset (spaces, ' ' , maxlen - g_utf8_strlen(b1, 255) - g_utf8_strlen(b2, 255) - 4);
		spaces[maxlen - g_utf8_strlen(b1, 255) - g_utf8_strlen(b2, 255) - 4] = '\0';
		g_snprintf( buffer, sizeof (buffer), "%s%s[%s]", b1, spaces, b2);
	    }
	} 
	else 
	    g_snprintf( buffer, sizeof (buffer), "%c %s [%s]", hotkey, mz->mpoint, mz->path);
    else
	if (g_utf8_strlen(mz->mpoint, 255) != 0)
	    g_snprintf( buffer, sizeof (buffer), "%c %s", hotkey, mz->mpoint);
	else
	{
	    memset (buffer, '-', maxlen - 2);
	    buffer[maxlen-2] = '\0';

	    if (mz->name && selmnt_separators_name_show)
	    {
		int start_pos;

		g_snprintf(b2, sizeof (b2), mz->name);
		if ( ((g_utf8_strlen(buffer, 255)) - (g_utf8_strlen(mz->name, 255)) ) < 8)
		    b2[g_utf8_strlen(buffer, 255) - 8] = 0;

		g_snprintf(b1, sizeof (b1), " %s ", b2);

		if ((start_pos = (g_utf8_strlen(buffer, 255) - g_utf8_strlen(b1, 255)) / 2) < 0)
		    start_pos = 0;

		for (int i = start_pos; ((i - start_pos) < g_utf8_strlen(b1, 255) && (i < g_utf8_strlen(buffer, 255))); i++)
		    buffer[i] = b1[i - start_pos];
	    }
	}

	LISTBOX_APPEND_TEXT( listbox, hotkey, buffer, NULL, NULL );

	mz = mz->next;
	i++;
    }

    /* Select the default entry */
    listbox_select_entry( listbox->list, 0 );

    i = run_listbox( listbox );

// we must return dirnum
    return (i);

}

//------------------ init Mountp
// First - we need helper, that must identify our last dir in this mountpoint
// from dir_history

static gpointer path_from_history (WPanel *panel, char *mountpoint)
{
    GList *hd;

    hd = panel->dir_history;

    if ( !hd ) 
	return NULL;

    if ( ! strcmp (mountpoint,PATH_SEP_STR) ) 
	return NULL;

    while (hd->next)
	hd = hd->next;

    do {
	struct stat sb;
	if (strcmp(mountpoint, strdup(hd->data)))
	    if (!strncmp(mountpoint, strdup(hd->data), strlen(mountpoint)) &&
		!stat(hd->data, &sb))
		return hd->data;

	hd = hd->prev;
    }  while (hd->prev);

    return NULL;
}

Mountp *add_to_mountp_list ( Mountp *mounts )
{
    if (!mounts)
    {
	mounts = malloc (sizeof (Mountp)+1);
	memset (mounts, 0, sizeof (Mountp));
    }
    else
    {
	mounts->next = malloc (sizeof (Mountp)+1);
	memset (mounts->next, 0, sizeof (Mountp));

	mounts->next->prev = mounts;
	mounts = mounts->next;
    }

    return mounts;
}

Mountp *init_mountp ( WPanel *panel )
{
    Mountp *mounts = NULL;
    GSList *temp = NULL;
    struct fs_usage fs_use;

#ifdef ENABLE_VFS_SMB
    char *xsmb_profile;
    int xsmb_item = 0;
#endif /* ENABLE_VFS_SMB */
    char *xnet_profile;
    int xnet_item = 0;
    FILE *file = NULL;

    selmnt_filter = load_selmnt_filter ();

    temp = read_file_system_list ();

    while (temp)
    {
	if (mc_search(selmnt_filter, NULL, ((struct mount_entry *) (temp->data))->me_mountdir, MC_SEARCH_T_REGEX))
	{
	    temp = temp->next;
	    continue;
	}

	mounts = add_to_mountp_list (mounts);

	get_fs_usage (((struct mount_entry *) (temp->data))->me_mountdir, NULL, &fs_use);

//	mounts->type = temp->me_dev;
//	mounts->typename = temp->me_type;
	mounts->mpoint = strdup (((struct mount_entry *) (temp->data))->me_mountdir);
	mounts->path = mc_cd_mountpoint_and_dir ? path_from_history (panel, ((struct mount_entry *) (temp->data))->me_mountdir) : NULL;
	mounts->name = strdup (((struct mount_entry *) (temp->data))->me_devname);
	mounts->avail = getuid () ? fs_use.fsu_bavail/2 : fs_use.fsu_bfree/2;
	mounts->total = fs_use.fsu_blocks/2;

	temp = temp->next;
    }
    free (temp);

    if (selmnt_with_hotlist)
    {
	if (hotlist_file_name == NULL)
	    hotlist_file_name = mc_config_get_full_path (MC_HOTLIST_FILE);

	if (file = fopen(hotlist_file_name, "r"))
	{
	    char *line = NULL;
	    static char symbols[256], path[256], mpoint[256];
	    size_t linecap = 0;
	    ssize_t linelen;
	    int hotlist_items = 0;

	    while ((linelen = getline(&line, &linecap, file)) > 0)
		if ((strstr(line, "ENTRY")) && !(strstr(line, " ENTRY")) && (strstr(line, "URL")))
		{
		    if (hotlist_items == 0)
		    {
			if (mounts)
			{
			    mounts = add_to_mountp_list (mounts);

			    mounts->mpoint = "";
			    mounts->name = "Hotlist";
			    mounts->path = NULL;
			    mounts->avail = 0;
			    mounts->total = 0;
			}

			hotlist_items++;
		    }

		    sscanf(line, "%[^\"]\"%[^\"]\"%[^\"]\"%[^\"]\"", symbols, mpoint, symbols, path);

		    mounts = add_to_mountp_list (mounts);

		    mounts->mpoint = strdup (mpoint);
		    mounts->name = NULL;
		    mounts->path = strdup (path);
		    mounts->avail = 0;
		    mounts->total = 0;

		}
	    free(line);
	    fclose(file);
	}
    }

#ifdef ENABLE_VFS_SMB
    xsmb_profile = g_build_filename (mc_config_get_data_path (), XSMB_PROFILE,  (char *) NULL);
    if (file = fopen(xsmb_profile, "r"))
    {
	fclose(file);
	xsmb_item++;
    }
#endif /* ENABLE_VFS_SMB */

    xnet_profile = g_build_filename (mc_config_get_data_path (), XNET_PROFILE, (char *) NULL);
    if (file = fopen(xnet_profile, "r"))
    {
	fclose(file);
	xnet_item++;
    }

    if ((mounts) && (
#ifdef ENABLE_VFS_SMB
        xsmb_item+
#endif /* ENABLE_VFS_SMB */
        xnet_item )
        )
    {
	mounts = add_to_mountp_list (mounts);

	mounts->mpoint = "";
	mounts->name = "Network";
	mounts->path = NULL;
	mounts->avail = 0;
	mounts->total = 0;
    }

#ifdef ENABLE_VFS_SMB
    if (xsmb_item)
    {
	char *profile = 
	    g_strconcat (xsmb_profile, "/xsmb", VFS_PATH_URL_DELIMITER, (char *) NULL);

	mounts = add_to_mountp_list (mounts);

	mounts->mpoint = _("~sSamba Resources");
	mounts->name = NULL;
	mounts->path = g_strdup (profile);
	mounts->avail = 0;
	mounts->total = 0;

	g_free(profile);
    }
#endif /* ENABLE_VFS_SMB */

    if (xnet_item)
    {
	char *profile = 
	    g_strconcat (xnet_profile, "/xnet", VFS_PATH_URL_DELIMITER, (char *) NULL);

	mounts = add_to_mountp_list (mounts);

	mounts->mpoint = _("~nNetwork Resources");
	mounts->name = NULL;
	mounts->path = g_strdup (profile);
	mounts->avail = 0;
	mounts->total = 0;

	g_free(profile);
    }

    return mounts; /* must be freed! */
}


static int select_mountpoint( WPanel *panel )
{
    int i, inta;
    char *s = NULL;
    Mountp *mount_ls = init_mountp(panel);

/* HERE we WILL add some special functions, like fast ftp or network client call */

/*
Network link - netlink_cmd
FTP link - ftplink_cmd
*/

/* HERE we FINISH add some special functions */

    if (mount_ls)
    {
	if (selmnt_always_show || (mount_ls->prev || mount_ls->next))
	{
	    inta = show_mnt (mount_ls, panel);

	    if (inta != -1)
	    {
		while (mount_ls->prev)           /* goto first */
		    mount_ls = mount_ls->prev;

		for (i=0; i<inta; i++)
		    if (mount_ls->next)
			mount_ls = mount_ls->next;
// resulting dirname
		if ((mc_cd_mountpoint_and_dir && mount_ls->path) ||
			(mount_ls->mpoint[0] != '/'))
		{
		    s = mount_ls->path;
		}
		else
		{
		    if (mount_ls->mpoint)
			s = mount_ls->mpoint;
		}
	    }

	    if (s) /* execute CD */
	    {
		int r;
		vfs_path_t *cd_vpath;

		cd_vpath = vfs_path_from_str_flags (s, VPF_NO_CANON);
		r = do_panel_cd (panel, cd_vpath, cd_exact);
		vfs_path_free (cd_vpath);
	    }
	    else /* execute command */ 
	    {
	    }
	}
    }
    free (mount_ls);
    return 1;
}

//------------------ programming interfaces
void select_mnt_left ( void )
{
    switch_to_listing ( 0 );
    select_mountpoint ( left_panel );
}


void select_mnt_right ( void )
{
    switch_to_listing ( 1 );
    select_mountpoint ( right_panel );
}
