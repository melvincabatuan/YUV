#include <jni.h>

#include <android/log.h>
#include <android/bitmap.h>
#include <queue>
#include <limits>

#include "opencv2/imgproc.hpp"

#define  LOG_TAG    "YUV"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define  DEBUG 0


using namespace cv;

#define toInt(pValue) \
 (0xff & (int32_t) pValue)

#define max(pValue1, pValue2) \
 (pValue1<pValue2) ? pValue2 : pValue1

#define clamp(pValue, pLowest, pHighest) \
 ((pValue < 0) ? pLowest : (pValue > pHighest) ? pHighest: pValue)




void extractVU( Mat &image,  Mat &V, Mat &U){
   
    // accept only char type matrices
    CV_Assert(image.depth() != sizeof(uchar));

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
                *colorV++ =  *data++; //- 128; // converts [0,255] to [-128,127]
                *colorU++ =  *data++; //- 128; // converts [0,255] to [-128,127]    
        }
    }
}




/* YUV conversion globals */
cv::Mat imageU, imageV;

/* FPS log globals */
float fps = 0;
std::queue<int64> time_queue;

/*
 * Base rate: 15 fps on cvtColor(..., CV_GRAY2BGRA); // consistent w/ OpenCV Java Camera
 *	
 *
 */
void onCameraFrame(const cv::Mat &srcNV21, cv::Mat &mbgra, int pMode){

	CV_Assert( !srcNV21.empty() || !mbgra.empty() );

	// FPS log declarations
    int64 now = cv::getTickCount();
    time_queue.push(now);    
    int64 then;

   
   if (imageV.empty())
       imageV = Mat(mbgra.rows/2, mbgra.cols/2, CV_8UC1);

   if (imageU.empty())
       imageU = Mat(mbgra.rows/2, mbgra.cols/2, CV_8UC1);
 
   Mat VU = srcNV21( cv::Rect( 0, mbgra.rows, mbgra.cols, mbgra.rows/2) );

   extractVU( VU, imageV, imageU);
   
   
#if DEBUG
   LOGI("VU.size() = [%d, %d]", VU.size().height, VU.size().width);
   LOGI("imageV.size() = [%d, %d]", imageV.size().height, imageV.size().width);
#endif



/***********************************************************************************************/
 
    if (pMode == 0){

    	cvtColor( srcNV21(cv::Rect(0, 0, mbgra.cols, mbgra.rows) ), mbgra, CV_GRAY2BGRA);

    }

    else if (pMode == 1){

          Mat imageV_scaled;
          pyrUp(imageV, imageV_scaled, Size(mbgra.cols, mbgra.rows));

#if DEBUG
  LOGI("imageV_scaled.size() = [%d, %d]", imageV_scaled.size().height, imageV_scaled.size().width);
#endif

    	  cvtColor( imageV_scaled, mbgra, CV_GRAY2BGRA);
        //cvtColor(srcNV21(cv::Rect(0,0,bitmapInfo.width,bitmapInfo.height)), mbgra, CV_GRAY2BGRA);

    }

    else if (pMode == 2){

          Mat imageU_scaled;
          pyrUp(imageU, imageU_scaled, Size(mbgra.cols, mbgra.rows));
    	  cvtColor( imageU_scaled, mbgra, CV_GRAY2BGRA);

    }

    else {

        cvtColor(srcNV21(cv::Rect(0,0, mbgra.cols, mbgra.rows)), mbgra, CV_GRAY2BGRA);

    }
    
    
    // FPS log routine
    if (time_queue.size() >= 2)
        then = time_queue.front();
    else
        then = 0;

    if (time_queue.size() >= 30)
        time_queue.pop();

    fps = time_queue.size() * (float)cv::getTickFrequency() / (now-then);
    
    char buffer[128];
    sprintf(buffer, "fps: %dx%d @ %.3f", mbgra.cols, mbgra.rows, fps);
    cv::putText(mbgra, std::string(buffer), cv::Point(8,64),
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(12,255,12,255));

}



#ifdef __cplusplus
extern "C" {
#endif



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

   /// Access source array data... byte to Mat 
   jbyte* source = (jbyte*)pEnv->GetPrimitiveArrayCritical(pSource, 0);
   if (source == NULL) abort();

   Mat srcNV21(bitmapInfo.height + bitmapInfo.height/2, bitmapInfo.width, CV_8UC1, (unsigned char *)source);
   Mat mbgra(bitmapInfo.height, bitmapInfo.width, CV_8UC4, (unsigned char *)bitmapContent);

#if DEBUG
   LOGI("bitmapInfo.size() = [%d, %d]", bitmapInfo.height, bitmapInfo.width);
   LOGI("srcNV21.size() = [%d, %d]", srcNV21.size().height, srcNV21.size().width);
#endif


// Process Camera Frame:
/***********************************************************************************************/


	onCameraFrame(srcNV21, mbgra, pMode);


/***********************************************************************************************/
 
   
   /// Release Java byte buffer and unlock backing bitmap
   pEnv-> ReleasePrimitiveArrayCritical(pSource,source,0);
   if (AndroidBitmap_unlockPixels(pEnv, pTarget) < 0) abort();

}

#ifdef __cplusplus
}
#endif
