#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <string.h>

#include <X11/Xlib-xcb.h>
#include <xcb/xcb_atom.h>

#include <time.h>

void getmixerdata(char *);
void updaterootwindow(xcb_connection_t *, xcb_screen_t *, char *);
void getdatum(char *);

void
getmixerdata(char * buf)
{
	char *file;
	mixer_devinfo_t dinfo, *infos;
	mixer_ctrl_t *values;
	int fd, i, j, ndev, pos;


	if ((file = getenv("MIXERDEVICE")) == 0 || *file == '\0')
		file = "/dev/mixer";

	if ((fd = open(file, O_RDWR)) == -1)
                if ((fd = open(file, O_RDONLY)) == -1)
                        err(1, "%s", file);

	for (ndev = 0; ; ndev++) {
		dinfo.index = ndev;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &dinfo) <0)
			break;
	}

	if (!ndev)
		errx(1, "no mixer devices configured");

	if ((infos = calloc(ndev, sizeof *infos)) == NULL ||
	    (values = calloc(ndev, sizeof *values)) == NULL)
		err(1, "calloc()");

	for (i = 0; i < ndev; i++) {
		infos[i].index = i;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &infos[i]) < 0) {
			ndev--;
			i--;
			continue;	
		}
	}

	for (i = 0; i < ndev; i++) {
		if (infos[i].type != AUDIO_MIXER_CLASS &&
		    infos[i].prev == AUDIO_MIXER_LAST) {
			if (strcmp(infos[i].label.name, "master") == 0) {
				values[i].dev = i;
				values[i].type = infos[i].type;
				if (ioctl(fd, AUDIO_MIXER_READ, &values[i]) < 0)
					err(1, "AUDIO_MIXER_READ");
				snprintf(buf, 20, "// vol(%d,%d)", values[i].un.value.level[0], 
					values[i].un.value.level[1]);
			}
		}
	}

}

void
updaterootwindow(xcb_connection_t *c, xcb_screen_t *s, char *status)
{
	xcb_change_property (c,
		XCB_PROP_MODE_REPLACE,
		s->root,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING,
		8,
		strlen (status),
		status );
	xcb_flush (c);
}

void
getdatum(char *buf)
{
	time_t tval;
	struct tm *tp;

	time(&tval);
	tp = localtime(&tval);
	if (tp == NULL)
		errx(1, "conversion error");
	(void)strftime(buf, 20, "%F %R", tp);
}

int
main(void)
{
	char *mixerdata, *date, status[80];
	size_t len;
	xcb_connection_t *c;
	xcb_screen_t *screen;

	c = xcb_connect (NULL, NULL);
	screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;

	date = calloc(20, sizeof(char));
	mixerdata = calloc(10, sizeof(char));

	while (1) {
		len = 0;
		getmixerdata(mixerdata);
		getdatum(date);
		strlcpy(status, date, sizeof(status));
		strlcat(status, mixerdata, sizeof(status));

		updaterootwindow(c,screen,status);

		sleep(2);
	}
}
