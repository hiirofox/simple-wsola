#pragma once

class ReSampler//������fftд
{
private:
	constexpr static int MaxInBufferSize = 65536;//һ��Ҫ�㹻��
	constexpr static int MaxOutBufferSize = 65536;
	float rate = 1.0;//ԭʼ�����ʣ�Ŀ�������

	float inbuf[MaxInBufferSize];
	int pos = 0, posHop = 0;
	float outbuf[MaxOutBufferSize];
	int writepos = 0;//дָ��
	int readpos = 0;//��ָ��

	constexpr static int MaxFFTLen = 32768;//���FFT����
	float windowin[MaxFFTLen];//���봰
	float windowout[MaxFFTLen];//�����
	float bufre[MaxFFTLen];
	float bufim[MaxFFTLen];
	int fftlenin = 2048;//Ҳ����blocksize
	int fftlenout = 2048;
	int hopsizein = fftlenin / 4;
	int hopsizeout = fftlenout / 4;
public:
	ReSampler()
	{
		memset(inbuf, 0, sizeof(inbuf));
		memset(outbuf, 0, sizeof(outbuf));
		memset(windowin, 0, sizeof(windowin));
		memset(windowout, 0, sizeof(windowout));
		memset(bufre, 0, sizeof(bufre));
		memset(bufim, 0, sizeof(bufim));
		for (int i = 0; i < fftlenin; ++i)
		{
			windowin[i] = 0.5 - 0.5 * cosf(2.0 * M_PI * i / fftlenin);
		}
		for (int i = 0; i < fftlenout; ++i)
		{
			windowout[i] = 0.5 - 0.5 * cosf(2.0 * M_PI * i / fftlenout);
		}
	}
	void SetRate(float rate)//src:dst
	{
		this->rate = rate;
		float n = (float)fftlenin / rate;//�������
		hopsizeout = n / 4;
		int m = 1 << (int)(ceilf(log2f(n)));
		if (m != fftlenout)
		{
			fftlenout = m;
			for (int i = 0; i < fftlenout; ++i)
			{
				windowout[i] = 0.5 - 0.5 * cosf(2.0 * M_PI * i / fftlenout);
			}
		}
	}
	void ProcessIn(const float* buf, int len)
	{
		for (int i = 0; i < len; ++i)
		{
			inbuf[pos] = buf[i];
			posHop++;
			if (posHop >= hopsizein)
			{
				posHop = 0;
				int start = MaxInBufferSize * 100 + pos - fftlenin;//���봰��߽�
				for (int j = 0; j < fftlenin; ++j)
				{
					bufre[j] = inbuf[(start + j) % MaxInBufferSize] * windowin[j];
					bufim[j] = 0.0f;
				}
				fft_f32(bufre, bufim, fftlenin, 1);//fft
				for (int i = fftlenin / 2; i < fftlenout; ++i)//ȥ������
				{
					bufre[i] = 0.0f;
					bufim[i] = 0.0f;
				}
				if (rate > 1.0)//����
				{
					for (int i = fftlenin / 2 / rate; i < fftlenin / 2; ++i)//Ŀ��Ĵ���
					{
						bufre[i] = 0.0f;
						bufim[i] = 0.0f;
					}
				}
				for (int i = 0; i < fftlenout / 2; ++i)//����
				{
					bufre[fftlenout - i - 1] = -bufre[i];//�Բ����ҵ�fft�����е�����
					bufim[fftlenout - i - 1] = bufim[i];
				}
				fft_f32(bufre, bufim, fftlenout, -1);//��fft
				for (int j = 0; j < fftlenout; ++j)
				{
					bufre[j] *= windowout[j];
				}

				float normEnergy = 1.0 / sqrtf(fftlenin * fftlenout);//��һ��

				int n = (float)fftlenin / rate;//�������

				float y0 = 0, y1 = 0, y2 = 0, y3 = 0;
				for (int j = 0, k = writepos; j < n; ++j)
				{
					const float p0 = 1.0 / 3.0;
					const float p1 = 1.0 / 2.0;
					const float p2 = 1.0 / 6.0;
					float indexf = (float)j * fftlenout / n;
					int index = indexf;
					if (index < 2)
					{
						y0 = 0;
						y1 = 0;
						y2 = bufre[0];
						y3 = bufre[1];
					}
					else
					{
						y0 = bufre[index - 2];
						y1 = bufre[index - 1];
						y2 = bufre[index + 0];
						y3 = bufre[index + 1];
					}
					float c0 = y1;
					float x = indexf - (int)indexf;
					float c1 = y2 - p0 * y0 - p1 * y1 - p2 * y3;
					float c2 = p1 * (y0 + y2) - y1;
					float c3 = p2 * (y3 - y0) + p1 * (y1 - y2);
					float out = ((c3 * x + c2) * x + c1) * x + c0;

					outbuf[k % MaxOutBufferSize] += out * normEnergy;
					k++;
				}
				writepos += hopsizeout;

			}
			pos = (pos + 1) % MaxInBufferSize;
		}
	}
	bool PrepareOut(int len)
	{
		return writepos - readpos > len + fftlenin;
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

/*float rate2 = (float)fftlenout / n;
				float t = 0;
				float normEnergy = 1.0 / sqrtf(fftlenin * n);
				float y0 = 0, y1 = 0, y2 = 0, y3 = 0;
				for (int j = 0, k = writepos; j < n; ++j)
				{
					float in = bufre[(int)t] * windowout[(int)t] * normEnergy;
					const float p0 = 1.0 / 3.0;
					const float p1 = 1.0 / 2.0;
					const float p2 = 1.0 / 6.0;
					y0 = y1;
					y1 = y2;
					y2 = y3;
					y3 = in;
					float x = t - (int)t;
					float c0 = y1;
					float c1 = y2 - p0 * y0 - p1 * y1 - p2 * y3;
					float c2 = p1 * (y0 + y2) - y1;
					float c3 = p2 * (y3 - y0) + p1 * (y1 - y2);
					float out = ((c3 * x + c2) * x + c1) * x + c0;

					outbuf[k % MaxOutBufferSize] += out;
					k++;
					t += rate2;
				}
				writepos += hopsizeout;*/