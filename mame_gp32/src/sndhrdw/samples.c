#include "driver.h"



static int firstchannel,numchannels;


/* Start one of the samples loaded from disk. Note: channel must be in the range */
/* 0 .. Samplesinterface->channels-1. It is NOT the discrete channel to pass to */
/* osd_play_sample() */
void sample_start(int channel,int samplenum,int loop)
{
	if (Machine->sample_rate == 0) return;
	if (Machine->samples == 0) return;
	if (Machine->samples->sample[samplenum] == 0) return;
	if (channel >= numchannels)
	{
		return;
	}
	if (samplenum >= Machine->samples->total)
	{
		return;
	}

	if ( Machine->samples->sample[samplenum]->resolution == 8 )
	{
		osd_play_sample(channel + firstchannel,
				Machine->samples->sample[samplenum]->data,
				Machine->samples->sample[samplenum]->length,
				Machine->samples->sample[samplenum]->smpfreq,
				Machine->samples->sample[samplenum]->volume,
				loop);
	}
	else
	{
		osd_play_sample_16(channel + firstchannel,
				(short *) Machine->samples->sample[samplenum]->data,
				Machine->samples->sample[samplenum]->length,
				Machine->samples->sample[samplenum]->smpfreq,
				Machine->samples->sample[samplenum]->volume,
				loop);
	}
}

void sample_adjust(int channel,int freq,int volume)
{
	if (Machine->sample_rate == 0) return;
	if (Machine->samples == 0) return;
	if (channel >= numchannels)
	{
		return;
	}

	osd_adjust_sample(channel + firstchannel,freq,volume);
}

void sample_stop(int channel)
{
	if (Machine->sample_rate == 0) return;
	if (channel >= numchannels)
	{
		return;
	}

	osd_stop_sample(channel + firstchannel);
}

int sample_playing(int channel)
{
	if (Machine->sample_rate == 0) return 0;
	if (channel >= numchannels)
	{
		return 0;
	}

	return !osd_get_sample_status(channel + firstchannel);
}


int samples_sh_start(struct Samplesinterface *interface)
{
	numchannels = interface->channels;
	firstchannel = get_play_channels(numchannels);
	return 0;
}

void samples_sh_stop(void)
{
}

void samples_sh_update(void)
{
}
