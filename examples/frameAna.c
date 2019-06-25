#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <straight/straight.h>
#include <straight/straightFrames.h>

#if defined(_DEBUG) && !defined(DEBUG)
#define DEBUG
#endif

char inputFileName[256] = "";
char outputFileName[256] = "";

int main(int argc, char *argv[])
{
    StraightConfig config;
    Straight straight;
    StraightSource source;
    StraightSpecgram specgram;
    StraightFile sf;
    StraightInputFrames inputFrames;
    double *inputWave;
    long inputWaveLength;
    char chunkList[128];

    if (argc < 3) {
        fprintf(stderr, "frameAna <input audio file> <output STF file>\n");
        exit(1);
    }

    strcpy(inputFileName, argv[1]);
    strcpy(outputFileName, argv[2]);

#ifdef USE_TANDEM
#ifdef DEBUG
    fprintf(stderr, "DEBUG: use TANDEM-STRAIGHT\n");
#endif
    straightInitConfigTandem(&config);
#else
#ifdef DEBUG
    fprintf(stderr, "DEBUG: use Legacy-STRAIGHT\n");
#endif
    straightInitConfig(&config);
    strcpy(chunkList, "F0  AP  SPEC");
#endif
    config.samplingFrequency = 16000.0;

    if ((straight = straightInitialize(&config)) != NULL) {
        if (straightReadAudioFile(straight, inputFileName, NULL) == ST_FALSE) {
            fprintf(stderr, "Error: Cannot open input audio file: %s\n", inputFileName);
        } else {
            fprintf(stderr, "Input audio file: %s\n", inputFileName);

            inputWave = straightGetCurrentWave(straight, &inputWaveLength);
        
            if ((inputFrames = straightInputFramesInitialize(straight, NULL, NULL, NULL)) != NULL) {
                double positionSec;
                long position;
                long pushLength;
                long defaultPushLength;
                long numFrames;
                long frameOffset;
                long sourceFrameOffset, sourceUpdatedFrames, sourcePopLength;
                long specgramFrameOffset, specgramUpdatedFrames, specgramPopLength;
                long flushLength;

                defaultPushLength = straightGetFrameLengthInPoint(straight)/* / 4*/;
                numFrames = inputWaveLength / straightGetFrameShiftInPoint(straight);
#ifdef DEBUG
                fprintf(stderr, "DEBUG: inputWaveLength = %ld, numFrames = %ld\n", inputWaveLength, numFrames);
#endif

                source = straightSourceInitialize(straight, NULL);
                straightSourceCreate(straight, source, numFrames);
                        
                specgram = straightSpecgramInitialize(straight, NULL);
                straightSpecgramCreate(straight, specgram, numFrames);

                frameOffset = 0;
                
                for (position = 0; position < inputWaveLength;) {
                    positionSec = straightPointToSecond(straight, position);

                    if (inputWaveLength - position < defaultPushLength) {
                        pushLength = inputWaveLength - position;
                    } else {
                        pushLength = defaultPushLength;
                    }
#ifdef DEBUG
                    fprintf(stderr, "DEBUG: position = %ld, positionSec = %f, pushLength = %ld\n", position, positionSec, pushLength);
#endif
                    
                    straightInputFramesPush(inputFrames,
                                            inputWave + position, pushLength);
                            
                    /*---- begin: getting data from analysis engine ----*/
                    sourcePopLength = straightInputFramesPopSource(inputFrames, source, position,
                                                                   &sourceFrameOffset, &sourceUpdatedFrames);
                    specgramPopLength = straightInputFramesPopSpecgram(inputFrames, specgram, position,
                                                                       &specgramFrameOffset, &specgramUpdatedFrames);
                    /*---- end: getting data from analysis engine ----*/

#ifdef DEBUG
                    fprintf(stderr, "DEBUG: sourcePopLength = %ld, specgramPopLength = %ld\n",
                            sourcePopLength, specgramPopLength);
                    fprintf(stderr, "DEBUG: sourceFrameOffset = %ld, sourceUpdatedFrames = %ld, specgramFrameOffset = %ld, specgramUpdatedFrames = %ld\n",
                            sourceFrameOffset, sourceUpdatedFrames, specgramFrameOffset, specgramUpdatedFrames);
#endif

                    frameOffset += specgramUpdatedFrames;
                    position += pushLength;
                }

                while ((flushLength = straightInputFramesPrepareFlush(inputFrames, 0)) > 0) {
#ifdef DEBUG
                    fprintf(stderr, "DEBUG: flushLength = %ld\n", flushLength);
#endif
                    
                    /*---- begin: getting data from analysis engine ----*/
                    sourcePopLength = straightInputFramesFlushSource(inputFrames, source, position,
                                                                     &sourceFrameOffset, &sourceUpdatedFrames);
                    specgramPopLength = straightInputFramesFlushSpecgram(inputFrames, specgram, position,
                                                                         &specgramFrameOffset, &specgramUpdatedFrames);
                    /*---- end: getting data from analysis engine ----*/

#ifdef DEBUG
                    fprintf(stderr, "DEBUG: flush: sourceFrameOffset = %ld, sourceUpdatedFrames = %ld, specgramFrameOffset = %ld, specgramUpdatedFrames = %ld\n",
                            sourceFrameOffset, sourceUpdatedFrames, specgramFrameOffset, specgramUpdatedFrames);
#endif
                }

                if ((sf = straightFileOpen(outputFileName, "w")) != NULL) {
                    fprintf(stderr, "Output STF file: %s\n", outputFileName);

#ifdef USE_TANDEM
                    strcpy(chunkList, "F0  ");
                    if (straightSourceIsUnvoicedF0Supported(source)) {
                        strcat(chunkList, "UVF0PRLV");
                    }
                    if (straightSourceIsF0CandidatesSupported(source)) {
                        strcat(chunkList, "F0CN");
                    }
                    strcat(chunkList, "AP  SPEC");
#endif
                    straightFileSetChunkList(sf, chunkList);
                    straightFileWriteHeader(sf, straight);
                    straightFileWriteF0Chunk(sf, source);
#ifdef USE_TANDEM
                    if (straightSourceIsUnvoicedF0Supported(source)) {
                        straightFileWriteUnvoicedF0Chunk(sf, source);
                        straightFileWritePeriodicityLevelChunk(sf, source);
                    }
                    if (straightSourceIsF0CandidatesSupported(source)) {
                        straightFileWriteF0CandidatesChunk(sf, source);
                    }
#endif
                    straightFileWriteAperiodicityChunk(sf, source);
                    straightFileWriteSpecgramChunk(sf, specgram);

                    straightFileClose(sf);
                } else {
                    fprintf(stderr, "Error: Cannot open output file: %s\n", outputFileName);
                }
                
                straightSourceDestroy(source);
                straightSpecgramDestroy(specgram);
                
                straightInputFramesDestroy(inputFrames);
            }
        }
            
        straightDestroy(straight);
    } else {
        fprintf(stderr, "Error: Cannot initialize STRAIGHT engine\n");
    }

    return 0;
}
