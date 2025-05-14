#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include "fft.h"
#include "resample.h"

class WSOLA
{
private:
	constexpr static int MaxFFTLen = 16384;//FFT长度
	constexpr static int MaxInBufferSize = 65536;//一定要足够大
	constexpr static int MaxOutBufferSize = 65536;
	constexpr static int MaxBlockSize = 2048;//块大小
	constexpr static int MaxRange = 2048;//搜索范围
	int blockSize = MaxBlockSize;
	int hopSize = blockSize / 4;
	int range = MaxRange;//搜索范围

	float step = 1.0, globalStep = 1.0;//步长
	float stepsum = 0;

	float window[MaxBlockSize];
	float window2[MaxBlockSize + MaxRange];

	float inbuf[MaxInBufferSize];
	int pos = 0;
	float copybuf[MaxInBufferSize];
	float globalBuf[MaxInBufferSize];

	float outbuf[MaxOutBufferSize];
	int writepos = 0;//写指针
	int readpos = 0;//读指针
	int GetMaxCorrIndex1()//找最大相关(传统方法)
	{
		int index = 0;
		float max = -999999;
		for (int i = 0; i < range; ++i)
		{
			float sum = 0;
			for (int j = 0; j < blockSize; ++j)
			{
				sum += copybuf[i + j] * globalBuf[j];
			}
			if (sum > max)
			{
				max = sum;
				index = i;
			}
		}
		//printf("index:%5d\t\tmax:%5.8f\n", index, max);
		return index;
	}
	int FFTLen = 4096;
	float re1[MaxFFTLen];
	float im1[MaxFFTLen];
	float re2[MaxFFTLen];
	float im2[MaxFFTLen];
	int GetMaxCorrIndex2()//找最大相关(fft) 修好了
	{
		int len1 = range + blockSize;
		for (int i = 0; i < len1; ++i) {
			float w = window2[i];
			re1[i] = copybuf[i] * w * 0.001;
			im1[i] = 0.0f;
		}
		for (int i = len1; i < FFTLen; ++i) {
			re1[i] = 0.0f;
			im1[i] = 0.0f;
		}
		for (int i = 0; i < blockSize; ++i) {
			float w = window[i];
			re2[i] = globalBuf[i] * w * 0.001;
			im2[i] = 0.0f;
		}
		for (int i = blockSize; i < FFTLen; ++i) {
			re2[i] = 0.0f;
			im2[i] = 0.0f;
		}
		fft_f32(re1, im1, FFTLen, 1);
		fft_f32(re2, im2, FFTLen, 1);
		for (int i = 0; i < FFTLen; ++i) {
			float re1v = re1[i];
			float im1v = im1[i];
			float re2v = re2[i];
			float im2v = -im2[i];//这个得共轭
			re1[i] = re1v * re2v - im1v * im2v;
			im1[i] = re1v * im2v + im1v * re2v;
		}
		fft_f32(re1, im1, FFTLen, -1);
		int index = 0;
		float max = -9999999999;
		for (int i = 0; i < range; ++i) {
			float r = re1[i] * re1[i] + im1[i] * im1[i];
			if (r > max) {
				max = r;
				index = i;
			}
		}
		printf("index:%5d\t\tmax:%5.8f\n", index, max);
		return index;
	}
	int GetMaxCorrIndex3()//纯ola
	{
		return 0;
	}
public:
	WSOLA()
	{
		memset(inbuf, 0, sizeof(inbuf));
		memset(outbuf, 0, sizeof(outbuf));
		SetBlockRange(2048, 2048);
	}
	void SetBlockRange(int blockSize, int searchRange)
	{
		if (blockSize + range > MaxFFTLen)return;

		this->blockSize = blockSize;
		this->range = searchRange;
		for (int i = 0; i < blockSize; ++i)
		{
			window[i] = 0.5 - 0.5 * cosf(2.0 * M_PI * i / blockSize);
		}
		for (int i = 0; i < blockSize + range; ++i)
		{
			window2[i] = 0.5 - 0.5 * cosf(2.0 * M_PI * i / (blockSize + range));
		}
		hopSize = blockSize / 4;
		FFTLen = 1 << (int)(ceilf(log2f(blockSize + range)));
	}
	void SetTimeSkretch(float ratio)
	{
		globalStep = ratio;
	}
	void ProcessIn(const float* buf, int len)
	{
		for (int i = 0; i < len; ++i)
		{
			inbuf[pos] = buf[i];

			stepsum += step;
			if (stepsum >= hopSize)//该更新了
			{
				stepsum -= hopSize;
				int start = MaxInBufferSize * 100 + pos - range - hopSize - blockSize;
				for (int j = 0; j < range + blockSize; ++j)
				{
					copybuf[j] = inbuf[(start + j) % MaxInBufferSize];
				}

				int index = GetMaxCorrIndex2();//找与目标块的最大相关
				int start2 = start + index + hopSize;//跳步的目标块，标准wsola实现
				//int start2 = start + index;//不跳步的，测试一下可以实现固定音高保留共振峰
				for (int j = 0; j < blockSize; ++j)//更新目标块
				{
					//globalBuf[j] = inbuf[(start2 + j) % MaxInBufferSize] * window[j];
					globalBuf[j] = inbuf[(start2 + j) % MaxInBufferSize];
				}

				int start3 = start + index;
				float normv = hopSize / (blockSize / 2.0);//hop = 1/4
				for (int j = 0, k = writepos; j < blockSize; ++j)//叠加
				{
					outbuf[k % MaxOutBufferSize] += copybuf[j + index] * window[j] * normv;
					k++;
				}
				writepos += hopSize;
				step = globalStep;//等处理完后再更新
			}

			pos = (pos + 1) % MaxInBufferSize;
		}
	}
	int PrepareOut(int len)
	{
		int n = writepos - readpos - (len + blockSize);
		if (n < 0)return 0;
		return n;
	}
	void ProcessOut(float* buf, int len)
	{
		for (int i = 0; i < len; ++i)
		{
			int index = readpos % MaxOutBufferSize;
			buf[i] = outbuf[index];
			outbuf[index] = 0;
			readpos++;
		}
	}
};

class WSOLA_PitchShifter	//最终屎山
{
private:
	constexpr static int MaxBufSize = 65536;
	float buffer[MaxBufSize];

	WSOLA wsola;
	ReSampler rs;
	float pitch = 1.0;
public:
	void SetPitch(float pitch)
	{
		this->pitch = pitch;
		wsola.SetTimeSkretch(pitch);
		rs.SetRate(pitch);
	}
	void SetBlockRange(int blockSize, int searchRange)
	{
		wsola.SetBlockRange(blockSize, searchRange);
	}
	void ProcessBlock(const float* in, float* out, int numSamples)
	{
		if (pitch > 1.0)//这时wsola是拉长的，resample放前面
		{
			rs.ProcessIn(in, numSamples);
			int len2 = rs.PrepareOut(0);
			rs.ProcessOut(buffer, len2);//缩短
			wsola.ProcessIn(buffer, len2);
			if (wsola.PrepareOut(numSamples))
			{
				wsola.ProcessOut(out, numSamples);
			}
			else
			{
				for (int i = 0; i < numSamples; ++i)
				{
					out[i] = 0;
				}
			}
		}
		else //resample放后面
		{
			int len2 = (float)numSamples / pitch;
			wsola.ProcessIn(in, numSamples);
			len2 = wsola.PrepareOut(0);
			wsola.ProcessOut(buffer, len2);
			rs.ProcessIn(buffer, len2);
			if (rs.PrepareOut(numSamples))
			{
				rs.ProcessOut(out, numSamples);
			}
			else
			{
				for (int i = 0; i < numSamples; ++i)
				{
					out[i] = 0;
				}
			}
		}
	}
};