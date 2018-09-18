#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <string.h>
#include <unistd.h>
#include <android/log.h>
#include <codec_app_def.h>
#include "welsenc_WelsEncTest_EncoderTest.h"
#include "codec_def.h"
#include "codec_api.h"
#include "codec_app_def.h"
#include "codec_ver.h"

#define BITRATE_STEP 100

#define LOG_TAG "openh264Demo"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define now_ms std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count
#define now_us std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count
//#define DEBUG_ENCODER 1

typedef struct
{
    int width;
    int height;
    int iAvgBitRate;
    int iFps;
    int iCrf;
    char* input;
    char* output;
}TestElement;

void WelsEncTraceFunc(void *ctx, int level, const char *string)
{
    LOGE("%s", string);
}

void set_default_param(SEncParamExt *pParam, int width, int height, int iFps, int iAvgBitRate)
{
#define THREADNUM 1
    pParam->iPicWidth      = width;                 // width of picture in samples
    pParam->iPicHeight     = height;                  // height of picture in samples
    pParam->fMaxFrameRate    = iFps;

    pParam->iEntropyCodingModeFlag = 1;			                      // 0: cavlc; 1:cabac
    pParam->iNumRefFrame     = 1;                                    //ref frame number
    pParam->iMultipleThreadIdc = THREADNUM;                         //6;//s_open264Param.threads;//2;	/// # 0: auto(dynamic imp. internal encoder); 1: multiple threads imp. disabled; > 1: count number of threads;
    pParam->bUseLoadBalancing = true;

    pParam->iRCMode          = (RC_MODES)RC_BITRATE_MODE;              //RC_BITRATE_MODE  RC_CRF_MODE
//    pParam->iTargetBitrate   = iAvgBitRate * 1000;            // target bitrate desired
//    pParam->iMaxBitrate      = iAvgBitRate * 1000;           //pParam->iTargetBitrate * 13/10;
    pParam->iTargetBitrate   = 1024 * 1000;            // target bitrate desired
    pParam->iMaxBitrate      = 1024 * 1000;           //pParam->iTargetBitrate * 13/10;
    pParam->uiIntraPeriod    = 250;                             // period of Intra frame
    pParam->iLoopFilterDisableIdc = 0;

    //pParam->iMinQp           = 25;
    //pParam->iMaxQp           = 25;

    pParam->bEnableDenoise    = 0;    // denoise control
    pParam->bEnableBackgroundDetection = 0; // background detection control
    pParam->bEnableAdaptiveQuant = 0; // adaptive quantization control
    pParam->bEnableSceneChangeDetect = 0;   //wanglijun modify
    pParam->bEnableFrameSkip = false; //false    // frame skipping
    pParam->bEnableLongTermReference = 0;
    pParam->iLTRRefNum = 0;
    pParam->iLtrMarkPeriod = 0;

    int iIndexLayer = 0;
    pParam->iTemporalLayerNum = 1;    // layer number at temporal level
    pParam->iSpatialLayerNum  = 1;    // layer number at spatial level
    pParam->sSpatialLayers[iIndexLayer].uiProfileIdc       = PRO_MAIN;
    pParam->sSpatialLayers[iIndexLayer].uiLevelIdc       = LEVEL_5_2;
    pParam->sSpatialLayers[iIndexLayer].iDLayerQp        =  26;
    pParam->sSpatialLayers[iIndexLayer].iVideoWidth        = width;
    pParam->sSpatialLayers[iIndexLayer].iVideoHeight       = height;
    pParam->sSpatialLayers[iIndexLayer].fFrameRate         = pParam->fMaxFrameRate;
    pParam->sSpatialLayers[iIndexLayer].iSpatialBitrate    = pParam->iTargetBitrate;
    pParam->sSpatialLayers[iIndexLayer].iMaxSpatialBitrate    = pParam->iTargetBitrate;
    if(pParam->iMultipleThreadIdc == 1)
    {
        pParam->sSpatialLayers[iIndexLayer].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
    }
    else
    {
        pParam->sSpatialLayers[iIndexLayer].sSliceArgument.uiSliceMode = SM_FIXEDSLCNUM_SLICE;
    }
    pParam->sSpatialLayers[iIndexLayer].sSliceArgument.uiSliceNum = THREADNUM;
    //pParam->sSliceArgument[iIndexLayer].uiSliceMode = SM_FIXEDSLCNUM_SLICE;
    //pParam->sSliceArgument[iIndexLayer].uiSliceNum = 2;
    //pParam->sSliceArgument[iIndexLayer].uiSliceMode = SM_SIZELIMITED_SLICE;
    //pParam->sSliceArgument[iIndexLayer].uiSliceSizeConstraint = 2000;

    pParam->eSpsPpsIdStrategy = CONSTANT_ID;//INCREASING_ID; CONSTANT_ID
    pParam->iComplexityMode = MEDIUM_COMPLEXITY;//LOW_COMPLEXITY;//MEDIUM_COMPLEXITY;
    pParam->eSpsPpsIdStrategy = CONSTANT_ID;
    pParam->bSimulcastAVC         = true;
#undef THREADNUM
}

int encoder_one_file(char *inpath, char *outpath, int width, int height, int iFps = 25, int iAvgBitRate = 0)
{
    WelsTraceCallback pcallback = NULL;
    uint32_t iLevel = WELS_LOG_DEFAULT;
    int iRet = -1;
    int i = 0;
    int iSize = 0;
    int iTotalLen = 0;
    int64_t iFileSize = 0;
    int64_t pts = 0;
    int64_t iStart = 0, iTotal = 0;
    int64_t iTmp = 0;
    float fMaxTime = 0;
    float fMinTime = INT_MAX;
    uint8_t *data = NULL;
    SEncParamExt sSvcParam;
    SSourcePicture* pSrcPic = NULL;
    SFrameBSInfo sFbi = {0};
    int i_delay = 0;
    int iEncFrames;
    int iSkipEncFrames = 0;
    int iRealEncFrames = 0;
    char *filePath = inpath;
    char *bsPath = outpath;
    bool bCanBeRead = false;
    ISVCEncoder* pSVCEncoder = NULL;

    FILE *pFile = NULL;
    FILE *pFpBs = NULL;
    pFile = fopen(filePath,"rb+" );
    if(pFile == NULL)
    {
        LOGE("open filePath %s failed",filePath);
        goto ErrDone;
    }

    pFpBs = fopen(bsPath,"wb");
    if(pFpBs == NULL)
    {
        LOGE("open bsPath %s failed",bsPath);
        goto ErrDone;
    }

    iRet = WelsCreateSVCEncoder (&pSVCEncoder);
    if(iRet)
    {
        LOGE("failed init Encoder!\n");
        goto ErrDone;
    }
    pSVCEncoder->GetDefaultParams(&sSvcParam);
    set_default_param(&sSvcParam, width, height, iFps, iAvgBitRate);
#ifdef DEBUG_ENCODER
    pcallback = WelsEncTraceFunc;
    iLevel = WELS_LOG_DETAIL;
    pSVCEncoder->SetOption(ENCODER_OPTION_TRACE_CALLBACK, (void *)&pcallback);
    pSVCEncoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, (void *)&iLevel);
#endif

    // SVC encoder initialization
    if (cmResultSuccess !=  (iRet = pSVCEncoder->InitializeExt (&sSvcParam)))
    {
        LOGE ("SVC encoder Initialize failed\n");
        goto ErrDone;
    }

    iSize = width * height * 3/2;
    data = (uint8_t *)malloc(iSize);
    if(data == NULL)
    {
        LOGE("failed malloc data be encode!\n");
        goto ErrDone;
    }

    pSrcPic = new SSourcePicture;
    if(pSrcPic == NULL)
    {
        LOGE("failed new SSourcePicture !\n");
        goto ErrDone;
    }
    memset(pSrcPic, 0, sizeof(SSourcePicture));
    pSrcPic->iColorFormat = videoFormatI420;
    pSrcPic->uiTimeStamp = 0;
    pSrcPic->iPicWidth = width;
    pSrcPic->iPicHeight = height;
    pSrcPic->iStride[0] = width;
    pSrcPic->iStride[1] = pSrcPic->iStride[2] = (pSrcPic->iStride[0] >> 1);

    pSrcPic->pData[0] = data;
    pSrcPic->pData[1] = pSrcPic->pData[0] + (width * height);
    pSrcPic->pData[2] = pSrcPic->pData[1] + (width * height >> 2);

    fseek(pFile, 0, SEEK_END);
    iFileSize = ftell(pFile);
    iTotalLen = iFileSize / (width * height * 3/2);
    //iTotalLen = 142;
    fseek(pFile, 0, SEEK_SET);
    while(i < iTotalLen) {
        //while(i < 500) {

        double fpts = ((double)(i * 1000))/iFps;
        pts = (int64_t)(fpts);
        i++;

        //memset(data,100,width * height);
        if(fread(data, 1, iSize, pFile) != iSize)
        {
            LOGE("failed read %d element data\n", iSize);
            break;
        }
        pSrcPic->uiTimeStamp = pts;
        iStart =  now_us();
        iEncFrames = pSVCEncoder->EncodeFrame(pSrcPic, &sFbi);
        iTmp = now_us() - iStart;;
        iTotal += iTmp;
        if(iTmp > fMaxTime)
        {
            fMaxTime = iTmp;
        }
        else if(iTmp < fMinTime)
        {
            fMinTime = iTmp;
        }
        if(sFbi.eFrameType == videoFrameTypeSkip)
        {
            iSkipEncFrames++;
            continue;
        }

        if (iEncFrames == cmResultSuccess)
        {
            int iLayer = 0;
            int iFrameSize = 0;
            while (iLayer < sFbi.iLayerNum)
            {
                SLayerBSInfo* pLayerBsInfo = &sFbi.sLayerInfo[iLayer];
                if (pLayerBsInfo != NULL)
                {
                    int iLayerSize = 0;
                    int iNalIdx = pLayerBsInfo->iNalCount - 1;
                    do
                    {
                        iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
                        -- iNalIdx;
                    } while (iNalIdx >= 0);
                    fwrite (pLayerBsInfo->pBsBuf, 1, iLayerSize, pFpBs); // write pure bit stream into file
                    iFrameSize += iLayerSize;
                }
                ++ iLayer;
            }
            ++iRealEncFrames; // excluding skipped frame time
        }
        else
        {
            LOGE ("EncodeFrame(), ret: %d, frame index: %d.\n", iEncFrames, i-1);
        }
    }

    //i_delay = pSVCEncoder->GetDelayedFrame();
    i_delay = 0;
    i = 0;
    while(i++ < i_delay)
    {
        iStart = now_us();
        iEncFrames = pSVCEncoder->EncodeFrame (NULL, &sFbi);
        iTmp = now_us() - iStart;;
        iTotal += iTmp;
        if(iTmp > fMaxTime)
        {
            fMaxTime = iTmp;
        }
        else if(iTmp < fMinTime)
        {
            fMinTime = iTmp;
        }

        if(sFbi.eFrameType == videoFrameTypeSkip)
        {
            iSkipEncFrames++;
            continue;
        }

        if (iEncFrames == cmResultSuccess)
        {
            int iLayer = 0;
            int iFrameSize = 0;
            while (iLayer < sFbi.iLayerNum)
            {
                SLayerBSInfo* pLayerBsInfo = &sFbi.sLayerInfo[iLayer];
                if (pLayerBsInfo != NULL)
                {
                    int iLayerSize = 0;
                    int iNalIdx = pLayerBsInfo->iNalCount - 1;
                    do
                    {
                        iLayerSize += pLayerBsInfo->pNalLengthInByte[iNalIdx];
                        -- iNalIdx;
                    } while (iNalIdx >= 0);
                    fwrite (pLayerBsInfo->pBsBuf, 1, iLayerSize, pFpBs); // write pure bit stream into file
                    iFrameSize += iLayerSize;
                }
                ++ iLayer;
            }
            ++iRealEncFrames; // excluding skipped frame time
        }
        else
        {
            LOGE ("EncodeDelayedFrame(), ret: %d, frame index: %d.\n", iEncFrames, i);
        }
    }

    if (iRealEncFrames > 0)
    {
        double dElapsed = iTotal / 1e6;
        LOGE ("Width:\t\t\t%d\nHeight:\t\t\t%d\nFrames:\t\t\t%d\nTotal time:\t\t%.2f sec\nAVGTime:\t\t%.0f us\nMaxTime:\t\t%.0f us\nMinTime:\t\t%.0f us\nFPS:\t\t\t%f fps\n",
                sSvcParam.iPicWidth, sSvcParam.iPicHeight,
                iRealEncFrames, dElapsed, (double)iTotal/iRealEncFrames, fMaxTime, fMinTime, (iRealEncFrames * 1.0) / dElapsed);
    }


ErrDone:
    if(pFile) {
        fclose(pFile);
        pFile = NULL;
    }

    if(pFpBs)
    {
        fclose(pFpBs);
        pFpBs = NULL;
    }


    if(data)
    {
        free(data);
        data = NULL;
    }

    if(pSrcPic)
    {
        delete pSrcPic;
        pSrcPic = NULL;
    }

    if(pSVCEncoder != NULL)
    {
        WelsDestroySVCEncoder (pSVCEncoder);
        pSVCEncoder = NULL;
    }

    return iRet;
}


TestElement testElement[] = {
//       {720, 576, 500, 25, 25, (char*)"videocap720x576_debug.yuv",  (char*)"android_mix_enc.h264"},
//       {1280, 720, 2880,  25, 25, (char*)"1280x720_300.yuv",  (char*)"android_mix_enc.h264"},

        [0]={640, 360, 1410, 24, 21, (char*)"Pedestrian_area_640x360_24.yuv",  (char*)"Pedestrian_area_640x360_24_crf21.h264"},
        [1]={640, 360, 850, 24, 25, (char*)"Pedestrian_area_640x360_24.yuv",  (char*)"Pedestrian_area_640x360_24_crf25.h264"},
        [2]={640, 360, 524, 24, 29, (char*)"Pedestrian_area_640x360_24.yuv",  (char*)"Pedestrian_area_640x360_24_crf29.h264"},
        [3]={640, 360, 327, 24, 33, (char*)"Pedestrian_area_640x360_24.yuv",  (char*)"Pedestrian_area_640x360_24_crf33.h264"},

        [4]={640, 360, 1805, 15, 21, (char*)"SocialOutdoor_640x360_15.yuv",  (char*)"SocialOutdoor_640x360_15_crf21.h264"},
        [5]={640, 360, 1160, 15, 25, (char*)"SocialOutdoor_640x360_15.yuv",  (char*)"SocialOutdoor_640x360_15_crf25.h264"},
        [6]={640, 360, 758, 15, 29, (char*)"SocialOutdoor_640x360_15.yuv",  (char*)"SocialOutdoor_640x360_15_crf29.h264"},
        [7]={640, 360, 491, 15, 33, (char*)"SocialOutdoor_640x360_15.yuv",  (char*)"SocialOutdoor_640x360_15_crf33.h264"},

        [8]={640, 360, 3505, 25, 21, (char*)"taishan_640x360_25.yuv",  (char*)"taishan_640x360_25_crf21.h264"},
        [9]={640, 360, 1719, 25, 25, (char*)"taishan_640x360_25.yuv",  (char*)"taishan_640x360_25_crf25.h264"},
        [10]={640, 360, 729, 25, 29, (char*)"taishan_640x360_25.yuv",  (char*)"taishan_640x360_25_crf29.h264"},
        [11]={640, 360, 332, 25, 33, (char*)"taishan_640x360_25.yuv",  (char*)"taishan_640x360_25_crf33.h264"},

        [12]={832, 480, 2765, 50, 21, (char*)"BasketballDrill_832x480_50.yuv",  (char*)"BasketballDrill_832x480_50_crf21.h264"},
        [13]={832, 480, 1510, 50, 25, (char*)"BasketballDrill_832x480_50.yuv",  (char*)"BasketballDrill_832x480_50_crf25.h264"},
        [14]={832, 480, 864, 50, 29, (char*)"BasketballDrill_832x480_50.yuv",  (char*)"BasketballDrill_832x480_50_crf29.h264"},
        [15]={832, 480, 510, 50, 33, (char*)"BasketballDrill_832x480_50.yuv",  (char*)"BasketballDrill_832x480_50_crf33.h264"},

        [16]={832, 480, 2444, 60, 21, (char*)"BQMall_832x480_60.yuv",  (char*)"BQMall_832x480_60_crf21.h264"},
        [17]={832, 480, 1327, 60, 25, (char*)"BQMall_832x480_60.yuv",  (char*)"BQMall_832x480_60_crf25.h264"},
        [18]={832, 480, 760, 60, 29, (char*)"BQMall_832x480_60.yuv",  (char*)"BQMall_832x480_60_crf29.h264"},
        [19]={832, 480, 458, 60, 33, (char*)"BQMall_832x480_60.yuv",  (char*)"BQMall_832x480_60_crf33.h264"},

        [20]={832, 480, 5318, 50, 21, (char*)"PartyScene_832x480_50.yuv",  (char*)"PartyScene_832x480_50_crf21.h264"},
        [21]={832, 480, 2877, 50, 25, (char*)"PartyScene_832x480_50.yuv",  (char*)"PartyScene_832x480_50_crf25.h264"},
        [22]={832, 480, 1475, 50, 29, (char*)"PartyScene_832x480_50.yuv",  (char*)"PartyScene_832x480_50_crf29.h264"},
        [23]={832, 480, 722, 50, 33, (char*)"PartyScene_832x480_50.yuv",  (char*)"PartyScene_832x480_50_crf33.h264"},

        [24]={1280, 720, 1506, 60, 21, (char*)"FourPeople_1280x720_60.yuv",  (char*)"FourPeople_1280x720_60_crf21.h264"},
        [25]={1280, 720, 735, 60, 25, (char*)"FourPeople_1280x720_60.yuv",  (char*)"FourPeople_1280x720_60_crf25.h264"},
        [26]={1280, 720, 427, 60, 29, (char*)"FourPeople_1280x720_60.yuv",  (char*)"FourPeople_1280x720_60_crf29.h264"},
        [27]={1280, 720, 249, 60, 33, (char*)"FourPeople_1280x720_60.yuv",  (char*)"FourPeople_1280x720_60_crf33.h264"},

        [28]={1280, 720, 1268, 60, 21,(char*)"Johnny_1280x720_60.yuv",  (char*)"Johnny_1280x720_60_crf21.h264"},
        [29]={1280, 720, 484, 60, 25, (char*)"Johnny_1280x720_60.yuv",  (char*)"Johnny_1280x720_60_crf25.h264"},
        [30]={1280, 720, 255, 60, 29, (char*)"Johnny_1280x720_60.yuv",  (char*)"Johnny_1280x720_60_crf29.h264"},
        [31]={1280, 720, 142, 60, 33, (char*)"Johnny_1280x720_60.yuv",  (char*)"Johnny_1280x720_60_crf33.h264"},

        [32]={1280, 720, 15817, 50, 21, (char*)"Parkjoy_1280x720_50.yuv",  (char*)"Parkjoy_1280x720_50_crf21.h264"},
        [33]={1280, 720, 8889, 50, 25, (char*)"Parkjoy_1280x720_50.yuv",  (char*)"Parkjoy_1280x720_50_crf25.h264"},
        [34]={1280, 720, 4510, 50, 29, (char*)"Parkjoy_1280x720_50.yuv",  (char*)"Parkjoy_1280x720_50_crf29.h264"},
        [35]={1280, 720, 2176, 50, 33, (char*)"Parkjoy_1280x720_50.yuv",  (char*)"Parkjoy_1280x720_50_crf33.h264"},

        [36]={1920, 1080,  12758, 50, 21, (char*)"BasketballDrive_1920x1080_50.yuv",  (char*)"BasketballDrive_1920x1080_50_crf21.h264"},
        [37]={1920, 1080,  5970, 50, 25, (char*)"BasketballDrive_1920x1080_50.yuv",  (char*)"BasketballDrive_1920x1080_50_crf25.h264"},
        [38]={1920, 1080,  3362, 50, 29, (char*)"BasketballDrive_1920x1080_50.yuv",  (char*)"BasketballDrive_1920x1080_50_crf29.h264"},
        [39]={1920, 1080,  2060, 50, 33, (char*)"BasketballDrive_1920x1080_50.yuv",  (char*)"BasketballDrive_1920x1080_50_crf33.h264"},

        [40]={1920, 1080,  17772, 60, 21,(char*)"BQTerrace_1920x1080_60.yuv",  (char*)"BQTerrace_1920x1080_60_crf21.h264"},
        [41]={1920, 1080,  5252, 60, 25, (char*)"BQTerrace_1920x1080_60.yuv",  (char*)"BQTerrace_1920x1080_60_crf25.h264"},
        [42]={1920, 1080,  2244, 60, 29, (char*)"BQTerrace_1920x1080_60.yuv",  (char*)"BQTerrace_1920x1080_60_crf29.h264"},
        [43]={1920, 1080,  1124, 60, 33, (char*)"BQTerrace_1920x1080_60.yuv",  (char*)"BQTerrace_1920x1080_60_crf33.h264"},

        [44]={1920, 1080, 11547, 50, 21, (char*)"Cactus_1920x1080_50.yuv",  (char*)"Cactus_1920x1080_50_crf21.h264"},
        [45]={1920, 1080, 4823, 50, 25, (char*)"Cactus_1920x1080_50.yuv",  (char*)"Cactus_1920x1080_50_crf25.h264"},
        [46]={1920, 1080, 2617, 50, 29, (char*)"Cactus_1920x1080_50.yuv",  (char*)"Cactus_1920x1080_50_crf29.h264"},
        [47]={1920, 1080, 1497, 50, 33, (char*)"Cactus_1920x1080_50.yuv",  (char*)"Cactus_1920x1080_50_crf33.h264"},
        [48]={832, 480, 1327, 60, 25, (char*)"832x480.yuv",  (char*)"832x480.h264"}
};


JNIEXPORT jint JNICALL Java_welsenc_WelsEncTest_encoderTest
        (JNIEnv *env, jclass jc)
{
    int size = sizeof(testElement) / sizeof(TestElement);
    int i = 28;
    //JniRegister::ALiRegister(env);
    LOGE ("================ Start to run Java_encoder_test_alivc_aliyun_com_encodertest_EncoderTest_encoderTest ================\n");

    int64_t now = now_ms();
    //while(i < size)
    {
        TestElement element = testElement[i];

        char input[512];
        char output[512];
        memset(input, 0, sizeof(input));
        memset(output, 0, sizeof(output));

        LOGE("------------------------start test %d case-----------------\n\n",i);

        sprintf(input, "%s/%s", "/sdcard", element.input);
        sprintf(output, "%s/%s","/sdcard",element.output);

        int ret = encoder_one_file(input, output , element.width, element.height, 25, element.iAvgBitRate);
        if(ret != 0)
        {
            LOGE("encoder test failed to test %d " ,i);
            return ret;
        }
        i++;
    }
    int64_t avgE = ( now_ms() - now );
    LOGE("encoder total cost %ld ms\n" ,avgE);

    return 0;

}
