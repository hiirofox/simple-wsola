#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string>

// WAV�ļ�ͷ�ṹ
typedef struct {
    char riff[4];          // "RIFF"��ʶ
    uint32_t fileSize;     // �ļ���С��ȥ8�ֽ�
    char wave[4];          // "WAVE"��ʶ
    char fmt[4];           // "fmt "��ʶ
    uint32_t fmtSize;      // fmt���С
    uint16_t audioFormat;  // ��Ƶ��ʽ (PCM = 1)
    uint16_t numChannels;  // ͨ����
    uint32_t sampleRate;   // ������
    uint32_t byteRate;     // ÿ���ֽ���
    uint16_t blockAlign;   // �����
    uint16_t bitsPerSample;// ÿ��������λ��
    char data[4];          // "data"��ʶ
    uint32_t dataSize;     // ���ݴ�С
} WAVHeader;

// WAV�ļ���������
class WavStream {
private:
    FILE* fileHandle;      // �ļ����
    WAVHeader header;      // WAV�ļ�ͷ
    bool isReading;        // �Ƿ��ڶ�ȡģʽ
    uint32_t dataPosition; // ���ݶ���ʼλ��
    uint32_t currentPos;   // ��ǰλ��
    bool endOfFile;        // �Ƿ񵽴��ļ�ĩβ

    // ��ʼ��WAVͷ
    void InitWavHeader(int sampleRate, int channels = 2, int bitsPerSample = 16) {
        memcpy(header.riff, "RIFF", 4);
        header.fileSize = 0; // ����Ϊ0����������
        memcpy(header.wave, "WAVE", 4);
        memcpy(header.fmt, "fmt ", 4);
        header.fmtSize = 16;
        header.audioFormat = 1; // PCM
        header.numChannels = channels;
        header.sampleRate = sampleRate;
        header.bitsPerSample = bitsPerSample;
        header.byteRate = sampleRate * channels * bitsPerSample / 8;
        header.blockAlign = channels * bitsPerSample / 8;
        memcpy(header.data, "data", 4);
        header.dataSize = 0; // ����Ϊ0����������
    }

    // ��ȡWAVͷ��Ϣ
    bool ReadWavHeader() {
        if (fread(&header, sizeof(WAVHeader), 1, fileHandle) != 1) {
            return false;
        }

        // ��֤WAVͷ
        if (memcmp(header.riff, "RIFF", 4) != 0 ||
            memcmp(header.wave, "WAVE", 4) != 0 ||
            memcmp(header.fmt, "fmt ", 4) != 0) {
            return false;
        }

        // ��¼���ݶ�λ��
        dataPosition = sizeof(WAVHeader);
        currentPos = dataPosition;

        return true;
    }

    // ��16λ����ת��Ϊfloat
    float Int16ToFloat(int16_t sample) {
        return sample / 32768.0f;
    }

    // ��floatת��Ϊ16λ����
    int16_t FloatToInt16(float sample) {
        // ������[-1.0, 1.0]��Χ��
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;

        return (int16_t)(sample * 32767.0f);
    }

public:
    WavStream() : fileHandle(NULL), isReading(false), dataPosition(0), currentPos(0), endOfFile(false) {
        memset(&header, 0, sizeof(WAVHeader));
    }

    ~WavStream() {
        if (fileHandle) {
            // �����д��ģʽ����Ҫ�����ļ�ͷ
            if (!isReading) {
                // ��ȡ�ļ���С
                fseek(fileHandle, 0, SEEK_END);
                long fileSize = ftell(fileHandle);

                // �����ļ�ͷ�еĴ�С��Ϣ
                header.fileSize = fileSize - 8;
                header.dataSize = fileSize - dataPosition;

                // д����º���ļ�ͷ
                fseek(fileHandle, 0, SEEK_SET);
                fwrite(&header, sizeof(WAVHeader), 1, fileHandle);
            }

            fclose(fileHandle);
            fileHandle = NULL;
        }
    }

    // ������WAV�ļ����ж�ȡ
    bool OpenWav(const char* fileName) {
        if (fileHandle) {
            fclose(fileHandle);
            fileHandle = NULL;
        }

        fileHandle = fopen(fileName, "rb");
        if (!fileHandle) {
            return false;
        }

        isReading = true;
        endOfFile = false;

        // ��ȡ����֤WAVͷ
        if (!ReadWavHeader()) {
            fclose(fileHandle);
            fileHandle = NULL;
            return false;
        }

        return true;
    }

    // �����µ�WAV�ļ�����д��
    bool CreateWav(const char* fileName, int sampleRate = 44100) {
        if (fileHandle) {
            fclose(fileHandle);
            fileHandle = NULL;
        }

        fileHandle = fopen(fileName, "wb");
        if (!fileHandle) {
            return false;
        }

        isReading = false;

        // ��ʼ��WAVͷ
        InitWavHeader(sampleRate);

        // д��WAVͷ
        fwrite(&header, sizeof(WAVHeader), 1, fileHandle);
        dataPosition = sizeof(WAVHeader);
        currentPos = dataPosition;

        return true;
    }

    // ��ȡ������
    int GetSampleRate() {
        if (!fileHandle) {
            return 0;
        }

        return header.sampleRate;
    }

    // ���ò����ʣ����ڴ���ģʽ����Ч��
    bool SetSampleRate(int sampleRate) {
        if (!fileHandle || isReading) {
            return false;
        }

        header.sampleRate = sampleRate;
        header.byteRate = sampleRate * header.numChannels * header.bitsPerSample / 8;

        // �����ļ�ͷ
        long currentPosition = ftell(fileHandle);
        fseek(fileHandle, 0, SEEK_SET);
        fwrite(&header, sizeof(WAVHeader), 1, fileHandle);
        fseek(fileHandle, currentPosition, SEEK_SET);

        return true;
    }

    // ��ȡ��Ƶ��
    int ReadBlock(float* input_l, float* input_r, int numSamples) {
        if (!fileHandle || !isReading || endOfFile) {
            return 0;
        }

        int16_t* buffer = (int16_t*)malloc(numSamples * header.numChannels * sizeof(int16_t));
        if (!buffer) {
            return 0;
        }

        size_t samplesRead = fread(buffer, sizeof(int16_t) * header.numChannels, numSamples, fileHandle);
        currentPos += samplesRead * sizeof(int16_t) * header.numChannels;

        // ת������
        for (size_t i = 0; i < samplesRead; i++) {
            // ������
            input_l[i] = Int16ToFloat(buffer[i * header.numChannels]);

            // ������������ǵ��������������������ݣ�
            if (header.numChannels > 1) {
                input_r[i] = Int16ToFloat(buffer[i * header.numChannels + 1]);
            }
            else {
                input_r[i] = input_l[i];
            }
        }

        free(buffer);

        // ����Ƿ��ѵ����ļ�ĩβ
        if (samplesRead < numSamples) {
            endOfFile = true;
        }

        return samplesRead;
    }

    // д����Ƶ��
    bool WriteBlock(const float* output_l, const float* output_r, int numSamples) {
        if (!fileHandle || isReading) {
            return false;
        }

        int16_t* buffer = (int16_t*)malloc(numSamples * header.numChannels * sizeof(int16_t));
        if (!buffer) {
            return false;
        }

        // ת������
        for (int i = 0; i < numSamples; i++) {
            // ������
            buffer[i * header.numChannels] = FloatToInt16(output_l[i]);

            // ������������ǵ���������������������ݣ�
            if (header.numChannels > 1) {
                buffer[i * header.numChannels + 1] = FloatToInt16(output_r[i]);
            }
        }

        size_t written = fwrite(buffer, sizeof(int16_t) * header.numChannels, numSamples, fileHandle);
        currentPos += written * sizeof(int16_t) * header.numChannels;

        free(buffer);

        return (written == numSamples);
    }

    // ����Ƿ񵽴��ļ�ĩβ
    bool EndOfWav() {
        return endOfFile;
    }
};
