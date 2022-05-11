/**
 *  Copyright 2019 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 */

#ifndef AAC_DECODE_UTIL_h
#define AAC_DECODE_UTIL_h

#include <AudioDecodeUtil.h>

namespace android {

class AacDecodeUtil  : public AudioDecodeUtil {
    public:
        class IntfImpl;

        AacDecodeUtil(std::string & codecName, const std::shared_ptr<IntfImpl> &intfImpl);
        virtual ~AacDecodeUtil();
        virtual c2_status_t getLibName(const char ** lib, const char ** optionalLib) override;
        virtual uint32_t getFrameHdrBufLen() override;
        virtual uint32_t getOutBufferLen() override;
        virtual c2_status_t checkFrameHeader(unsigned char * pBuffer, size_t length, uint32_t *pOffset) override;
        virtual c2_status_t parseFrame(uint8_t * pBuffer, int len, UniaDecFrameInfo *info) override;
        virtual c2_status_t getDecoderProp(AUDIOFORMAT *formatType, bool *isHwBased) override;
        virtual c2_status_t handleBOS(uint32_t* offset, uint32_t length) override;
        virtual c2_status_t handleEOS(uint8_t **ppBuffer, uint32_t* length) override;
        virtual c2_status_t setParameter(UA_ParaType index,int32_t value) override;
        virtual c2_status_t getParameter(UA_ParaType index,int32_t * value) override;
        virtual size_t getPushModeInputLen() override;

    private:
        bool bFrameChecked;
        std::shared_ptr<IntfImpl> mIntf;

};

}
#endif


