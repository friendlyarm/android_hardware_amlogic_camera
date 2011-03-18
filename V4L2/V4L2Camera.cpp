#define LOG_NDEBUG 0
#define NDEBUG 0

#define LOG_TAG "V4L2Camera"
#include <utils/Log.h>
#include "V4L2Camera.h"
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <jpegenc/amljpeg_enc.h>
#include <cutils/properties.h>


extern "C" {	
int set_white_balance(int camera_fd,const char *swb);
int SetExposure(int camera_fd,const char *sbn);
int set_effect(int camera_fd,const char *sef);

}

namespace android {

static void dump_to_file(const char *fname,uint8_t *buf, uint32_t size);


#define V4L2_PREVIEW_BUFF_NUM (2)
#define V4L2_TAKEPIC_BUFF_NUM (1)
#define V4L2_JPEG_QUALITY	(90)


V4L2Camera::V4L2Camera(char* devname)
{
	int namelen = strlen(devname)+1;
	m_pDevName = new char[namelen];
	strcpy(m_pDevName,devname);
	m_iDevFd = -1;

	m_V4L2BufNum = 0;
	pV4L2Frames = NULL;
	pV4L2FrameSize = NULL;
	m_iPicIdx = -1;
	m_v4l2_qulity = 90;
}
V4L2Camera::~V4L2Camera()
{
	delete m_pDevName;
}
static int opengt2005Flag=0;

status_t	V4L2Camera::Open()
{
int temp_id=-1;
char camera_b09[PROPERTY_VALUE_MAX];
	
	property_get("camera.b09", camera_b09, "camera");

	if(strcmp(camera_b09,"1")==0){
		LOGD("*****do camera_b09 special  %s\n",camera_b09);
		if(strcasecmp(m_pDevName,"/dev/video0")==0)
    	{
    	opengt2005Flag=1;
    	}
		if((strcasecmp(m_pDevName,"/dev/video1")==0)&&(!opengt2005Flag)&&(m_iDevFd == -1))
		{
		  temp_id = open("/dev/video0", O_RDWR);
		  if (temp_id != -1)
		  	{
		  	LOGD("*****open %s success %d \n", "video0+++",temp_id);
			opengt2005Flag=1;
			close(temp_id);
			usleep(100);
			}
		  }
		}
	if(m_iDevFd == -1)
	{
		m_iDevFd = open(m_pDevName, O_RDWR);
    	if (m_iDevFd != -1)
		{
    		//LOGD("open %s success %d \n", m_pDevName,m_iDevFd);
      		return NO_ERROR;
    	}
		else
		{
			LOGD("open %s fail\n", m_pDevName);
			return UNKNOWN_ERROR;
		}
	}

	return NO_ERROR;
}
status_t	V4L2Camera::Close()
{
	if(m_iDevFd != -1)
	{
		close(m_iDevFd);
		m_iDevFd = -1;
	}
	return NO_ERROR;
}
status_t	V4L2Camera::InitParameters(CameraParameters& pParameters)
{
	//set the limited & the default parameter
//==========================must set parameter for CTS will check them
	pParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,CameraParameters::PIXEL_FORMAT_YUV420SP);
	pParameters.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);

	pParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
	pParameters.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);

	pParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,"15,20");
	pParameters.setPreviewFrameRate(15);

	pParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,"640x480");
	pParameters.setPreviewSize(640, 480);

	pParameters.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, "800x600");
	pParameters.setPictureSize(800,600);

	pParameters.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES,CameraParameters::FOCUS_MODE_AUTO);		
	pParameters.set(CameraParameters::KEY_FOCUS_MODE,CameraParameters::FOCUS_MODE_AUTO);

	pParameters.set(CameraParameters::KEY_FOCAL_LENGTH,"4.31");

	pParameters.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE,"54.8");
	pParameters.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE,"42.5");

//==========================

	pParameters.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE,"auto,daylight,incandescent,fluorescent");
	pParameters.set(CameraParameters::KEY_WHITE_BALANCE,"auto");
	
	pParameters.set(CameraParameters::KEY_SUPPORTED_EFFECTS,"none,negative,sepia");        
	pParameters.set(CameraParameters::KEY_EFFECT,"none");

	//pParameters.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES,"auto,on,off,torch");		
	//pParameters.set(CameraParameters::KEY_FLASH_MODE,"auto");

	//pParameters.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES,"auto,night,snow");		
	//pParameters.set(CameraParameters::KEY_SCENE_MODE,"auto");



	pParameters.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION,4);		
	pParameters.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION,-4);
	pParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP,1);		
	pParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION,0);

#if 0
	pParameters.set(CameraParameters::KEY_MAX_ZOOM,3);		
	pParameters.set(CameraParameters::KEY_ZOOM_RATIOS,"100,120,140,160,200,220,150,280,290,300");
	pParameters.set(CameraParameters::KEY_ZOOM_SUPPORTED,CameraParameters::TRUE);
	pParameters.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED,1);
	pParameters.set(CameraParameters::KEY_ZOOM,1);
#endif

	return NO_ERROR;
}

//write parameter to v4l2 driver
status_t	V4L2Camera::SetParameters(CameraParameters& pParameters)
{
	m_hParameter = pParameters;
	int preview_width, preview_height,preview_FrameRate;
	const char *white_balance=NULL;
	const char *exposure=NULL;
	const char *effect=NULL;
	//const char *night_mode=NULL;
	const char *qulity=NULL;
	int n=0;
	

	pParameters.getPreviewSize(&preview_width, &preview_height); 
    //LOGV("getPreviewSize %dx%d ",preview_width,preview_height); 

	//cts need 320*240 size
#if 0
	if(preview_width >800&&preview_height >600)
		pParameters.setPreviewSize(800, 600);
	else if(preview_width <800&&preview_height <600&&preview_width>640&&preview_height>480)
		pParameters.setPreviewSize(640, 480);
	else if(preview_width <640&&preview_height <480&&preview_width>352&&preview_height>288)
		pParameters.setPreviewSize(352, 288);
	else if(preview_width <352&&preview_height <288&&preview_width>176&&preview_height>144)
		pParameters.setPreviewSize(176, 144);
#endif
	
	white_balance=pParameters.get(CameraParameters::KEY_WHITE_BALANCE);
   // LOGV("white_balance=%s ",white_balance); 
	

	exposure=pParameters.get(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    //LOGV("exposure=%s ",exposure); 
	effect=pParameters.get(CameraParameters::KEY_EFFECT);
    //LOGV("effect=%s ",effect); 
	//night_mode=pParameters.get(CameraParameters::KEY_SCENE_MODE);
    //LOGV("night_mode=%s ",night_mode); 
	qulity=pParameters.get(CameraParameters::KEY_JPEG_QUALITY);
    //LOGV("qulity=%s ",qulity); 
	if(exposure)
		SetExposure(m_iDevFd,exposure);
	if(white_balance)
		set_white_balance(m_iDevFd,white_balance);
	if(effect)
		set_effect(m_iDevFd,effect);
	//if(night_mode)
		//set_night_mode(night_mode);
	if(qulity){
		if(strcasecmp(qulity,"70")==0)
			m_v4l2_qulity=70;	
		else if(strcasecmp(qulity,"80")==0)
			m_v4l2_qulity=80;
		else if(strcasecmp(qulity,"90")==0)
			m_v4l2_qulity=90;
		else		
			m_v4l2_qulity=90;
		}
		
	//LOGD("V4L2Camera::SetParameters");
	return NO_ERROR;
}

status_t	V4L2Camera::StartPreview()
{
	int w,h;
	m_hParameter.getPreviewSize(&w,&h);
	if( (NO_ERROR == V4L2_BufferInit(w,h,V4L2_PREVIEW_BUFF_NUM,V4L2_PIX_FMT_RGB565X))
		&& (V4L2_StreamOn() == NO_ERROR))
		return NO_ERROR;
	else
		return UNKNOWN_ERROR;
}
status_t	V4L2Camera::StopPreview()
{
	if( (NO_ERROR == V4L2_StreamOff())
		&& (V4L2_BufferUnInit() == NO_ERROR))
		return NO_ERROR;
	else
		return UNKNOWN_ERROR;
}

status_t	V4L2Camera::TakePicture()
{
	int w,h;
	m_hParameter.getPictureSize(&w,&h);
	V4L2_BufferInit(w,h,V4L2_TAKEPIC_BUFF_NUM,V4L2_PIX_FMT_RGB24);
	V4L2_StreamOn();
	m_iPicIdx = V4L2_BufferDeQue();
	V4L2_StreamOff();
	return NO_ERROR;
}

status_t	V4L2Camera::TakePictureEnd()
{
	m_iPicIdx = -1;
	return 	V4L2_BufferUnInit();
}

status_t	V4L2Camera::GetPreviewFrame(uint8_t* framebuf)
{
	//LOGD("V4L2Camera::GetPreviewFrame\n");
	int idx = V4L2_BufferDeQue();
	memcpy((char*)framebuf,pV4L2Frames[idx],pV4L2FrameSize[idx]);
	V4L2_BufferEnQue(idx);
	return NO_ERROR;	
}

status_t	V4L2Camera::GetRawFrame(uint8_t* framebuf) 
{
	if(m_iPicIdx!=-1)
	{
		memcpy(framebuf,pV4L2Frames[m_iPicIdx],pV4L2FrameSize[m_iPicIdx]);
	}
	else
		LOGD("GetRawFraem index -1");
	return NO_ERROR;	
}
status_t	V4L2Camera::GetJpegFrame(uint8_t* framebuf)
{
	if(m_iPicIdx!=-1)
	{
		jpeg_enc_t enc;
		m_hParameter.getPictureSize(&enc.width,&enc.height);
		enc.idata = (unsigned char*)pV4L2Frames[m_iPicIdx];	
		enc.odata = (unsigned char*)framebuf;
		enc.ibuff_size =  pV4L2FrameSize[m_iPicIdx];
		enc.obuff_size =  pV4L2FrameSize[m_iPicIdx];
		enc.quality = m_v4l2_qulity;
		encode_jpeg(&enc);
	}
	else
		LOGD("GetRawFraem index -1");
	return NO_ERROR;		
}


//===============================
//functions for set V4L2
status_t V4L2Camera::V4L2_BufferInit(int Buf_W,int Buf_H,int Buf_Num,int colorfmt)
{
	struct v4l2_format hformat;
	memset(&hformat,0,sizeof(v4l2_format));
	hformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	hformat.fmt.pix.width = Buf_W;
	hformat.fmt.pix.height = Buf_H;
	hformat.fmt.pix.pixelformat = colorfmt;
//	LOGD("V4L2_BufferInit::Set Video Size %d,%d",Buf_W,Buf_H);
	if (ioctl(m_iDevFd, VIDIOC_S_FMT, &hformat) == -1) 
	{
		return UNKNOWN_ERROR;
	}

	//requeset buffers in V4L2
	v4l2_requestbuffers hbuf_req;
	memset(&hbuf_req,0,sizeof(v4l2_requestbuffers));
	hbuf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	hbuf_req.memory = V4L2_MEMORY_MMAP;
	hbuf_req.count = Buf_Num; //just set two frames for hal have cache buffer
	if (ioctl(m_iDevFd, VIDIOC_REQBUFS, &hbuf_req) == -1) 
	{
		LOGD("Req V4L2 buffer fail");
		return UNKNOWN_ERROR;
	}
	else
	{
		if (hbuf_req.count < Buf_Num) 
		{
		    LOGD("Req V4L2 buffer Fail");
			return UNKNOWN_ERROR;
		}
		else//memmap these buffer to user space
		{
			pV4L2Frames = (void**)new int[Buf_Num];
			pV4L2FrameSize = new int[Buf_Num];
			int i = 0;
			v4l2_buffer hbuf_query;
			memset(&hbuf_query,0,sizeof(v4l2_buffer));

			hbuf_query.type = hbuf_req.type;
			hbuf_query.memory = V4L2_MEMORY_MMAP;
			for(;i<Buf_Num;i++)
			{
				hbuf_query.index = i;
				if (ioctl(m_iDevFd, VIDIOC_QUERYBUF, &hbuf_query) == -1) 
				{
					LOGD("Memap V4L2 buffer Fail");
					return UNKNOWN_ERROR;
				}

				pV4L2FrameSize[i] = hbuf_query.length;
				LOGD("V4L2_BufferInit::Get Buffer Idx %d Len %d",i,pV4L2FrameSize[i]);
				pV4L2Frames[i] = mmap(NULL,pV4L2FrameSize[i],PROT_READ | PROT_WRITE,MAP_SHARED,m_iDevFd,hbuf_query.m.offset);
				if(pV4L2Frames[i] == MAP_FAILED)
				{
					LOGD("Memap V4L2 buffer Fail");
					return UNKNOWN_ERROR;
				}
				//enqueue buffer
				if (ioctl(m_iDevFd, VIDIOC_QBUF, &hbuf_query) == -1) 
				{
					LOGD("GetPreviewFrame nque buffer fail");
					return UNKNOWN_ERROR;
			    }
			}
			m_V4L2BufNum = Buf_Num;
		}
	}
	return NO_ERROR;
}

status_t V4L2Camera::V4L2_BufferUnInit()
{
	if(m_V4L2BufNum > 0)
	{
		//un-memmap
		int i = 0;
		for (; i < m_V4L2BufNum; i++) 
		{
			munmap(pV4L2Frames[i], pV4L2FrameSize[i]);
			pV4L2Frames[i] = NULL;
			pV4L2FrameSize[i] = 0;
		}
		m_V4L2BufNum = 0;
		delete pV4L2Frames;
		delete pV4L2FrameSize;
		pV4L2FrameSize = NULL;
		pV4L2Frames = NULL;
	}
	return NO_ERROR;
}

status_t V4L2Camera::V4L2_BufferEnQue(int idx)
{
	v4l2_buffer hbuf_query;
	memset(&hbuf_query,0,sizeof(v4l2_buffer));
	hbuf_query.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	hbuf_query.memory = V4L2_MEMORY_MMAP;//加和不加index有什么区别?
	hbuf_query.index = idx;
    if (ioctl(m_iDevFd, VIDIOC_QBUF, &hbuf_query) == -1) 
	{
		LOGD("V4L2_BufferEnQue fail");
		return UNKNOWN_ERROR;
    }

	//LOGD("V4L2_BufferEnQue success");
	return NO_ERROR;
}
int  V4L2Camera::V4L2_BufferDeQue()
{
//	LOGD("V4L2_BufferDeQue ");
	v4l2_buffer hbuf_query;
	memset(&hbuf_query,0,sizeof(v4l2_buffer));
	hbuf_query.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	hbuf_query.memory = V4L2_MEMORY_MMAP;//加和不加index有什么区别?
    if (ioctl(m_iDevFd, VIDIOC_DQBUF, &hbuf_query) == -1) 
	{
		LOGD("V4L2_StreamGet Deque buffer fail");
		return UNKNOWN_ERROR;
    }
	//LOGD("V4L2_StreamGet bufferidx %d\n",hbuf_query.index);
	assert (hbuf_query.index < m_V4L2BufNum);
	return hbuf_query.index;	
}

status_t	V4L2Camera::V4L2_StreamOn()
{
	//LOGD("V4L2_StreamOn");
	int stream_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(m_iDevFd, VIDIOC_STREAMON, &stream_type) == -1)
		LOGD("V4L2_StreamOn Fail");
	//LOGD("V4L2_StreamOn Succes");
	return NO_ERROR;
}

status_t	V4L2Camera::V4L2_StreamOff()
{
	//LOGD("V4L2_StreamOff");
	int stream_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(m_iDevFd, VIDIOC_STREAMOFF, &stream_type) == -1)
		LOGD("V4L2_StreamOff  Fail");
	//else
		//LOGD("V4L2_StreamOff  Success");
	return NO_ERROR;
}

//extern CameraInterface* HAL_GetFakeCamera();
extern CameraInterface* HAL_GetCameraInterface(int Id)
{
	LOGD("HAL_GetCameraInterface return V4L2 interface");
	if(Id == 0)
		return new V4L2Camera("/dev/video0");
	else
		return new V4L2Camera("/dev/video1");
}



#if 0
//debug funtctions
static void dump_to_file(const char *fname,uint8_t *buf, uint32_t size)
{
    int nw, cnt = 0;
    uint32_t written = 0;

    LOGV("opening file [%s]\n", fname);
    int fd = open(fname, O_RDWR | O_CREAT);
    if (fd < 0) {
        LOGE("failed to create file [%s]: %s", fname, strerror(errno));
        return;
    }

    LOGV("writing %d bytes to file [%s]\n", size, fname);
    while (written < size) {
        nw = ::write(fd,
                     buf + written,
                     size - written);
        if (nw < 0) {
            LOGE("failed to write to file [%s]: %s",
                 fname, strerror(errno));
            break;
        }
        written += nw;
        cnt++;
    }
    LOGV("done writing %d bytes to file [%s] in %d passes\n",
         size, fname, cnt);
    ::close(fd);
}
#endif

};
