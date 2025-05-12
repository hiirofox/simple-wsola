#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string>

// WAV文件头结构
typedef struct {
    char riff[4];          // "RIFF"标识
    uint32_t fileSize;     // 文件大小减去8字节
    char wave[4];          // "WAVE"标识
    char fmt[4];           // "fmt "标识
    uint32_t fmtSize;      // fmt块大小
    uint16_t audioFormat;  // 音频格式 (PCM = 1)
    uint16_t numChannels;  // 通道数
    uint32_t sampleRate;   // 采样率
    uint32_t byteRate;     // 每秒字节数
    uint16_t blockAlign;   // 块对齐
    uint16_t bitsPerSample;// 每个样本的位数
    char data[4];          // "data"标识
    uint32_t dataSize;     // 数据大小
} WAVHeader;

// WAV文件流处理类
class WavStream {
private:
    FILE* fileHandle;      // 文件句柄
    WAVHeader header;      // WAV文件头
    bool isReading;        // 是否处于读取模式
    uint32_t dataPosition; // 数据段起始位置
    uint32_t currentPos;   // 当前位置
    bool endOfFile;        // 是否到达文件末尾

    // 初始化WAV头
    void InitWavHeader(int sampleRate, int channels = 2, int bitsPerSample = 16) {
        memcpy(header.riff, "RIFF", 4);
        header.fileSize = 0; // 先设为0，后续更新
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
        header.dataSize = 0; // 先设为0，后续更新
    }

    // 读取WAV头信息
    bool ReadWavHeader() {
        if (fread(&header, sizeof(WAVHeader), 1, fileHandle) != 1) {
            return false;
        }

        // 验证WAV头
        if (memcmp(header.riff, "RIFF", 4) != 0 ||
            memcmp(header.wave, "WAVE", 4) != 0 ||
            memcmp(header.fmt, "fmt ", 4) != 0) {
            return false;
        }

        // 记录数据段位置
        dataPosition = sizeof(WAVHeader);
        currentPos = dataPosition;

        return true;
    }

    // 将16位整型转换为float
    float Int16ToFloat(int16_t sample) {
        return sample / 32768.0f;
    }

    // 将float转换为16位整型
    int16_t FloatToInt16(float sample) {
        // 限制在[-1.0, 1.0]范围内
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
            // 如果是写入模式，需要更新文件头
            if (!isReading) {
                // 获取文件大小
                fseek(fileHandle, 0, SEEK_END);
                long fileSize = ftell(fileHandle);

                // 更新文件头中的大小信息
                header.fileSize = fileSize - 8;
                header.dataSize = fileSize - dataPosition;

                // 写入更新后的文件头
                fseek(fileHandle, 0, SEEK_SET);
                fwrite(&header, sizeof(WAVHeader), 1, fileHandle);
            }

            fclose(fileHandle);
            fileHandle = NULL;
        }
    }

    // 打开现有WAV文件进行读取
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

        // 读取并验证WAV头
        if (!ReadWavHeader()) {
            fclose(fileHandle);
            fileHandle = NULL;
            return false;
        }

        return true;
    }

    // 创建新的WAV文件进行写入
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

        // 初始化WAV头
        InitWavHeader(sampleRate);

        // 写入WAV头
        fwrite(&header, sizeof(WAVHeader), 1, fileHandle);
        dataPosition = sizeof(WAVHeader);
        currentPos = dataPosition;

        return true;
    }

    // 获取采样率
    int GetSampleRate() {
        if (!fileHandle) {
            return 0;
        }

        return header.sampleRate;
    }

    // 设置采样率（仅在创建模式下有效）
    bool SetSampleRate(int sampleRate) {
        if (!fileHandle || isReading) {
            return false;
        }

        header.sampleRate = sampleRate;
        header.byteRate = sampleRate * header.numChannels * header.bitsPerSample / 8;

        // 更新文件头
        long currentPosition = ftell(fileHandle);
        fseek(fileHandle, 0, SEEK_SET);
        fwrite(&header, sizeof(WAVHeader), 1, fileHandle);
        fseek(fileHandle, currentPosition, SEEK_SET);

        return true;
    }

    // 读取音频块
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

        // 转换数据
        for (size_t i = 0; i < samplesRead; i++) {
            // 左声道
            input_l[i] = Int16ToFloat(buffer[i * header.numChannels]);

            // 右声道（如果是单声道，则复制左声道数据）
            if (header.numChannels > 1) {
                input_r[i] = Int16ToFloat(buffer[i * header.numChannels + 1]);
            }
            else {
                input_r[i] = input_l[i];
            }
        }

        free(buffer);

        // 检查是否已到达文件末尾
        if (samplesRead < numSamples) {
            endOfFile = true;
        }

        return samplesRead;
    }

    // 写入音频块
    bool WriteBlock(const float* output_l, const float* output_r, int numSamples) {
        if (!fileHandle || isReading) {
            return false;
        }

        int16_t* buffer = (int16_t*)malloc(numSamples * header.numChannels * sizeof(int16_t));
        if (!buffer) {
            return false;
        }

        // 转换数据
        for (int i = 0; i < numSamples; i++) {
            // 左声道
            buffer[i * header.numChannels] = FloatToInt16(output_l[i]);

            // 右声道（如果是单声道，则忽略右声道数据）
            if (header.numChannels > 1) {
                buffer[i * header.numChannels + 1] = FloatToInt16(output_r[i]);
            }
        }

        size_t written = fwrite(buffer, sizeof(int16_t) * header.numChannels, numSamples, fileHandle);
        currentPos += written * sizeof(int16_t) * header.numChannels;

        free(buffer);

        return (written == numSamples);
    }

    // 检查是否到达文件末尾
    bool EndOfWav() {
        return endOfFile;
    }
};
