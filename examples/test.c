if ((outputFrames = straightOutputFramesInitialize(straight, NULL, NULL)) != NULL) {
        defaultPushLength = straightOutputFramesGetFrameLengthInPoint(straight) / 4;
        numFrames = straightSpecgramGetNumFrames(specgram);
    
        for (position = 0;;) {
            positionSec = straightPointToSecond(straight, position);
            
            straightSpecgramQuantizePosition(straight, specgram, positionSec, &frame);
            
            if (frame >= numFrames) {
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

            /* 音声出力処理またはファイルへの書き込み処理。outputWaveに合成音声のデータが入っている。 */
            :
            :
            
            position += pushLength;
        }

        straightOutputFramesDestroy(outputFrames);
    }