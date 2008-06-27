/*
 * Linux and BSD
 */
#include	"u.h"
#include	<sys/ioctl.h>
#ifdef __linux__
#include	<linux/soundcard.h>
#else
#include	<sys/soundcard.h>
#endif
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"devaudio.h"

enum
{
	Channels = 2,
	Rate = 44100,
	Bits = 16,
};

typedef struct A A;
struct A
{
	int afd;
	int cfd;
	int speed;
};

static A a[MaxAudio];

void
audiodevopen(int dev)
{
	int t;
	int afd, cfd;
	ulong ul;
	char adev[40], mixer[40];
	
	a[dev].afd = -1;
	a[dev].cfd = -1;

	if(dev == 0){
		strcpy(adev, "/dev/dsp");
		strcpy(mixer, "/dev/mixer");
	}else{
		snprint(adev, sizeof adev, "/dev/dsp%d", dev);
		snprint(mixer, sizeof mixer, "/dev/mixer%d", dev);
	}

	afd = -1;
	cfd = -1;
	if((afd = open(adev, OWRITE)) < 0 || (cfd = open(mixer, ORDWR)) < 0)
		goto err;

	t = Bits;
	if(ioctl(afd, SNDCTL_DSP_SAMPLESIZE, &t) < 0)
		goto err;

	t = Channels-1;
	if(ioctl(afd, SNDCTL_DSP_STEREO, &t) < 0)
		goto err;
	
	ul = Rate;
	if(ioctl(afd, SNDCTL_DSP_SPEED, &ul) < 0)
		goto err;

	a[dev].afd = afd;
	a[dev].cfd = cfd;
	a[dev].speed = Rate;
	return;

err:
	if(afd >= 0)
		close(afd);
	afd = -1;
	oserror();
}

void
audiodevclose(int dev)
{
	close(a[dev].afd);
	close(a[dev].cfd);
	a[dev].afd = -1;
	a[dev].cfd = -1;
}

static struct {
	int id9;
	int id;
} names[] = {
	Vaudio,	SOUND_MIXER_VOLUME,
	Vpcm,	SOUND_MIXER_PCM,
	Vbass, 		SOUND_MIXER_BASS,
	Vtreb, 		SOUND_MIXER_TREBLE,
	Vline, 		SOUND_MIXER_LINE,
	Vpcm, 		SOUND_MIXER_PCM,
	Vsynth, 		SOUND_MIXER_SYNTH,
	Vcd, 		SOUND_MIXER_CD,
	Vmic, 		SOUND_MIXER_MIC,
	Vspeaker,	SOUND_MIXER_SPEAKER
//	"record", 		SOUND_MIXER_RECLEV,
//	"mix",		SOUND_MIXER_IMIX,
//	"pcm2",		SOUND_MIXER_ALTPCM,
//	"line1",		SOUND_MIXER_LINE1,
//	"line2",		SOUND_MIXER_LINE2,
//	"line3",		SOUND_MIXER_LINE3,
//	"digital1",	SOUND_MIXER_DIGITAL1,
//	"digital2",	SOUND_MIXER_DIGITAL2,
//	"digital3",	SOUND_MIXER_DIGITAL3,
//	"phonein",		SOUND_MIXER_PHONEIN,
//	"phoneout",		SOUND_MIXER_PHONEOUT,
//	"radio",		SOUND_MIXER_RADIO,
//	"video",		SOUND_MIXER_VIDEO,
//	"monitor",	SOUND_MIXER_MONITOR,
//	"igain",		SOUND_MIXER_IGAIN,
//	"ogain",		SOUND_MIXER_OGAIN,
};

static int
lookname(int id9)
{
	int i;
	
	for(i=0; i<nelem(names); i++)
		if(names[i].id9 == id9)
			return names[i].id;
	return -1;
}

void
audiodevsetvol(int dev, int what, int left, int right)
{
	int id;
	ulong x;
	int v;
	
	if(a[dev].cfd < 0)
		error("audio device not open");
	if(what == Vspeed){
		x = left;
		if(ioctl(a[dev].afd, SNDCTL_DSP_SPEED, &x) < 0)
			oserror();
		a[dev].speed = x;
		return;
	}
	if((id = lookname(what)) < 0)
		error("no such volume");
	v = left | (right<<8);
	if(ioctl(a[dev].cfd, MIXER_WRITE(id), &v) < 0)
		oserror();
}

void
audiodevgetvol(int dev, int what, int *left, int *right)
{
	int id;
	int v;
	
	if(a[dev].cfd < 0)
		error("audio device not open");
	if(what == Vspeed){
		*left = *right = a[dev].speed;
		return;
	}
	if((id = lookname(what)) < 0)
		error("no such volume");
	if(ioctl(a[dev].cfd, MIXER_READ(id), &v) < 0)
		oserror();
	*left = v&0xFF;
	*right = (v>>8)&0xFF;
}

int
audiodevwrite(int dev, void *v, int n)
{
	int m, tot;
	
	for(tot=0; tot<n; tot+=m)
		if((m = write(a[dev].afd, (uchar*)v+tot, n-tot)) <= 0)
			oserror();
	return tot;
}

int
audiodevread(int dev, void *v, int n)
{
	error("no reading");
	return -1;
}
