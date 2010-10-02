enum
{
	Fmono		= 1,
	Fin		= 2,
	Fout		= 4,

	Vaudio		= 0,
	Vpcm,
	Vsynth,
	Vcd,
	Vline,
	Vmic,
	Vspeaker,
	Vtreb,
	Vbass,
	Vspeed,
	Nvol,
	
	MaxAudio = 4,
};

void	audiodevopen(int);
void	audiodevclose(int);
int	audiodevread(int, void*, int);
int	audiodevwrite(int, void*, int);
void	audiodevgetvol(int, int, int*, int*);
void	audiodevsetvol(int, int, int, int);
