/* 
 * frameSynth: simple application that converts input STF file
 *             into 64bit double binary sound file (NOT wave file)
 */
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
    StraightOutputFrames outputFrames;
    char chunkId[8];
    long chunkSize;

    if (argc < 3) {
        fprintf(stderr, "frameSynth <input STF file> <output 64bit-double file>\n");
        exit(1);
    }
    
    strcpy(inputFileName, argv[1]);
    strcpy(outputFileName, argv[2]);

#ifdef USE_TANDEM
    straightInitConfigTandem(&config);
#else
    straightInitConfig(&config);
#endif
    config.samplingFrequency = 16000.0;

    if ((straight = straightInitialize(&config)) != NULL) {
        if ((sf = straightFileOpen(inputFileName, "r")) != NULL) {
            fprintf(stderr, "Input STF file: %s\n", inputFileName);
        
            if (straightFileLoadHeader(sf, straight) == ST_TRUE) {
                source = straightSourceInitialize(straight, NULL);
                specgram = straightSpecgramInitialize(straight, NULL);
            
                while (straightFileLoadChunkInfo(sf, chunkId, &chunkSize) == ST_TRUE) {
                    if (strcmp(chunkId, "F0  ") == 0) {
                        straightFileLoadF0(sf, source);
                    } else if (strcmp(chunkId, "UVF0") == 0) {
                        straightFileLoadUnvoicedF0(sf, source);
                    } else if (strcmp(chunkId, "PRLV") == 0) {
                        straightFileLoadPeriodicityLevel(sf, source);
                    } else if (strcmp(chunkId, "F0CN") == 0) {
                        straightFileLoadF0Candidates(sf, source);
                    } else if (strcmp(chunkId, "AP  ") == 0 || strcmp(chunkId, "APSG") == 0) {
                        straightFileLoadAperiodicity(sf, source);
                    } else if (strcmp(chunkId, "SPEC") == 0) {
                        straightFileLoadSpecgram(sf, specgram);
                    } else {
                        straightFileSkipChunk(sf, chunkSize);
                    }
                }

                if ((outputFrames = straightOutputFramesInitialize(straight, NULL, NULL)) != NULL) {
                    double positionSec;
                    long position;
                    long pushLength, popLength;
                    long defaultPushLength;
                    long numFrames;
                    long frame;
                    double *outputWave;
                    FILE *fp;

                    if ((fp = fopen(outputFileName, "wb")) == NULL) {
                        fprintf(stderr, "Error: Cannot open output file: %s\n", outputFileName);
                    } else {
                        fprintf(stderr, "Output 64bit-double file: %s\n", outputFileName);
                        
                        defaultPushLength = straightGetFrameLengthInPoint(straight) / 4;
                        outputWave = malloc(sizeof(double) * defaultPushLength);
                        numFrames = straightSpecgramGetNumFrames(specgram);
                    
                        for (position = 0;;) {
                            positionSec = straightPointToSecond(straight, position);
                            
                            straightSpecgramQuantizePosition(straight, specgram, positionSec, &frame);
#ifdef DEBUG
                            fprintf(stderr, "DEBUG: position = %ld, positionSec = %f, frame = %ld / %ld\n",
                                    position, positionSec, frame, numFrames);
#endif
                            
                            if (frame >= numFrames) {
#ifdef DEBUG
                                fprintf(stderr, "DEBUG: break: frame (%ld) >= numFrames (%ld)\n", frame, numFrames);
#endif
                                break;
                            }
                        
                            /*---- begin: sending data to synthesis engine ----*/
                            pushLength = straightOutputFramesPushBegin(outputFrames, defaultPushLength);
                            if (pushLength <= 0) {
                                fprintf(stderr, "Error: push failed: pushLength = %ld\n", pushLength);
                                break;
                            }
                        
                            straightOutputFramesPushSource(outputFrames, source, position);    
                            straightOutputFramesPushSpecgram(outputFrames, specgram, position);
                        
                            straightOutputFramesPushEnd(outputFrames);
                            /*---- end: sending data to synthesis engine ----*/

                            popLength = straightOutputFramesPop(outputFrames, outputWave);
#ifdef DEBUG
                            fprintf(stderr, "DEBUG: frame = %ld / %ld, position = %ld, pushLength = %ld, popLength = %ld\n",
                                    frame, numFrames, position, pushLength, popLength);
#endif


                            fwrite(outputWave, sizeof(double), popLength, fp);
                            
                            position += pushLength;
                        }

                        while ((popLength = straightOutputFramesFlush(outputFrames, outputWave, defaultPushLength)) > 0) {
#ifdef DEBUG
                            fprintf(stderr, "DEBUG: popLength = %ld in flush\n", popLength);
#endif
                            fwrite(outputWave, sizeof(double), popLength, fp);
                        }

                        straightOutputFramesDestroy(outputFrames);

                        fclose(fp);
                    }
                }

    
                straightSourceDestroy(source);
                straightSpecgramDestroy(specgram);
            } else {
                fprintf(stderr, "Error: Cannot load STF header: %s\n", inputFileName);
            }
            
            straightFileClose(sf);
        } else {
            fprintf(stderr, "Error: Cannot open input STF file: %s\n", inputFileName);
        }
            
        straightDestroy(straight);
    } else {
        fprintf(stderr, "Error: Cannot initialize STRAIGHT engine\n");
    }

    return 0;
}
