#include "com_cabatuan_yuv_MainActivity.h"
#include <android/log.h>
#include <android/bitmap.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits>

#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;

#define  LOG_TAG    "YUV"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define  DEBUG 0



#define toInt(pValue) \
 (0xff & (int32_t) pValue)

#define max(pValue1, pValue2) \
 (pValue1<pValue2) ? pValue2 : pValue1

#define clamp(pValue, pLowest, pHighest) \
 ((pValue < 0) ? pLowest : (pValue > pHighest) ? pHighest: pValue)



void extractVU( Mat &image,  Mat &V, Mat &U){

	int nRows = image.rows;   // number of lines
    int nCols = image.cols;   // number of columns  

    if (image.isContinuous()) {
    // then no padded pixels
        nCols = nCols * nRows;
		nRows = 1; // it is now a 1D array
	}   

    // for all pixels
    for (int j=0; j<nRows; j++) {
        // pointer to first column of line j
        uchar* data   = image.ptr<uchar>(j);
        uchar* colorV = V.ptr<uchar>(j);
        uchar* colorU = U.ptr<uchar>(j);

		for (int i = 0; i < nCols; i += 2) {
		        // process each pixel
                *colorV++ = toInt(*data++); //- 128; // converts [0,255] to [-128,127]
                *colorU++ = toInt(*data++); //- 128; // converts [0,255] to [-128,127]    
        }
    }
}




Mat imageU, imageV;


/*
 * Class:     com_cabatuan_yuv_MainActivity
 * Method:    process
 * Signature: (Landroid/graphics/Bitmap;[BI)V
 */
JNIEXPORT void JNICALL Java_com_cabatuan_yuv_MainActivity_process
  (JNIEnv *pEnv, jobject clazz, jobject pTarget, jbyteArray pSource, jint pMode){

   AndroidBitmapInfo bitmapInfo;
   uint32_t* bitmapContent; // Links to Bitmap content

   if(AndroidBitmap_getInfo(pEnv, pTarget, &bitmapInfo) < 0) abort();
   if(bitmapInfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) abort();
   if(AndroidBitmap_lockPixels(pEnv, pTarget, (void**)&bitmapContent) < 0) abort();

   /// Access source array data... OK
   jbyte* source = (jbyte*)pEnv->GetPrimitiveArrayCritical(pSource, 0);
   if (source == NULL) abort();

   Mat srcNV21(bitmapInfo.height + bitmapInfo.height/2, bitmapInfo.width, CV_8UC1, (unsigned char *)source);
   Mat mbgra(bitmapInfo.height, bitmapInfo.width, CV_8UC4, (unsigned char *)bitmapContent);


/***********************************************************************************************/
   //LOGI("bitmapInfo.size() = [%d, %d]", bitmapInfo.height, bitmapInfo.width);

   //LOGI("srcNV21.size() = [%d, %d]", srcNV21.size().height, srcNV21.size().width);


   if (imageV.empty())
       imageV = Mat(bitmapInfo.height/2, bitmapInfo.width/2, CV_8UC1);

   if (imageU.empty())
       imageU = Mat(bitmapInfo.height/2, bitmapInfo.width/2, CV_8UC1);
 
   Mat VU = srcNV21(cv::Rect( 0, bitmapInfo.height, bitmapInfo.width, bitmapInfo.height/2));

   //LOGI("VU.size() = [%d, %d]", VU.size().height, VU.size().width);

   extractVU( VU, imageV, imageU);

   //LOGI("imageV.size() = [%d, %d]", imageV.size().height, imageV.size().width);

/***********************************************************************************************/
    if (pMode == 0){

    	cvtColor(srcNV21(cv::Rect(0,0,bitmapInfo.width,bitmapInfo.height)), mbgra, CV_GRAY2BGRA);

    }

    else if (pMode == 1){

          Mat imageV_scaled;
          pyrUp(imageV, imageV_scaled, Size(bitmapInfo.width, bitmapInfo.height));

          //LOGI("imageV_scaled.size() = [%d, %d]", imageV_scaled.size().height, imageV_scaled.size().width);

    	  cvtColor( imageV_scaled, mbgra, CV_GRAY2BGRA);

        //cvtColor(srcNV21(cv::Rect(0,0,bitmapInfo.width,bitmapInfo.height)), mbgra, CV_GRAY2BGRA);

    }

    else if (pMode == 2){

          Mat imageU_scaled;
          pyrUp(imageU, imageU_scaled, Size(bitmapInfo.width, bitmapInfo.height));
    	  cvtColor( imageU_scaled, mbgra, CV_GRAY2BGRA);

    }

    else {

        cvtColor(srcNV21(cv::Rect(0,0,bitmapInfo.width,bitmapInfo.height)), mbgra, CV_GRAY2BGRA);

    }


/************************************************************************************************/ 
   
   /// Release Java byte buffer and unlock backing bitmap
   pEnv-> ReleasePrimitiveArrayCritical(pSource,source,0);
   if (AndroidBitmap_unlockPixels(pEnv, pTarget) < 0) abort();

}
