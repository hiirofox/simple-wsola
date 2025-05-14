#include "wav.h"
#include "wsola.h"

WavStream reader;
WavStream writer;

const int BufLen = 512;
float bufl[BufLen];
float bufr[BufLen];

WSOLA wsola_l;
WSOLA wsola_r;

ReSampler rsl;
ReSampler rsr;

WSOLA_PitchShifter psl;
WSOLA_PitchShifter psr;

void test1();
void test2();
void test3();
int main()
{
	reader.OpenWav("D:\\Projects\\c++\\WSOLA\\[06]Explorer.wav");
	writer.CreateWav("D:\\Projects\\c++\\WSOLA\\output.wav");
	//test1();
	//test2();
	test3();
	return 0;

}
void test1()
{
	wsola_l.SetTimeSkretch(10);
	wsola_r.SetTimeSkretch(10);
	int count = 0;
	while (!reader.EndOfWav())
	{
		reader.ReadBlock(bufl, bufr, BufLen);
		for (int i = 0; i < BufLen; ++i)
		{
			bufl[i] *= 0.005;
			bufr[i] *= 0.005;
		}
		wsola_l.ProcessIn(bufl, BufLen);
		wsola_r.ProcessIn(bufr, BufLen);

		while (wsola_l.PrepareOut(BufLen) && wsola_r.PrepareOut(BufLen))
		{
			wsola_l.ProcessOut(bufl, BufLen);
			wsola_r.ProcessOut(bufr, BufLen);
			for (int i = 0; i < BufLen; ++i)
			{
				bufl[i] /= 0.005;
				bufr[i] /= 0.005;
			}
			writer.WriteBlock(bufl, bufr, BufLen);
		}
	}
	memset(bufl, 0, BufLen * sizeof(float));//填充0
	memset(bufr, 0, BufLen * sizeof(float));//排空里面的数据
	for (int i = 0; i < 24; ++i)
	{
		wsola_l.ProcessIn(bufl, BufLen);
		wsola_r.ProcessIn(bufr, BufLen);
		while (wsola_l.PrepareOut(BufLen) && wsola_r.PrepareOut(BufLen))
		{
			wsola_l.ProcessOut(bufl, BufLen);
			wsola_r.ProcessOut(bufr, BufLen);
			writer.WriteBlock(bufl, bufr, BufLen);
		}
	}
	writer.SetSampleRate(48000);
	writer.EndOfWav();
}
void test2()
{
	rsl.SetRate(0.22);
	rsr.SetRate(0.22);

	int count = 0;
	while (!reader.EndOfWav())
	{
		reader.ReadBlock(bufl, bufr, BufLen);
		for (int i = 0; i < BufLen; ++i)
		{
			bufl[i] *= 0.005;
			bufr[i] *= 0.005;
		}
		rsl.ProcessIn(bufl, BufLen);
		rsr.ProcessIn(bufr, BufLen);

		while (rsl.PrepareOut(BufLen) && rsr.PrepareOut(BufLen))
		{
			rsl.ProcessOut(bufl, BufLen);
			rsr.ProcessOut(bufr, BufLen);
			for (int i = 0; i < BufLen; ++i)
			{
				bufl[i] /= 0.005;
				bufr[i] /= 0.005;
			}
			writer.WriteBlock(bufl, bufr, BufLen);
		}
	}
	memset(bufl, 0, BufLen * sizeof(float));//填充0
	memset(bufr, 0, BufLen * sizeof(float));//排空里面的数据
	for (int i = 0; i < 24; ++i)
	{
		rsl.ProcessIn(bufl, BufLen);
		rsr.ProcessIn(bufr, BufLen);
		while (rsl.PrepareOut(BufLen) && rsr.PrepareOut(BufLen))
		{
			rsl.ProcessOut(bufl, BufLen);
			rsr.ProcessOut(bufr, BufLen);
			writer.WriteBlock(bufl, bufr, BufLen);
		}
	}
	writer.SetSampleRate(48000);
	writer.EndOfWav();
}

void test3()
{
	psl.SetPitch(1);
	psr.SetPitch(1);
	while (!reader.EndOfWav())
	{
		reader.ReadBlock(bufl, bufr, BufLen);
		psl.ProcessBlock(bufl, bufl, BufLen);
		psr.ProcessBlock(bufr, bufr, BufLen);
		writer.WriteBlock(bufl, bufr, BufLen);
	}
	memset(bufl, 0, BufLen * sizeof(float));//填充0
	memset(bufr, 0, BufLen * sizeof(float));//排空里面的数据
	for (int i = 0; i < 20; ++i)
	{
		psl.ProcessBlock(bufl, bufl, BufLen);
		psr.ProcessBlock(bufr, bufr, BufLen);
		writer.WriteBlock(bufl, bufr, BufLen);
	}
	writer.SetSampleRate(48000);
	writer.EndOfWav();
}