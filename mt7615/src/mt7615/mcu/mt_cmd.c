#ifdef MTK_LICENSE
/*
 ***************************************************************************
 * MediaTek Inc.
 *
 * All rights reserved. source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of MediaTek. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of MediaTek, Inc. is obtained.
 ***************************************************************************

	Module Name:
	mt_cmd.c
*/
#endif /* MTK_LICENSE */
#ifdef COMPOS_WIN
#include "MtConfig.h"
#if defined(EVENT_TRACING)
#include "mt_cmd.tmh"
#endif
#elif defined (COMPOS_TESTMODE_WIN)
#include "config.h"
#else
#include "rt_config.h"
#endif

#if defined(RLM_CAL_CACHE_SUPPORT) || defined(PRE_CAL_TRX_SET2_SUPPORT)
#include "phy/rlm_cal_cache.h"
#endif /* defined(RLM_CAL_CACHE_SUPPORT) || defined(PRE_CAL_TRX_SET2_SUPPORT) */

//#include "hdev/hdev.h"

//
// Define for MtCmdTx retry count
//
#define MT_CMD_RETRY_COUNT 5
#define MT_CMD_RETRY_INTEVAL 200    // usec

UINT16 GetRealPortQueueID(struct cmd_msg *msg, UINT8 cmd_type)
{
	RTMP_ADAPTER *ad = (RTMP_ADAPTER *)(msg->priv);

	if (ad->chipCap.hif_type == HIF_MT)
    {
		if (cmd_type == MT_FW_SCATTER)
        {
			return (UINT16)ad->chipCap.PDA_PORT;
		}
        else
        {
			return (UINT16)P1_Q0;
		}
	}
	return (UINT16)CPU_TX_PORT;
}


VOID MtAndesFreeCmdMsg(struct cmd_msg *msg)
{
#if defined(COMPOS_WIN) || defined(COMPOS_TESTMODE_WIN)
#ifdef COMPOS_WIN
	os_free_mem(msg);
#endif /* COMPOS_WIN */
#ifdef COMPOS_TESTMODE_WIN
	TestModeFreeCmdMsg(msg);
#endif /* COMPOS_TESTMODE_WIN */
#else
	AndesFreeCmdMsg(msg);
#endif
}


struct cmd_msg *MtAndesAllocCmdMsg(RTMP_ADAPTER *ad, unsigned int length)
{
#ifdef COMPOS_WIN
	RTMP_CHIP_CAP *cap = &ad->chipCap;
	struct cmd_msg *msg = NULL;
	VOID *net_pkt = NULL;
	UINT16 AllocateSize = cap->cmd_header_len + length;

    if (AllocateSize < HW_MSG_CMD_BUFFER_LEN)
    {
        msg = (struct cmd_msg *)MtGetMsgCmdBuffer(ad);

        if (msg == NULL)
        {
            return NULL;
        }

        net_pkt = msg->net_pkt;

    }
    else
    {
	    os_alloc_mem(ad, (CHAR **)&net_pkt, AllocateSize);

    	if (!net_pkt)
    	{
    		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                                ("can not allocate net_pkt\n"));
    		return NULL;
    	}

    	os_alloc_mem(ad, (CHAR **)&msg, sizeof(*msg));

    	if (!msg)
    	{
    		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                                ("can not allocate cmd msg\n"));
    		os_free_mem(net_pkt);
    		return NULL;
    	}
	}

	os_zero_mem(msg, sizeof(*msg));

	msg->net_pkt = net_pkt;
	msg->priv = (void *)ad;
	msg->cmd_tx_len = AllocateSize;
	return msg;

#elif defined(COMPOS_TESTMODE_WIN)
	return TMAllocCmdMsg(ad,length);//Test Mode WDM
#else
	return AndesAllocCmdMsg(ad,length);
#endif
}


VOID MtAndesInitCmdMsg(struct cmd_msg *msg, CMD_ATTRIBUTE attr)
{
#if defined (COMPOS_WIN) || defined(COMPOS_TESTMODE_WIN)
	FW_TXD *fw_txd = msg->net_pkt;
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)msg->priv;

    /* Msg data */
    SET_CMD_MSG_PORT_QUEUE_ID(msg, GetRealPortQueueID(msg, attr.type));
    SET_CMD_MSG_MCU_DEST(msg, attr.mcu_dest);
    SET_CMD_MSG_TYPE(msg, attr.type);
    SET_CMD_MSG_CTRL_FLAGS(msg, attr.ctrl.flags);
    SET_CMD_MSG_EXT_TYPE(msg, attr.ext_type);

    SET_CMD_MSG_CTRL_RSP_WAIT_MS_TIME(msg, attr.ctrl.wait_ms_time);
    SET_CMD_MSG_RETRY_TIMES(msg, 0);

    SET_CMD_MSG_CTRL_RSP_EXPECT_SIZE(msg, attr.ctrl.expect_size);

    SET_CMD_MSG_RSP_WB_BUF_IN_CALBK(msg, attr.rsp.wb_buf_in_calbk);
    SET_CMD_MSG_RSP_HANDLER(msg, attr.rsp.handler);


	/* Fill FW CMD Header */
	fw_txd->fw_txd_0.field.length = msg->cmd_tx_len;
	fw_txd->fw_txd_0.field.pq_id = GetRealPortQueueID(msg, attr.type);
	fw_txd->fw_txd_1.field.cid = attr.type;
	fw_txd->fw_txd_1.field.pkt_type_id = PKT_ID_CMD;
    fw_txd->fw_txd_1.field.set_query = IS_CMD_ATTR_NA_FLAG_SET(attr)
                    ? CMD_NA : (IS_CMD_ATTR_SET_QUERY_FLAG_SET(attr));
#ifdef MT7615
	fw_txd->fw_txd_1.field1.pkt_ft = TMI_PKT_FT_HIF_CMD;
#endif /* MT7615 */

	fw_txd->fw_txd_2.field.ext_cid = attr.ext_type;

    if ((IS_EXT_CMD_AND_SET_NEED_RSP(msg)) && !(IS_CMD_MSG_NA_FLAG_SET(msg)))
	{
		fw_txd->fw_txd_2.field.ext_cid_option = EXT_CID_OPTION_NEED_ACK;
	}
	else
	{
		fw_txd->fw_txd_2.field.ext_cid_option = EXT_CID_OPTION_NO_NEED_ACK;
	}

    fw_txd->fw_txd_0.word = cpu2le32(fw_txd->fw_txd_0.word);
    fw_txd->fw_txd_1.word = cpu2le32(fw_txd->fw_txd_1.word);
    fw_txd->fw_txd_2.word = cpu2le32(fw_txd->fw_txd_2.word);

#else
    AndesInitCmdMsg(msg, attr);
#endif
}


VOID MtAndesAppendCmdMsg(struct cmd_msg *msg, CHAR *data, UINT32 len)
{
#if defined(COMPOS_WIN) || defined(COMPOS_TESTMODE_WIN)
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER*) msg->priv;
	CHAR *payload = (CHAR*)msg->net_pkt +
            pAd->chipCap.cmd_header_len + msg->current_pos;
	msg->current_pos += len;
	os_move_mem(payload,data,len);
#else
	AndesAppendCmdMsg(msg,data,len);
#endif
}

#ifdef SINGLE_SKU_V2

VOID mt_FillSkuParameter(RTMP_ADAPTER *pAd,UINT8 channel, UCHAR Band, UCHAR TxStream, UINT8 *txPowerSku)
{
	CH_POWER *ch, *ch_temp;
	UCHAR start_ch;
	UINT8 i,j;
	UINT8 TxOffset = 0;
    UCHAR band_local = 0;

    /* -----------------------------------------------------------------------------------------------------------------------*/
    /* This part is due to MtCmdChannelSwitch is not ready for 802.11j and variable channel_band is always 0                  */
    /* -----------------------------------------------------------------------------------------------------------------------*/
    
    if (channel >= 16) // must be 5G
    {
        band_local = 1;
    }
    else if ((channel <= 14) && (channel >= 8)) // depends on "channel_band" in MtCmdChannelSwitch
    {
        band_local = Band;
    }
    else if (channel <=8) // must be 2.4G
    {
        band_local = 0;
    }
	
	DlListForEachSafe(ch, ch_temp, &pAd->SingleSkuPwrList, CH_POWER, List)
	{
		start_ch = ch->StartChannel;

		//if (channel >= start_ch)
		//{
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: channel = %d, start_ch = %d , Band = %d\n", __FUNCTION__, channel, start_ch, Band));
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: ch->num = %d\n", __FUNCTION__, ch->num));

			for (j = 0; j < ch->num; j++)
			{

				MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: In for loop, channel = %d, ch->Channel[%d] = %d\n", __FUNCTION__, channel, j, ch->Channel[j]));

                if (Band == ch->band)
                {
                    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: Band check, ch->Channel[%d] = %d \n", __FUNCTION__, j, ch->Channel[j]));
    
        			if (channel == ch->Channel[j])
        			{	
    					for (i = 0; i < SINGLE_SKU_TABLE_CCK_LENGTH; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("CCK[%d]: 0x%x \n", i, ch->PwrCCK[i]));

    					for (i = 0; i < SINGLE_SKU_TABLE_OFDM_LENGTH; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("OFDM[%d]: 0x%x \n", i, ch->PwrOFDM[i]));

    					for (i = 0; i < SINGLE_SKU_TABLE_VHT_LENGTH; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("VHT20[%d]: 0x%x \n", i, ch->PwrVHT20[i]));

    					for (i = 0; i < SINGLE_SKU_TABLE_VHT_LENGTH; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("VHT40[%d]: 0x%x \n", i, ch->PwrVHT40[i]));

    					for (i = 0; i < SINGLE_SKU_TABLE_VHT_LENGTH; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("VHT80[%d]: 0x%x \n", i, ch->PwrVHT80[i]));

    					for (i = 0; i < SINGLE_SKU_TABLE_VHT_LENGTH; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("VHT160[%d]: 0x%x \n", i, ch->PwrVHT160[i]));

    					for (i = 0; i < SINGLE_SKU_TABLE_TX_OFFSET_NUM; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("TxStreamDelta(%dT): 0x%x (ref to 4T)\n", (3-i), ch->PwrTxStreamDelta[i]));			

                        for (i = 0; i < SINGLE_SKU_TABLE_NSS_OFFSET_NUM; i++)
                            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("TxNSSDelta(%dSS): 0x%x (ref to 4SS)\n", i, ch->PwrTxNSSDelta[i]));      


    					MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("TxStream = %d \n", TxStream));

    					/* check the TxStream 1T/2T/3T/4T*/
    					if (TxStream == 1)
    					{
    						TxOffset = ch->PwrTxStreamDelta[2];
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("ch->PwrTxStreamDelta[2] = %d \n", ch->PwrTxStreamDelta[2]));
    					}	
    					else if (TxStream == 2)
    					{
    						TxOffset = ch->PwrTxStreamDelta[1];
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("ch->PwrTxStreamDelta[1] = %d \n", ch->PwrTxStreamDelta[1]));
    					}
    					else if (TxStream == 3)
    					{
    						TxOffset = ch->PwrTxStreamDelta[0];
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("ch->PwrTxStreamDelta[0] = %d \n", ch->PwrTxStreamDelta[0]));
    					}
    					else if (TxStream == 4)
    					{
    						TxOffset = 0;
    					}
    					else
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("The TxStream value is invalid. \n"));

    					MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("TxOffset = %d \n", TxOffset));

    					/* Fill in the SKU table for destination channel*/
        				txPowerSku[SKU_CCK_1_2] 	   = ch->PwrCCK[0]    ?  (ch->PwrCCK[0]    + TxOffset) : 0x3F;
            			txPowerSku[SKU_CCK_55_11] 	   = ch->PwrCCK[1]    ?  (ch->PwrCCK[1]    + TxOffset) : 0x3F;
        				txPowerSku[SKU_OFDM_6_9] 	   = ch->PwrOFDM[0]   ?  (ch->PwrOFDM[0]   + TxOffset) : 0x3F;
            			txPowerSku[SKU_OFDM_12_18]     = ch->PwrOFDM[1]   ?  (ch->PwrOFDM[1]   + TxOffset) : 0x3F;
            			txPowerSku[SKU_OFDM_24_36]     = ch->PwrOFDM[2]   ?  (ch->PwrOFDM[2]   + TxOffset) : 0x3F;
            			txPowerSku[SKU_OFDM_48]        = ch->PwrOFDM[3]   ?  (ch->PwrOFDM[3]   + TxOffset) : 0x3F;
            			txPowerSku[SKU_OFDM_54]        = ch->PwrOFDM[4]   ?  (ch->PwrOFDM[4]   + TxOffset) : 0x3F;
        				txPowerSku[SKU_HT20_0_8]       = ch->PwrVHT20[0]  ?  (ch->PwrVHT20[0]  + TxOffset) : 0x3F;
        				/*MCS32 is a special rate will chose the max power, normally will be OFDM 6M */
        				txPowerSku[SKU_HT20_32] 	   =  ch->PwrOFDM[0]  ?  (ch->PwrOFDM[0]   + TxOffset) : 0x3F;
        				txPowerSku[SKU_HT20_1_2_9_10]  = ch->PwrVHT20[1]  ?  (ch->PwrVHT20[1]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT20_3_4_11_12] = ch->PwrVHT20[2]  ?  (ch->PwrVHT20[2]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT20_5_13] 	   = ch->PwrVHT20[3]  ?  (ch->PwrVHT20[3]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT20_6_14] 	   = ch->PwrVHT20[3]  ?  (ch->PwrVHT20[3]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT20_7_15] 	   = ch->PwrVHT20[4]  ?  (ch->PwrVHT20[4]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_HT40_0_8] 	   = ch->PwrVHT40[0]  ?  (ch->PwrVHT40[0]  + TxOffset) : 0x3F;
        				/*MCS32 is a special rate will chose the max power, normally will be OFDM 6M */
        				txPowerSku[SKU_HT40_32] 	   =  ch->PwrOFDM[0]  ?  (ch->PwrOFDM[0]   + TxOffset) : 0x3F;
        				txPowerSku[SKU_HT40_1_2_9_10]  = ch->PwrVHT40[1]  ?  (ch->PwrVHT40[1]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT40_3_4_11_12] = ch->PwrVHT40[2]  ?  (ch->PwrVHT40[2]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT40_5_13] 	   = ch->PwrVHT40[3]  ?  (ch->PwrVHT40[3]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT40_6_14] 	   = ch->PwrVHT40[3]  ?  (ch->PwrVHT40[3]  + TxOffset) : 0x3F;
            			txPowerSku[SKU_HT40_7_15] 	   = ch->PwrVHT40[4]  ?  (ch->PwrVHT40[4]  + TxOffset) : 0x3F;

    					txPowerSku[SKU_VHT20_0] 	   = ch->PwrVHT20[0]  ?  (ch->PwrVHT20[0]  + TxOffset) : 0x3F;
    					txPowerSku[SKU_VHT20_1_2] 	   = ch->PwrVHT20[1]  ?  (ch->PwrVHT20[1]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT20_3_4] 	   = ch->PwrVHT20[2]  ?  (ch->PwrVHT20[2]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT20_5_6] 	   = ch->PwrVHT20[3]  ?  (ch->PwrVHT20[3]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT20_7] 	   = ch->PwrVHT20[4]  ?  (ch->PwrVHT20[4]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT20_8] 	   = ch->PwrVHT20[5]  ?  (ch->PwrVHT20[5]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT20_9] 	   = ch->PwrVHT20[6]  ?  (ch->PwrVHT20[6]  + TxOffset) : 0x3F;

    					txPowerSku[SKU_VHT40_0] 	   = ch->PwrVHT40[0]  ?  (ch->PwrVHT40[0]  + TxOffset) : 0x3F;
    					txPowerSku[SKU_VHT40_1_2]	   = ch->PwrVHT40[1]  ?  (ch->PwrVHT40[1]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT40_3_4]	   = ch->PwrVHT40[2]  ?  (ch->PwrVHT40[2]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT40_5_6]	   = ch->PwrVHT40[3]  ?  (ch->PwrVHT40[3]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT40_7] 	   = ch->PwrVHT40[4]  ?  (ch->PwrVHT40[4]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT40_8] 	   = ch->PwrVHT40[5]  ?  (ch->PwrVHT40[5]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT40_9] 	   = ch->PwrVHT40[6]  ?  (ch->PwrVHT40[6]  + TxOffset) : 0x3F;

    					txPowerSku[SKU_VHT80_0] 	   = ch->PwrVHT80[0]  ?  (ch->PwrVHT80[0]  + TxOffset) : 0x3F;
    					txPowerSku[SKU_VHT80_1_2] 	   = ch->PwrVHT80[1]  ?  (ch->PwrVHT80[1]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT80_3_4] 	   = ch->PwrVHT80[2]  ?  (ch->PwrVHT80[2]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT80_5_6] 	   = ch->PwrVHT80[3]  ?  (ch->PwrVHT80[3]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT80_7] 	   = ch->PwrVHT80[4]  ?  (ch->PwrVHT80[4]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT80_8] 	   = ch->PwrVHT80[5]  ?  (ch->PwrVHT80[5]  + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT80_9] 	   = ch->PwrVHT80[6]  ?  (ch->PwrVHT80[6]  + TxOffset) : 0x3F;

    					txPowerSku[SKU_VHT160_0] 	   = ch->PwrVHT160[0] ?  (ch->PwrVHT160[0] + TxOffset) : 0x3F;
    					txPowerSku[SKU_VHT160_1_2] 	   = ch->PwrVHT160[1] ?  (ch->PwrVHT160[1] + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT160_3_4] 	   = ch->PwrVHT160[2] ?  (ch->PwrVHT160[2] + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT160_5_6] 	   = ch->PwrVHT160[3] ?  (ch->PwrVHT160[3] + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT160_7] 	   = ch->PwrVHT160[4] ?  (ch->PwrVHT160[4] + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT160_8] 	   = ch->PwrVHT160[5] ?  (ch->PwrVHT160[5] + TxOffset) : 0x3F;
        				txPowerSku[SKU_VHT160_9] 	   = ch->PwrVHT160[6] ?  (ch->PwrVHT160[6] + TxOffset) : 0x3F;

                        txPowerSku[SKU_1SS_Delta] 	   = ch->PwrTxNSSDelta[0] ?  ch->PwrTxNSSDelta[0] : 0x0;
                        txPowerSku[SKU_2SS_Delta] 	   = ch->PwrTxNSSDelta[1] ?  ch->PwrTxNSSDelta[1] : 0x0;
                        txPowerSku[SKU_3SS_Delta] 	   = ch->PwrTxNSSDelta[2] ?  ch->PwrTxNSSDelta[2] : 0x0;
                        txPowerSku[SKU_4SS_Delta] 	   = ch->PwrTxNSSDelta[3] ?  ch->PwrTxNSSDelta[3] : 0x0;

    					for (i = 0; i < SKU_TOTAL_SIZE; i++)
    						MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("txPowerSku[%d]: 0x%x \n", i, txPowerSku[i]));

                        break;
    			    }
			    }
		    }
	    //}
	}
}

VOID mt_FillBfBackOffParameter(RTMP_ADAPTER *pAd, UINT8 Nsstream, UINT8 *BFGainTable)
{
    BF_POWER *nss, *nss_temp;
    UCHAR start_nss;
    UINT8 i,j;
   
    DlListForEachSafe(nss, nss_temp, &pAd->BfBackOffPwrList, BF_POWER, List)
    {
        
        start_nss = nss->StartNsstream;

        if (Nsstream >= start_nss)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: Nsstream = %d, start_nss = %d \n", __FUNCTION__, Nsstream, start_nss));
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: nss->num = %d\n", __FUNCTION__, nss->num));

            for (j = 0; j < nss->num; j++)
            {

                MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: In for loop, Nsstream = %d, nss->Nsstream[%d] = %d\n", __FUNCTION__, Nsstream, j, nss->Nsstream[j]));

                if (Nsstream == nss->Nsstream[j])
                {   

                    for (i = 0; i < BF_GAIN_STREAM_SIZE; i++)
                        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("BFGainTable[%d]: 0x%x \n", i, nss->BFGain[i]));

                    /* Fill in the SKU table for destination channel*/
                    BFGainTable[BF_GAIN_1T] = nss->BFGain[0] ? nss->BFGain[0] : 0x00;
                    BFGainTable[BF_GAIN_2T] = nss->BFGain[1] ? nss->BFGain[1] : 0x00;
                    BFGainTable[BF_GAIN_3T] = nss->BFGain[2] ? nss->BFGain[2] : 0x00;
                    BFGainTable[BF_GAIN_4T] = nss->BFGain[3] ? nss->BFGain[3] : 0x00;                    

                    for (i = 0; i < BF_GAIN_STREAM_SIZE; i++)
                        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("NSS = %d, BFGainTable[%d]: 0x%x \n", Nsstream, i, BFGainTable[i]));

                    break;
                }
            }
        }
    }
}

VOID mt_FillBFBackoff(RTMP_ADAPTER *pAd,UINT8 channel, UCHAR Band, UINT8 *BFPowerBackOff)
{
    BFback_POWER *ch, *ch_temp;
    UCHAR start_ch;
    UINT8 i,j;
    UCHAR band_local = 0;

    /* -----------------------------------------------------------------------------------------------------------------------*/
    /* This part is due to MtCmdChannelSwitch is not ready for 802.11j and variable channel_band is always 0                  */
    /* -----------------------------------------------------------------------------------------------------------------------*/
    
    if (channel >= 16) // must be 5G
    {
        band_local = 1;
    }
    else if ((channel <= 14) && (channel >= 8)) // depends on "channel_band" in MtCmdChannelSwitch
    {
        band_local = Band;
    }
    else if (channel <=8 ) // must be 2.4G
    {
        band_local = 0;
    }
    
    DlListForEachSafe(ch, ch_temp, &pAd->BFBackoffList, BFback_POWER, List)
    {
        
        start_ch = ch->StartChannel;

        //if (channel >= start_ch)
        //{
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: channel = %d, start_ch = %d , Band = %d\n", __FUNCTION__, channel, start_ch, Band));
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: ch->num = %d\n", __FUNCTION__, ch->num));

            for (j = 0; j < ch->num; j++)
            {

                MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: In for loop, channel = %d, ch->Channel[%d] = %d\n", __FUNCTION__, channel, j, ch->Channel[j]));

                if (Band == ch->band)
                {
                    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: Band check, ch->Channel[%d] = %d \n", __FUNCTION__, j, ch->Channel[j]));
    
                    if (channel == ch->Channel[j])
                    {   
                        for (i = 0; i < 3; i++)
                            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("Max Power[%d]: 0x%x \n", i, ch->PwrMax[i]));

                        /* Fill in the SKU table for destination channel*/
                        BFPowerBackOff[0] = ch->PwrMax[0] ? (ch->PwrMax[0]) : 0x3F;
                        BFPowerBackOff[1] = ch->PwrMax[1] ? (ch->PwrMax[1]) : 0x3F;
                        BFPowerBackOff[2] = ch->PwrMax[2] ? (ch->PwrMax[2]) : 0x3F;
                        
                        for (i = 0; i < 3; i++)
                            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("BFPowerBackOff[%d]: 0x%x \n", i, BFPowerBackOff[i]));

                        break;
                    }
                }
            }
        //}
    }
}

#endif


static  VOID EventExtCmdResult(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	struct _EVENT_EXT_CMD_RESULT_T *EventExtCmdResult =
                            (struct _EVENT_EXT_CMD_RESULT_T *)Data;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s: EventExtCmdResult.ucExTenCID = 0x%x\n",
            __FUNCTION__, EventExtCmdResult->ucExTenCID));

	EventExtCmdResult->u4Status = le2cpu32(EventExtCmdResult->u4Status);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s: EventExtCmdResult.u4Status = 0x%x\n",
            __FUNCTION__, EventExtCmdResult->u4Status));

#ifdef LINUX
//	RTMP_OS_TXRXHOOK_CALL(WLAN_CALIB_TEST_RSP,NULL,EventExtCmdResult->u4Status,pAd);
#endif /* LINUX */
}


/*****************************************
	ExT_CID = 0x01
*****************************************/
#ifdef RTMP_EFUSE_SUPPORT
static VOID CmdEfuseAccessReadCb(struct cmd_msg *msg, char *data, UINT16 len)
{
	CMD_ACCESS_EFUSE_T *pCmdAccessEfuse = (CMD_ACCESS_EFUSE_T *)data;
	EFUSE_ACCESS_DATA_T *pEfuseValue = NULL;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                            ("%s\n", __FUNCTION__));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                        ("Address:%x,IsValied:%x\n",
                        le2cpu32(pCmdAccessEfuse->u4Address),
                        le2cpu32(pCmdAccessEfuse->u4Valid)));

	pEfuseValue = (EFUSE_ACCESS_DATA_T *)msg->attr.rsp.wb_buf_in_calbk;
	*pEfuseValue->pIsValid = le2cpu32(pCmdAccessEfuse->u4Valid);

	os_move_mem(&pEfuseValue->pValue[0],
        &pCmdAccessEfuse->aucData[0] , EFUSE_BLOCK_SIZE);
}



VOID MtCmdEfuseAccessRead(RTMP_ADAPTER *pAd, USHORT offset,
                                PUCHAR pData, PUINT pIsValid)
{
	struct cmd_msg *msg;
	CMD_ACCESS_EFUSE_T CmdAccessEfuse;
	EFUSE_ACCESS_DATA_T efuseAccessData;
	int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_ACCESS_EFUSE_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&efuseAccessData, sizeof(efuseAccessData));

	efuseAccessData.pValue = (PUSHORT)pData;
	efuseAccessData.pIsValid = pIsValid;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_EFUSE_ACCESS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(CMD_ACCESS_EFUSE_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, &efuseAccessData);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdEfuseAccessReadCb);

    MtAndesInitCmdMsg(msg, attr);

	os_zero_mem(&CmdAccessEfuse, sizeof(CmdAccessEfuse));

	CmdAccessEfuse.u4Address = (offset / EFUSE_BLOCK_SIZE)*EFUSE_BLOCK_SIZE;
#ifdef RT_BIG_ENDIAN
	CmdAccessEfuse.u4Address = cpu2le32(CmdAccessEfuse.u4Address);
#endif

	MtAndesAppendCmdMsg(msg, (char *)&CmdAccessEfuse, sizeof(CmdAccessEfuse));
	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
}


static VOID CmdEfuseAccessWriteCb(struct cmd_msg *msg, char *data, UINT16 len)
{
	CMD_ACCESS_EFUSE_T *pCmdAccessEfuse = (CMD_ACCESS_EFUSE_T *)data;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                                ("%s\n", __FUNCTION__));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                        ("Address:%x,IsValied:%x\n",
        le2cpu32(pCmdAccessEfuse->u4Address),le2cpu32(pCmdAccessEfuse->u4Valid)));
}


VOID MtCmdEfuseAccessWrite(RTMP_ADAPTER *pAd, USHORT offset,PUCHAR pData)
{

	struct cmd_msg *msg;
	CMD_ACCESS_EFUSE_T CmdAccessEfuse;
	int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_ACCESS_EFUSE_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_EFUSE_ACCESS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(CMD_ACCESS_EFUSE_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdEfuseAccessWriteCb);

    MtAndesInitCmdMsg(msg, attr);

	os_zero_mem(&CmdAccessEfuse, sizeof(CmdAccessEfuse));

	os_move_mem(&CmdAccessEfuse.aucData[0],&pData[0],EFUSE_BLOCK_SIZE);
	CmdAccessEfuse.u4Address = (offset / EFUSE_BLOCK_SIZE)*EFUSE_BLOCK_SIZE;
#ifdef RT_BIG_ENDIAN
	CmdAccessEfuse.u4Address = cpu2le32(CmdAccessEfuse.u4Address);
#endif 

	MtAndesAppendCmdMsg(msg, (char *)&CmdAccessEfuse, sizeof(CmdAccessEfuse));
	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
}


INT32 MtCmdEfuseFreeBlockCount(RTMP_ADAPTER *pAd, UINT32 GetFreeBlock, UINT32 *Value)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_EFUSE_FREE_BLOCK_T CmdEfuseFreeBlock;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s: GetFreeBlock = %x\n", __FUNCTION__, GetFreeBlock));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(struct _EXT_CMD_EFUSE_FREE_BLOCK_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_EFUSE_FREE_BLOCK);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_EFUSE_FREE_BLOCK_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, Value);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);

	os_zero_mem(&CmdEfuseFreeBlock, sizeof(CmdEfuseFreeBlock));

	CmdEfuseFreeBlock.ucGetFreeBlock = (UINT8)GetFreeBlock;

	MtAndesAppendCmdMsg(msg, (char *)&CmdEfuseFreeBlock, sizeof(CmdEfuseFreeBlock));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}
#endif /* RTMP_EFUSE_SUPPORT */


/*****************************************
	ExT_CID = 0x02
*****************************************/

static VOID CmdRFRegAccessReadCb(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	struct _CMD_RF_REG_ACCESS_T *RFRegAccess =
                            (struct _CMD_RF_REG_ACCESS_T *)Data;
	os_move_mem(msg->attr.rsp.wb_buf_in_calbk, &RFRegAccess->Data, Len - 8);
	*msg->attr.rsp.wb_buf_in_calbk = le2cpu32(*msg->attr.rsp.wb_buf_in_calbk);
}


INT32 MtCmdRFRegAccessWrite(RTMP_ADAPTER *pAd, UINT32 RFIdx,
                                UINT32 Offset, UINT32 Value)
{
	struct cmd_msg *msg;
	struct _CMD_RF_REG_ACCESS_T RFRegAccess;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
				("%s: RFIdx = %d, Offset = %x, Value = %x\n",
				__FUNCTION__, RFIdx, Offset, Value));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(struct _CMD_RF_REG_ACCESS_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_RF_REG_ACCESS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);

	os_zero_mem(&RFRegAccess, sizeof(RFRegAccess));

	RFRegAccess.WiFiStream = cpu2le32(RFIdx);
	RFRegAccess.Address = cpu2le32(Offset);
	RFRegAccess.Data = cpu2le32(Value);

	MtAndesAppendCmdMsg(msg, (char *)&RFRegAccess, sizeof(RFRegAccess));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


INT32 MtCmdRFRegAccessRead(RTMP_ADAPTER *pAd, UINT32 RFIdx,
                                UINT32 Offset, UINT32 *Value)
{
	struct cmd_msg *msg;
	struct _CMD_RF_REG_ACCESS_T RFRegAccess;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s: RFIdx = %d, Offset = %x\n", __FUNCTION__, RFIdx, Offset));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(struct _CMD_RF_REG_ACCESS_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_RF_REG_ACCESS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 12);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, Value);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdRFRegAccessReadCb);

    MtAndesInitCmdMsg(msg, attr);

	os_zero_mem(&RFRegAccess, sizeof(RFRegAccess));

	RFRegAccess.WiFiStream = cpu2le32(RFIdx);
	RFRegAccess.Address = cpu2le32(Offset);

	MtAndesAppendCmdMsg(msg, (char *)&RFRegAccess, sizeof(RFRegAccess));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


/*****************************************
	ExT_CID = 0x04
*****************************************/
//unify
//send command
static VOID MtCmdATERFTestResp(struct cmd_msg *msg, char *data, UINT16 len)
{
#ifdef CONFIG_ATE
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)msg->priv;
#endif
	EXT_EVENT_RF_TEST_RESULT_T *result = (EXT_EVENT_RF_TEST_RESULT_T *)data;
#ifdef INTERNAL_CAPTURE_SUPPORT
	EVENT_WIFI_ICAP_T *Eventdata = NULL;
	EVENT_WIFI_ICAP_T *IcapInfo = NULL;
#endif/* INTERNAL_CAPTURE_SUPPORT */
	switch (le2cpu32(result->u4FuncIndex))
    {
    	case RDD_TEST_MODE:
    	case RE_CALIBRATION:
    	case CALIBRATION_BYPASS:
            break;
#ifdef INTERNAL_CAPTURE_SUPPORT
	case GET_ICAP_WIFI_SPECTRUM:
		Eventdata = (EVENT_WIFI_ICAP_T *)data;
		IcapInfo = (EVENT_WIFI_ICAP_T *)msg->attr.rsp.wb_buf_in_calbk;
		if (IcapInfo)
        {
			IcapInfo->u4StartAddr1 = le2cpu32(Eventdata->u4StartAddr1);
			IcapInfo->u4StartAddr2 = le2cpu32(Eventdata->u4StartAddr2);
			IcapInfo->u4StartAddr3 = le2cpu32(Eventdata->u4StartAddr3);
			IcapInfo->u4EndAddr = le2cpu32(Eventdata->u4EndAddr);
			IcapInfo->u4StopAddr = le2cpu32(Eventdata->u4StopAddr);
			IcapInfo->u4Wrap = le2cpu32(Eventdata->u4Wrap);
		}
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s, GET_CAP_WIFI_SPECTRUM\n", __FUNCTION__));

		break;
#endif/* INTERNAL_CAPTURE_SUPPORT */
	default:
		break;
	}

#ifdef CONFIG_ATE
	MT_ATERFTestCB(pAd, data, len);
#endif
}

static INT32 MtCmdRfTest(RTMP_ADAPTER *pAd, CMD_TEST_CTRL_T TestCtrl,
                                    char *rsp_payload, UINT16 rsp_len)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: Action = %d\n", __FUNCTION__,TestCtrl.ucAction));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(TestCtrl));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_RF_TEST);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 5000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, rsp_len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, rsp_payload);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdATERFTestResp);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&TestCtrl, sizeof(TestCtrl));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}

//IcapLen only be used when OpMode is OPERATION_ICAP_OVERLAP
INT32 MtCmdRfTestSwitchMode(RTMP_ADAPTER *pAd, UINT32 OpMode,
                                UINT8 IcapLen, UINT16 rsp_len)
{
	INT32 ret = 0;
	CMD_TEST_CTRL_T TestCtrl;
	os_zero_mem(&TestCtrl, sizeof(TestCtrl));
	TestCtrl.ucAction = ACTION_SWITCH_TO_RFTEST;
	TestCtrl.u.u4OpMode = cpu2le32(OpMode);
	if (OPERATION_ICAP_OVERLAP == OpMode)
    {
		TestCtrl.ucIcapLen = IcapLen;
    }
    ret = MtCmdRfTest(pAd, TestCtrl, NULL, rsp_len);
	return ret;
}

#ifdef INTERNAL_CAPTURE_SUPPORT
INT32 MtCmdWifiSpectrum(
    IN	RTMP_ADAPTER *pAd, 
    IN	EXT_CMD_WIFI_SPECTRUM_CTRL_T WifiSpectrumCtrl,
    IN	CHAR *rsp_payload, 
    IN	UINT16 rsp_len)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s----------------->\n",__FUNCTION__));
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
    ("%s: FuncIndex = %d\n", __FUNCTION__, WifiSpectrumCtrl.u4FuncIndex));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(WifiSpectrumCtrl));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }
    
#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_WIFI_SPECTRUM);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 5000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, rsp_payload);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_WIFI_SPECTRUM,TRUE, 5000,
					   TRUE, TRUE, rsp_len, rsp_payload, EventExtCmdResult);
#endif

    MtAndesAppendCmdMsg(msg, (char *)&WifiSpectrumCtrl, sizeof(WifiSpectrumCtrl));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
        
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s<-----------------\n",__FUNCTION__));

    return ret;
        
error:
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));
           
    return ret;
}    

INT32 MtCmdWifiSpectrumParamSet(
    IN	RTMP_ADAPTER *pAd,
    IN	UINT32  Trigger,
    IN	UINT32  RingCapEn,
    IN	UINT32  TriggerEvent,
    IN	UINT32  CaptureNode,
    IN	UINT32  CaptureLen,
    IN	UINT32  CapStopCycle,
    IN	UINT8   BW,
    IN 	UINT32  MACTriggerEvent,
    IN	UINT32  SourceAddressLSB,
    IN 	UINT32  SourceAddressMSB,
    IN	UINT32  Band)
{
    INT32 ret = 0;	
    ICAP_WIFI_SPECTRUM_SET_STRUC_T	*pWifiSpecInfo;
    EXT_CMD_WIFI_SPECTRUM_CTRL_T WifiSpectrumCtrl;

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s----------------->\n", __FUNCTION__));

    os_zero_mem(&WifiSpectrumCtrl, sizeof(WifiSpectrumCtrl));
    WifiSpectrumCtrl.u4FuncIndex = WIFI_SPECTRUM_CTRL_FUNCID_SET_PARAMETER;
#ifdef RT_BIG_ENDIAN
	WifiSpectrumCtrl.u4FuncIndex = cpu2le32(WifiSpectrumCtrl.u4FuncIndex);
#endif

    pWifiSpecInfo = &WifiSpectrumCtrl.rWifiSpecInfo;
       
    pWifiSpecInfo->fgTrigger = cpu2le32(Trigger);
    pWifiSpecInfo->fgRingCapEn = cpu2le32(RingCapEn);
    pWifiSpecInfo->u4TriggerEvent = cpu2le32(TriggerEvent);
    pWifiSpecInfo->u4CaptureNode = cpu2le32(CaptureNode);
    pWifiSpecInfo->u4CaptureLen = cpu2le32(CaptureLen);
    pWifiSpecInfo->u4CapStopCycle = cpu2le32(CapStopCycle);
    pWifiSpecInfo->ucBW = BW;
    pWifiSpecInfo->u4MACTriggerEvent = cpu2le32(MACTriggerEvent);
    pWifiSpecInfo->u4SourceAddressLSB = cpu2le32(SourceAddressLSB);
    pWifiSpecInfo->u4SourceAddressMSB = cpu2le32(SourceAddressMSB);
    pWifiSpecInfo->u4Band = cpu2le32(Band);
    
    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s : \n prIcapInfo->fgTrigger = 0x%08x\n"
    " prIcapInfo->fgRingCapEn = 0x%08x\n prIcapInfo->u4TriggerEvent = 0x%08x\n prIcapInfo->u4CaptureNode = 0x%08x\n"
    " prIcapInfo->u4CaptureLen = 0x%08x\n prIcapInfo->u4CapStopCycle = 0x%08x\n prIcapInfo->BW = 0x%08x\n"
    " prIcapInfo->u4MACTriggerEvent = 0x%08x\n prIcapInfo->u4SourceAddressLSB = 0x%08x\n"
    " prIcapInfo->u4SourceAddressMSB = 0x%08x\n prIcapInfo->u4Band = 0x%08x\n", __FUNCTION__,
    Trigger, RingCapEn, TriggerEvent, CaptureNode,CaptureLen, CapStopCycle, BW, MACTriggerEvent,
    SourceAddressLSB, SourceAddressMSB, Band));
       
    ret = MtCmdWifiSpectrum(pAd, WifiSpectrumCtrl, NULL, WIFISPECTRUM_DEFAULT_RESP_LEN);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));
    
    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s<-----------------\n", __FUNCTION__)); 
    
    return ret;
}

INT32 MtCmdWifiSpectrumResultGet(
    IN RTMP_ADAPTER *pAd, 
    IN EVENT_WIFI_ICAP_T *WifiSpecInfo)
{
    INT32 ret ;
    PRTMP_REG_PAIR pRegCapture = NULL, pRegInterface = NULL; 

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s----------------->\n", __FUNCTION__));    

    os_alloc_mem(pAd, (UCHAR **)&pRegCapture, sizeof(RTMP_REG_PAIR)); 
    os_alloc_mem(pAd, (UCHAR **)&pRegInterface, sizeof(RTMP_REG_PAIR));

    NdisZeroMemory(pRegCapture, sizeof(RTMP_REG_PAIR));
    NdisZeroMemory(pRegInterface, sizeof(RTMP_REG_PAIR));
    
    pRegCapture->Register = RBISTCR0;
    MtCmdMultipleMacRegAccessRead(pAd, pRegCapture, 1);
    if (((pRegCapture->Value) & BIT(CR_RBIST_CAPTURE))== 0) // Stop capture
    {
		//RBISTCR10[28:26] = 3��b000
		pRegInterface->Register = RBISTCR10;
        pRegInterface->Value = ((pRegInterface->Value) & ~BITS(SYSRAM_INTF_SEL1, SYSRAM_INTF_SEL3));
        MtCmdMultipleMacRegAccessWrite(pAd, pRegInterface, 1);
		ret = NDIS_STATUS_SUCCESS;
	}
	else
	{
        ret = NDIS_STATUS_FAILURE;
	}

    os_free_mem(pRegCapture);
    os_free_mem(pRegInterface);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));
    
    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("%s<-----------------\n", __FUNCTION__)); 
    
    return ret;
}

INT32 MtCmdWifiSpectrumRawDataProc(
    IN RTMP_ADAPTER *pAd)
{
    #define DUMP_DATA_EXPIRE 5000
        
    INT32 ret = 0;
    UCHAR retval;
    UINT32 CaptureNode = 0;
    EXT_CMD_WIFI_SPECTRUM_CTRL_T WifiSpectrumCtrl;

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s----------------->\n",__FUNCTION__));

    RTMP_OS_INIT_COMPLETION(&pAd->WifiSpectrumDumpDataDone);

    os_zero_mem(&WifiSpectrumCtrl, sizeof(WifiSpectrumCtrl));

    CaptureNode = Get_Icap_WifiSpec_Capture_Node_Info(pAd);    
    
    WifiSpectrumCtrl.u4FuncIndex = WIFI_SPECTRUM_CTRL_FUNCID_DUMP_RAW_DATA;
#ifdef RT_BIG_ENDIAN
	WifiSpectrumCtrl.u4FuncIndex = cpu2le32(WifiSpectrumCtrl.u4FuncIndex);
#endif


    pAd->Srcf_IQ = RtmpOSFileOpen(pAd->Src_IQ, O_WRONLY|O_CREAT, 0);
    if (IS_FILE_OPEN_ERR(pAd->Srcf_IQ))
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("--> Error opening %s\n", pAd->Src_IQ));
        goto error;
    }
        
    pAd->Srcf_Gain = RtmpOSFileOpen(pAd->Src_Gain, O_WRONLY|O_CREAT, 0);
    if (IS_FILE_OPEN_ERR(pAd->Srcf_Gain))
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("--> Error opening %s\n", pAd->Src_Gain));
        goto error;
    }

    ret = MtCmdWifiSpectrum(pAd, WifiSpectrumCtrl, NULL, WIFISPECTRUM_DEFAULT_RESP_LEN);
    
    if (!RTMP_OS_WAIT_FOR_COMPLETION_TIMEOUT(&pAd->WifiSpectrumDumpDataDone, 
        RTMPMsecsToJiffies(DUMP_DATA_EXPIRE)))
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("Dump data timeout\n"));

        pAd->WifiSpectrumDataCounter = 0;
        
        retval = RtmpOSFileClose(pAd->Srcf_IQ);
        if (retval)
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
            ("--> Error %d closing %s\n", -retval, pAd->Src_IQ));
            goto error;
        }
        
        pAd->Srcf_IQ = NULL;
        
        retval = RtmpOSFileClose(pAd->Srcf_Gain);
        if (retval)
        {
            MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
            ("--> Error %d closing %s\n", -retval, pAd->Src_Gain));
            goto error;
        } 

        pAd->Srcf_Gain = NULL;
        
        goto error;
    }

    ret = pAd->WifiSpectrumStatus;
    
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));
    
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s<-----------------\n", __FUNCTION__));
        
    return ret;
       
error:                  
    ret = NDIS_STATUS_FAILURE;
    
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));
    
    return ret;
}

INT32 MtCmdRfTestIcapParamSet(
    IN	RTMP_ADAPTER *pAd,
    IN	UINT32  Trigger,
    IN	UINT32  RingCapEn,
    IN	UINT32  TriggerEvent,
    IN	UINT32  CaptureNode,
    IN	UINT32  CaptureLen,
    IN	UINT32  CapStopCycle,
    IN	UINT8   BW,
    IN 	UINT32  MACTriggerEvent,
    IN	UINT32  SourceAddressLSB,
    IN 	UINT32  SourceAddressMSB,
    IN	UINT32  Band)
{
    INT32 ret = 0;	
    ICAP_WIFI_SPECTRUM_SET_STRUC_T	*prIcapInfo;
    CMD_TEST_CTRL_T TestCtrl;

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s----------------->\n", __FUNCTION__));

    os_zero_mem(&TestCtrl, sizeof(TestCtrl));
    TestCtrl.ucAction = ACTION_IN_RFTEST;
	TestCtrl.u.rRfATInfo.u4FuncIndex = SET_ICAP_WIFI_SPECTRUM;  
#ifdef RT_BIG_ENDIAN
	TestCtrl.u.rRfATInfo.u4FuncIndex = cpu2le32(TestCtrl.u.rRfATInfo.u4FuncIndex);
#endif

    prIcapInfo = &TestCtrl.u.rRfATInfo.Data.rIcapInfo;
       
	prIcapInfo->fgTrigger = cpu2le32(Trigger);
	prIcapInfo->fgRingCapEn = cpu2le32(RingCapEn);
	prIcapInfo->u4TriggerEvent = cpu2le32(TriggerEvent);
	prIcapInfo->u4CaptureNode = cpu2le32(CaptureNode);
	prIcapInfo->u4CaptureLen = cpu2le32(CaptureLen);
	prIcapInfo->u4CapStopCycle = cpu2le32(CapStopCycle);
	prIcapInfo->ucBW = BW;
	prIcapInfo->u4MACTriggerEvent = cpu2le32(MACTriggerEvent);
	prIcapInfo->u4SourceAddressLSB = cpu2le32(SourceAddressLSB);
	prIcapInfo->u4SourceAddressMSB = cpu2le32(SourceAddressMSB);
	prIcapInfo->u4Band = cpu2le32(Band);
    
    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s : \n prIcapInfo->fgTrigger = 0x%08x\n"
    " prIcapInfo->fgRingCapEn = 0x%08x\n prIcapInfo->u4TriggerEvent = 0x%08x\n prIcapInfo->u4CaptureNode = 0x%08x\n"
    " prIcapInfo->u4CaptureLen = 0x%08x\n prIcapInfo->u4CapStopCycle = 0x%08x\n prIcapInfo->BW = 0x%08x\n"
    " prIcapInfo->u4MACTriggerEvent = 0x%08x\n prIcapInfo->u4SourceAddressLSB = 0x%08x\n"
    " prIcapInfo->u4SourceAddressMSB = 0x%08x\n prIcapInfo->u4Band = 0x%08x\n", __FUNCTION__,
    Trigger, RingCapEn, TriggerEvent, CaptureNode,CaptureLen, CapStopCycle, BW, MACTriggerEvent,
    SourceAddressLSB, SourceAddressMSB, Band));
       
    ret = MtCmdRfTest(pAd, TestCtrl, NULL, RF_TEST_DEFAULT_RESP_LEN);
    
    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s<-----------------\n", __FUNCTION__)); 
    
    return ret;
}

INT32 MtCmdRfTestIcapResultGet(
    IN RTMP_ADAPTER *pAd, 
    IN EVENT_WIFI_ICAP_T *IcapInfo)
{
    INT32 ret = 0;
    CMD_TEST_CTRL_T TestCtrl;

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s----------------->\n", __FUNCTION__));    

    os_zero_mem(&TestCtrl, sizeof(TestCtrl));
    TestCtrl.ucAction = ACTION_IN_RFTEST;
	TestCtrl.u.rRfATInfo.u4FuncIndex = GET_ICAP_WIFI_SPECTRUM;
#ifdef RT_BIG_ENDIAN
	TestCtrl.u.rRfATInfo.u4FuncIndex = cpu2le32(TestCtrl.u.rRfATInfo.u4FuncIndex);
#endif

	ret = MtCmdRfTest(pAd, TestCtrl, NULL, RF_TEST_DEFAULT_RESP_LEN);

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s<-----------------\n", __FUNCTION__)); 
    
    return ret;
}
#endif/* INTERNAL_CAPTURE_SUPPORT */

INT32 MtCmdRfTestSetADC(
IN	RTMP_ADAPTER *pAd,
IN	UINT32	ChannelFreq,
IN	UINT8	AntIndex,
IN	UINT8	BW,
IN	UINT8	SX,
IN	UINT8	DbdcIdx,
IN	UINT8	RunType,
IN	UINT8	FType)
{
	INT32 ret = 0;
	CMD_TEST_CTRL_T TestCtrl;
	SET_ADC_T	*prSetADC;

	os_zero_mem(&TestCtrl, sizeof(TestCtrl));
	TestCtrl.ucAction = ACTION_IN_RFTEST;
	TestCtrl.u.rRfATInfo.u4FuncIndex = SET_ADC;
#ifdef RT_BIG_ENDIAN
	TestCtrl.u.rRfATInfo.u4FuncIndex = cpu2le32(TestCtrl.u.rRfATInfo.u4FuncIndex);
#endif


	prSetADC = &TestCtrl.u.rRfATInfo.Data.rSetADC;
	prSetADC->u4ChannelFreq = cpu2le32(ChannelFreq);
	prSetADC->ucAntIndex = AntIndex;
	prSetADC->ucBW = BW;
	prSetADC->ucSX = SX;
	prSetADC->ucDbdcIdx = DbdcIdx;
	prSetADC->ucRunType = RunType;
	prSetADC->ucFType = FType;

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: <SetADC> Structure parser Checking Log \n\
				--------------------------------------------------------------\n\
				ChannelFreq = %d \n\
				AntIndex = %d \n\
				BW = %d \n\
				SX= %d \n\
				DbdcIdx = %d \n\
				RunType = %d \n\
				FType = %d \n\n",__FUNCTION__,\
				ChannelFreq,\
				prSetADC->ucAntIndex,\
				prSetADC->ucBW,\
				prSetADC->ucSX,\
				prSetADC->ucDbdcIdx,\
				prSetADC->ucRunType,\
				prSetADC->ucFType));

	ret = MtCmdRfTest(pAd, TestCtrl, NULL, RF_TEST_DEFAULT_RESP_LEN);
    return ret;
}

INT32 MtCmdRfTestSetRxGain(
IN	RTMP_ADAPTER *pAd,
IN	UINT8	LPFG,
IN	UINT8	LNA,
IN	UINT8	DbdcIdx,
IN	UINT8	AntIndex)
{
	INT32 ret = 0;
	CMD_TEST_CTRL_T TestCtrl;
	SET_RX_GAIN_T *prSetRxGain;

	os_zero_mem(&TestCtrl, sizeof(TestCtrl));
	TestCtrl.ucAction = ACTION_IN_RFTEST;
	TestCtrl.u.rRfATInfo.u4FuncIndex = SET_RX_GAIN;
#ifdef RT_BIG_ENDIAN
	TestCtrl.u.rRfATInfo.u4FuncIndex = cpu2le32(TestCtrl.u.rRfATInfo.u4FuncIndex);
#endif


	prSetRxGain = &TestCtrl.u.rRfATInfo.Data.rSetRxGain;
	prSetRxGain->ucLPFG = LPFG;
	prSetRxGain->ucLNA = LNA;
	prSetRxGain->ucDbdcIdx = DbdcIdx;
	prSetRxGain->ucAntIndex = AntIndex;

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: <SetRxGain> Structure parser Checking Log \n\
				--------------------------------------------------------------\n\
				LPFG = %d \n\
				LNA = %d \n\
				DbdcIdx = %d \n\
				AntIndex= %d \n\n",__FUNCTION__,\
				prSetRxGain->ucLPFG,\
				prSetRxGain->ucLNA,\
				prSetRxGain->ucDbdcIdx,\
				prSetRxGain->ucAntIndex));

	ret = MtCmdRfTest(pAd, TestCtrl, NULL, RF_TEST_DEFAULT_RESP_LEN);
    return ret;
}

INT32 MtCmdRfTestSetTTG(
IN	RTMP_ADAPTER *pAd,
IN	UINT32	ChannelFreq,
IN	UINT32	ToneFreq,
IN	UINT8	TTGPwrIdx,
IN	UINT8	DbdcIdx)
{
	INT32 ret = 0;
	CMD_TEST_CTRL_T TestCtrl;
	SET_TTG_T	*prSetTTG;

	os_zero_mem(&TestCtrl, sizeof(TestCtrl));
	TestCtrl.ucAction = ACTION_IN_RFTEST;
	TestCtrl.u.rRfATInfo.u4FuncIndex = SET_TTG;
#ifdef RT_BIG_ENDIAN
	TestCtrl.u.rRfATInfo.u4FuncIndex = cpu2le32(TestCtrl.u.rRfATInfo.u4FuncIndex);
#endif


	prSetTTG = &TestCtrl.u.rRfATInfo.Data.rSetTTG;
	prSetTTG->u4ChannelFreq = cpu2le32(ChannelFreq);
	prSetTTG->u4ToneFreq = cpu2le32(ToneFreq);
	prSetTTG->ucTTGPwrIdx = TTGPwrIdx;
	prSetTTG->ucDbdcIdx = DbdcIdx;

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: <SetTTG> Structure parser Checking Log \n\
				--------------------------------------------------------------\n\
				ChannelFreq = %d \n\
				ToneFreq = %d \n\
				TTGPwrIdx = %d \n\
				DbdcIdx= %d \n\n",__FUNCTION__,\
				ChannelFreq,\
				ToneFreq,\
				prSetTTG->ucTTGPwrIdx,\
				prSetTTG->ucDbdcIdx));

	ret = MtCmdRfTest(pAd, TestCtrl, NULL, RF_TEST_DEFAULT_RESP_LEN);
    return ret;
}

INT32 MtCmdRfTestSetTTGOnOff(
IN	RTMP_ADAPTER *pAd,
IN	UINT8	TTGEnable,
IN	UINT8	DbdcIdx,
IN	UINT8	AntIndex)
{
	INT32 ret = 0;
	CMD_TEST_CTRL_T TestCtrl;
	TTG_ON_OFF_T *prTTGOnOff;

	os_zero_mem(&TestCtrl, sizeof(TestCtrl));
	TestCtrl.ucAction = ACTION_IN_RFTEST;
	TestCtrl.u.rRfATInfo.u4FuncIndex = TTG_ON_OFF;
#ifdef RT_BIG_ENDIAN
	TestCtrl.u.rRfATInfo.u4FuncIndex = cpu2le32(TestCtrl.u.rRfATInfo.u4FuncIndex);
#endif


	prTTGOnOff = &TestCtrl.u.rRfATInfo.Data.rTTGOnOff;
	prTTGOnOff->ucTTGEnable = TTGEnable;
	prTTGOnOff->ucDbdcIdx = DbdcIdx;
	prTTGOnOff->ucAntIndex = AntIndex;

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: <SetTTGOnOff> Structure parser Checking Log \n\
				--------------------------------------------------------------\n\
				TTGEnable = %d \n\
				DbdcIdx = %d \n\
				AntIndex = %d \n\n",__FUNCTION__,\
				prTTGOnOff->ucTTGEnable,\
				prTTGOnOff->ucDbdcIdx,\
				prTTGOnOff->ucAntIndex));

	ret = MtCmdRfTest(pAd, TestCtrl, NULL, RF_TEST_DEFAULT_RESP_LEN);
    return ret;
}

static INT32 MtCmdRfTestTrigger(RTMP_ADAPTER *pAd,
                    PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo, UINT16 rsp_len)
{
	INT32 ret = 0;
	CMD_TEST_CTRL_T TestCtrl;
	os_zero_mem(&TestCtrl, sizeof(TestCtrl));
	TestCtrl.ucAction = ACTION_IN_RFTEST;
	TestCtrl.ucIcapLen = RF_TEST_ICAP_LEN;
	os_move_mem(&TestCtrl.u.rRfATInfo, &rRfATInfo, sizeof(rRfATInfo));
	ret = MtCmdRfTest(pAd, TestCtrl, NULL, rsp_len);
	return ret;
}

INT32 MtCmdDoCalibration(RTMP_ADAPTER *pAd, UINT32 func_idx,
                                UINT32 item, UINT32 band_idx)
{
	struct _PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo;
	INT32 ret = 0;
	UINT16 rsp_len = RF_TEST_DEFAULT_RESP_LEN;
	os_zero_mem(&rRfATInfo, sizeof(rRfATInfo));
	rRfATInfo.u4FuncIndex = cpu2le32(func_idx);
#ifdef MT7615
	if (item == CAL_ALL)
    {
		rsp_len = RC_CAL_RESP_LEN;
    }
    rRfATInfo.Data.rCalParam.u4FuncData = cpu2le32(item);
	rRfATInfo.Data.rCalParam.ucDbdcIdx = band_idx;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
			("%s, func_idx:%x, func_data:%x, band_idx:%x\n", __FUNCTION__,
                			func_idx, item,
                			band_idx));
#else
	rRfATInfo.Data.u4FuncData = cpu2le32(item);
#endif
	ret = MtCmdRfTestTrigger(pAd, rRfATInfo, rsp_len);

	return ret;
}

INT32 MtCmdTxContinous(RTMP_ADAPTER *pAd, UINT32 PhyMode, UINT32 BW,
            UINT32 PriCh, UINT32 CentralCh, UINT32 Mcs, UINT32 WFSel,
                            UINT32 TxfdMode, UINT8 Band, UINT8 onoff)
{
	struct _PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo;
	INT32 ret = 0;
	UINT8 TXDRate = 0;
	os_zero_mem(&rRfATInfo, sizeof(rRfATInfo));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s, mode:0x%x, bw:0x%x, prich(Control CH):0x%x, mcs:0x%x\n",
                            __FUNCTION__, PhyMode, BW, PriCh, Mcs));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("wfsel:0x%x, TxfdMode:0x%x, Band:0x%xon/off:0x%x\n",
                            WFSel, TxfdMode, Band, onoff));
	if (onoff == 0)
	{
		rRfATInfo.u4FuncIndex = CONTINUOUS_TX_STOP;
		rRfATInfo.Data.u4FuncData = Band;
#ifdef RT_BIG_ENDIAN
		rRfATInfo.u4FuncIndex = cpu2le32(rRfATInfo.u4FuncIndex);
		rRfATInfo.Data.u4FuncData = cpu2le32(rRfATInfo.Data.u4FuncData);
#endif
	}
	else
	{
		rRfATInfo.u4FuncIndex = CONTINUOUS_TX_START;
#ifdef RT_BIG_ENDIAN
		rRfATInfo.u4FuncIndex = cpu2le32(rRfATInfo.u4FuncIndex);
#endif
		/* 0: All 1:TX0 2:TX1 */
		rRfATInfo.Data.rConTxParam.ucCentralCh = (UINT8)CentralCh;
		rRfATInfo.Data.rConTxParam.ucCtrlCh = (UINT8)PriCh;
		rRfATInfo.Data.rConTxParam.ucAntIndex = (UINT8)WFSel;
		rRfATInfo.Data.rConTxParam.ucBW = (UINT8)BW;
		if (0 == PhyMode) //CCK
		{
			switch (Mcs)
			{
				//long preamble
				case 0:
					TXDRate = 0;
					break;
				case 1:
					TXDRate = 1;
					break;
				case 2:
					TXDRate = 2;
					break;
				case 3:
					TXDRate = 3;
					break;
				//short preamble
				case 9:
					TXDRate = 5;
					break;
				case 10:
					TXDRate = 6;
					break;
				case 11:
					TXDRate = 7;
					break;
			}
		}
		else if (1 == PhyMode) //OFDM
		{
			switch (Mcs)
			{
				case 0:
					TXDRate = 11;
					break;
				case 1:
					TXDRate = 15;
					break;
				case 2:
					TXDRate = 10;
					break;
				case 3:
					TXDRate = 14;
					break;
				case 4:
					TXDRate = 9;
					break;
				case 5:
					TXDRate = 13;
					break;
				case 6:
					TXDRate = 8;
					break;
				case 7:
					TXDRate = 12;
					break;
			}
		}
		else if (2 == PhyMode || 3 == PhyMode || 4 == PhyMode)
		{
			/* 2. MODULATION_SYSTEM_HT20 ||
			   3.MODULATION_SYSTEM_HT40 || 4. VHT */
			TXDRate = (UINT8)Mcs;
		}

		rRfATInfo.Data.rConTxParam.u2RateCode = PhyMode << 6 | TXDRate;
#ifdef RT_BIG_ENDIAN
		rRfATInfo.Data.rConTxParam.u2RateCode = cpu2le16(rRfATInfo.Data.rConTxParam.u2RateCode);
#endif
		rRfATInfo.Data.rConTxParam.ucBand = Band;
		rRfATInfo.Data.rConTxParam.ucTxfdMode = TxfdMode;
	}
	ret = MtCmdRfTestTrigger(pAd, rRfATInfo, RF_TEST_DEFAULT_RESP_LEN);

	return ret;
}

INT32 MtCmdTxTone(RTMP_ADAPTER *pAd, UINT8 BandIdx, UINT8 Control,
                    UINT8 AntIndex, UINT8 ToneType, UINT8 ToneFreq,
                    INT32 DcOffset_I, INT32 DcOffset_Q, UINT32 Band)
{
	INT32 ret = 0;
	struct _PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo;

	os_zero_mem(&rRfATInfo, sizeof(rRfATInfo));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
		 ("%s, Control:%d, AntIndex:%d, ToneType:%d, ToneFreq:%d\n",
		        __FUNCTION__, Control, AntIndex, ToneType, ToneFreq));
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("BandIdx:%d, DcOffset_I:%d, DcOffset_Q:%d, Band:%d\n",
                        BandIdx, DcOffset_I, DcOffset_Q, Band));

    if (Control)
    {
	    rRfATInfo.u4FuncIndex = TX_TONE_START;
#ifdef RT_BIG_ENDIAN
		rRfATInfo.u4FuncIndex = cpu2le32(rRfATInfo.u4FuncIndex);
#endif
		rRfATInfo.Data.rTxToneParam.ucAntIndex = AntIndex;
		rRfATInfo.Data.rTxToneParam.ucToneType = ToneType;
		rRfATInfo.Data.rTxToneParam.ucToneFreq = ToneFreq;
		rRfATInfo.Data.rTxToneParam.ucDbdcIdx = BandIdx;
		rRfATInfo.Data.rTxToneParam.i4DcOffsetI = cpu2le32(DcOffset_I);
		rRfATInfo.Data.rTxToneParam.i4DcOffsetQ = cpu2le32(DcOffset_Q);
		rRfATInfo.Data.rTxToneParam.u4Band = cpu2le32(Band);
	}
    else
	{
	    rRfATInfo.u4FuncIndex = TX_TONE_STOP;
#ifdef RT_BIG_ENDIAN
		rRfATInfo.u4FuncIndex = cpu2le32(rRfATInfo.u4FuncIndex);
#endif

		rRfATInfo.Data.u4FuncData = cpu2le32(Band);
    }


	ret = MtCmdRfTestTrigger(pAd, rRfATInfo, RF_TEST_DEFAULT_RESP_LEN);

	return ret;
}
/*type:
	RF_AT_EXT_FUNCID_TX_TONE_RF_GAIN
	RF_AT_EXT_FUNCID_TX_TONE_DIGITAL_GAIN
*/
INT32 MtCmdTxTonePower(RTMP_ADAPTER *pAd, INT32 type,
            INT32 dec, UINT8 TxAntennaSel, UINT8 Band)
{
	INT32 ret = 0;
	struct _PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo;

	os_zero_mem(&rRfATInfo, sizeof(rRfATInfo));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s, type:%d, dec:%d, TxAntennaSel: %d\n",
                __FUNCTION__, type, dec, TxAntennaSel));

    	rRfATInfo.u4FuncIndex = cpu2le32(type);
	/* 0: All 1:TX0 2:TX1 */
	switch (TxAntennaSel)
    {
    	case 0:
    	case 1:
    	case 2:
    	case 3:
    	case 4:
		    rRfATInfo.Data.rTxToneGainParam.ucAntIndex = TxAntennaSel;
		    break;
	    default:    //for future more than 3*3 ant
		    rRfATInfo.Data.rTxToneGainParam.ucAntIndex = TxAntennaSel - 1;
		    break;
	}
	rRfATInfo.Data.rTxToneGainParam.ucTonePowerGain = (UINT8)dec;

	rRfATInfo.Data.rTxToneGainParam.ucBand = Band;

	ret = MtCmdRfTestTrigger(pAd, rRfATInfo, RF_TEST_DEFAULT_RESP_LEN);

	return ret;
}

INT32 MtCmdSetRDDTestExt(RTMP_ADAPTER *pAd, UINT32 rdd_idx,
                        UINT32 rdd_in_sel, UINT32 is_start)
{
	INT32 ret = 0;
	struct _PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo;

    os_zero_mem(&rRfATInfo, sizeof(rRfATInfo));

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            		("%s, rdd_num:%u, IsStart:%d\n",
            		__FUNCTION__, rdd_idx, is_start));

    if (IS_MT7615(pAd) || IS_MT7636(pAd) || IS_MT7637(pAd))
    {
		rRfATInfo.u4FuncIndex = RDD_TEST_MODE;
#ifdef RT_BIG_ENDIAN
		rRfATInfo.u4FuncIndex = cpu2le32(rRfATInfo.u4FuncIndex);
#endif
		rRfATInfo.Data.rRDDParam.ucRddCtrl = is_start;
		rRfATInfo.Data.rRDDParam.ucRddIdx = rdd_idx;
		rRfATInfo.Data.rRDDParam.ucRddInSel = rdd_in_sel;
		ret = MtCmdRfTestTrigger(pAd, rRfATInfo, RF_TEST_DEFAULT_RESP_LEN);
	}
    else
    {
		ret = MtCmdSetRDDTest(pAd, is_start);
	}
	return ret;
}

INT32 MtCmdSetRDDTest(RTMP_ADAPTER *pAd, UINT32 IsStart)
{
	INT32 ret = 0;
	struct _PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo;
	os_zero_mem(&rRfATInfo, sizeof(rRfATInfo));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s, IsStart:%d\n", __FUNCTION__, IsStart));
	rRfATInfo.u4FuncIndex = RDD_TEST_MODE;
#ifdef RT_BIG_ENDIAN
	rRfATInfo.u4FuncIndex = cpu2le32(rRfATInfo.u4FuncIndex);
#endif
	rRfATInfo.Data.u4FuncData = cpu2le32(IsStart);// 0 Stop, 1 Start
	ret = MtCmdRfTestTrigger(pAd, rRfATInfo, RF_TEST_DEFAULT_RESP_LEN);

	return ret;
}

INT32 MtCmdSetCalDump(RTMP_ADAPTER *pAd, UINT32 IsEnable)
{
	INT32 ret = 0;
	struct _PARAM_MTK_WIFI_TEST_STRUC_T rRfATInfo;
	os_zero_mem(&rRfATInfo, sizeof(rRfATInfo));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s, IsEnable = %d\n", __FUNCTION__, IsEnable));
	rRfATInfo.u4FuncIndex = CAL_RESULT_DUMP_FLAG;
#ifdef RT_BIG_ENDIAN
	rRfATInfo.u4FuncIndex = cpu2le32(rRfATInfo.u4FuncIndex);
#endif
	rRfATInfo.Data.u4FuncData = cpu2le32(IsEnable);// 0 Disable, 1 Enable
	ret = MtCmdRfTestTrigger(pAd, rRfATInfo, RF_TEST_DEFAULT_RESP_LEN);

	return ret;
}


/*****************************************
	ExT_CID = 0x05
	1: On
	2: Off
*****************************************/
INT32 MtCmdRadioOnOffCtrl(RTMP_ADAPTER *pAd, UINT8 On)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_RADIO_ON_OFF_CTRL_T RadioOnOffCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s: On = %d\n", __FUNCTION__, On));

#ifdef MT7615
    // TODO: Shiang-Mt7615, fix me!
    return 0;
#endif /* MT7615 */

	msg = MtAndesAllocCmdMsg(pAd, sizeof(RadioOnOffCtrl));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_RADIO_ON_OFF_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&RadioOnOffCtrl, sizeof(RadioOnOffCtrl));

	if (On == WIFI_RADIO_ON)
    {
		RadioOnOffCtrl.ucWiFiRadioCtrl = WIFI_RADIO_ON;
	}
    else if (On == WIFI_RADIO_OFF)
	{
	    RadioOnOffCtrl.ucWiFiRadioCtrl = WIFI_RADIO_OFF;
	}
    else
	{
	    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
		    ("%s: unknown state, On=%d\n", __FUNCTION__, On));
    }
	MtAndesAppendCmdMsg(msg, (char *)&RadioOnOffCtrl, sizeof(RadioOnOffCtrl));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


/*****************************************
	ExT_CID = 0x06
*****************************************/
INT32 MtCmdWiFiRxDisable(RTMP_ADAPTER *pAd)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_WIFI_RX_DISABLE_T WiFiRxDisable;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s: WiFiRxDisable = %d\n", __FUNCTION__,WIFI_RX_DISABLE));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(WiFiRxDisable));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_WIFI_RX_DISABLE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&WiFiRxDisable, sizeof(WiFiRxDisable));

	WiFiRxDisable.ucWiFiRxDisableCtrl = WIFI_RX_DISABLE;

	MtAndesAppendCmdMsg(msg, (char *)&WiFiRxDisable, sizeof(WiFiRxDisable));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


/*****************************************
	ExT_CID = 0x07
*****************************************/
/*TODO: Star check to Hanmin*/

static VOID CmdExtPmMgtBitRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
 {
	 struct _EVENT_EXT_CMD_RESULT_T *EventExtCmdResult =
                                    (struct _EVENT_EXT_CMD_RESULT_T *)Data;

	 MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.ucExTenCID = 0x%x\n",
            __FUNCTION__, EventExtCmdResult->ucExTenCID));
	 EventExtCmdResult->u4Status = le2cpu32(EventExtCmdResult->u4Status);
	 MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.u4Status = 0x%x\n",
                __FUNCTION__, EventExtCmdResult->u4Status));
 }

INT32 MtCmdExtPwrMgtBitWifi(RTMP_ADAPTER *pAd, MT_PWR_MGT_BIT_WIFI_T rPwrMgtBitWifi)
{
	struct cmd_msg *msg;
	EXT_CMD_PWR_MGT_BIT_T PwrMgtBitWifi = {0};
	INT32 Ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_PWR_MGT_BIT_T));
	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	PwrMgtBitWifi.ucWlanIdx = rPwrMgtBitWifi.ucWlanIdx;
	PwrMgtBitWifi.ucPwrMgtBit = rPwrMgtBitWifi.ucPwrMgtBit;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:ucWlanIdx(%d), ucPwrMgtBit(%d)\n", __FUNCTION__,
        rPwrMgtBitWifi.ucWlanIdx, rPwrMgtBitWifi.ucPwrMgtBit));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PWR_MGT_BIT_WIFI);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdExtPmMgtBitRsp);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&PwrMgtBitWifi, sizeof(PwrMgtBitWifi));

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(Ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}


static VOID CmdExtPmStateCtrlRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
 {
	 struct _EVENT_EXT_CMD_RESULT_T *EventExtCmdResult =
                                        (struct _EVENT_EXT_CMD_RESULT_T *)Data;

	 MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.ucExTenCID = 0x%x\n",
                __FUNCTION__, EventExtCmdResult->ucExTenCID));
	 EventExtCmdResult->u4Status = le2cpu32(EventExtCmdResult->u4Status);
	 MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.u4Status = 0x%x\n",
                __FUNCTION__, EventExtCmdResult->u4Status));
 }

INT32 MtCmdExtPmStateCtrl(RTMP_ADAPTER *pAd, MT_PMSTAT_CTRL_T PmStatCtrl)
{
	struct cmd_msg *msg = NULL;
	EXT_CMD_PM_STATE_CTRL_T CmdPmStateCtrl = {0};
	INT32 Ret = 0;
	struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_PM_STATE_CTRL_T));
	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	/* Fill parameter here*/
	CmdPmStateCtrl.ucWlanIdx = PmStatCtrl.WlanIdx;
	CmdPmStateCtrl.ucOwnMacIdx = PmStatCtrl.OwnMacIdx;
	CmdPmStateCtrl.ucPmNumber = PmStatCtrl.PmNumber;
	CmdPmStateCtrl.ucPmState = PmStatCtrl.PmState;
	CmdPmStateCtrl.ucDbdcIdx = PmStatCtrl.DbdcIdx;
	os_move_mem(CmdPmStateCtrl.aucBssid, PmStatCtrl.Bssid, MAC_ADDR_LEN);


	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_PM_STATE_CTRL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, CmdExtPmStateCtrlRsp);

	MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CmdPmStateCtrl, sizeof(CmdPmStateCtrl));

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(Ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}



/*****************************************
	ExT_CID = 0x08
*****************************************/
UCHAR GetCfgBw2RawBw(UCHAR CfgBw)
{
	switch(CfgBw)
    {
    	case BW_20:
    		return CMD_BW_20;
    	break;
    	case BW_40:
    		return CMD_BW_40;
    	break;
    	case BW_80:
    		return CMD_BW_80;
    	break;
    	case BW_160:
    		return CMD_BW_160;
    	break;
    	case BW_10:
    		return CMD_BW_10;
    	break;
    	case BW_5:
    		return CMD_BW_5;
    	break;
    	case BW_8080:
    		return CMD_BW_8080;
    	break;
    	default:
    		return CMD_BW_20;
	}
	return CMD_BW_20;
}

#ifdef NEW_SET_RX_STREAM
//TODO: temporary to keep channel setting
MT_SWITCH_CHANNEL_CFG CurrentSwChCfg[2];
#endif

INT32 MtCmdChannelSwitch(RTMP_ADAPTER *pAd, MT_SWITCH_CHANNEL_CFG SwChCfg)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_CHAN_SWITCH_T CmdChanSwitch;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};
	UINT8 TxPowerDrop = 0;
#ifdef SINGLE_SKU_V2
	UCHAR TxStream;
    UCHAR fg5Gband = 0;
#endif
    UINT8 SKUIdx = 0;
	

	if (SwChCfg.CentralChannel == 0)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
            ("%s: central channel = 0 is invalid\n", __FUNCTION__));
		return -1;
	}


	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s: control_chl = %d,control_ch2=%d, central_chl = %d DBDCIdx= %d, Band= %d \n",
        __FUNCTION__, SwChCfg.ControlChannel, SwChCfg.ControlChannel2, SwChCfg.CentralChannel, SwChCfg.BandIdx, SwChCfg.Channel_Band));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("BW = %d,TXStream = %d, RXStream = %d, scan(%d)\n",
            SwChCfg.Bw, SwChCfg.TxStream, SwChCfg.RxStream, SwChCfg.bScan));


	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdChanSwitch));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_CHANNEL_SWITCH);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 5000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    os_zero_mem(&CmdChanSwitch, sizeof(CmdChanSwitch));

	CmdChanSwitch.ucPrimCh = SwChCfg.ControlChannel;
	CmdChanSwitch.ucCentralCh = SwChCfg.CentralChannel;
	CmdChanSwitch.ucCentralCh2 = SwChCfg.ControlChannel2;
	CmdChanSwitch.ucTxStreamNum = SwChCfg.TxStream;
	CmdChanSwitch.ucRxStreamNum = SwChCfg.RxStream;
	CmdChanSwitch.ucDbdcIdx = SwChCfg.BandIdx;
	CmdChanSwitch.ucBW = GetCfgBw2RawBw(SwChCfg.Bw);
	CmdChanSwitch.ucBand = SwChCfg.Channel_Band;
    CmdChanSwitch.u4OutBandFreq = SwChCfg.OutBandFreq;
	
#ifdef COMPOS_TESTMODE_WIN
	if(SwChCfg.isMCC)
	{
		// MCC
		CmdChanSwitch.ucSwitchReason = CH_SWITCH_INTERNAL_USED_BY_FW_3;
	}
	else
#endif
		CmdChanSwitch.ucSwitchReason = CH_SWITCH_BY_NORMAL_TX_RX;

	if (SwChCfg.bScan)
	{

#if defined (MT7615)
		if (IS_MT7615(pAd))
		{
			CmdChanSwitch.ucSwitchReason = CH_SWITCH_SCAN_BYPASS_DPD;
		}
#endif
#ifdef MT_DFS_SUPPORT
		DfsSetScanRunning(pAd, TRUE);
#endif

	}
#ifdef MT_DFS_SUPPORT
    else
    {
        if(SwChCfg.DfsParam.bDfsCheck)
	{
		CmdChanSwitch.ucSwitchReason = CH_SWITCH_DFS;
	}
	DfsSetScanRunning(pAd, FALSE);
    }
#endif

	/* check Tx Power setting from UI. */
	if ((pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] > 90) && (pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] < 100))
		TxPowerDrop = 0;
	else if ((pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] > 60) && (pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] <= 90))  /* reduce Pwr for 1 dB. */
		TxPowerDrop = 1;
	else if ((pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] > 30) && (pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] <= 60))  /* reduce Pwr for 3 dB. */
		TxPowerDrop = 3;
	else if ((pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] > 15) && (pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] <= 30))  /* reduce Pwr for 6 dB. */
		TxPowerDrop = 6;
	else if ((pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] > 9) && (pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] <= 15))   /* reduce Pwr for 9 dB. */
		TxPowerDrop = 9;
	else if ((pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] > 0) && (pAd->CommonCfg.TxPowerPercentage[SwChCfg.BandIdx] <= 9))   /* reduce Pwr for 12 dB. */
		TxPowerDrop = 12;

	CmdChanSwitch.ucTxPowerDrop = TxPowerDrop;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,(" TxPowerDrop = 0x%x \n", CmdChanSwitch.ucTxPowerDrop));

	for (SKUIdx = 0; SKUIdx < SKU_SIZE; SKUIdx++)
	{
		CmdChanSwitch.aucTxPowerSKU[SKUIdx] = 0x3F;
	}

#ifdef SINGLE_SKU_V2

    if (SwChCfg.Channel_Band == 0) // Not 802.11j
    {
        if (SwChCfg.ControlChannel <= 14)
        {
            fg5Gband = 0;
        }
        else
        {
            fg5Gband = 1;
        }
    }
    else
    {
        fg5Gband = 1;
    }

	if (pAd->CommonCfg.dbdc_mode)
	{
		if (fg5Gband)
			TxStream = pAd->dbdc_5G_tx_stream;
		else
			TxStream = pAd->dbdc_2G_tx_stream;
	} else {
		TxStream = pAd->Antenna.field.TxPath;
	}

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,(" fg5Gband = 0x%x \n", fg5Gband));

    mt_FillSkuParameter(pAd,SwChCfg.ControlChannel, fg5Gband, TxStream, CmdChanSwitch.aucTxPowerSKU);

    for (SKUIdx = 0; SKUIdx < SKU_SIZE; SKUIdx++)
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("%s: CmdChanSwitch.aucTxPowerSKU[%d]: 0x%x \n", __FUNCTION__, SKUIdx, CmdChanSwitch.aucTxPowerSKU[SKUIdx]));

    os_move_mem(pAd->TxPowerSKU, CmdChanSwitch.aucTxPowerSKU, SKU_SIZE);

    for (SKUIdx = 0; SKUIdx < SKU_SIZE; SKUIdx++)
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,("%s: pAd->TxPowerSKU[%d]: 0x%x \n", __FUNCTION__, SKUIdx, pAd->TxPowerSKU[SKUIdx]));
#endif

	MtAndesAppendCmdMsg(msg, (char *)&CmdChanSwitch, sizeof(CmdChanSwitch));

#ifdef NEW_SET_RX_STREAM
    //TODO: temporary to keep channel setting
    os_move_mem(&CurrentSwChCfg[SwChCfg.BandIdx], &SwChCfg, sizeof(MT_SWITCH_CHANNEL_CFG));
#endif
#ifdef BACKGROUND_SCAN_SUPPORT
    /* Backup swtich channel configuration for background scan */
    os_move_mem(&pAd->BgndScanCtrl.CurrentSwChCfg[0], &SwChCfg, sizeof(MT_SWITCH_CHANNEL_CFG));
#endif

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}

#ifdef NEW_SET_RX_STREAM
INT MtCmdSetRxPath(struct _RTMP_ADAPTER *pAd, UINT32 Path, UCHAR BandIdx)
{
    MT_SWITCH_CHANNEL_CFG *pSwChCfg = &CurrentSwChCfg[BandIdx];
	struct cmd_msg *msg;
	struct _EXT_CMD_CHAN_SWITCH_T CmdChanSwitch;
	INT32 ret = 0,i=0;
    struct _CMD_ATTRIBUTE attr = {0};
#ifdef SINGLE_SKU_V2
	UCHAR TxStream;
    UCHAR fg5Gband = 0;
#endif


	if (pSwChCfg->CentralChannel== 0)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: central channel = 0 is invalid\n", __FUNCTION__));
		return -1;
	}

    //TODO: Pat: Update new path. It is rx path actually.
    pSwChCfg->RxStream = Path;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: control_chl = %d,control_ch2=%d, central_chl = %d, BW = %d,TXStream = %d, \
		RXStream = %d, BandIdx =%d,  scan(%d), Channel_Band = %d\n", __FUNCTION__, \
	pSwChCfg->ControlChannel, pSwChCfg->ControlChannel2, pSwChCfg->CentralChannel, \
	pSwChCfg->Bw, pSwChCfg->TxStream, pSwChCfg->RxStream, pSwChCfg->BandIdx, pSwChCfg->bScan, pSwChCfg->Channel_Band));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdChanSwitch));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_SET_RX_PATH);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

	os_zero_mem(&CmdChanSwitch, sizeof(CmdChanSwitch));

	CmdChanSwitch.ucPrimCh = pSwChCfg->ControlChannel;
	CmdChanSwitch.ucCentralCh = pSwChCfg->CentralChannel;
	CmdChanSwitch.ucCentralCh2 = pSwChCfg->ControlChannel2;
	CmdChanSwitch.ucTxStreamNum = pSwChCfg->TxStream;
	CmdChanSwitch.ucRxStreamNum = pSwChCfg->RxStream;
	CmdChanSwitch.ucDbdcIdx = pSwChCfg->BandIdx;
	CmdChanSwitch.ucBW = GetCfgBw2RawBw(pSwChCfg->Bw);
	CmdChanSwitch.ucBand = pSwChCfg->Channel_Band;

	for(i=0;i<SKU_SIZE;i++)
	{
		CmdChanSwitch.aucTxPowerSKU[i]=0xff;
	}

#ifdef SINGLE_SKU_V2
    if (pSwChCfg->Channel_Band == 0) // Not 802.11j
    {
        if (pSwChCfg->ControlChannel <= 14)
        {
            fg5Gband = 0;
        }
        else
        {
            fg5Gband = 1;
        }
    }
    else
    {
        fg5Gband = 1;
    }

	if (pAd->CommonCfg.dbdc_mode)
	{
		if (fg5Gband)
			TxStream = pAd->dbdc_5G_tx_stream;
		else
			TxStream = pAd->dbdc_2G_tx_stream;
	} else {
		TxStream = pAd->Antenna.field.TxPath;
	}

	mt_FillSkuParameter(pAd, pSwChCfg->ControlChannel, fg5Gband, TxStream, CmdChanSwitch.aucTxPowerSKU);
#endif

	MtAndesAppendCmdMsg(msg, (char *)&CmdChanSwitch, sizeof(CmdChanSwitch));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;


}
#endif


INT MtCmdSetTxRxPath(struct _RTMP_ADAPTER *pAd,MT_SWITCH_CHANNEL_CFG SwChCfg)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_CHAN_SWITCH_T CmdChanSwitch;
	INT32 ret = 0,i=0;
    struct _CMD_ATTRIBUTE attr = {0};
#ifdef SINGLE_SKU_V2
	UCHAR TxStream;
    UCHAR fg5Gband = 0;
#endif


	MT_SWITCH_CHANNEL_CFG *pSwChCfg = &SwChCfg;
	UCHAR RxPath=0;

	if (pSwChCfg->CentralChannel== 0)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: central channel = 0 is invalid\n", __FUNCTION__));
		return -1;
	}

	/*for normal case*/
	if(pSwChCfg->Bw==BW_160 || pSwChCfg->Bw==BW_8080)
	{
			/*if bw 160, 1 stream use WIFI (0,2), 2 stream use WIFI (0,1,2,3)*/
			RxPath = (SwChCfg.RxStream > 1) ? 0xf:0x5;
			/*bw160 & 80+80 should always apply 4 TxStream*/
			if (pSwChCfg->TxStream == 1)
			{
				pSwChCfg->TxStream = 2;
			}
			else
			{
				pSwChCfg->TxStream = 4;
			}
	}else
	{
		/*for normal case*/
	    for (i = 0; i < SwChCfg.RxStream ; i++)
	    {
	    	RxPath |= (1 << (i+(SwChCfg.BandIdx*2)));
	    }
	}


	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: control_chl = %d,control_ch2=%d, central_chl = %d, BW = %d,TXStream = %d, \
		RXStream = %d,RXPath = %x, BandIdx =%d,  scan(%d), Channel_Band = %d\n", __FUNCTION__, \
	pSwChCfg->ControlChannel, pSwChCfg->ControlChannel2, pSwChCfg->CentralChannel, \
	pSwChCfg->Bw, pSwChCfg->TxStream, pSwChCfg->RxStream, RxPath, pSwChCfg->BandIdx, pSwChCfg->bScan, pSwChCfg->Channel_Band));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdChanSwitch));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_SET_RX_PATH);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

	os_zero_mem(&CmdChanSwitch, sizeof(CmdChanSwitch));

	CmdChanSwitch.ucPrimCh = pSwChCfg->ControlChannel;
	CmdChanSwitch.ucCentralCh = pSwChCfg->CentralChannel;
	CmdChanSwitch.ucCentralCh2 = pSwChCfg->ControlChannel2;
	CmdChanSwitch.ucTxStreamNum = pSwChCfg->TxStream;
	CmdChanSwitch.ucRxStreamNum = RxPath;
	CmdChanSwitch.ucDbdcIdx = pSwChCfg->BandIdx;
	CmdChanSwitch.ucBW = GetCfgBw2RawBw(pSwChCfg->Bw);
	CmdChanSwitch.ucBand = pSwChCfg->Channel_Band;
	CmdChanSwitch.u2CacCase = 0;


#ifdef COMPOS_TESTMODE_WIN
	if(SwChCfg.isMCC)
	{
		// MCC
		CmdChanSwitch.ucSwitchReason = CH_SWITCH_INTERNAL_USED_BY_FW_3;
	}
	else
#endif
		CmdChanSwitch.ucSwitchReason = CH_SWITCH_BY_NORMAL_TX_RX;

	if (SwChCfg.bScan)
	{

#if defined (MT7615)
		if (IS_MT7615(pAd))
		{
			CmdChanSwitch.ucSwitchReason = CH_SWITCH_SCAN_BYPASS_DPD;
		}
#endif
	}
#ifdef MT_DFS_SUPPORT
    else
    {
        if(SwChCfg.DfsParam.bDfsCheck)
        {
		CmdChanSwitch.ucSwitchReason = CH_SWITCH_DFS;
        }
    }
#endif

	for(i=0;i<SKU_SIZE;i++)
	{
		CmdChanSwitch.aucTxPowerSKU[i]=0xff;
	}

#ifdef SINGLE_SKU_V2
    if (pSwChCfg->Channel_Band == 0) // Not 802.11j
    {
        if (pSwChCfg->ControlChannel <= 14)
        {
            fg5Gband = 0;
        }
        else
        {
            fg5Gband = 1;
        }
    }
    else
    {
        fg5Gband = 1;
    }

	if (pAd->CommonCfg.dbdc_mode)
	{
		if (fg5Gband)
			TxStream = pAd->dbdc_5G_tx_stream;
		else
			TxStream = pAd->dbdc_2G_tx_stream;
	} else {
		TxStream = pAd->Antenna.field.TxPath;
	}

	mt_FillSkuParameter(pAd,SwChCfg.ControlChannel, fg5Gband, TxStream, CmdChanSwitch.aucTxPowerSKU);
#endif

	MtAndesAppendCmdMsg(msg, (char *)&CmdChanSwitch, sizeof(CmdChanSwitch));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;


}


/*****************************************
	ExT_CID = 0x0e
*****************************************/

static VOID CmdMultipleMacRegAccessReadCb(struct cmd_msg *msg,
											char *data, UINT16 len)
{

	UINT32 Index;
	UINT32 Num = (len - 20) / sizeof(EXT_EVENT_MULTI_CR_ACCESS_RD_T);
	EXT_EVENT_MULTI_CR_ACCESS_RD_T *EventMultiCRAccessRD =
                        (EXT_EVENT_MULTI_CR_ACCESS_RD_T *)(data + 20);
	RTMP_REG_PAIR *RegPair = (RTMP_REG_PAIR *)msg->attr.rsp.wb_buf_in_calbk;
#ifdef INTERNAL_CAPTURE_SUPPORT
       RTMP_REG_PAIR *Start;
	Start = RegPair;
#endif/* INTERNAL_CAPTURE_SUPPORT */
	for (Index = 0; Index < Num; Index++)
	{
		RegPair->Register = le2cpu32(EventMultiCRAccessRD->u4Addr);
		RegPair->Value = le2cpu32(EventMultiCRAccessRD->u4Data);

		EventMultiCRAccessRD++;
		RegPair++;
	}
#ifdef INTERNAL_CAPTURE_SUPPORT
	RegPair = Start;
	for (Index = 0; Index < Num; Index++)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("0x%08x=0x%08x \n", RegPair->Register, RegPair->Value));
		RegPair++;
	}
#endif/* INTERNAL_CAPTURE_SUPPORT */
}


INT32 MtCmdMultipleMacRegAccessRead(RTMP_ADAPTER *pAd, RTMP_REG_PAIR *RegPair,
														        UINT32 Num)
{
	struct cmd_msg *msg;
	CMD_MULTI_CR_ACCESS_T MultiCR;
	INT32 Ret;
	UINT32 Index;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_MULTI_CR_ACCESS_T) * Num);
	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_MULTIPLE_REG_ACCESS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, ((12 * Num) + 20));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, RegPair);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdMultipleMacRegAccessReadCb);

    MtAndesInitCmdMsg(msg, attr);
	for (Index = 0; Index < Num; Index++)
	{
		os_zero_mem(&MultiCR, sizeof(MultiCR));
		MultiCR.u4Type = cpu2le32(MAC_CR);
		MultiCR.u4Addr = cpu2le32(RegPair[Index].Register);

		MtAndesAppendCmdMsg(msg, (char *)&MultiCR, sizeof(MultiCR));
	}

	Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}


static VOID CmdMultipleMacRegAccessWriteCb(struct cmd_msg *msg,
											char *data, UINT16 len)
{
	EXT_EVENT_MULTI_CR_ACCESS_WR_T *EventMultiCRAccessWR =
                                (EXT_EVENT_MULTI_CR_ACCESS_WR_T *)(data + 20);

	EventMultiCRAccessWR->u4Status = le2cpu32(EventMultiCRAccessWR->u4Status);

	if (EventMultiCRAccessWR->u4Status)
    {
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
		                    ("%s: fail\n", __FUNCTION__));
    }
}

INT32 MtCmdMultipleMacRegAccessWrite(RTMP_ADAPTER *pAd, RTMP_REG_PAIR *RegPair,
														        UINT32 Num)
{
	struct cmd_msg *msg;
	CMD_MULTI_CR_ACCESS_T MultiCR;
	INT32 Ret;
	UINT32 Index;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_MULTI_CR_ACCESS_T) * Num);
	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_MULTIPLE_REG_ACCESS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 32);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdMultipleMacRegAccessWriteCb);

    MtAndesInitCmdMsg(msg, attr);

	for (Index = 0; Index < Num; Index++)
	{
		os_zero_mem(&MultiCR, sizeof(MultiCR));
		MultiCR.u4Type = MAC_CR;
#ifdef RT_BIG_ENDIAN
		MultiCR.u4Type = cpu2le32(MultiCR.u4Type);
#endif
		MultiCR.u4Addr = cpu2le32(RegPair[Index].Register);
		MultiCR.u4Data = cpu2le32(RegPair[Index].Value);

        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: offset: = %x\n", __FUNCTION__, RegPair[Index].Register));

		MtAndesAppendCmdMsg(msg, (char *)&MultiCR, sizeof(MultiCR));
	}

	Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}


static VOID CmdMultipleRfRegAccessWriteCb(struct cmd_msg *msg,
											char *data, UINT16 len)
{
	EXT_EVENT_MULTI_CR_ACCESS_WR_T *EventMultiCRAccessWR =
                                (EXT_EVENT_MULTI_CR_ACCESS_WR_T *)(data + 20);

	EventMultiCRAccessWR->u4Status = le2cpu32(EventMultiCRAccessWR->u4Status);

	if (EventMultiCRAccessWR->u4Status)
    {
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
		                    ("%s: fail\n", __FUNCTION__));
    }
}

INT32 MtCmdMultipleRfRegAccessWrite(RTMP_ADAPTER *pAd,
                    MT_RF_REG_PAIR *RegPair, UINT32 Num)
{
	struct cmd_msg *msg;
	CMD_MULTI_CR_ACCESS_T MultiCR;
	INT32 Ret;
	UINT32 Index;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_MULTI_CR_ACCESS_T) * Num);
	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_MULTIPLE_REG_ACCESS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 32);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdMultipleRfRegAccessWriteCb);

    MtAndesInitCmdMsg(msg, attr);
	for (Index = 0; Index < Num; Index++)
	{
		os_zero_mem(&MultiCR, sizeof(MultiCR));
		MultiCR.u4Type = cpu2le32((RF_CR & 0xff) |
							((RegPair->WiFiStream & 0xffffff) << 8));
		MultiCR.u4Addr = cpu2le32(RegPair[Index].Register);
		MultiCR.u4Data = cpu2le32(RegPair[Index].Value);

		MtAndesAppendCmdMsg(msg, (char *)&MultiCR, sizeof(MultiCR));
	}

	Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}


/*****************************************
	ExT_CID = 0x10
*****************************************/
static VOID CmdSecKeyRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{

	P_EVENT_SEC_ADDREMOVE_STRUC_T EvtSecKey;
	UINT32 Status;
	UINT32 WlanIndex;

	EvtSecKey = (struct _EVENT_SEC_ADDREMOVE_STRUC_T *)Data;

	Status = le2cpu32(EvtSecKey->u4Status);
	WlanIndex = le2cpu32(EvtSecKey->u4WlanIdx);

	if (Status != 0)
    {
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
            ("%s, error set key, wlan idx(%d), status: 0x%x\n",
                            __FUNCTION__, WlanIndex, Status));
	}
    else
    {
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                    ("%s, wlan idx(%d), status: 0x%x\n",
                    __FUNCTION__, WlanIndex, Status));
	}
}

INT32 MtCmdSecKeyReq(RTMP_ADAPTER *pAd, UINT8 AddRemove, UINT8 Keytype,
                    UINT8 *pAddr, UINT8 Alg, UINT8 KeyID, UINT8 KeyLen,
                                     UINT8 WlanIdx, UINT8 *KeyMaterial)
{
	struct cmd_msg *msg;
	struct _CMD_SEC_ADDREMOVE_KEY_STRUC_T CmdSecKey;
	int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdSecKey));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_SEC_ADDREMOVE_KEY);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EVENT_SEC_ADDREMOVE_STRUC_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdSecKeyRsp);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&CmdSecKey, sizeof(CmdSecKey));

	CmdSecKey.ucAddRemove = AddRemove;
	CmdSecKey.ucKeyType = Keytype;
	os_move_mem(CmdSecKey.aucPeerAddr, pAddr, 6);
	CmdSecKey.ucAlgorithmId = Alg;
	CmdSecKey.ucKeyId = KeyID;
	CmdSecKey.ucKeyLen = KeyLen;
	os_move_mem(CmdSecKey.aucKeyMaterial, KeyMaterial, KeyLen);
	CmdSecKey.ucWlanIndex = WlanIdx;
	MtAndesAppendCmdMsg(msg, (char *)&CmdSecKey, sizeof(CmdSecKey));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


/*****************************************
	ExT_CID = 0x11
*****************************************/
#ifndef COMPOS_WIN
#ifdef CONFIG_ATE
static INT32 MtCmdFillTxPowerInfo(RTMP_ADAPTER *pAd,
	EXT_CMD_TX_POWER_CTRL_T *CmdTxPwrCtrl, ATE_TXPOWER TxPower)
{
	INT32 ret = 0;
	UINT32 i;
	UINT8 data = 0;
	UINT32 Group = MtATEGetTxPwrGroup(TxPower.Channel,
					  TxPower.Band_idx,
					  TxPower.Ant_idx);
	UINT16 begin_addr = EFUSE_CONTENT_START;

	for(i = EFUSE_CONTENT_START; i <= EFUSE_CONTENT_END; i++) {
		data = pAd->EEPROMImage[i];
		if (ATE_ON(pAd)) {
			if (i == Group)
				data  = TxPower.Power;
		}
		pAd->EEPROMImage[i] = data;
		CmdTxPwrCtrl->aucBinContent[i-begin_addr] = data;
	}
	/* Debug print */
	for(i = EFUSE_CONTENT_START; i <= EFUSE_CONTENT_END; i++) {
		MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("[0x%x]%x ",i,CmdTxPwrCtrl->aucBinContent[i-begin_addr]));
	}
	return ret;
}
/*****************************************
	ExT_CID = 0x1C
*****************************************/
static VOID MtCmdGetTxPowerRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
	P_EXT_EVENT_ID_GET_TX_POWER_T prEventExtCmdResult =
                            (P_EXT_EVENT_ID_GET_TX_POWER_T )Data;
    P_EXT_EVENT_ID_GET_TX_POWER_T prTxPower =
            (P_EXT_EVENT_ID_GET_TX_POWER_T)msg->attr.rsp.wb_buf_in_calbk;
    prTxPower->ucTxPwrType = prEventExtCmdResult->ucTxPwrType;
    prTxPower->ucEfuseAddr = prEventExtCmdResult->ucEfuseAddr;
    prTxPower->ucEfuseContent = prEventExtCmdResult->ucEfuseContent;
    prTxPower->ucBand = prEventExtCmdResult->ucBand;

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s, Type:%d Addr:%x Power:%x Band: %d \n",
            __FUNCTION__, prTxPower->ucTxPwrType, prTxPower->ucEfuseAddr,
                        prTxPower->ucEfuseContent, prTxPower->ucBand));
}


INT32 MtCmdGetTxPower(RTMP_ADAPTER *pAd, UINT8 pwrType, UINT8 centerCh,
            UINT8 dbdc_idx, UINT8 Ch_Band, P_EXT_EVENT_ID_GET_TX_POWER_T prTxPwrResult)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_GET_TX_POWER_T pwr;
   	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GET_TX_POWER_T));
	if (!msg) {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s, Type:%d Channel:%x Band: %d \n",
    	    __FUNCTION__, pwrType, centerCh, dbdc_idx));
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_GET_TX_POWER);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_ID_GET_TX_POWER_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, prTxPwrResult);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetTxPowerRsp);

    MtAndesInitCmdMsg(msg, attr);
	NdisZeroMemory(&pwr, sizeof(pwr));

	pwr.ucTxPwrType = pwrType;
	pwr.ucCenterChannel = centerCh;
	pwr.ucDbdcIdx = dbdc_idx;
	pwr.ucBand = Ch_Band;

	MtAndesAppendCmdMsg(msg, (char *)&pwr, sizeof(pwr));
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

	error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;

}

INT32 MtCmdSetTxPowerCtrl(RTMP_ADAPTER *pAd, ATE_TXPOWER TxPower)
{

	struct cmd_msg *msg;
	EXT_CMD_TX_POWER_CTRL_T CmdTxPwrCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_TX_POWER_CTRL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_SET_TX_POWER_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&CmdTxPwrCtrl, sizeof(CmdTxPwrCtrl));

	CmdTxPwrCtrl.ucCenterChannel = TxPower.Channel;
	CmdTxPwrCtrl.ucDbdcIdx = TxPower.Dbdc_idx;
	CmdTxPwrCtrl.ucBand = TxPower.Band_idx;

	MtCmdFillTxPowerInfo(pAd, &CmdTxPwrCtrl, TxPower);
	MtAndesAppendCmdMsg(msg, (char *)&CmdTxPwrCtrl, sizeof(CmdTxPwrCtrl));
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}
#endif /* CONFIG_ATE */
#endif /* COMPOS_WIN */


/*****************************************

	ExT_CID = 0x12 Thermal Cal

	ucEnable            0:Disable; 1:Enable
	ucSourceMode       0:EFuse; 1:Buffer mode.
	ucRFDiffTemp		 Indicate the temperature difference to trigger RF re-calibration.
	The default value in MT7603 is +/- 40.

	ucHiBBPHT         Set the high temperature threshold to trigger HT BBP calibration.
	ucHiBBPNT         Set the normal Temperature threshold to trigger NT BBP calibration.

	cLoBBPLT          Set the low temperature threshold to trigger LT BBP calibration.
	                    It's might be a negative value.
	cLoBBPNT          Set the normal temperature threshold to trigger NT BBP calibration.
	                    It's might be a negative value.
	For default setting please set ucRFDiffTemp/ ucHiBBPHT/ucHiBBPNT/cLoBBPLT/cLoBBPNT to 0xFF,
	Otherwise, FW will set calibration as these input parameters.
*****************************************/
INT32 MtCmdThermoCal(RTMP_ADAPTER *pAd, UINT8 IsEnable, UINT8 SourceMode,
            UINT8 RFDiffTemp, UINT8 HiBBPHT, UINT8 HiBBPNT, INT8 LoBBPLT, INT8 LoBBPNT)
{
	INT32 ret = 0;
	struct cmd_msg *msg;
	struct _CMD_SET_THERMO_CAL_T Thermo;
    struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&Thermo, sizeof(Thermo));

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: IsEnable = %d, SourceMode = %d, RFDiffTemp = %d\n",
                __FUNCTION__, IsEnable, SourceMode,RFDiffTemp));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: sizeof(Thermo) = %lu\n", __FUNCTION__, (ULONG)sizeof(Thermo)));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(Thermo));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_THERMO_CAL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    Thermo.ucEnable = IsEnable;
	Thermo.ucSourceMode = SourceMode;
	Thermo.ucRFDiffTemp = RFDiffTemp;
	Thermo.ucHiBBPHT = HiBBPHT;
	Thermo.ucHiBBPNT = HiBBPNT;
	Thermo.cLoBBPLT = LoBBPLT;
	Thermo.cLoBBPNT = LoBBPNT;

#ifndef COMPOS_WIN//windows ndis does not have EEPROMImage
	Thermo.ucThermoSetting[0].u2Addr = 0x53;
	Thermo.ucThermoSetting[0].ucValue =
        pAd->EEPROMImage[Thermo.ucThermoSetting[0].u2Addr];
	Thermo.ucThermoSetting[1].u2Addr = 0x54;
	Thermo.ucThermoSetting[1].ucValue =
        pAd->EEPROMImage[Thermo.ucThermoSetting[1].u2Addr];
	Thermo.ucThermoSetting[2].u2Addr  = 0x55;
	Thermo.ucThermoSetting[2].ucValue =
        pAd->EEPROMImage[Thermo.ucThermoSetting[2].u2Addr];
#ifdef RT_BIG_ENDIAN
	Thermo.ucThermoSetting[0].u2Addr = cpu2le16(Thermo.ucThermoSetting[0].u2Addr);
	Thermo.ucThermoSetting[1].u2Addr = cpu2le16(Thermo.ucThermoSetting[1].u2Addr);
	Thermo.ucThermoSetting[2].u2Addr  = cpu2le16(Thermo.ucThermoSetting[2].u2Addr);
#endif
#endif
	MtAndesAppendCmdMsg(msg, (char *)&Thermo, sizeof(Thermo));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


/*****************************************
	ExT_CID = 0x13
*****************************************/
INT32 MtCmdFwLog2Host(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT8 FWLog2HostCtrl)
{

	struct cmd_msg *msg;
	INT32 Ret = 0;
	EXT_CMD_FW_LOG_2_HOST_CTRL_T CmdFwLog2HostCtrl;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
		        (":%s: McuDest(%d):%s\n", __FUNCTION__,
			    McuDest, McuDest == 0 ? "HOST2N9":"HOST2CR4"));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdFwLog2HostCtrl));
	if (!msg)
    {
        Ret = NDIS_STATUS_RESOURCES;
	    return Ret;
    }

    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_FW_LOG_2_HOST);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    os_zero_mem(&CmdFwLog2HostCtrl, sizeof(CmdFwLog2HostCtrl));

	CmdFwLog2HostCtrl.ucFwLog2HostCtrl = FWLog2HostCtrl;

	MtAndesAppendCmdMsg(msg, (char *)&CmdFwLog2HostCtrl,
									sizeof(CmdFwLog2HostCtrl));

	Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		("%s: %s (ret = %d)\n", __FUNCTION__, McuDest == 0 ? "N9":"CR4", Ret));
	return Ret;
}


/*****************************************
	ExT_CID = 0x15
*****************************************/
#ifdef CONFIG_MULTI_CHANNEL
INT MtCmdMccStart(struct _RTMP_ADAPTER *pAd, UINT32 Num,
    MT_MCC_ENTRT_T *MccEntries, USHORT IdleTime, USHORT NullRepeatCnt, ULONG StartTsf)
{
	struct cmd_msg *msg;
    EXT_CMD_MCC_START_T mcc_start_msg;
    int ret;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("\x1b[31m @@@@@ %s:channel(%u,%u), bw(%u,%u), role(%u,%u)\n",
	        __FUNCTION__,MccEntries[0].Channel,MccEntries[1].Channel,
	        MccEntries[0].Bw,MccEntries[1].Bw, MccEntries[0].Role,
	                                        MccEntries[1].Role,));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("cycle_time(%u,%u), wait_time(%u), null_cnt(%u), start_tsf(0x%ld)\x1b[m\n",
                            MccEntries[0].StayTime, MccEntries[1].StayTime,
                                        IdleTime, NullRepeatCnt, StartTsf));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_MCC_START_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_MCC_OFFLOAD_START);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);

    os_zero_mem(&mcc_start_msg, sizeof(EXT_CMD_MCC_START_T));

    mcc_start_msg.u2IdleInterval = cpu2le16(IdleTime); /* ms */
    mcc_start_msg.ucRepeatCnt = (UINT8)NullRepeatCnt;
    mcc_start_msg.ucStartIdx = 0;
    mcc_start_msg.u4StartInstant = cpu2le32(StartTsf);
    mcc_start_msg.u2FreePSEPageTh = 0x11; /* 0:  Disable PSE threshold check */
#ifdef RT_BIG_ENDIAN
	mcc_start_msg.u2FreePSEPageTh = cpu2le16(mcc_start_msg.u2FreePSEPageTh);
#endif

    mcc_start_msg.ucPreSwitchInterval = 0; /* for SDIO */

    mcc_start_msg.ucWlanIdx0 =MccEntries[0].WlanIdx;
    mcc_start_msg.ucPrimaryChannel0 =  MccEntries[0].Channel;
    mcc_start_msg.ucCenterChannel0Seg0 = MccEntries[0].CentralSeg0;
    mcc_start_msg.ucCenterChannel0Seg1 = MccEntries[0].CentralSeg1;
    mcc_start_msg.ucBandwidth0 = MccEntries[0].Bw;
    mcc_start_msg.ucTrxStream0 = 0; /* 2T2R  */
    mcc_start_msg.u2StayInterval0 = cpu2le16(MccEntries[0].StayTime);
    mcc_start_msg.ucRole0 = MccEntries[0].Role;
    mcc_start_msg.ucOmIdx0 = MccEntries[0].OwnMACAddressIdx;
    mcc_start_msg.ucBssIdx0 = MccEntries[0].BssIdx;
    mcc_start_msg.ucWmmIdx0 = MccEntries[0].WmmIdx;

    mcc_start_msg.ucWlanIdx1 = MccEntries[1].WlanIdx; ;
    mcc_start_msg.ucPrimaryChannel1 = MccEntries[1].Channel;
    mcc_start_msg.ucCenterChannel1Seg0 = MccEntries[1].CentralSeg0;
    mcc_start_msg.ucCenterChannel1Seg1 = MccEntries[1].CentralSeg1;
    mcc_start_msg.ucBandwidth1 = MccEntries[1].Bw;
    mcc_start_msg.ucTrxStream1 = 0; /* 2T2R */
    mcc_start_msg.u2StayInterval1 = cpu2le16(MccEntries[1].StayTime);
    mcc_start_msg.ucRole1 = MccEntries[1].Role;
    mcc_start_msg.ucOmIdx1 = MccEntries[1].OwnMACAddressIdx;
    mcc_start_msg.ucBssIdx1 = MccEntries[1].BssIdx;
    mcc_start_msg.ucWmmIdx1 = MccEntries[1].WmmIdx;

    MtAndesAppendCmdMsg(msg, (char *)&mcc_start_msg, sizeof(EXT_CMD_MCC_START_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	return ret;
}
#endif /*CONFIG_MULTI_CHANNEL*/


/*****************************************
	ExT_CID = 0x16
*****************************************/
#ifdef CONFIG_MULTI_CHANNEL
INT32 MtCmdMccStop(struct _RTMP_ADAPTER *pAd, UCHAR ParkingIndex,
    UCHAR AutoResumeMode, UINT16 AutoResumeInterval, ULONG AutoResumeTsf)
{
	struct cmd_msg *msg;
    EXT_CMD_MCC_STOP_T mcc_stop_msg;
	int ret;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("\x1b[32m @@@@@ %s:parking_channel_idx(%u)\n",
                        __FUNCTION__, ParkingIndex));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("auto_resume_mode(%u), auto_resume_tsf(0x%08x) \x1b[m\n",
                                AutoResumeMode, AutoResumeTsf));


	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_MCC_STOP_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_LED);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&mcc_stop_msg, sizeof(EXT_CMD_MCC_STOP_T));

	mcc_stop_msg.ucParkIdx = ParkingIndex;
    mcc_stop_msg.ucAutoResumeMode = AutoResumeMode;
    mcc_stop_msg.u2AutoResumeInterval = cpu2le16(AutoResumeInterval);
    mcc_stop_msg.u4AutoResumeInstant = cpu2le32(AutoResumeTsf);
    mcc_stop_msg.u2IdleInterval  = 0; /* no resume */
    mcc_stop_msg.u2StayInterval0 = 0; /* no resume */
    mcc_stop_msg.u2StayInterval1 = 0; /* no resume */
#ifdef RT_BIG_ENDIAN
	mcc_stop_msg.u2IdleInterval  = cpu2le16(mcc_stop_msg.u2IdleInterval);
    mcc_stop_msg.u2StayInterval0 = cpu2le16(mcc_stop_msg.u2StayInterval0); 
    mcc_stop_msg.u2StayInterval1 = cpu2le16(mcc_stop_msg.u2StayInterval1); 
#endif

	MtAndesAppendCmdMsg(msg, (char *)&mcc_stop_msg, sizeof(EXT_CMD_MCC_STOP_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	return ret;
}
#endif /* CONFIG_MULTI_CHANNEL */


/*****************************************
	ExT_CID = 0x17
*****************************************/
INT32 MtCmdLEDCtrl(
    RTMP_ADAPTER *pAd,
    UINT32    LEDNumber,
    UINT32    LEDBehavior)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_ID_LED_T    ExtLedCMD;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ID_LED_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		return ret;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_LED);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&ExtLedCMD, sizeof(EXT_CMD_ID_LED_T));

	// Filled RF Reg Access CMD setting
	ExtLedCMD.u4LedNo = cpu2le32(LEDNumber);
	ExtLedCMD.u4LedCtrl = cpu2le32(LEDBehavior);


	MtAndesAppendCmdMsg(msg, (char *)&ExtLedCMD,
								sizeof(EXT_CMD_ID_LED_T));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);

   	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: --> LEDNumber: %x, LEDBehavior: %d\n",
                __FUNCTION__, LEDNumber, LEDBehavior));
    return ret;
}


/*****************************************
	ExT_CID = 0x1e
*****************************************/


#if defined(MT_MAC) && (!defined(MT7636)) && defined(TXBF_SUPPORT)
INT32 CmdETxBfAidSetting(
          RTMP_ADAPTER *pAd,
          UINT_16       Aid)
{
	struct cmd_msg *msg;
	UINT8 Input[4];
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: Aid = %d\n", __FUNCTION__, Aid));

	msg = MtAndesAllocCmdMsg(pAd, 4);

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	Input[0] = BF_AID_SET;
    Input[1] = 0;
#ifdef RT_BIG_ENDIAN
	Aid = cpu2le16(Aid);
#endif

	os_move_mem(&Input[2], &Aid, 2);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&Input[0], 4);

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdTxBfApClientCluster(
          RTMP_ADAPTER *pAd,
          UINT_8       ucWlanIdx,
          UINT_8       ucCmmWlanId)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_TXBf_APCLIENT_CLUSTER_T rBfApClientCluster;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: ucWlanIdx = %d, ucPfmuIdx = %d\n", __FUNCTION__, ucWlanIdx, ucCmmWlanId));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_TXBf_APCLIENT_CLUSTER_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	rBfApClientCluster.ucPfmuProfileFormatId = BF_APCLIENT_CLUSTER;
	rBfApClientCluster.ucWlanIdx             = ucWlanIdx;
	rBfApClientCluster.ucCmmWlanId           = ucCmmWlanId;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&rBfApClientCluster, sizeof(EXT_CMD_TXBf_APCLIENT_CLUSTER_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdTxBfReptClonedStaToNormalSta(
          RTMP_ADAPTER *pAd,
          UINT_8       ucWlanIdx,
          UINT_8       ucCliIdx)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_REPT_CLONED_STA_BF_T rBfReptClonedStaToNormalSta;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: ucWlanIdx = %d, ucCliIdx = %d\n", __FUNCTION__, ucWlanIdx, ucCliIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_REPT_CLONED_STA_BF_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	rBfReptClonedStaToNormalSta.ucCmdCategoryID = BF_REPT_CLONED_STA_TO_NORMAL_STA;
	rBfReptClonedStaToNormalSta.ucWlanIdx       = ucWlanIdx;
	rBfReptClonedStaToNormalSta.ucCliIdx        = ucCliIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&rBfReptClonedStaToNormalSta, sizeof(EXT_CMD_REPT_CLONED_STA_BF_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdITxBfPhaseCal(
          RTMP_ADAPTER *pAd,
          UCHAR        ucGroup,
          UCHAR        ucGroup_L_M_H,
          BOOLEAN	   fgSX2,
          UCHAR        ucPhaseCalType,
          UCHAR        ucPhaseVerifyLnaGainLevel)
{
	struct cmd_msg *msg;
	EXT_CMD_ITXBf_PHASE_CAL_CTRL_T aucIBfPhaseCal;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s::: Enable iBF phase calibration : ucGroup = %d, ucGroup_L_M_H = %d, fgSX2 = %d\n",
	                                                     __FUNCTION__, ucGroup, ucGroup_L_M_H, fgSX2));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ITXBf_PHASE_CAL_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&aucIBfPhaseCal, sizeof(EXT_CMD_ITXBf_PHASE_CAL_CTRL_T));

    aucIBfPhaseCal.ucCmdCategoryID = BF_PHASE_CALIBRATION;
    aucIBfPhaseCal.ucGroup         = ucGroup;
    aucIBfPhaseCal.ucGroup_L_M_H   = ucGroup_L_M_H;
    aucIBfPhaseCal.fgSX2           = fgSX2;
    aucIBfPhaseCal.ucPhaseCalType  = ucPhaseCalType;
    aucIBfPhaseCal.ucPhaseVerifyLnaGainLevel = ucPhaseVerifyLnaGainLevel;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)&aucIBfPhaseCal, sizeof(EXT_CMD_ITXBf_PHASE_CAL_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));
	
	return ret;
}


INT32 CmdTxBfLnaGain(
          RTMP_ADAPTER *pAd,
          UCHAR        ucLnaGain)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	UCHAR aucCmdBuf[4];
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s::: LNA gain setting for iBF phase calibration : %d\n",
	                                                     __FUNCTION__, ucLnaGain));

	msg = MtAndesAllocCmdMsg(pAd, 4);

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    aucCmdBuf[0] = BF_LNA_GAIN_CONFIG;
    aucCmdBuf[1] = ucLnaGain;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)aucCmdBuf, 4);

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));
	
	return ret;
}


INT32 CmdITxBfPhaseComp(
          RTMP_ADAPTER *pAd,
          UCHAR        ucBW,
          UCHAR        ucBand,
          UCHAR        ucDbdcBandIdx,
          UCHAR	       ucGroup,
          BOOLEAN      fgRdFromE2p,
          BOOLEAN      fgDisComp)
{
	struct cmd_msg *msg;
	EXT_CMD_ITXBf_PHASE_COMP_CTRL_T aucIBfPhaseComp;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s::: Enable iBF phase compensation : fgRdFromE2p = %d, ucBW = %d, ucDbdcBandIdx = %d\n",
	                                                     __FUNCTION__, fgRdFromE2p, ucBW, ucDbdcBandIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ITXBf_PHASE_COMP_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&aucIBfPhaseComp, sizeof(EXT_CMD_ITXBf_PHASE_COMP_CTRL_T));

    aucIBfPhaseComp.ucCmdCategoryID = BF_IBF_PHASE_COMP;
    aucIBfPhaseComp.ucBW            = ucBW;
    aucIBfPhaseComp.ucBand          = ucBand;
    aucIBfPhaseComp.ucDbdcBandIdx   = ucDbdcBandIdx;
    aucIBfPhaseComp.fgRdFromE2p     = fgRdFromE2p;
    aucIBfPhaseComp.fgDisComp       = fgDisComp;

    if (ucGroup == 0)
    {
        os_move_mem(aucIBfPhaseComp.aucBuf, &pAd->iBfPhaseG0, sizeof(IBF_PHASE_G0_T));
    }
    else
    {
        os_move_mem(aucIBfPhaseComp.aucBuf, &pAd->iBfPhaseGx[ucGroup-1], sizeof(IBF_PHASE_Gx_T));
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 1000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)&aucIBfPhaseComp, sizeof(EXT_CMD_ITXBf_PHASE_COMP_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdTxBfTxApplyCtrl(
          RTMP_ADAPTER *pAd,
          UCHAR			ucWlanId,
          BOOLEAN       fgETxBf,
	      BOOLEAN       fgITxBf,
	      BOOLEAN       fgMuTxBf,
	      BOOLEAN       fgPhaseCali)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_TXBf_TX_APPLY_CTRL_T aucTxBfTxApplyCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: TxBf Tx Apply ucWLanId = %d, fgETxBf = %d, "
        "fgITxBf = %d, fgMuTxBf = %d\n", __FUNCTION__,
            ucWlanId, fgETxBf, fgITxBf, fgMuTxBf));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_TXBf_TX_APPLY_CTRL_T));
	os_zero_mem(&aucTxBfTxApplyCtrl, sizeof(EXT_CMD_TXBf_TX_APPLY_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    aucTxBfTxApplyCtrl.ucCmdCategoryID = BF_DATA_PACKET_APPLY;
    aucTxBfTxApplyCtrl.ucWlanIdx       = ucWlanId;
    aucTxBfTxApplyCtrl.fgETxBf         = fgETxBf;
    aucTxBfTxApplyCtrl.fgITxBf         = fgITxBf;
    aucTxBfTxApplyCtrl.fgMuTxBf        = fgMuTxBf;
    aucTxBfTxApplyCtrl.fgPhaseCali     = fgPhaseCali;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)&aucTxBfTxApplyCtrl, sizeof(EXT_CMD_TXBf_TX_APPLY_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfSoundingPeriodicTriggerCtrl(
          RTMP_ADAPTER *pAd,
          UCHAR			SndgEn,
	      UINT32        u4SNDPeriod,
          UCHAR         ucSu_Mu,
	      UCHAR         ucMuNum,
          PUCHAR        pwlanidx)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_ETXBf_SND_PERIODIC_TRIGGER_CTRL_T ETxBfSndPeriodicTriggerCtrl;
	struct _EXT_CMD_ETXBf_MU_SND_PERIODIC_TRIGGER_CTRL_T ETxBfMuSndPeriodicTriggerCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: Sounding trigger enable = %d\n", __FUNCTION__, SndgEn));

    switch (ucSu_Mu)
    {
    case MU_SOUNDING:
    case MU_PERIODIC_SOUNDING:
    case BF_PROCESSING:
	    msg = MtAndesAllocCmdMsg(pAd, sizeof(ETxBfMuSndPeriodicTriggerCtrl));
	    os_zero_mem(&ETxBfMuSndPeriodicTriggerCtrl, sizeof(ETxBfMuSndPeriodicTriggerCtrl));
	    break;
    case SU_SOUNDING:
	case SU_PERIODIC_SOUNDING:
        msg = MtAndesAllocCmdMsg(pAd, sizeof(ETxBfSndPeriodicTriggerCtrl));
        os_zero_mem(&ETxBfSndPeriodicTriggerCtrl, sizeof(ETxBfSndPeriodicTriggerCtrl));
        break;
    default:
        ret = NDIS_STATUS_INVALID_DATA;
        goto error;
        break;
    }

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	if (SndgEn)
	{
	    switch (ucSu_Mu)
	    {
	    case MU_SOUNDING:
	    case MU_PERIODIC_SOUNDING:
	        ETxBfMuSndPeriodicTriggerCtrl.ucCmdCategoryID     = BF_SOUNDING_ON;
	        ETxBfMuSndPeriodicTriggerCtrl.ucSuMuSndMode       = ucSu_Mu;
	        ETxBfMuSndPeriodicTriggerCtrl.u4SoundingInterval  = cpu2le32(u4SNDPeriod);
	        ETxBfMuSndPeriodicTriggerCtrl.ucWlanIdx[0]        = pwlanidx[0];
	        ETxBfMuSndPeriodicTriggerCtrl.ucWlanIdx[1]        = pwlanidx[1];
	        ETxBfMuSndPeriodicTriggerCtrl.ucWlanIdx[2]        = pwlanidx[2];
	        ETxBfMuSndPeriodicTriggerCtrl.ucWlanIdx[3]        = pwlanidx[3];
	        ETxBfMuSndPeriodicTriggerCtrl.ucStaNum            = ucMuNum;
	        break;

	    case SU_SOUNDING:
	    case SU_PERIODIC_SOUNDING:
	        ETxBfSndPeriodicTriggerCtrl.ucCmdCategoryID       = BF_SOUNDING_ON;
	        ETxBfSndPeriodicTriggerCtrl.ucSuMuSndMode         = ucSu_Mu;
	        ETxBfSndPeriodicTriggerCtrl.u4SoundingInterval    = cpu2le32(u4SNDPeriod);
	        ETxBfSndPeriodicTriggerCtrl.ucWlanIdx[0]          = pwlanidx[0];
	        ETxBfSndPeriodicTriggerCtrl.ucWlanIdx[1]          = pwlanidx[1];
	        ETxBfSndPeriodicTriggerCtrl.ucWlanIdx[2]          = pwlanidx[2];
	        ETxBfSndPeriodicTriggerCtrl.ucWlanIdx[3]          = pwlanidx[3];
	        ETxBfSndPeriodicTriggerCtrl.ucStaNum              = ucMuNum;
	        break;

	    case BF_PROCESSING:
	        ETxBfSndPeriodicTriggerCtrl.ucCmdCategoryID       = BF_SOUNDING_ON;
	        ETxBfSndPeriodicTriggerCtrl.ucSuMuSndMode         = ucSu_Mu;
	        break;

	    default:
	        ret = NDIS_STATUS_INVALID_DATA;
	        goto error;
	        break;
	    }
	}
	else
    {
        ETxBfMuSndPeriodicTriggerCtrl.ucCmdCategoryID = BF_SOUNDING_OFF;
        ETxBfSndPeriodicTriggerCtrl.ucCmdCategoryID   = BF_SOUNDING_OFF;
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

    switch (ucSu_Mu)
    {
    case MU_SOUNDING:
    case MU_PERIODIC_SOUNDING:
	    MtAndesAppendCmdMsg(msg, (char *)&ETxBfMuSndPeriodicTriggerCtrl,
                                sizeof(ETxBfMuSndPeriodicTriggerCtrl));
	    break;

    case SU_SOUNDING:
    case SU_PERIODIC_SOUNDING:
    case BF_PROCESSING:
        MtAndesAppendCmdMsg(msg, (char *)&ETxBfSndPeriodicTriggerCtrl,
                                sizeof(ETxBfSndPeriodicTriggerCtrl));
        break;

    default:
        break;
    }

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdPfmuMemAlloc(
          RTMP_ADAPTER *pAd,
          UCHAR        ucSu_Mu,
          UCHAR        ucWlanId)
{
	struct cmd_msg *msg;
	UCHAR aucCmdBuf[4];
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s: ucWlanId = %d\n", __FUNCTION__, ucWlanId));

	msg = MtAndesAllocCmdMsg(pAd, 4);
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	aucCmdBuf[0] = BF_PFMU_MEM_ALLOCATE;
	aucCmdBuf[1] = ucSu_Mu;
	aucCmdBuf[2] = ucWlanId;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&aucCmdBuf[0], 4);

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdPfmuMemRelease(
          RTMP_ADAPTER *pAd,
          UCHAR        ucPfmuIdx)
{
	struct cmd_msg *msg;
	UCHAR aucCmdBuf[4];
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
	("%s: PFMU ID = %d\n", __FUNCTION__, ucPfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, 2);

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	aucCmdBuf[0] = BF_PFMU_MEM_RELEASE;
	aucCmdBuf[1] = ucPfmuIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&aucCmdBuf[0], 4);

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdPfmuMemAllocMapRead(
          RTMP_ADAPTER *pAd)
{
	struct cmd_msg *msg;
	UCHAR aucCmdBuf[4];
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, 1);

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	aucCmdBuf[0] = BF_PFMU_MEM_ALLOC_MAP_READ;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&aucCmdBuf[0], 4);

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfPfmuProfileTagRead(
          RTMP_ADAPTER *pAd,
          UCHAR        PfmuIdx,
          BOOLEAN      fgBFer)
{
	struct cmd_msg *msg;
	EXT_CMD_ETXBf_PFMU_PROFILE_TAG_R_T ETxBfPfmuProfileTag;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PFMU ID = %d\n", __FUNCTION__, PfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_TAG_R_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ETxBfPfmuProfileTag, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_TAG_R_T));

	ETxBfPfmuProfileTag.ucPfmuProfileFormatId = BF_PFMU_TAG_READ;
	ETxBfPfmuProfileTag.ucPfmuIdx             = PfmuIdx;
	ETxBfPfmuProfileTag.fgBFer                = fgBFer;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&ETxBfPfmuProfileTag,
            sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_TAG_R_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfPfmuProfileTagWrite(
          RTMP_ADAPTER *pAd,
          PUCHAR       prPfmuTag1,
          PUCHAR       prPfmuTag2,
          UCHAR        PfmuIdx)
{
	struct cmd_msg *msg;
	EXT_CMD_ETXBf_PFMU_PROFILE_TAG_W_T ETxBfPfmuProfileTag;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PFMU ID = %d\n", __FUNCTION__, PfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_TAG_W_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ETxBfPfmuProfileTag, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_TAG_W_T));

	ETxBfPfmuProfileTag.ucPfmuProfileFormatId = BF_PFMU_TAG_WRITE;
	ETxBfPfmuProfileTag.ucPfmuIdx             = PfmuIdx;
	os_move_mem(ETxBfPfmuProfileTag.ucBuf,
	               prPfmuTag1,
	               sizeof(PFMU_PROFILE_TAG1));
	os_move_mem(ETxBfPfmuProfileTag.ucBuf + sizeof(PFMU_PROFILE_TAG1),
	               prPfmuTag2,
	               sizeof(PFMU_PROFILE_TAG2));
#ifdef RT_BIG_ENDIAN
	RTMPEndianChange(ETxBfPfmuProfileTag.ucBuf,sizeof(PFMU_PROFILE_TAG1)+sizeof(PFMU_PROFILE_TAG2));
#endif

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&ETxBfPfmuProfileTag,
        sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_TAG_W_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfPfmuProfileDataRead(
          RTMP_ADAPTER *pAd,
          UCHAR        PfmuIdx,
          BOOLEAN      fgBFer,
          USHORT       subCarrIdx)
{
	struct cmd_msg *msg;
	EXT_CMD_ETXBf_PFMU_PROFILE_DATA_R_T ETxBfPfmuProfileData;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PFMU ID = %d\n", __FUNCTION__, PfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_R_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ETxBfPfmuProfileData, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_R_T));

	ETxBfPfmuProfileData.ucPfmuProfileFormatId = BF_PROFILE_READ;
	ETxBfPfmuProfileData.ucPfmuIdx             = PfmuIdx;
	ETxBfPfmuProfileData.fgBFer                = fgBFer;
	ETxBfPfmuProfileData.u2SubCarrIdx          = cpu2le16(subCarrIdx);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 500);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&ETxBfPfmuProfileData,
        sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_R_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfPfmuProfileDataWrite(
          RTMP_ADAPTER *pAd,
          UCHAR        PfmuIdx,
          USHORT       SubCarrIdx,
          PUCHAR       pProfileData)
{
	struct cmd_msg *msg;
	EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_T ETxBfPfmuProfileData;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PFMU ID = %d\n", __FUNCTION__, PfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ETxBfPfmuProfileData, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_T));

	ETxBfPfmuProfileData.ucPfmuProfileFormatId = BF_PROFILE_WRITE;
	ETxBfPfmuProfileData.ucPfmuIdx             = PfmuIdx;
	ETxBfPfmuProfileData.u2SubCarr             = cpu2le16(SubCarrIdx);
	os_move_mem(ETxBfPfmuProfileData.ucBuf, pProfileData, sizeof(ETxBfPfmuProfileData) - 4);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Buf[0~3]= %x:%x:%x:%x\n",
	    ETxBfPfmuProfileData.ucBuf[0], ETxBfPfmuProfileData.ucBuf[1],
	    ETxBfPfmuProfileData.ucBuf[2], ETxBfPfmuProfileData.ucBuf[3]));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Buf[4~7]= %x:%x:%x:%x\n",
	    ETxBfPfmuProfileData.ucBuf[4], ETxBfPfmuProfileData.ucBuf[5],
	    ETxBfPfmuProfileData.ucBuf[6], ETxBfPfmuProfileData.ucBuf[7]));
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Buf[8~11]= %x:%x:%x:%x\n",
	    ETxBfPfmuProfileData.ucBuf[8], ETxBfPfmuProfileData.ucBuf[9],
	    ETxBfPfmuProfileData.ucBuf[10], ETxBfPfmuProfileData.ucBuf[11]));
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("Buf[12~15]= %x:%x:%x:%x\n",
	    ETxBfPfmuProfileData.ucBuf[12], ETxBfPfmuProfileData.ucBuf[13],
	    ETxBfPfmuProfileData.ucBuf[14], ETxBfPfmuProfileData.ucBuf[15]));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

	MtAndesAppendCmdMsg(msg, (char *)&ETxBfPfmuProfileData,
        sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfPfmuProfileDataWrite20MAll(
          RTMP_ADAPTER     *pAd,
          UCHAR            PfmuIdx,
          PUCHAR           pProfileData)
{
	struct cmd_msg *msg;
	EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_20M_ALL_T ETxBfPfmuProfileData;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};
    //UINT_16 u2Loop, u2Temp;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PFMU ID = %d\n", __FUNCTION__, PfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_20M_ALL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ETxBfPfmuProfileData, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_20M_ALL_T));

	ETxBfPfmuProfileData.ucPfmuProfileFormatId = BF_PROFILE_WRITE_20M_ALL;
	ETxBfPfmuProfileData.ucPfmuIdx             = PfmuIdx;
	os_move_mem(ETxBfPfmuProfileData.ucBuf, pProfileData, 512);
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

	MtAndesAppendCmdMsg(msg, (char *)&ETxBfPfmuProfileData,
        sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_DATA_W_20M_ALL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfPfmuProfilePnRead(
          RTMP_ADAPTER *pAd,
          UCHAR        PfmuIdx)
{
	struct cmd_msg *msg;
	EXT_CMD_ETXBf_PFMU_PROFILE_PN_R_T ETxBfPfmuProfilePn;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PFMU ID = %d\n", __FUNCTION__, PfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_PN_R_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ETxBfPfmuProfilePn, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_PN_R_T));

	ETxBfPfmuProfilePn.ucPfmuProfileFormatId = BF_PN_READ;
	ETxBfPfmuProfilePn.ucPfmuIdx             = PfmuIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 500);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&ETxBfPfmuProfilePn,
        sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_PN_R_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 CmdETxBfPfmuProfilePnWrite(
          RTMP_ADAPTER *pAd,
          UCHAR        PfmuIdx,
          UCHAR        ucBw,
          PUCHAR       pProfileData)
{
	struct cmd_msg *msg;
	EXT_CMD_ETXBf_PFMU_PROFILE_PN_W_T ETxBfPfmuProfileData;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PFMU ID = %d\n", __FUNCTION__, PfmuIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_PN_W_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ETxBfPfmuProfileData, sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_PN_W_T));

	ETxBfPfmuProfileData.ucPfmuProfileFormatId = BF_PN_WRITE;
	ETxBfPfmuProfileData.ucPfmuIdx             = PfmuIdx;
    ETxBfPfmuProfileData.ucBW                  = ucBw;

	switch (ucBw)
	{
	case P_DBW20M:
#ifdef RT_BIG_ENDIAN
		RTMPEndianChange(pProfileData,sizeof(PFMU_PN_DBW20M));
#endif
	    os_move_mem(ETxBfPfmuProfileData.ucBuf, pProfileData, sizeof(PFMU_PN_DBW20M));
	    break;
	case P_DBW40M:
#ifdef RT_BIG_ENDIAN
		RTMPEndianChange(pProfileData,sizeof(PFMU_PN_DBW40M));
#endif
	    os_move_mem(ETxBfPfmuProfileData.ucBuf, pProfileData, sizeof(PFMU_PN_DBW40M));
	    break;
	case P_DBW80M:
#ifdef RT_BIG_ENDIAN
		RTMPEndianChange(pProfileData,sizeof(PFMU_PN_DBW80M));
#endif
	    os_move_mem(ETxBfPfmuProfileData.ucBuf, pProfileData, sizeof(PFMU_PN_DBW80M));
	    break;
	case P_DBW160M:
#ifdef RT_BIG_ENDIAN
		RTMPEndianChange(pProfileData,sizeof(PFMU_PN_DBW80_80M));
#endif
	    os_move_mem(ETxBfPfmuProfileData.ucBuf, pProfileData, sizeof(PFMU_PN_DBW80_80M));
	    break;
	default:
	    return 1;
	    break;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&ETxBfPfmuProfileData,
        sizeof(EXT_CMD_ETXBf_PFMU_PROFILE_PN_W_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 CmdETxBfStaRecRead(
          RTMP_ADAPTER *pAd,
          UCHAR        ucWlanID)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	CHAR ucCmd[8];
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: WLAN ID = %d\n", __FUNCTION__, ucWlanID));

	msg = MtAndesAllocCmdMsg(pAd, 8);

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	ucCmd[0] = BF_STA_REC_READ;
	ucCmd[1] = ucWlanID;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, &ucCmd[0], 8);

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 CmdTxBfTxPwrBackOff(
          RTMP_ADAPTER *pAd,
          UCHAR        ucBand,
          PUCHAR       paucTxPwrFccBfOnCase,
          PUCHAR       paucTxPwrFccBfOffCase)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_BF_TX_PWR_BACK_OFF_T rTxPwrBackOff;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: ucBand = %d\n", __FUNCTION__, ucBand));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_BF_TX_PWR_BACK_OFF_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	rTxPwrBackOff.ucCmdCategoryID = BF_TX_POWER_BACK_OFF;
	rTxPwrBackOff.ucBand          = ucBand;
	os_move_mem(rTxPwrBackOff.aucTxPwrFccBfOnCase, paucTxPwrFccBfOnCase, 10);
	os_move_mem(rTxPwrBackOff.aucTxPwrFccBfOffCase, paucTxPwrFccBfOffCase, 10);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&rTxPwrBackOff, sizeof(EXT_CMD_BF_TX_PWR_BACK_OFF_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;	
}


INT32 CmdTxBfAwareCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN       fgBfAwareCtrl)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_BF_AWARE_CTRL_T rTxBfAwareCtrl;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgBfAwareCtrl = %d\n", __FUNCTION__, fgBfAwareCtrl));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_BF_AWARE_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	rTxBfAwareCtrl.ucCmdCategoryID = BF_AWARE_CTRL;
	rTxBfAwareCtrl.fgBfAwareCtrl   = fgBfAwareCtrl;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&rTxBfAwareCtrl, sizeof(EXT_CMD_BF_AWARE_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;	
}


INT32 CmdTxBfHwEnableStatusUpdate(
          RTMP_ADAPTER *pAd,
          BOOLEAN       fgEBf,
          BOOLEAN       fgIBf)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_BF_HW_ENABLE_STATUS_UPDATE_T rTxBfHwEnStatusUpdate;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgEBfHwEnable = %d, fgIBfHwEnable = %d\n", __FUNCTION__, fgEBf, fgIBf));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_BF_HW_ENABLE_STATUS_UPDATE_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	rTxBfHwEnStatusUpdate.ucCmdCategoryID = BF_HW_ENABLE_STATUS_UPDATE;
	rTxBfHwEnStatusUpdate.fgEBfHwEnStatus = fgEBf;
	rTxBfHwEnStatusUpdate.fgIBfHwEnStatus = fgIBf;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BF_ACTION);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&rTxBfHwEnStatusUpdate, sizeof(EXT_CMD_BF_HW_ENABLE_STATUS_UPDATE_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;	
}
#endif /* MT_MAC && TXBF_SUPPORT */


/*****************************************
	ExT_CID = 0x21
*****************************************/
static VOID CmdEfuseBufferModeRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	struct _EVENT_EXT_CMD_RESULT_T *EventExtCmdResult =
                                    (struct _EVENT_EXT_CMD_RESULT_T *)Data;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s: EventExtCmdResult.ucExTenCID = 0x%x\n",
                __FUNCTION__, EventExtCmdResult->ucExTenCID));
	EventExtCmdResult->u4Status = le2cpu32(EventExtCmdResult->u4Status);
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s: EventExtCmdResult.u4Status = 0x%x\n",
                __FUNCTION__, EventExtCmdResult->u4Status));
}

VOID MtCmdEfusBufferModeSet(RTMP_ADAPTER *pAd, UINT8 EepromType)
{
	struct cmd_msg *msg = NULL;
	EXT_CMD_EFUSE_BUFFER_MODE_T *CmdEfuseBufferMode = NULL;
	int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	os_alloc_mem(pAd, (UCHAR **)&CmdEfuseBufferMode, sizeof(EXT_CMD_EFUSE_BUFFER_MODE_T));
	if (!CmdEfuseBufferMode)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}


	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_EFUSE_BUFFER_MODE_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_EFUSE_BUFFER_MODE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 60000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdEfuseBufferModeRsp);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(CmdEfuseBufferMode, sizeof(EXT_CMD_EFUSE_BUFFER_MODE_T));

	switch(EepromType)
    {
	case EEPROM_EFUSE:
    	CmdEfuseBufferMode->ucSourceMode = EEPROM_MODE_EFUSE;
    	CmdEfuseBufferMode->ucCount = 0;
    	break;
	case EEPROM_FLASH:
		CmdEfuseBufferMode->ucSourceMode = EEPROM_MODE_BUFFER;
		if(pAd->chipOps.bufferModeEfuseFill)
		{
        	pAd->chipOps.bufferModeEfuseFill(pAd, CmdEfuseBufferMode);
        }
			else
		{
			/*force to efuse mode*/
			CmdEfuseBufferMode->ucSourceMode = EEPROM_MODE_EFUSE;
			CmdEfuseBufferMode->ucCount = 0;
		}
		break;
    default:
		ret = NDIS_STATUS_FAILURE;
		goto error;
	}

 	MtAndesAppendCmdMsg(msg, (char *)CmdEfuseBufferMode,
                    sizeof(EXT_CMD_EFUSE_BUFFER_MODE_T));
	ret = pAd->chipOps.MtCmdTx(pAd, msg);
	goto done;

error:
	if (msg)
    {
		MtAndesFreeCmdMsg(msg);
    }
done:
	if (CmdEfuseBufferMode)
		os_free_mem(CmdEfuseBufferMode);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
	        ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return;
}

/*****************************************
	ExT_CID = 0x27
*****************************************/
INT32 MtCmdEdcaParameterSet(RTMP_ADAPTER *pAd, MT_EDCA_CTRL_T EdcaParam)
{
	struct cmd_msg *msg;
#ifdef RT_BIG_ENDIAN
	P_TX_AC_PARAM_T pAcParam;
	INT32 i = 0;
#endif
	INT32 ret = 0,size = 0;
    struct _CMD_ATTRIBUTE attr = {0};
	size = 4 + sizeof(TX_AC_PARAM_T)*EdcaParam.ucTotalNum;
#ifdef RT_BIG_ENDIAN
	for(i = 0; i < EdcaParam.ucTotalNum; i++)
	{
		pAcParam = &EdcaParam.rAcParam[i];
		pAcParam->u2Txop = cpu2le16(pAcParam->u2Txop);
		pAcParam->u2WinMax = cpu2le16(pAcParam->u2WinMax);
	}
#endif
	msg = MtAndesAllocCmdMsg(pAd, size);
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_EDCA_SET);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    if (size <= sizeof(MT_EDCA_CTRL_T))
	    MtAndesAppendCmdMsg(msg, (char *)&EdcaParam, size);

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);
	return ret;
error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


 /*****************************************
	 ExT_CID = 0x28
 *****************************************/
 INT32 MtCmdSlotTimeSet(RTMP_ADAPTER *pAd, UINT8 SlotTime,
        UINT8 SifsTime,UINT8 RifsTime,UINT16 EifsTime, UCHAR BandIdx)
 {
	struct cmd_msg *msg;
	INT32 ret = 0;
	CMD_SLOT_TIME_SET_T cmdSlotTime;
    struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&cmdSlotTime, sizeof(CMD_SLOT_TIME_SET_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_SLOT_TIME_SET_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_SLOT_TIME_SET);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	cmdSlotTime.u2Eifs= cpu2le16(EifsTime);
	cmdSlotTime.ucRifs = RifsTime;
	cmdSlotTime.ucSifs = SifsTime;
	cmdSlotTime.ucSlotTime = SlotTime;
	cmdSlotTime.ucBandNum = (UINT8)BandIdx;

	MtAndesAppendCmdMsg(msg, (char *)&cmdSlotTime, sizeof(CMD_SLOT_TIME_SET_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);
	return ret;
error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
 }

/*****************************************
	ExT_CID = 0x23
*****************************************/

#ifdef THERMAL_PROTECT_SUPPORT
INT32 MtCmdThermalProtect(
    RTMP_ADAPTER *pAd,
    UINT8 HighEn,
    CHAR HighTempTh,
    UINT8 LowEn,
    CHAR LowTempTh,
    UINT32 RechkTimer,
    UINT8 RFOffEn,
    CHAR RFOffTh,    
    UINT8 ucType
    )
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_THERMAL_PROTECT_T ThermalProtect;
    struct _CMD_ATTRIBUTE attr = {0};
	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_THERMAL_PROTECT_T));
	if (!msg)
	{
         ret = NDIS_STATUS_RESOURCES;
         goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_THERMAL_PROTECT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&ThermalProtect, sizeof(ThermalProtect));

	ThermalProtect.ucHighEnable = HighEn;
	ThermalProtect.cHighTempThreshold = HighTempTh;
	ThermalProtect.ucLowEnable = LowEn;
	ThermalProtect.cLowTempThreshold = LowTempTh;
    ThermalProtect.RecheckTimer = cpu2le32(RechkTimer);
    ThermalProtect.ucRFOffEnable = RFOffEn;
    ThermalProtect.cRFOffThreshold = RFOffTh;
    ThermalProtect.ucType = ucType;

	MtAndesAppendCmdMsg(msg, (char *)&ThermalProtect, sizeof(ThermalProtect));
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
      MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
   	return ret;
}


INT32
MtCmdThermalProtectAdmitDuty(
	RTMP_ADAPTER *pAd,
	UINT32 u4Lv0Duty,
	UINT32 u4Lv1Duty,
	UINT32 u4Lv2Duty,
	UINT32 u4Lv3Duty
	)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_THERMAL_PROTECT_T ThermalProtect;
    struct _CMD_ATTRIBUTE attr = {0};
	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_THERMAL_PROTECT_T));
	if (!msg)
	{
         ret = NDIS_STATUS_RESOURCES;
         goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_THERMAL_PROTECT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&ThermalProtect, sizeof(ThermalProtect));

    ThermalProtect.ucExtraTag = THERAML_PROTECTION_TAG_SET_ADMIT_DUTY;
	ThermalProtect.ucLv0Duty = (UINT8)u4Lv0Duty;
	ThermalProtect.ucLv1Duty = (UINT8)u4Lv1Duty;
	ThermalProtect.ucLv2Duty = (UINT8)u4Lv2Duty;
	ThermalProtect.ucLv3Duty = (UINT8)u4Lv3Duty;

	MtAndesAppendCmdMsg(msg, (char *)&ThermalProtect, sizeof(ThermalProtect));
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
      MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
   	return ret;

}
#endif


/*****************************************
	ExT_CID = 0x2c
*****************************************/
static VOID MtCmdThemalSensorRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
	struct _EXT_EVENT_GET_SENSOR_RESULT_T *EventExtCmdResult =
                                (struct _EXT_EVENT_GET_SENSOR_RESULT_T *)Data;
	EventExtCmdResult->u4SensorResult =
                                le2cpu32(EventExtCmdResult->u4SensorResult);
    os_move_mem(msg->attr.rsp.wb_buf_in_calbk, &EventExtCmdResult->u4SensorResult,
                                sizeof(EventExtCmdResult->u4SensorResult));
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("ThemalSensor = 0x%x\n", EventExtCmdResult->u4SensorResult));
}

/*
ActionIdx:  0: get temperature; 1: get thermo sensor ADC
*/
INT32 MtCmdGetThermalSensorResult(RTMP_ADAPTER *pAd, UINT8 ActionIdx,
                                                UINT32 *SensorResult)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_GET_SENSOR_RESULT_T Cmdmsg;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};
	os_zero_mem(&Cmdmsg, sizeof(Cmdmsg));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: Action = %d\n", __FUNCTION__,ActionIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(Cmdmsg));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_GET_THEMAL_SENSOR);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, SensorResult);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdThemalSensorRsp);

    MtAndesInitCmdMsg(msg, attr);
    Cmdmsg.ucActionIdx = ActionIdx;
	MtAndesAppendCmdMsg(msg, (char *)&Cmdmsg, sizeof(Cmdmsg));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


/*****************************************
	ExT_CID = 0x2d
*****************************************/
INT32 MtCmdTmrCal(RTMP_ADAPTER *pAd, UINT8 Enable, UINT8 Band,
                            UINT8 Bw, UINT8 Ant, UINT8 Role)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	EXT_CMD_TMR_CAL_T TmrCal;
    struct _CMD_ATTRIBUTE attr = {0};

    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_TMR_CAL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_TMR_CAL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&TmrCal, sizeof(TmrCal));

	TmrCal.ucEnable = Enable;
	TmrCal.ucBand = Band;
	TmrCal.ucBW = Bw;
	TmrCal.ucAnt = Ant;//only ant 0 support at present.
	TmrCal.ucRole = Role;

	MtAndesAppendCmdMsg(msg, (char *)&TmrCal, sizeof(TmrCal));
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}

/*****************************************
	ExT_CID = 0x2E
*****************************************/

#ifdef MT_WOW_SUPPORT
static VOID EventExtCmdPacketFilterRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	P_EXT_EVENT_PF_GENERAL_T pPFRsp = (P_EXT_EVENT_PF_GENERAL_T)Data;

	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
	    ("%s: u4PfCmdType = 0x%x u4Status = 0x%x\n", __FUNCTION__,
	        le2cpu32(pPFRsp->u4PfCmdType), le2cpu32(pPFRsp->u4Status)));
}

static VOID EventExtCmdWakeupOptionRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	P_EXT_EVENT_WAKEUP_OPTION_T pWakeOptRsp = (P_EXT_EVENT_WAKEUP_OPTION_T)Data;

	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
	    ("%s: u4PfCmdType = 0x%x u4Status = 0x%x\n", __FUNCTION__,
	    le2cpu32(pWakeOptRsp->u4PfCmdType), le2cpu32(pWakeOptRsp->u4Status)));

}

VOID MT76xxAndesWOWEnable(
	PRTMP_ADAPTER pAd,
	PSTA_ADMIN_CONFIG pStaCfg)
{

	//hw-enable cmd
	//1. magic, parameter=enable
	//2. eapol , param = enable
	//3. bssid , param = bssid[3:0]
	//4. mode, parm = white
	//5. PF, param = enable
	//wakeup command param = choose usb. others dont' care

	struct wifi_dev *wdev = &pStaCfg->wdev;
	UINT32 BandIdx = 0; 

	MAC_TABLE_ENTRY *pEntry = NULL;

	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
	struct _CMD_ATTRIBUTE attr = {0};

	CMD_PACKET_FILTER_GLOBAL_T CmdPFGlobal;
	CMD_PACKET_FILTER_GTK_T CmdGTK;
	CMD_PACKET_FILTER_ARPNS_T CmdArpNs;
	CMD_PACKET_FILTER_WAKEUP_OPTION_T CmdWakeupOption;

	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                            ("%s:\n", __FUNCTION__));

	pEntry = GetAssociatedAPByWdev(pAd, wdev);
	ASSERT(pEntry);

#ifdef DBDC_MODE
	BandIdx = HcGetBandByWdev(wdev);
#endif /* DBDC_MODE */	

	/* Security configuration */
	if (IS_AKM_PSK(pEntry->SecConfig.AKMMap))
	{
    	os_zero_mem(&CmdGTK, sizeof(CmdGTK));

    	msg = AndesAllocCmdMsg(pAd, sizeof(CMD_PACKET_FILTER_GTK_T));
    	if (!msg)
    	{
    		ret = NDIS_STATUS_RESOURCES;
    		goto error;
    	}

    	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    	SET_CMD_ATTR_TYPE(attr, EXT_CID);
    	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PACKET_FILTER);
    	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_PF_GENERAL_T));
    	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    	SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdPacketFilterRsp);

    	AndesInitCmdMsg(msg, attr);

    	CmdGTK.PFType = cpu2le32(_ENUM_TYPE_GTK_REKEY);

    	if (IS_AKM_WPA1PSK(wdev->SecConfig.AKMMap))
    		CmdGTK.WPAVersion = cpu2le32(PF_WPA);
    	else
    		CmdGTK.WPAVersion = cpu2le32(PF_WPA2);

    	MTWF_LOG(DBG_CAT_P2P, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
            ("%s::Bssid(%02x:%02x:%02x:%02x:%02x:%02x), Wcid(%d, %d), McMcIdx(%d)\n",
    				__FUNCTION__,
    				PRINT_MAC(wdev->bssid),
    				pEntry->wcid, wdev->tr_tb_idx,
    				wdev->bss_info_argument.ucBcMcWlanIdx));

    	// TODO: Pat: how if big endian
    	NdisCopyMemory(CmdGTK.PTK, pEntry->SecConfig.PTK, 64);

    	CmdGTK.BssidIndex = cpu2le32(wdev->bss_info_argument.ucBssIndex);
    	CmdGTK.OwnMacIndex = cpu2le32(wdev->OmacIdx);
    	CmdGTK.WmmIndex = cpu2le32(PF_WMM_0);

    	if (IS_AKM_PSK(pEntry->SecConfig.AKMMap))
    	{
    		NdisCopyMemory(CmdGTK.ReplayCounter, pEntry->SecConfig.Handshake.ReplayCounter, LEN_KEY_DESC_REPLAY);
    		CmdGTK.GroupKeyIndex = cpu2le32(wdev->bss_info_argument.ucBcMcWlanIdx);
    		CmdGTK.PairKeyIndex = cpu2le32(pEntry->wcid);
    	}

    	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s::GTK offload::BssidIndex %d, GroupKeyIndex %d, OwnMacIndex %d, PairKeyIndex %d, WmmIndex %d\n",
    			__FUNCTION__, CmdGTK.BssidIndex, CmdGTK.GroupKeyIndex, CmdGTK.OwnMacIndex, CmdGTK.PairKeyIndex, CmdGTK.WmmIndex));

    	AndesAppendCmdMsg(msg, (char *)&CmdGTK, sizeof(CMD_PACKET_FILTER_GTK_T));

    	ret = AndesSendCmdMsg(pAd, msg);
	}

    	/* ARP/NS offlaod */
    	os_zero_mem(&CmdArpNs, sizeof(CMD_PACKET_FILTER_ARPNS_T));

    	msg = AndesAllocCmdMsg(pAd, sizeof(CMD_PACKET_FILTER_ARPNS_T));
    	if (!msg)
    	{
    		ret = NDIS_STATUS_RESOURCES;
    		goto error;
    	}

    	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    	SET_CMD_ATTR_TYPE(attr, EXT_CID);
    	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PACKET_FILTER);
    	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_PF_GENERAL_T));
    	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    	SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdPacketFilterRsp);

    	AndesInitCmdMsg(msg, attr);
    	CmdArpNs.PFType = cpu2le32(_ENUM_TYPE_ARPNS);
    	CmdArpNs.IPIndex = cpu2le32(PF_ARP_NS_SET_0);
    	CmdArpNs.Enable = cpu2le32(PF_ARP_NS_ENABLE);
    	CmdArpNs.BssidEnable = cpu2le32(PF_BSSID_0);
    	CmdArpNs.Offload = cpu2le32(PF_ARP_OFFLOAD);
    	CmdArpNs.Type = cpu2le32(PF_ARP_NS_ALL_PKT); 
    	CmdArpNs.IPAddress[0] = pAd->WOW_Cfg.IPAddress[0];	// 192
    	CmdArpNs.IPAddress[1] = pAd->WOW_Cfg.IPAddress[1];	// 168
    	CmdArpNs.IPAddress[2] = pAd->WOW_Cfg.IPAddress[2];	// 2
    	CmdArpNs.IPAddress[3] = pAd->WOW_Cfg.IPAddress[3];	// 10
    	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, 
            ("%s::ARP offload::IP(%d:%d:%d:%d)\n",
    			__FUNCTION__, CmdArpNs.IPAddress[0], CmdArpNs.IPAddress[1], CmdArpNs.IPAddress[2], CmdArpNs.IPAddress[3]));
    	
    		
    	AndesAppendCmdMsg(msg, (char *)&CmdArpNs, sizeof(CMD_PACKET_FILTER_ARPNS_T));

    	ret = AndesSendCmdMsg(pAd, msg);

	/* Wakeup option */
	os_zero_mem(&CmdWakeupOption, sizeof(CMD_PACKET_FILTER_WAKEUP_OPTION_T));

	msg = AndesAllocCmdMsg(pAd, sizeof(CMD_PACKET_FILTER_WAKEUP_OPTION_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_WAKEUP_OPTION);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_WAKEUP_OPTION_T));
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdWakeupOptionRsp);

	AndesInitCmdMsg(msg, attr);
	CmdWakeupOption.WakeupInterface = cpu2le32(pAd->WOW_Cfg.nWakeupInterface);

	if (pAd->WOW_Cfg.nWakeupInterface == WOW_WAKEUP_BY_GPIO) {
	    UINT32 GpioParameter = 0;
	    
	    CmdWakeupOption.GPIONumber= cpu2le32(pAd->WOW_Cfg.nSelectedGPIO); // which GPIO
	    CmdWakeupOption.GPIOTimer = cpu2le32(pAd->WOW_Cfg.nHoldTime); // unit is us

            if (pAd->WOW_Cfg.bGPIOHighLow == WOW_GPIO_LOW_TO_HIGH)
                GpioParameter = WOW_GPIO_LOW_TO_HIGH_PARAMETER;
            else
                GpioParameter = WOW_GPIO_HIGH_TO_LOW_PARAMETER;   
            
            CmdWakeupOption.GpioParameter = cpu2le32(GpioParameter);
	}

    	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, 
            ("%s::Wakeup Option::nWakeupInterface(%d), GPIONumber(%d), GPIOTimer(%d), GpioParameter(0x%x)\n",
    			__FUNCTION__, 
    			CmdWakeupOption.WakeupInterface, 
    			CmdWakeupOption.GPIONumber, 
    			CmdWakeupOption.GPIOTimer, 
    			CmdWakeupOption.GpioParameter));

	AndesAppendCmdMsg(msg, (char *)&CmdWakeupOption, sizeof(CMD_PACKET_FILTER_WAKEUP_OPTION_T));

	ret = AndesSendCmdMsg(pAd, msg);


	/* WOW enable */
	os_zero_mem(&CmdPFGlobal, sizeof(CMD_PACKET_FILTER_GLOBAL_T));

	msg = AndesAllocCmdMsg(pAd, sizeof(CMD_PACKET_FILTER_GLOBAL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PACKET_FILTER);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_PF_GENERAL_T));
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdPacketFilterRsp);

	AndesInitCmdMsg(msg, attr);
	CmdPFGlobal.PFType = cpu2le32(_ENUM_TYPE_GLOBAL_EN);
	CmdPFGlobal.FunctionSelect = cpu2le32(_ENUM_GLOBAL_WOW_EN);
	CmdPFGlobal.Enable = cpu2le32(PF_BSSID_0);
	CmdPFGlobal.Band = cpu2le32(BandIdx);//cpu2le32(PF_BAND_0);

	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, 
        ("%s::Wakeup option::Band(%d)\n",
			__FUNCTION__, BandIdx));

	AndesAppendCmdMsg(msg, (char *)&CmdPFGlobal, sizeof(CMD_PACKET_FILTER_GLOBAL_T));

	ret = AndesSendCmdMsg(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return;
}

VOID MT76xxAndesWOWDisable(
    PRTMP_ADAPTER pAd,
    PSTA_ADMIN_CONFIG pStaCfg)
{
    struct wifi_dev *wdev = &pStaCfg->wdev;
    UINT32 BandIdx = 0; 
	CMD_PACKET_FILTER_GLOBAL_T CmdPFGlobal;

	struct cmd_msg *msg;
	INT32 ret = NDIS_STATUS_SUCCESS;
    struct _CMD_ATTRIBUTE attr = {0};
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                            ("%s:\n", __FUNCTION__));

    ASSERT(wdev);

#ifdef DBDC_MODE
	BandIdx = HcGetBandByWdev(wdev);
#endif /* DBDC_MODE */	
   
	// WOW disable
	os_zero_mem(&CmdPFGlobal, sizeof(CMD_PACKET_FILTER_GLOBAL_T));

	msg = AndesAllocCmdMsg(pAd, sizeof(CMD_PACKET_FILTER_GLOBAL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PACKET_FILTER);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_PF_GENERAL_T));
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdPacketFilterRsp);

	AndesInitCmdMsg(msg, attr);
	CmdPFGlobal.PFType = cpu2le32(_ENUM_TYPE_GLOBAL_EN);
	CmdPFGlobal.FunctionSelect = cpu2le32(_ENUM_GLOBAL_WOW_EN);
	CmdPFGlobal.Enable = cpu2le32(PF_BSSID_DISABLE);
	CmdPFGlobal.Band = cpu2le32(BandIdx);//cpu2le32(PF_BAND_0);

	AndesAppendCmdMsg(msg, (char *)&CmdPFGlobal, sizeof(CMD_PACKET_FILTER_GLOBAL_T));

	ret = AndesSendCmdMsg(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return;
}

#endif

#ifdef RACTRL_FW_OFFLOAD_SUPPORT
/*****************************************
	ExT_CID = 0x30
*****************************************/
static VOID MtCmdGetTxStatisticRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
	P_EXT_EVENT_TX_STATISTIC_RESULT_T prEventExtCmdResult =
                            (P_EXT_EVENT_TX_STATISTIC_RESULT_T)Data;
    P_EXT_EVENT_TX_STATISTIC_RESULT_T prTxStat =
        (P_EXT_EVENT_TX_STATISTIC_RESULT_T)msg->attr.rsp.wb_buf_in_calbk;
    prTxStat->u4Field = le2cpu32(prEventExtCmdResult->u4Field);
    if (prTxStat->u4Field & GET_TX_STAT_TOTAL_TX_CNT)
    {
        prTxStat->u4TotalTxCount =
            le2cpu32(prEventExtCmdResult->u4TotalTxCount);
        prTxStat->u4TotalTxFailCount =
            le2cpu32(prEventExtCmdResult->u4TotalTxFailCount);
        prTxStat->u4TotalCurrBwTxCnt=
            le2cpu32(prEventExtCmdResult->u4TotalCurrBwTxCnt);
        prTxStat->u4TotalOtherBwTxCnt=
            le2cpu32(prEventExtCmdResult->u4TotalOtherBwTxCnt);
    }
    if (prTxStat->u4Field & GET_TX_STAT_LAST_TX_RATE)
    {
        os_move_mem(&prTxStat->rLastTxRate,
                &prEventExtCmdResult->rLastTxRate, sizeof(RA_PHY_CFG_T));
    }
    if (prTxStat->u4Field & GET_TX_STAT_ENTRY_TX_RATE)
    {
        os_move_mem(&prTxStat->rEntryTxRate,
                &prEventExtCmdResult->rEntryTxRate, sizeof(RA_PHY_CFG_T));
    }
}

INT32 MtCmdGetTxStatistic(struct _RTMP_ADAPTER *pAd, UINT32 u4Field,
        UINT8 ucWcid, P_EXT_EVENT_TX_STATISTIC_RESULT_T prTxStatResult)
{
	struct cmd_msg *msg;
	EXT_CMD_GET_TX_STATISTIC_T rTxStatCmd;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&rTxStatCmd, sizeof(rTxStatCmd));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:u4Field=0x%8x, ucWcid=%d\n", __FUNCTION__,u4Field, ucWcid));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(rTxStatCmd));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_GET_TX_STATISTICS);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_EVENT_TX_STATISTIC_RESULT_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, prTxStatResult);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetTxStatisticRsp);

    MtAndesInitCmdMsg(msg, attr);
	rTxStatCmd.u4Field = cpu2le32(u4Field);
    rTxStatCmd.ucWlanIdx = ucWcid;
	MtAndesAppendCmdMsg(msg, (char *)&rTxStatCmd, sizeof(rTxStatCmd));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	if (ret)
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}
#endif /* RACTRL_FW_OFFLOAD_SUPPORT */

#ifdef PRETBTT_INT_EVENT_SUPPORT
/*****************************************
    ExT_CID = 0x33
*****************************************/
INT32 MtCmdTrgrPretbttIntEventSet(RTMP_ADAPTER *pAd,
                CMD_TRGR_PRETBTT_INT_EVENT_T trgr_pretbtt_int_event)
{
    struct cmd_msg *msg;
    INT32 ret = 0, size = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    size = sizeof(CMD_TRGR_PRETBTT_INT_EVENT_T);

    msg = MtAndesAllocCmdMsg(pAd, size);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        MTWF_LOG(DBG_CAT_HIF, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:Error Allocate Fail\n", __FUNCTION__));
        goto error;
    }
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TRGR_PRETBTT_INT_EVENT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MTWF_LOG(DBG_CAT_HIF, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:bcn_update.ucEnable = %d\n",
                    __FUNCTION__, trgr_pretbtt_int_event.ucEnable));
#ifdef RT_BIG_ENDIAN	
	trgr_pretbtt_int_event.u2BcnPeriod = cpu2le16(trgr_pretbtt_int_event.u2BcnPeriod);
#endif
	
    MtAndesAppendCmdMsg(msg, (char *)&trgr_pretbtt_int_event, size);

    ret = pAd->chipOps.MtCmdTx(pAd,msg);

error:
    MTWF_LOG(DBG_CAT_HIF, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

VOID MtSetTriggerPretbttIntEvent(RTMP_ADAPTER *pAd, INT apidx,
        UCHAR HWBssidIdx, BOOLEAN Enable, UINT16 BeaconPeriod)
{
    CMD_TRGR_PRETBTT_INT_EVENT_T trgr_pretbtt_int_event;
    os_zero_mem(&trgr_pretbtt_int_event, sizeof(CMD_TRGR_PRETBTT_INT_EVENT_T));

    trgr_pretbtt_int_event.ucHwBssidIdx = HWBssidIdx;
    if (HWBssidIdx > 0) //HWBssid > 0 case, no extendable bssid.
    {
        trgr_pretbtt_int_event.ucExtBssidIdx = 0;
    }
    else
    {
        trgr_pretbtt_int_event.ucExtBssidIdx = (UINT8)apidx;
    }
    trgr_pretbtt_int_event.ucEnable = Enable;
    trgr_pretbtt_int_event.u2BcnPeriod = BeaconPeriod;

    MTWF_LOG(DBG_CAT_HIF, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s:trgr_pretbtt_int_event.ucHwBssidIdx = %d\n",
                __FUNCTION__, trgr_pretbtt_int_event.ucHwBssidIdx));
    MTWF_LOG(DBG_CAT_HIF, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s:trgr_pretbtt_int_event.ucExtBssidIdx = %d\n",
                __FUNCTION__, trgr_pretbtt_int_event.ucExtBssidIdx));
    MTWF_LOG(DBG_CAT_HIF, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s:trgr_pretbtt_int_event.u2BcnPeriod = %d\n",
                __FUNCTION__, trgr_pretbtt_int_event.u2BcnPeriod));
    MTWF_LOG(DBG_CAT_HIF, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s:trgr_pretbtt_int_event.ucEnable = %d\n",
                __FUNCTION__, trgr_pretbtt_int_event.ucEnable));
    MtCmdTrgrPretbttIntEventSet(pAd, trgr_pretbtt_int_event);
}
#endif /*PRETBTT_INT_EVENT_SUPPORT*/


/*****************************************
    ExT_CID = 0x48
*****************************************/
INT32 MtCmdMuarConfigSet(RTMP_ADAPTER *pAd, UCHAR *pdata)
{
    struct cmd_msg *msg;
    INT32 ret=0,size=0;
    EXT_CMD_MUAR_T *pconfig_muar = (EXT_CMD_MUAR_T *)pdata;
    struct _CMD_ATTRIBUTE attr = {0};

    size = sizeof(EXT_CMD_MUAR_T);
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:ucMuarModeSel = %d, ucForceClear = %d, "
                "ucEntryCnt = %d, ucAccessMode = %d\n",
                __FUNCTION__, pconfig_muar->ucMuarModeSel,
                pconfig_muar->ucForceClear, pconfig_muar->ucEntryCnt,
                pconfig_muar->ucAccessMode));

    size = size + (pconfig_muar->ucEntryCnt * sizeof(EXT_CMD_MUAR_MULTI_ENTRY_T));

    msg = MtAndesAllocCmdMsg(pAd, size);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CONFIG_MUAR);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)pconfig_muar, size);

    ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}


#ifdef BCN_OFFLOAD_SUPPORT
/*****************************************
    ExT_CID = 0x49
*****************************************/
INT32 MtCmdBcnOffloadSet(RTMP_ADAPTER *pAd, CMD_BCN_OFFLOAD_T bcn_offload)
{
    struct cmd_msg *msg;
    INT32 ret = 0,size = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    size = sizeof(CMD_BCN_OFFLOAD_T);

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s(): Enable = %d, OwnMacIdx = %d, WlanIdx = %d, Band = %d, Len = %d\n",
                __FUNCTION__, bcn_offload.ucEnable, bcn_offload.ucOwnMacIdx, 
                bcn_offload.ucWlanIdx, bcn_offload.ucBandIdx, bcn_offload.u2PktLength));

    msg = MtAndesAllocCmdMsg(pAd, size);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BCN_OFFLOAD);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
#ifdef RT_BIG_ENDIAN
	bcn_offload.u2PktLength = cpu2le16(bcn_offload.u2PktLength);
	bcn_offload.u2TimIePos = cpu2le16(bcn_offload.u2TimIePos);
	bcn_offload.u2CsaIePos = cpu2le16(bcn_offload.u2CsaIePos);
//	hex_dump("bcn_offload",(char *)&bcn_offload,size);
#endif
    MtAndesAppendCmdMsg(msg, (char *)&bcn_offload, size);

    ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}
#endif /*BCN_OFFLOAD_SUPPORT*/

#ifdef VOW_SUPPORT
/********************************/
/* EXT_EVENT_ID_DRR_CTRL = 0x36 */
/********************************/
static VOID MtCmdSetVoWDRRCtrlRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
    struct _EXT_CMD_VOW_DRR_CTRL_T *EventExtCmdResult = (struct _EXT_CMD_VOW_DRR_CTRL_T *)Data;
#if (NEW_MCU_INIT_CMD_API)
    NdisCopyMemory(msg->attr.rsp.wb_buf_in_calbk, Data, sizeof(struct _EXT_CMD_VOW_DRR_CTRL_T));
#else
    NdisCopyMemory(msg->rsp_payload, Data, sizeof(struct _EXT_CMD_VOW_DRR_CTRL_T));
#endif /* NEW_MCU_INIT_CMD_API */

#if (NEW_MCU_INIT_CMD_API)
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->attr.ext_type));
#else
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->ext_cmd_type));
#endif /* NEW_MCU_INIT_CMD_API */

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: ucCtrlStatus = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->ucCtrlStatus));
}

/*************************************/
/* EXT_EVENT_ID_BSSGROUP_CTRL = 0x37 */
/*************************************/
static VOID MtCmdSetVoWGroupCtrlRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
    struct _EXT_CMD_BSS_CTRL_T *EventExtCmdResult = (struct _EXT_CMD_BSS_CTRL_T *)Data;
#if (NEW_MCU_INIT_CMD_API)
    NdisCopyMemory(msg->attr.rsp.wb_buf_in_calbk, Data, sizeof(struct _EXT_CMD_BSS_CTRL_T));
#else
    NdisCopyMemory(msg->rsp_payload, Data, sizeof(struct _EXT_CMD_BSS_CTRL_T));
#endif /* NEW_MCU_INIT_CMD_API */

#if (NEW_MCU_INIT_CMD_API)
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->attr.ext_type));
#else
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->ext_cmd_type));
#endif /* NEW_MCU_INIT_CMD_API */

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: ucCtrlStatus = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->ucCtrlStatus));

}
/****************************************/
/* EXT_EVENT_ID_VOW_FEATURE_CTRL = 0x38 */
/***************************************/
static VOID MtCmdSetVoWFeatureCtrlRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
    //RTMP_ADAPTER *pad = (RTMP_ADAPTER *)(msg->priv);
    struct _EXT_CMD_VOW_FEATURE_CTRL_T *EventExtCmdResult = (struct _EXT_CMD_VOW_FEATURE_CTRL_T *)Data;
#if (NEW_MCU_INIT_CMD_API)
    NdisCopyMemory(msg->attr.rsp.wb_buf_in_calbk, Data, sizeof(struct _EXT_CMD_VOW_FEATURE_CTRL_T));
#else
    NdisCopyMemory(msg->rsp_payload, Data, sizeof(struct _EXT_CMD_VOW_FEATURE_CTRL_T));
#endif /* NEW_MCU_INIT_CMD_API */

#if (NEW_MCU_INIT_CMD_API)
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: ExtCmd = 0x%x\n",
                                    __FUNCTION__, msg->attr.ext_type));
#else
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: ExtCmd = 0x%x\n",
                                    __FUNCTION__, msg->ext_cmd_type));
#endif /* NEW_MCU_INIT_CMD_API */


    //if (EventExtCmdResult->ucCtrlStatus == FALSE)
    if(1)
    {
#if (NEW_MCU_INIT_CMD_API)
        if (IS_CMD_ATTR_SET_QUERY_FLAG_SET(msg->attr))
#else
        if (msg->set_query == CMD_QUERY)
#endif /* NEW_MCU_INIT_CMD_API */
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: Get fail!\n", __FUNCTION__));
        else
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: Set fail!\n", __FUNCTION__));

        if(EventExtCmdResult->u2IfApplyBss_0_to_16_CtrlFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2Bss_0_to_16_CtrlValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2Bss_0_to_16_CtrlValue));
        }

        if(EventExtCmdResult->u2IfApplyRefillPerildFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2RefillPerildValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2RefillPerildValue));
        }

        if(EventExtCmdResult->u2IfApplyDbdc1SearchRuleFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2Dbdc1SearchRuleValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2Dbdc1SearchRuleValue));
        }

        if(EventExtCmdResult->u2IfApplyDbdc0SearchRuleFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2Dbdc0SearchRuleValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2Dbdc0SearchRuleValue));
        }

        if(EventExtCmdResult->u2IfApplyEnTxopNoChangeBssFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2EnTxopNoChangeBssValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2EnTxopNoChangeBssValue));
        }

        if(EventExtCmdResult->u2IfApplyAirTimeFairnessFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2AirTimeFairnessValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2AirTimeFairnessValue));
        }

        if(EventExtCmdResult->u2IfApplyEnbwrefillFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2EnbwrefillValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2EnbwrefillValue));
        }

        if(EventExtCmdResult->u2IfApplyEnbwCtrlFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2EnbwCtrlValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2EnbwCtrlValue));
        }

        if(EventExtCmdResult->u2IfApplyBssCheckTimeToken_0_to_16_CtrlFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2BssCheckTimeToken_0_to_16_CtrlValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2BssCheckTimeToken_0_to_16_CtrlValue));
        }

        if(EventExtCmdResult->u2IfApplyBssCheckLengthToken_0_to_16_CtrlFlag)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u2BssCheckLengthToken_0_to_16_CtrlValue = 0x%0x\n",
                    __FUNCTION__, EventExtCmdResult->u2BssCheckLengthToken_0_to_16_CtrlValue));
        }
    }


    /* need to ask FW to add ExtendCID and CtrlStatus */
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: i am here~\n", __FUNCTION__));

}

/***************************************/
/* EXT_EVENT_ID_RX_AIRTIME_CTRL = 0x4a */
/***************************************/
static VOID MtCmdSetVoWRxAirtimeCtrlRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
    struct _EXT_CMD_RX_AT_CTRL_T *EventExtCmdResult = (struct _EXT_CMD_RX_AT_CTRL_T *)Data;
#ifdef RT_BIG_ENDIAN
	EventExtCmdResult->u4CtrlFieldID = le2cpu16(EventExtCmdResult->u4CtrlFieldID);
	EventExtCmdResult->u4CtrlSubFieldID = le2cpu16(EventExtCmdResult->u4CtrlSubFieldID);
	EventExtCmdResult->u4CtrlSetStatus = le2cpu32(EventExtCmdResult->u4CtrlSetStatus);
	EventExtCmdResult->u4CtrlGetStatus = le2cpu32(EventExtCmdResult->u4CtrlGetStatus);
	EventExtCmdResult->u4ReserveDW[0] = le2cpu32(EventExtCmdResult->u4ReserveDW[0]);
	EventExtCmdResult->u4ReserveDW[1] = le2cpu32(EventExtCmdResult->u4ReserveDW[1]);
#endif

	NdisMoveMemory(msg->attr.rsp.wb_buf_in_calbk, Data, Len);

#if (NEW_MCU_INIT_CMD_API)
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->attr.ext_type));
#else
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->ext_cmd_type));
#endif /* NEW_MCU_INIT_CMD_API */

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlGetStatus = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlGetStatus));

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlSetStatus = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlSetStatus));
    // show get RX Non Wi-Fi and OBSS counter
    if (EventExtCmdResult->u4CtrlFieldID == EMUM_RX_AT_REPORT_CTRL)
    {
        if (EventExtCmdResult->u4CtrlSubFieldID == ENUM_RX_AT_REPORT_SUB_TYPE_RX_NONWIFI_TIME)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: Non Wi-Fi for band%d = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->rRxAtGeneralCtrl.rRxAtReportSubCtrl.ucRxNonWiFiBandIdx,
                                    EventExtCmdResult->rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxNonWiFiBandTimer));
        }
        else if (EventExtCmdResult->u4CtrlSubFieldID == ENUM_RX_AT_REPORT_SUB_TYPE_RX_OBSS_TIME)
        {
            MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: OBSS for band%d = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->rRxAtGeneralCtrl.rRxAtReportSubCtrl.ucRxObssBandIdx,
                                    EventExtCmdResult->rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxObssBandTimer));
        }

    }

}

/**************************************/
/* EXT_EVENT_ID_AT_PROC_MODULE = 0x4b */
/**************************************/
static VOID MtCmdSetVoWModuleCtrlRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
    struct _EXT_CMD_RX_AT_CTRL_T *EventExtCmdResult = (struct _EXT_CMD_RX_AT_CTRL_T *)Data;
#ifdef RT_BIG_ENDIAN
	EventExtCmdResult->u4CtrlFieldID = le2cpu16(EventExtCmdResult->u4CtrlFieldID);
	EventExtCmdResult->u4CtrlSubFieldID = le2cpu16(EventExtCmdResult->u4CtrlSubFieldID);
	EventExtCmdResult->u4CtrlSetStatus = le2cpu32(EventExtCmdResult->u4CtrlSetStatus);
	EventExtCmdResult->u4CtrlGetStatus = le2cpu32(EventExtCmdResult->u4CtrlGetStatus);
	EventExtCmdResult->u4ReserveDW[0] = le2cpu32(EventExtCmdResult->u4ReserveDW[0]);
	EventExtCmdResult->u4ReserveDW[1] = le2cpu32(EventExtCmdResult->u4ReserveDW[1]);
#endif

#if (NEW_MCU_INIT_CMD_API)
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->attr.ext_type));
#else
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlFieldID = 0x%x, ExtCmd (0x%02x)\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlFieldID, msg->ext_cmd_type));
#endif /* NEW_MCU_INIT_CMD_API */

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlGetStatus = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlGetStatus));

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s: u4CtrlSetStatus = 0x%x\n",
                                    __FUNCTION__, EventExtCmdResult->u4CtrlSetStatus));
}

/******************************/
/* EXT_CMD_ID_DRR_CTRL = 0x36 */
/******************************/
/* For station DWRR configuration */
INT32 MtCmdSetVoWDRRCtrl(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_VOW_DRR_CTRL_T *param)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    EXT_CMD_VOW_DRR_CTRL_T result;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:struct size %u\n", __FUNCTION__, sizeof(*param)));

    NdisZeroMemory(&result, sizeof(result));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }
#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_DRR_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_CMD_VOW_DRR_CTRL_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, &result);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdSetVoWDRRCtrlRsp);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_DRR_CTRL, TRUE, //need wait is FALSE
        0, TRUE, TRUE, sizeof(result), (char *)&result, MtCmdSetVoWDRRCtrlRsp);
#endif /* NEW_MCU_INIT_CMD_API */
#ifdef RT_BIG_ENDIAN
	param->u4CtrlFieldID = cpu2le32(param->u4CtrlFieldID);
	param->u4ReserveDW = cpu2le32(param->u4ReserveDW);
	param->rAirTimeCtrlValue.u4ComValue = cpu2le32(param->rAirTimeCtrlValue.u4ComValue);
#endif

    MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

    ret = pAd->chipOps.MtCmdTx(pAd,msg);

    /* command TX result */
    if (ret != NDIS_STATUS_SUCCESS)
        goto error;

    /* FW response event result */
    if (result.ucCtrlStatus == TRUE)
        ret = NDIS_STATUS_SUCCESS;
    else
        ret = NDIS_STATUS_FAILURE;

error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

/***********************************/
/* EXT_CMD_ID_BSSGROUP_CTRL = 0x37 */
/***********************************/
/* for BSS configuration */
INT32 MtCmdSetVoWGroupCtrl(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_BSS_CTRL_T *param)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    EXT_CMD_BSS_CTRL_T result;
#ifdef RT_BIG_ENDIAN
	P_BW_BSS_TOKEN_SETTING_T pSetting= NULL;
	INT32 i = 0;
#endif	
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:struct size %u\n", __FUNCTION__, sizeof(*param)));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BSSGROUP_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_CMD_BSS_CTRL_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, &result);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdSetVoWGroupCtrlRsp);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_BSSGROUP_CTRL, TRUE,
        0, TRUE, TRUE, sizeof(result), (char *)&result, MtCmdSetVoWGroupCtrlRsp);
#endif /* NEW_MCU_INIT_CMD_API */
#ifdef RT_BIG_ENDIAN
	param->u4CtrlFieldID = cpu2le32(param->u4CtrlFieldID);
	param->u4ReserveDW = cpu2le32(param->u4ReserveDW);
	param->u4SingleFieldIDValue = cpu2le32(param->u4SingleFieldIDValue);	
	for(i = 0; i < (sizeof(param->arAllBssGroupMultiField) / sizeof(param->arAllBssGroupMultiField[0])); i++)
	{
		UINT32* u32p = NULL;
		pSetting = &param->arAllBssGroupMultiField[i];
		pSetting->u2MinRateToken = cpu2le16(pSetting->u2MinRateToken);		
		pSetting->u2MaxRateToken = cpu2le16(pSetting->u2MaxRateToken);	
		u32p = (UINT32 *)(&(pSetting->u2MinRateToken) + 1);
		*u32p = cpu2le32(*u32p);
		u32p++;
		*u32p = cpu2le32(*u32p);
		u32p++;
		*u32p = cpu2le32(*u32p);
	}
#endif

    MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    //printk("\x1b[31m%s: ret %d\x1b[m\n", __FUNCTION__, ret);
    /* command TX result */
    if (ret != NDIS_STATUS_SUCCESS)
        goto error;

    //printk("\x1b[31m%s: ucCtrlStatus %d\x1b[m\n", __FUNCTION__, result.ucCtrlStatus);
    /* FW response event result */
    if (result.ucCtrlStatus == TRUE)
        ret = NDIS_STATUS_SUCCESS;
    else
        ret = NDIS_STATUS_FAILURE;
error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

/**************************************/
/* EXT_CMD_ID_VOW_FEATURE_CTRL = 0x38 */
/**************************************/
/* for VOW feature control configuration */
INT32 MtCmdSetVoWFeatureCtrl(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_VOW_FEATURE_CTRL_T *param)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    EXT_CMD_VOW_FEATURE_CTRL_T result;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */
#ifdef RT_BIG_ENDIAN
	UINT16* u16p = NULL;
#endif
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:struct size %u\n", __FUNCTION__, sizeof(*param)));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_VOW_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_CMD_VOW_FEATURE_CTRL_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, &result);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdSetVoWFeatureCtrlRsp);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_VOW_FEATURE_CTRL, TRUE,
        0, TRUE, TRUE, sizeof(result), (char *)&result, MtCmdSetVoWFeatureCtrlRsp);
#endif /* NEW_MCU_INIT_CMD_API */
#ifdef RT_BIG_ENDIAN
	param->u2IfApplyBss_0_to_16_CtrlFlag = cpu2le16(param->u2IfApplyBss_0_to_16_CtrlFlag);
	u16p = &param->u2IfApplyBss_0_to_16_CtrlFlag + 1;
	*u16p = cpu2le16(*u16p);
	param->u2IfApplyBssCheckTimeToken_0_to_16_CtrlFlag = cpu2le16(param->u2IfApplyBssCheckTimeToken_0_to_16_CtrlFlag);
	param->u2Resreve1Flag = cpu2le16(param->u2Resreve1Flag);
	param->u2IfApplyBssCheckLengthToken_0_to_16_CtrlFlag = cpu2le16(param->u2IfApplyBssCheckLengthToken_0_to_16_CtrlFlag);
	param->u2Resreve2Flag = cpu2le16(param->u2Resreve2Flag);
	param->u2ResreveBackupFlag[0] = cpu2le32(param->u2ResreveBackupFlag[0]);
	param->u2ResreveBackupFlag[1] = cpu2le32(param->u2ResreveBackupFlag[1]);
	param->u2Bss_0_to_16_CtrlValue = cpu2le16(param->u2Bss_0_to_16_CtrlValue);
	u16p = &param->u2Bss_0_to_16_CtrlValue + 1; 
	*u16p = cpu2le16(*u16p);
	param->u2BssCheckTimeToken_0_to_16_CtrlValue = cpu2le16(param->u2BssCheckTimeToken_0_to_16_CtrlValue);
	param->u2Resreve1Value = cpu2le16(param->u2Resreve1Value);
	param->u2BssCheckLengthToken_0_to_16_CtrlValue = cpu2le16(param->u2BssCheckLengthToken_0_to_16_CtrlValue);
	param->u2Resreve2Value = cpu2le16(param->u2Resreve2Value);			
	param->u2ResreveBackupValue[0] = cpu2le32(param->u2ResreveBackupValue[0]);
	param->u2ResreveBackupValue[1] = cpu2le32(param->u2ResreveBackupValue[1]);
#endif

    MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

/*************************************/
/* EXT_CMD_ID_RX_AIRTIME_CTRL = 0x4a */
/*************************************/
/* RX airtime */
INT32 MtCmdSetVoWRxAirtimeCtrl(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_RX_AT_CTRL_T *param)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    EXT_CMD_RX_AT_CTRL_T result;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */
	EXT_CMD_RX_AT_CTRL_T tparam;

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:struct size %u\n", __FUNCTION__, sizeof(*param)));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RX_AIRTIME_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_CMD_RX_AT_CTRL_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, &result);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdSetVoWRxAirtimeCtrlRsp);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_RX_AIRTIME_CTRL, TRUE,
        0, TRUE, TRUE, sizeof(result), (char *)&result, MtCmdSetVoWRxAirtimeCtrlRsp);
#endif /* NEW_MCU_INIT_CMD_API */
	NdisCopyMemory(&tparam, param,	sizeof(*param));
#ifdef RT_BIG_ENDIAN
	tparam.u4CtrlFieldID = cpu2le16(tparam.u4CtrlFieldID);
	tparam.u4CtrlSubFieldID = cpu2le16(tparam.u4CtrlSubFieldID);
	tparam.u4CtrlSetStatus = cpu2le32(tparam.u4CtrlSetStatus);
	tparam.u4CtrlGetStatus = cpu2le32(tparam.u4CtrlGetStatus);
	tparam.u4ReserveDW[0] = cpu2le32(tparam.u4ReserveDW[0]);
	tparam.u4ReserveDW[1] = cpu2le32(tparam.u4ReserveDW[1]);
	
	tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[0]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[0]);
	tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[1]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[1]);
	
	tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[0] 
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[0]);
	tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[1] 
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[1]);

	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[0]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[0]);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[1]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[1]);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC0Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC0Backoff);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC1Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC1Backoff);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC2Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC2Backoff);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC3Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC3Backoff);


	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxNonWiFiBandTimer
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxNonWiFiBandTimer);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxObssBandTimer
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxObssBandTimer);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxMibObssBandTimer
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxMibObssBandTimer);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[0]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[0]);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[1]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[1]);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[2]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[2]);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[3]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[3]);
#endif
    MtAndesAppendCmdMsg(msg, (char *)(&tparam), sizeof(tparam));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

INT32 MtCmdGetVoWRxAirtimeCtrl(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_RX_AT_CTRL_T *param)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    P_EXT_CMD_RX_AT_CTRL_T p_result=param;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */
	EXT_CMD_RX_AT_CTRL_T tparam;

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:struct size %u\n", __FUNCTION__, sizeof(*param)));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RX_AIRTIME_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_CMD_RX_AT_CTRL_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, p_result);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdSetVoWRxAirtimeCtrlRsp);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_QUERY, EXT_CMD_ID_RX_AIRTIME_CTRL, TRUE,
        0, TRUE, TRUE, sizeof(EXT_CMD_RX_AT_CTRL_T), (char *)p_result, MtCmdSetVoWRxAirtimeCtrlRsp);
#endif /* NEW_MCU_INIT_CMD_API */
	NdisCopyMemory(&tparam, param,  sizeof(*param));
#ifdef RT_BIG_ENDIAN
	tparam.u4CtrlFieldID = cpu2le16(tparam.u4CtrlFieldID);
	tparam.u4CtrlSubFieldID = cpu2le16(tparam.u4CtrlSubFieldID);
	tparam.u4CtrlSetStatus = cpu2le32(tparam.u4CtrlSetStatus);
	tparam.u4CtrlGetStatus = cpu2le32(tparam.u4CtrlGetStatus);
	tparam.u4ReserveDW[0] = cpu2le32(tparam.u4ReserveDW[0]);
	tparam.u4ReserveDW[1] = cpu2le32(tparam.u4ReserveDW[1]);
	
	tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[0]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[0]);
	tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[1]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtFeatureSubCtrl.u4ReserveDW[1]);
	
	tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[0] 
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[0]);
	tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[1] 
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtBitWiseSubCtrl.u4ReserveDW[1]);

	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[0]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[0]);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[1]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.u4ReserveDW[1]);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC0Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC0Backoff);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC1Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC1Backoff);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC2Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC2Backoff);
	tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC3Backoff
		= cpu2le16(tparam.rRxAtGeneralCtrl.rRxAtTimeValueSubCtrl.rRxATBackOffCfg.u2AC3Backoff);


	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxNonWiFiBandTimer
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxNonWiFiBandTimer);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxObssBandTimer
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxObssBandTimer);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxMibObssBandTimer
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4RxMibObssBandTimer);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[0]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[0]);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[1]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[1]);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[2]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[2]);
	tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[3]
		= cpu2le32(tparam.rRxAtGeneralCtrl.rRxAtReportSubCtrl.u4StaAcRxTimer[3]);
#endif
    MtAndesAppendCmdMsg(msg, (char *)(&tparam), sizeof(tparam));


    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

/************************************/
/* EXT_CMD_ID_AT_PROC_MODULE = 0x4b */
/************************************/
/* N9 VOW module */
INT32 MtCmdSetVoWModuleCtrl(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_AT_PROC_MODULE_CTRL_T *param)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    EXT_CMD_AT_PROC_MODULE_CTRL_T result;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:struct size %u\n", __FUNCTION__, sizeof(*param)));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_AT_PROC_MODULE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_CMD_AT_PROC_MODULE_CTRL_T));
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, &result);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdSetVoWModuleCtrlRsp);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_AT_PROC_MODULE, TRUE,
        0, TRUE, TRUE, sizeof(result), (char *)&result, MtCmdSetVoWModuleCtrlRsp);
#endif /* NEW_MCU_INIT_CMD_API */

    MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

/************************************/
/* EXT_CMD_ID_AT_COUNTER_TEST = 0x?? */
/************************************/
INT32 MtCmdSetVoWCounterCtrl(struct _RTMP_ADAPTER *pAd, UCHAR cmd, UCHAR val)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    UCHAR param[2];
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(param));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    param[0] = cmd;
    param[1] = val;
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: cmd = %d, val = %d)\n",
                __FUNCTION__, cmd, val));

    //no wait and no reponse
#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, 0x4c);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RETRY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, 0x4c, FALSE,
        0, TRUE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    MtAndesAppendCmdMsg(msg, (char *)param, sizeof(param));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}

#ifdef MT7615_FPGA
#define EXT_CMD_ID_STA_QUEUE_LEN    0x14
#define EXT_CMD_ID_STA2_QUEUE_LEN   0x15
#define EXT_CMD_ID_QEMPTY_THRESHOLD 0x16
#define EXT_CMD_ID_STA_CNT          0x17
/* for CR4 commands */
/*****************************************
    ExT_CID = 0x14
*****************************************/
INT32 MtCmdSetStaQLen(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 qLen)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}


#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_STA_QUEUE_LEN);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_STA_QUEUE_LEN,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = qLen;
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;
error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;

}

/*****************************************
    ExT_CID = 0x15
*****************************************/
INT32 MtCmdSetSta2QLen(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 qLen)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}


#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_STA2_QUEUE_LEN);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_STA2_QUEUE_LEN,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = qLen;
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;
error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;

}

/*****************************************
    ExT_CID = 0x16
*****************************************/
INT32 MtCmdSetEmptyThreshold(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 threshold)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_QEMPTY_THRESHOLD);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_QEMPTY_THRESHOLD,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = threshold;
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;

error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}

/*****************************************
    ExT_CID = 0x17
*****************************************/
INT32 MtCmdSetStaCnt(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 cnt)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_STA_CNT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_STA_CNT,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = cnt;
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;
error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}

#endif /* MT7615_FPGA */
#endif /* VOW_SUPPORT */

#ifdef RED_SUPPORT
/*****************************************
    ExT_CID = 0x68
*****************************************/
INT32 MtCmdSetRedEnable(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 en)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RED_ENABLE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_RED_ENABLE,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = cpu2le32(en);
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;
error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}

/*****************************************
    ExT_CID = 0x69
*****************************************/
INT32 MtCmdSetRedShowSta(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 Num)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RED_SHOW_STA);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_RED_SHOW_STA,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = cpu2le32(Num);
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;
error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}


/*****************************************
    ExT_CID = 0x6A
*****************************************/
INT32 MtCmdSetRedTargetDelay(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 Num)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RED_TARGET_DELAY);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_RED_TARGET_DELAY,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = cpu2le32(Num);
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;
error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}
#endif/*RED_SUPPORT*/

/*****************************************
    ExT_CID = 0x75
*****************************************/
INT32 MtCmdSetCPSEnable(RTMP_ADAPTER *pAd, UINT8 McuDest, UINT32 Mode)
{

    struct cmd_msg *msg;
    INT32 Ret = 0;
    UINT32 Val;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif /* NEW_MCU_INIT_CMD_API */

    msg = MtAndesAllocCmdMsg(pAd, sizeof(UINT32));

	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, McuDest);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CP_SUPPORT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#else
    MtAndesInitCmdMsg(msg, McuDest, EXT_CID, CMD_SET, EXT_CMD_ID_CP_SUPPORT,
                        FALSE, 0, FALSE, FALSE, 0, NULL, NULL);
#endif /* NEW_MCU_INIT_CMD_API */

    Val = cpu2le32(Mode);
    MtAndesAppendCmdMsg(msg, (char *)&Val,
                                    sizeof(Val));

    Ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
    return Ret;
error:
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s:(ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}


#ifdef CFG_TDLS_SUPPORT
/*****************************************
	ExT_CID = 0x34
*****************************************/

static VOID cfg_tdls_send_CH_SW_SETUP_callback(struct cmd_msg *msg,
                                            INT8 *Data, UINT16 Len)
{
	INT32 chsw_fw_resp = 0;
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)msg->priv;

    P_EXT_EVENT_TDLS_SETUP_T prEventExtCmdResult =
                                        (P_EXT_EVENT_TDLS_SETUP_T)Data;

    chsw_fw_resp = prEventExtCmdResult->ucResultId;
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR
        ,("===> CHSW rsp(%d) u4StartTime(%d) u4EndTime(%d) "
            "u4TbttTime(%d) u4StayTime(%d) u4RestTime(%d) \n"
        , chsw_fw_resp,le2cpu32(prEventExtCmdResult->u4StartTime),
       le2cpu32(prEventExtCmdResult->u4EndTime),le2cpu32(prEventExtCmdResult->u4TbttTime)
        , le2cpu32(prEventExtCmdResult->u4StayTime), le2cpu32(prEventExtCmdResult->u4RestTime)));
	if(chsw_fw_resp == 0)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
            ("FW response!! %ld !!!\n", (jiffies * 1000) / OS_HZ));
		pAd->StaCfg[0].wpa_supplicant_info.CFG_Tdls_info.IamInOffChannel = TRUE;
	}
	else
	{
		BOOLEAN TimerCancelled;
		pAd->StaCfg[0].wpa_supplicant_info.CFG_Tdls_info.IamInOffChannel = FALSE;
		RTMPCancelTimer(&pAd->StaCfg[0].wpa_supplicant_info.CFG_Tdls_info.BaseChannelSwitchTimer, &TimerCancelled);
	}

}

INT cfg_tdls_send_CH_SW_SETUP(
	RTMP_ADAPTER *ad,
	UCHAR cmd,
	UCHAR offch_prim,
	UCHAR offch_center,
	UCHAR bw_off,
	UCHAR role,
	UINT16 stay_time,
	UINT32 start_time_tsf,
	UINT16 switch_time,
	UINT16 switch_timeout
)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_CFG_TDLS_CHSW_T CmdChanSwitch;
	INT32 ret = 0,i = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(ad, sizeof(EXT_CMD_CFG_TDLS_CHSW_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_TDLS_CHSW);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 24);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, cfg_tdls_send_CH_SW_SETUP_callback);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&CmdChanSwitch, sizeof(CmdChanSwitch));
	//CmdChanSwitch.cmd = cmd;
	CmdChanSwitch.ucOffBandwidth = bw_off;
	CmdChanSwitch.ucOffPrimaryChannel = offch_prim;
    CmdChanSwitch.ucOffCenterChannelSeg0= offch_center;
    CmdChanSwitch.ucOffCenterChannelSeg1 = 0;
	CmdChanSwitch.ucRole= role;
	CmdChanSwitch.u4StartTimeTsf = cpu2le32(start_time_tsf);
	CmdChanSwitch.u4SwitchTime = switch_time;
	CmdChanSwitch.u4SwitchTimeout = switch_timeout;
#ifdef RT_BIG_ENDIAN
	CmdChanSwitch.u4SwitchTime = cpu2le32(CmdChanSwitch.u4SwitchTime);
	CmdChanSwitch.u4SwitchTimeout = cpu2le32(CmdChanSwitch.u4SwitchTimeout);
#endif
	CmdChanSwitch.ucBssIndex = BSS0;
	MtAndesAppendCmdMsg(msg, (char *)&CmdChanSwitch, sizeof(CmdChanSwitch));
	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("==========================send cmd=============================\n");
	ret  = ad->chipOps.MtCmdTx(ad,msg);

	error:
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
		return ret;

}
#endif /* CFG_TDLS_SUPPORT */


#ifdef CONFIG_HW_HAL_OFFLOAD
/*****************************************
	ExT_CID = 0x3D
*****************************************/
VOID MtCmdATETestResp(struct cmd_msg *msg, char *data, UINT16 len)
{
	//RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)msg->priv;
	//ATE_CTRL *ate_ctrl = &pAd->ATECtrl;
}


INT32 MtCmdATETest(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_ATE_TEST_MODE_T *param)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
		("%s:struct size %lu\n", __FUNCTION__, (ULONG)sizeof(*param)));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_ATE_TEST_MODE);
    //SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET); // for iBF phase calibration
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdATETestResp);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}

INT32 MtCmdCfgOnOff(RTMP_ADAPTER *pAd, UINT8 Type, UINT8 Enable, UINT8 Band)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
	{
	    testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s, Type:%d Enable:%d Band:%d \n",
                __FUNCTION__, Type, Enable, Band));
	ATE_param.ucAteTestModeEn = testmode_en;

	/* =======================================================
	Type: 
	         0: TSSI
	         1: DPD  
	         2: Rate power offset  
	         3: Temperature compensation
             4: Thernal Sensor 
             5: Tx Power Ctrl
             6: Single SKU
             7: Power Percentage
    ======================================================== */
    
	switch (Type)
    {
    	case EXT_CFG_ONOFF_TSSI:
    		ATE_param.ucAteIdx = EXT_ATE_SET_TSSI;
    		break;
            
    	case EXT_CFG_ONOFF_DPD:
    		ATE_param.ucAteIdx = EXT_ATE_SET_DPD;
    		break;
            
    	case EXT_CFG_ONOFF_RATE_POWER_OFFSET:
    		ATE_param.ucAteIdx = EXT_ATE_SET_RATE_POWER_OFFSET;
    		break;
            
    	case EXT_CFG_ONOFF_TEMP_COMP:
    		ATE_param.ucAteIdx = EXT_ATE_SET_THERNAL_COMPENSATION;
    		break;
            
	    case EXT_CFG_ONOFF_THERMAL_SENSOR:
		    ATE_param.ucAteIdx = EXT_ATE_CFG_THERMAL_ONOFF;
		    break;
            
	    case EXT_CFG_ONOFF_TXPOWER_CTRL:
		    ATE_param.ucAteIdx = EXT_ATE_SET_TX_POWER_CONTROL_ALL_RF;
		    break;
            
        case EXT_CFG_ONOFF_SINGLE_SKU:
		    ATE_param.ucAteIdx = EXT_ATE_SET_SINGLE_SKU;
		    break;
            
        case EXT_CFG_ONOFF_POWER_PERCENTAGE:
		    ATE_param.ucAteIdx = EXT_ATE_SET_POWER_PERCENTAGE;
		    break;

        case EXT_CFG_ONOFF_BF_BACKOFF:
            ATE_param.ucAteIdx = EXT_ATE_SET_BF_BACKOFF;
            break;   
            
    	default:
    		break;
	}
	if (Type == 5){
		ATE_param.Data.u4Data = Enable;
#ifdef RT_BIG_ENDIAN
		ATE_param.Data.u4Data = cpu2le32(ATE_param.Data.u4Data);
#endif
	}
	else {
		ATE_param.Data.rCfgOnOff.ucEnable = Enable;
		ATE_param.Data.rCfgOnOff.ucBand = Band;
	}
	ret = MtCmdATETest(pAd, &ATE_param);

	return ret;
}

INT32 MtCmdSetAntennaPort(RTMP_ADAPTER *pAd, UINT8 RfModeMask,
                        UINT8 RfPortMask, UINT8 AntPortMask)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s, RfModeMask:%d RfPortMask:%d AntPortMask:%d\n",
        __FUNCTION__, RfModeMask, RfPortMask, AntPortMask));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_ANTENNA_PORT;
	ATE_param.Data.rCfgRfAntPortSetting.ucRfModeMask = RfModeMask;
	ATE_param.Data.rCfgRfAntPortSetting.ucRfPortMask = RfPortMask;
	ATE_param.Data.rCfgRfAntPortSetting.ucAntPortMask = AntPortMask;
	ret = MtCmdATETest(pAd, &ATE_param);
	return ret;
}


INT32 MtCmdATESetSlotTime(RTMP_ADAPTER *pAd, UINT8 SlotTime,
        UINT8 SifsTime,UINT8 RifsTime,UINT16 EifsTime, UCHAR BandIdx)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_SLOT_TIME;
	ATE_param.Data.rSlotTimeSet.u2Eifs = cpu2le16(EifsTime);
	ATE_param.Data.rSlotTimeSet.ucRifs = RifsTime;
	ATE_param.Data.rSlotTimeSet.ucSifs = SifsTime;
	ATE_param.Data.rSlotTimeSet.ucSlotTime = SlotTime;
	ATE_param.Data.rSlotTimeSet.ucBandNum = BandIdx;
	ret = MtCmdATETest(pAd, &ATE_param);
	return ret;
}

INT32 MtCmdATESetPowerDropLevel(RTMP_ADAPTER *pAd, UINT8 PowerDropLevel, UCHAR BandIdx)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_POWER_PERCENTAGE_LEVEL;
	ATE_param.Data.rPowerLevelSet.ucPowerDropLevel = PowerDropLevel;
	ATE_param.Data.rPowerLevelSet.ucBand = BandIdx;
	ret = MtCmdATETest(pAd, &ATE_param);
	return ret;
}

INT32 MtCmdRxFilterPktLen(RTMP_ADAPTER *pAd, UINT8 Enable,
                                UINT8 Band, UINT32 RxPktLen)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s, Enable:%d Band:%d RxPktLen:%d\n",
                __FUNCTION__, Enable, Band, RxPktLen));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_RX_FILTER_PKT_LEN;
	ATE_param.Data.rRxFilterPktLen.ucEnable = Enable;
	ATE_param.Data.rRxFilterPktLen.ucBand = Band;
	ATE_param.Data.rRxFilterPktLen.u4RxPktLen = cpu2le32(RxPktLen);
	ret = MtCmdATETest(pAd, &ATE_param);

	return ret;
}

INT32 MtCmdSetFreqOffset(RTMP_ADAPTER *pAd, UINT32 FreqOffset)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s, FreqOffset:%d\n", __FUNCTION__, FreqOffset));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_FREQ_OFFSET;
	ATE_param.Data.u4Data = cpu2le32(FreqOffset);
	ret = MtCmdATETest(pAd, &ATE_param);

	return ret;
}

static VOID MtCmdGetFreqOffsetRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	EVENT_EXT_GET_FREQOFFSET_T *Result = (EVENT_EXT_GET_FREQOFFSET_T *)Data;
	Result->u4FreqOffset = le2cpu32(Result->u4FreqOffset);
    os_move_mem(msg->attr.rsp.wb_buf_in_calbk,
                    &Result->u4FreqOffset, sizeof(Result->u4FreqOffset));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s, FreqOffset:%d\n", __FUNCTION__, Result->u4FreqOffset));
}


INT32 MtCmdGetFreqOffset(RTMP_ADAPTER *pAd, UINT32 *FreqOffsetResult)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
   	INT32 ret = 0;
	UINT8 testmode_en = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(ATE_param));
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_ATE_TEST_MODE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, FreqOffsetResult);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetFreqOffsetRsp);

    MtAndesInitCmdMsg(msg, attr);
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	NdisZeroMemory(&ATE_param, sizeof(ATE_param));

	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_GET_FREQ_OFFSET;
#ifdef RT_BIG_ENDIAN
	ATE_param.Data.u4Data = cpu2le32(ATE_param.Data.u4Data);
#endif
	MtAndesAppendCmdMsg(msg, (char *)&ATE_param, sizeof(ATE_param));
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;

}

static VOID MtCmdGetCfgStatRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	EXT_EVENT_ATE_TEST_MODE_T *Result = (EXT_EVENT_ATE_TEST_MODE_T *)Data;
	GET_TSSI_STATUS_T *TSSI_Status = (GET_TSSI_STATUS_T *)&Result->aucAteResult[0];
	GET_DPD_STATUS_T *DPD_Status = (GET_DPD_STATUS_T *)&Result->aucAteResult[0];
	GET_THERMO_COMP_STATUS_T *THER_Status = (GET_THERMO_COMP_STATUS_T *)&Result->aucAteResult[0];
	switch (Result->ucAteIdx) {
	case EXT_ATE_GET_TSSI:
		TSSI_Status->ucEnable = le2cpu32(TSSI_Status->ucEnable);
		TSSI_Status->ucBand = le2cpu32(TSSI_Status->ucBand);
		os_move_mem(msg->attr.rsp.wb_buf_in_calbk, &TSSI_Status->ucEnable, sizeof(TSSI_Status->ucEnable));
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s, TSSI Enable:%d Band:%d\n",__FUNCTION__, TSSI_Status->ucEnable, TSSI_Status->ucBand));
		break;
	case EXT_ATE_GET_DPD:
		DPD_Status->ucEnable = le2cpu32(DPD_Status->ucEnable);
		DPD_Status->ucBand = le2cpu32(DPD_Status->ucBand);
		os_move_mem(msg->attr.rsp.wb_buf_in_calbk, &DPD_Status->ucEnable, sizeof(DPD_Status->ucEnable));
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s, DPD Enable:%d Band:%d\n",__FUNCTION__, DPD_Status->ucEnable, DPD_Status->ucBand));
		break;
	case EXT_ATE_GET_THERNAL_COMPENSATION:
		THER_Status->ucEnable = le2cpu32(THER_Status->ucEnable);
		os_move_mem(msg->attr.rsp.wb_buf_in_calbk, &THER_Status->ucEnable, sizeof(THER_Status->ucEnable));
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s, THER Enable:%d \n",__FUNCTION__, THER_Status->ucEnable));
		break;
	default:
		break;
	}
}

INT32 MtCmdGetCfgOnOff(RTMP_ADAPTER *pAd, UINT32 Type, UINT8 Band, UINT32 *Status)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
   	INT32 ret = 0;
	UINT8 testmode_en = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(ATE_param));
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s\n",__FUNCTION__));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_ATE_TEST_MODE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, Status);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetCfgStatRsp);

    MtAndesInitCmdMsg(msg, attr);
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	NdisZeroMemory(&ATE_param, sizeof(ATE_param));

	ATE_param.ucAteTestModeEn = testmode_en;
	/* Type: 0: TSSI  1: DPD  2: Rate power offset  3: Temperature compensation */
	switch (Type) {
    	case 0:
    		ATE_param.ucAteIdx = EXT_ATE_GET_TSSI;
    		break;
    	case 1:
    		ATE_param.ucAteIdx = EXT_ATE_GET_DPD;
    		break;
    	case 2:
			ATE_param.ucAteIdx = EXT_ATE_GET_RATE_POWER_OFFSET;
    		break;
    	case 3:
    		ATE_param.ucAteIdx = EXT_ATE_GET_THERNAL_COMPENSATION;
    		break;
    	default:
    		break;
	}
	ATE_param.Data.u4Data = Band;
#ifdef RT_BIG_ENDIAN
	ATE_param.Data.u4Data = cpu2le32(ATE_param.Data.u4Data);
#endif
	MtAndesAppendCmdMsg(msg, (char *)&ATE_param, sizeof(ATE_param));
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;

}
INT32 MtCmdSetPhyCounter(RTMP_ADAPTER *pAd, UINT32 Control, UINT8 band_idx)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 1;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s, Control:%d\n", __FUNCTION__, Control));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_PHY_COUNT;
	ATE_param.Data.rPhyStatusCnt.ucEnable = Control;
	ATE_param.Data.rPhyStatusCnt.ucBand = band_idx;
	ret = MtCmdATETest(pAd, &ATE_param);
	return ret;
}

INT32 MtCmdSetRxvIndex(RTMP_ADAPTER *pAd, UINT8 Group_1,
                            UINT8 Group_2, UINT8 band_idx)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s, Group_1:%d Group_2:%d Band:%d\n",
        __FUNCTION__, Group_1, Group_2, band_idx));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_RXV_INDEX;
	ATE_param.Data.rSetRxvIdx.ucValue1 = Group_1;
	ATE_param.Data.rSetRxvIdx.ucValue2 = Group_2;
	ATE_param.Data.rSetRxvIdx.ucDbdcIdx = band_idx;
	ret = MtCmdATETest(pAd, &ATE_param);

	return ret;
}

INT32 MtCmdSetFAGCPath(RTMP_ADAPTER *pAd, UINT8 Path, UINT8 band_idx)
{
	INT32 ret = 0;
	struct _EXT_CMD_ATE_TEST_MODE_T ATE_param;
	UINT8 testmode_en = 0;
#ifdef CONFIG_ATE
	if(ATE_ON(pAd))
    {
        testmode_en = 1;
    }
#endif
	os_zero_mem(&ATE_param, sizeof(ATE_param));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s, Path:%d Band:%d\n", __FUNCTION__, Path, band_idx));
	ATE_param.ucAteTestModeEn = testmode_en;
	ATE_param.ucAteIdx = EXT_ATE_SET_FAGC_PATH;
	ATE_param.Data.rSetFagcRssiPath.ucValue = Path;
	ATE_param.Data.rSetFagcRssiPath.ucDbdcIdx = band_idx;
	ret = MtCmdATETest(pAd, &ATE_param);

	return ret;
}
#endif /* CONFIG_HW_HAL_OFFLOAD */

INT32 MtCmdClockSwitchDisable(RTMP_ADAPTER *pAd, UINT8 isDisable)
{
	INT32 ret = 0;
	struct cmd_msg *msg;
	struct _CMD_MCU_CLK_SWITCH_DISABLE_T clockDisable;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_MCU_CLK_SWITCH_DISABLE_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s, isDisable: %d \n", __FUNCTION__, isDisable));
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CLOCK_SWITCH_DISABLE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&clockDisable, sizeof(clockDisable));
	clockDisable.disable = isDisable;
	MtAndesAppendCmdMsg(msg, (char *)&clockDisable, sizeof(clockDisable));
	ret = pAd->chipOps.MtCmdTx(pAd,msg);

	error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}

/*****************************************
    ExT_CID = 0x3e
*****************************************/
INT32 MtCmdUpdateProtect(struct _RTMP_ADAPTER *pAd,
                                struct _EXT_CMD_UPDATE_PROTECT_T *param)
{
	struct cmd_msg *msg = NULL;
	INT32 ret = 0;
	struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PROTECT_CTRL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

	MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
			("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


/*****************************************
    ExT_CID = 0x3f
*****************************************/
INT32 MtCmdSetRdg(struct _RTMP_ADAPTER *pAd, struct _EXT_CMD_RDG_CTRL_T *param)
{
	struct cmd_msg *msg = NULL;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RDG_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
#ifdef RT_BIG_ENDIAN
	param->u4TxOP = cpu2le32(param->u4TxOP);
#endif
	MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
    if (ret) {
        MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
    }

    return ret;
}


/*****************************************
	ExT_CID = 0x42
*****************************************/
INT32 MtCmdSetSnifferMode(struct _RTMP_ADAPTER *pAd,
                            struct _EXT_CMD_SNIFFER_MODE_T *param)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(pAd, sizeof(*param));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_SNIFFER_MODE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)param, sizeof(*param));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:
	MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s:(ret = %d) sniffer_mode:%d\n",
                __FUNCTION__, ret,param->ucSnifferEn));
	return ret;
}

/*****************************************
	ExT_CID = 0x57
*****************************************/
static VOID CmdMemDumpRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	P_EXT_CMD_EVENT_DUMP_MEM_T PktBufEvt = (P_EXT_CMD_EVENT_DUMP_MEM_T)Data;
	MEM_DUMP_DATA_T *MemDumpData = NULL;
	UINT32 datasz = sizeof(PktBufEvt->ucData);

	MemDumpData = (MEM_DUMP_DATA_T *)msg->attr.rsp.wb_buf_in_calbk;
	os_move_mem(&MemDumpData->pValue[0], &PktBufEvt->ucData[0] , datasz);
}

VOID MtCmdMemDump(RTMP_ADAPTER *pAd, UINT32 Addr, PUINT8 pData)
{
	struct cmd_msg *msg = NULL;
	EXT_CMD_EVENT_DUMP_MEM_T *CmdMemDump = NULL;
	MEM_DUMP_DATA_T MemDumpData;
	int ret = 0;
	struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&MemDumpData, sizeof(MemDumpData));

	MemDumpData.pValue = pData;

	os_alloc_mem(pAd, (UCHAR **)&CmdMemDump, sizeof(EXT_CMD_EVENT_DUMP_MEM_T));
	if (!CmdMemDump)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}


	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_EVENT_DUMP_MEM_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    	SET_CMD_ATTR_TYPE(attr, EXT_CID);
    	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_DUMP_MEM);
    	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RSP);
    	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, sizeof(EXT_CMD_EVENT_DUMP_MEM_T));
   	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, &MemDumpData);
    	SET_CMD_ATTR_RSP_HANDLER(attr, CmdMemDumpRsp);

    	MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(CmdMemDump, sizeof(EXT_CMD_EVENT_DUMP_MEM_T));

	CmdMemDump->u4MemAddr = cpu2le32(Addr);

 	MtAndesAppendCmdMsg(msg, (char *)CmdMemDump, sizeof(EXT_CMD_EVENT_DUMP_MEM_T));
	ret = pAd->chipOps.MtCmdTx(pAd, msg);
	goto done;

error:
	if (msg)
    	{
		MtAndesFreeCmdMsg(msg);
    	}
done:
	if (CmdMemDump)
		os_free_mem(CmdMemDump);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
	        ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return;
}


/*****************************************
	CID
*****************************************/

/*****************************************
	CID = 0x01
*****************************************/

/*****************************************
	CID = 0x02
*****************************************/

/*****************************************
	CID = 0x3
*****************************************/

/*****************************************
	CID = 0x05
*****************************************/

/*****************************************
	CID = 0x07
*****************************************/

/*****************************************
	CID = 0x10
*****************************************/

/*****************************************
	CID = 0x20
*****************************************/
#if !defined(COMPOS_WIN)
INT32 MtCmdHIFLoopBackTest(
	IN  RTMP_ADAPTER *pAd, BOOLEAN IsEnable, UINT8 RxQ
	)
{
	struct cmd_msg *msg;
	INT32 ret = 0;
	CMD_HIF_LOOPBACK CmdMsg;
    struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&CmdMsg, sizeof(CmdMsg));
	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdMsg));
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, MT_HIF_LOOPBACK);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_NA);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_NA_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	CmdMsg.Loopback_Enable = IsEnable;
	CmdMsg.DestinationQid = RxQ;
#ifdef RT_BIG_ENDIAN
	CmdMsg.Loopback_Enable = cpu2le16(CmdMsg.Loopback_Enable);
	CmdMsg.DestinationQid = cpu2le16(CmdMsg.DestinationQid);
#endif
	MtAndesAppendCmdMsg(msg, (char *)&CmdMsg, sizeof(CmdMsg));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;

}
#endif
/*****************************************
	CID = 0xC2
*****************************************/

/*****************************************
	CID = 0xED
*****************************************/

/*****************************************
	CID = 0xEE
*****************************************/

/*****************************************
	CID = 0xEF
*****************************************/



/******************************************
	ROM CODE CMD
*******************************************/

static VOID CmdReStartDLRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	UINT8 Status;

	Status = *Data;

	switch (Status)
	{
		case WIFI_FW_DOWNLOAD_SUCCESS:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: Status Success!, Status(0)\n", __FUNCTION__));
			break;
		case WIFI_FW_DOWNLOAD_INVALID_PARAM:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: Status fail!, Status(1)\n", __FUNCTION__));
			break;
		default:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: Unknow Status(%d)\n", __FUNCTION__, Status));
			break;
	}
}


INT32 MtCmdRestartDLReq(RTMP_ADAPTER *ad)
{
	struct cmd_msg *msg;
	int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(ad, 0);
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, MT_RESTART_DL_REQ);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_NA);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_NA_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 7000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdReStartDLRsp);

    MtAndesInitCmdMsg(msg, attr);
	ret = ad->chipOps.MtCmdTx(ad, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


static VOID CmdAddrellLenRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	UINT8 Status;

	Status = *Data;

	switch (Status)
	{
		case TARGET_ADDRESS_LEN_SUCCESS:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s: Request target address and length success\n", __FUNCTION__));
			break;
		default:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
                ("%s: Unknow Status(%d)\n", __FUNCTION__, Status));
			break;
	}
}


INT32 MtCmdAddressLenReq(RTMP_ADAPTER *ad, UINT32 address,
                            UINT32 len, UINT32 data_mode)
{
	struct cmd_msg *msg;
	int ret = 0;
	UINT32 value;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("Start address = %x, DL length = %d, Data mode = %x\n",
									address, len, data_mode));
	msg = MtAndesAllocCmdMsg(ad, 12);
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, (address == ad->chipCap.rom_patch_offset) ?
                         MT_PATCH_START_REQ : MT_TARGET_ADDRESS_LEN_REQ);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_NA);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_NA_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdAddrellLenRsp);

    MtAndesInitCmdMsg(msg, attr);
	/* start address */
	value = cpu2le32(address);
	MtAndesAppendCmdMsg(msg, (char *)&value, 4);

	/* dl length */
	value = cpu2le32(len);
	MtAndesAppendCmdMsg(msg, (char *)&value, 4);

	/* data mode */
	value = cpu2le32(data_mode);
	MtAndesAppendCmdMsg(msg, (char *)&value, 4);

	ret = ad->chipOps.MtCmdTx(ad, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}



INT32 MtCmdFwScatter(RTMP_ADAPTER *ad, UINT8 *dl_payload,
                            UINT32 dl_len, UINT32 count)
{
	struct cmd_msg *msg;
	int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	msg = MtAndesAllocCmdMsg(ad, dl_len);
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, MT_FW_SCATTER);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_NA);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_NA);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)dl_payload, dl_len);

	ret = ad->chipOps.MtCmdTx(ad, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
        ("%s:(scatter = %d, ret = %d)\n", __FUNCTION__, count, ret));
	return ret;
}


static VOID CmdPatchSemRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
    RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)msg->priv;
    struct MCU_CTRL *Ctl = &pAd->MCUCtrl;

    Ctl->SemStatus = *Data;

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("Patch SEM Status=%d\n", Ctl->SemStatus));
}


 INT32 MtCmdPatchSemGet(RTMP_ADAPTER *ad, UINT32 Semaphore)
 {
	 struct cmd_msg *msg;
	 int ret = 0;
	 UINT32 value;
     struct _CMD_ATTRIBUTE attr = {0};

	 msg = MtAndesAllocCmdMsg(ad, 4);
	 if (!msg)
     {
		 ret = NDIS_STATUS_RESOURCES;
		 goto error;
	 }
     SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
     SET_CMD_ATTR_TYPE(attr, MT_PATCH_SEM_CONTROL);
     SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_NA);
     SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_NA_AND_WAIT_RETRY_RSP);
     SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
     SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
     SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
     SET_CMD_ATTR_RSP_HANDLER(attr, CmdPatchSemRsp);

     MtAndesInitCmdMsg(msg, attr);
	 /* Semaphore */
	 value = cpu2le32(Semaphore);
	 MtAndesAppendCmdMsg(msg, (char *)&value, 4);

	 ret = ad->chipOps.MtCmdTx(ad, msg);

 error:
	 MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
	 return ret;
 }


static VOID CmdPatchFinishRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	UINT8 Status;

	Status = *Data;

	switch (Status)
	{
		case WIFI_FW_DOWNLOAD_SUCCESS:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFI ROM Patch Download Success\n", __FUNCTION__));
			break;
		default:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFi ROM Patch Fail (%d)\n", __FUNCTION__, Status));
			break;
	}
}



 INT32 MtCmdPatchFinishReq(RTMP_ADAPTER *ad)
{
	struct cmd_msg *msg;
    int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                            ("%s\n", __FUNCTION__));

	msg = MtAndesAllocCmdMsg(ad, 0);
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, MT_PATCH_FINISH_REQ);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_NA);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_NA_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdPatchFinishRsp);

    MtAndesInitCmdMsg(msg, attr);
	ret = ad->chipOps.MtCmdTx(ad, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}


static VOID CmdStartDLRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	UINT8 Status;

	Status = *Data;

	switch (Status)
	{
		case WIFI_FW_DOWNLOAD_SUCCESS:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFI FW Download Success\n", __FUNCTION__));
			break;
		case WIFI_FW_DOWNLOAD_INVALID_PARAM:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFi FW Download Invalid Parameter\n", __FUNCTION__));
			break;
		case WIFI_FW_DOWNLOAD_INVALID_CRC:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFi FW Download Invalid CRC\n", __FUNCTION__));
			break;
		case WIFI_FW_DOWNLOAD_DECRYPTION_FAIL:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFi FW Download Decryption Fail\n", __FUNCTION__));
			break;
		case WIFI_FW_DOWNLOAD_UNKNOWN_CMD:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFi FW Download Unknown CMD\n", __FUNCTION__));
			break;
		case WIFI_FW_DOWNLOAD_TIMEOUT:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: WiFi FW Download Timeout\n", __FUNCTION__));
			break;
		default:
			MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                ("%s: Unknow Status(%d)\n", __FUNCTION__, Status));
			break;
	}
}


INT32 MtCmdFwStartReq(RTMP_ADAPTER *ad, UINT32 override, UINT32 address)
{
	struct cmd_msg *msg;
	int ret = 0;
	UINT32 value;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: override = %d, address = %d\n", __FUNCTION__, override, address));

	msg = MtAndesAllocCmdMsg(ad, 8);
	if (!msg)
    {
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, MT_FW_START_REQ);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_NA);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_NA_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdStartDLRsp);

    MtAndesInitCmdMsg(msg, attr);
	/* override */
	value = cpu2le32(override);
	MtAndesAppendCmdMsg(msg, (char *)&value, 4);

	/* entry point address */
	value = cpu2le32(address);

	MtAndesAppendCmdMsg(msg, (char *)&value, 4);

	ret = ad->chipOps.MtCmdTx(ad, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}

#ifdef DBDC_MODE
/*****************************************
	ExT_CID = 0x45
*****************************************/
static VOID MtCmdGetDbdcCtrlRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
	EXT_EVENT_DBDC_CTRL_T *pDbdcCmdResult = (EXT_EVENT_DBDC_CTRL_T*)Data;
	BCTRL_INFO_T *pDbdcRspResult = (BCTRL_INFO_T *)msg->attr.rsp.wb_buf_in_calbk;
	INT i;

	printk("Enable=%d,TotalNum=%d\n", pDbdcCmdResult->ucDbdcEnable,
                                    pDbdcCmdResult->ucTotalNum);

	pDbdcRspResult->TotalNum = pDbdcCmdResult->ucTotalNum;
	pDbdcRspResult->DBDCEnable = pDbdcCmdResult->ucDbdcEnable;
	for (i = 0; i < pDbdcCmdResult->ucTotalNum; i++)
	{
		pDbdcRspResult->BctrlEntries[i].Index =
                                pDbdcCmdResult->aBCtrlEntry[i].ucIndex;
		pDbdcRspResult->BctrlEntries[i].Type =
                                pDbdcCmdResult->aBCtrlEntry[i].ucType;
		pDbdcRspResult->BctrlEntries[i].BandIdx =
                                pDbdcCmdResult->aBCtrlEntry[i].ucBandIdx;

	}
}

INT32 MtCmdGetDbdcCtrl(RTMP_ADAPTER *pAd, BCTRL_INFO_T *pDbdcInfo)
{
	struct cmd_msg *msg;
	EXT_CMD_DBDC_CTRL_T DbdcCtrlCmd;

	INT32 ret = 0;
	INT32 Len = 0,i;
    struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&DbdcCtrlCmd, sizeof(EXT_CMD_DBDC_CTRL_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_DBDC_CTRL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	Len = 4 + sizeof(BAND_CTRL_ENTRY_T)*pDbdcInfo->TotalNum;


    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_DBDC_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pDbdcInfo);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetDbdcCtrlRsp);

    MtAndesInitCmdMsg(msg, attr);
	DbdcCtrlCmd.ucDbdcEnable = pDbdcInfo->DBDCEnable;
	DbdcCtrlCmd.ucTotalNum = pDbdcInfo->TotalNum;

	for (i = 0; i < pDbdcInfo->TotalNum; i++)
	{
		DbdcCtrlCmd.aBCtrlEntry[i].ucType = pDbdcInfo->BctrlEntries[i].Type;
		DbdcCtrlCmd.aBCtrlEntry[i].ucIndex = pDbdcInfo->BctrlEntries[i].Index;
	}

	MtAndesAppendCmdMsg(msg, (char *)&DbdcCtrlCmd, sizeof(EXT_CMD_DBDC_CTRL_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}


INT32 MtCmdSetDbdcCtrl(RTMP_ADAPTER *pAd, BCTRL_INFO_T *pBandInfo)
{
	struct cmd_msg *msg;
	EXT_CMD_DBDC_CTRL_T DbdcCtrlCmd;
	INT32 ret = 0;
	INT32 i = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&DbdcCtrlCmd, sizeof(EXT_CMD_DBDC_CTRL_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_DBDC_CTRL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_DBDC_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

	DbdcCtrlCmd.ucDbdcEnable = pBandInfo->DBDCEnable;
	DbdcCtrlCmd.ucTotalNum = pBandInfo->TotalNum;

	for (i = 0; i < pBandInfo->TotalNum; i++)
	{
		DbdcCtrlCmd.aBCtrlEntry[i].ucBandIdx = pBandInfo->BctrlEntries[i].BandIdx;
		DbdcCtrlCmd.aBCtrlEntry[i].ucType = pBandInfo->BctrlEntries[i].Type;
		DbdcCtrlCmd.aBCtrlEntry[i].ucIndex = pBandInfo->BctrlEntries[i].Index;
	}

	MtAndesAppendCmdMsg(msg, (char *)&DbdcCtrlCmd, sizeof(EXT_CMD_DBDC_CTRL_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

#endif /*DBDC_MODE*/


/*****************************************
	ExT_CID = 0x3C
*****************************************/
static VOID MtCmdGetEdcaRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
	EXT_EVENT_MAC_INFO_T *pEdcaCmdResult = (EXT_EVENT_MAC_INFO_T *)Data;
	MT_EDCA_CTRL_T *pEdcaRspResult =
                        (MT_EDCA_CTRL_T *)msg->attr.rsp.wb_buf_in_calbk;

	UINT32 i = 0;
	TX_AC_PARAM_T *pAcParm;

	pEdcaRspResult->ucTotalNum =
                pEdcaCmdResult->aucMacInfoResult.EdcaResult.ucTotalNum;

	for (i = 0; i < pEdcaRspResult->ucTotalNum; i++)
	{
		pAcParm = &pEdcaCmdResult->aucMacInfoResult.EdcaResult.rAcParam[i];

		pEdcaRspResult->rAcParam[i].u2Txop = le2cpu16(pAcParm->u2Txop);
		pEdcaRspResult->rAcParam[i].u2WinMax = le2cpu16(pAcParm->u2WinMax);
		pEdcaRspResult->rAcParam[i].ucAcNum = pAcParm->ucAcNum;
		pEdcaRspResult->rAcParam[i].ucAifs = pAcParm->ucAifs;
		pEdcaRspResult->rAcParam[i].ucWinMin = pAcParm->ucWinMin;
	}
}

INT32 MtCmdGetEdca(RTMP_ADAPTER *pAd,MT_EDCA_CTRL_T *pEdcaCtrl)
{
	struct cmd_msg *msg;
	EXT_CMD_GET_MAC_INFO_T MacInfoCmd;
	EXTRA_ARG_EDCA_T *pEdcaArg = &MacInfoCmd.aucExtraArgument.EdcaArg;
	INT32 ret = 0;
	INT32 Len = 0, i;
    struct _CMD_ATTRIBUTE attr = {0};

	os_zero_mem(&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GET_MAC_INFO_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	Len = 8 + sizeof(TX_AC_PARAM_T)*pEdcaCtrl->ucTotalNum;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_GET_MAC_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pEdcaCtrl);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetEdcaRsp);

    MtAndesInitCmdMsg(msg, attr);
	MacInfoCmd.u2MacInfoId = cpu2le16(MAC_INFO_TYPE_EDCA);
	pEdcaArg->ucTotalAcNum = pEdcaCtrl->ucTotalNum;
	for (i = 0; i < pEdcaCtrl->ucTotalNum; i++)
	{
		pEdcaArg->au4AcIndex[i] = cpu2le32(pEdcaCtrl->rAcParam[i].ucAcNum);
	}

	MtAndesAppendCmdMsg(msg, (char *)&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

static VOID MtCmdGetChBusyCntRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
	EXT_EVENT_MAC_INFO_T *pChBusyCntCmdResult = (EXT_EVENT_MAC_INFO_T*)Data;
	UINT32 *pChBusyCnt = (UINT32 *)msg->attr.rsp.wb_buf_in_calbk;
    *pChBusyCnt =
        le2cpu32(pChBusyCntCmdResult->aucMacInfoResult.ChBusyCntResult.u4ChBusyCnt);
}

INT32 MtCmdGetChBusyCnt(RTMP_ADAPTER *pAd,UCHAR ChIdx,UINT32 *pChBusyCnt)
{
	struct cmd_msg *msg;
	EXT_CMD_GET_MAC_INFO_T MacInfoCmd;
	EXTRA_ARG_CH_BUSY_CNT_T  *pChBusyCntArg =
            &MacInfoCmd.aucExtraArgument.ChBusyCntArg;
    struct _CMD_ATTRIBUTE attr = {0};

	INT32 ret = 0;
	INT32 Len = 0;

	os_zero_mem(&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GET_MAC_INFO_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	Len = 4 + sizeof(GET_CH_BUSY_CNT_T);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_GET_MAC_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pChBusyCnt);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetChBusyCntRsp);

    MtAndesInitCmdMsg(msg, attr);
	MacInfoCmd.u2MacInfoId = cpu2le16(MAC_INFO_TYPE_CHANNEL_BUSY_CNT);
	pChBusyCntArg->ucBand= ChIdx;

	MtAndesAppendCmdMsg(msg, (char *)&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

static VOID MtCmdGetWifiInterruptCntRsp(struct cmd_msg *msg,
                                    INT8 *Data, UINT16 Len)
{
    INT32 i;
	EXT_EVENT_MAC_INFO_T *pWifiInterruptCntCmdResult =
                                 (EXT_EVENT_MAC_INFO_T *)Data;
	UINT32 *pWifiInterruptCnt = (UINT32 *)msg->attr.rsp.wb_buf_in_calbk;
    UINT32 *pResultWifiInterruptCounter =
        pWifiInterruptCntCmdResult->aucMacInfoResult.WifiIntCntResult.u4WifiInterruptCounter;

    if (pWifiInterruptCntCmdResult->u2MacInfoId ==
                                            cpu2le16(MAC_INFO_TYPE_WIFI_INT_CNT))
    {
        for (i = 0; i < pWifiInterruptCntCmdResult->aucMacInfoResult.WifiIntCntResult.ucWifiInterruptNum; i++)
        {
            *pWifiInterruptCnt = le2cpu32(*pResultWifiInterruptCounter);
            pWifiInterruptCnt ++;
            pResultWifiInterruptCounter ++;
        }
    }
    else
    {
        MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:Get Wifi Interrupt Counter Error!\n", __FUNCTION__));
    }
}

INT32 MtCmdGetWifiInterruptCnt(RTMP_ADAPTER *pAd, UCHAR ChIdx, UCHAR WifiIntNum,
                                UINT32 WifiIntMask,UINT32 *pWifiInterruptCnt)
{
	struct cmd_msg *msg;
	EXT_CMD_GET_MAC_INFO_T MacInfoCmd;
	EXTRA_ARG_WF_INTERRUPT_CNT_T *pWifiInterruptCntArg =
                            &MacInfoCmd.aucExtraArgument.WifiInterruptCntArg;
    struct _CMD_ATTRIBUTE attr = {0};

	INT32 ret = 0;
	INT32 Len = 0;

	os_zero_mem(&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GET_MAC_INFO_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	Len = (sizeof(UINT32) * WifiIntNum) + sizeof(GET_WF_INTERRUPT_CNT_T) + 4;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_GET_MAC_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pWifiInterruptCnt);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetWifiInterruptCntRsp);

    MtAndesInitCmdMsg(msg, attr);
	MacInfoCmd.u2MacInfoId = cpu2le16(MAC_INFO_TYPE_WIFI_INT_CNT);
	pWifiInterruptCntArg->ucBand = ChIdx;
    pWifiInterruptCntArg->ucWifiInterruptNum = WifiIntNum;
    pWifiInterruptCntArg->u4WifiInterruptMask = cpu2le32(WifiIntMask);

	MtAndesAppendCmdMsg(msg, (char *)&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

    return ret;

error:

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

static VOID MtCmdGetTsfTimeRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
    EXT_EVENT_MAC_INFO_T *pTsfCmdResult = (EXT_EVENT_MAC_INFO_T *)Data;
    TSF_RESULT_T *pTsfResult = (TSF_RESULT_T *)msg->attr.rsp.wb_buf_in_calbk;
    pTsfResult->u4TsfBit0_31 =
            le2cpu32(pTsfCmdResult->aucMacInfoResult.TsfResult.u4TsfBit0_31);
    pTsfResult->u4TsfBit63_32 =
            le2cpu32(pTsfCmdResult->aucMacInfoResult.TsfResult.u4TsfBit63_32);
}

INT32 MtCmdGetTsfTime(RTMP_ADAPTER *pAd, UCHAR HwBssidIdx, TSF_RESULT_T *pTsfResult)
{
    struct cmd_msg *msg;
    EXT_CMD_GET_MAC_INFO_T MacInfoCmd;
    EXTRA_ARG_TSF_T  *pTsfArg = &MacInfoCmd.aucExtraArgument.TsfArg;
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    os_zero_mem(&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GET_MAC_INFO_T));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    Len = 4 + sizeof(TSF_RESULT_T);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_GET_MAC_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pTsfResult);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetTsfTimeRsp);

    MtAndesInitCmdMsg(msg, attr);
    MacInfoCmd.u2MacInfoId = cpu2le16(MAC_INFO_TYPE_TSF);
    pTsfArg->ucHwBssidIndex= HwBssidIdx;

    MtAndesAppendCmdMsg(msg, (char *)&MacInfoCmd, sizeof(EXT_CMD_GET_MAC_INFO_T));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}

INT32 MtCmdSetMacTxRx(RTMP_ADAPTER *pAd, UCHAR BandIdx, BOOLEAN bEnable)
{
	struct cmd_msg *msg;
	EXT_CMD_MAC_INIT_CTRL_T  MacInitCmd;
    struct _CMD_ATTRIBUTE attr = {0};

    INT32 ret = 0;

    os_zero_mem(&MacInitCmd, sizeof(EXT_CMD_MAC_INIT_CTRL_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_MAC_INIT_CTRL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_MAC_INIT_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MacInitCmd.ucBand = BandIdx;
	MacInitCmd.ucMacInitCtrl = bEnable;

	MtAndesAppendCmdMsg(msg, (char *)&MacInitCmd, sizeof(EXT_CMD_MAC_INIT_CTRL_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

#ifdef MT_DFS_SUPPORT
INT32 MtCmdSetDfsTxStart(RTMP_ADAPTER *pAd, UCHAR BandIdx)
{
	struct cmd_msg *msg;
	EXT_CMD_MAC_INIT_CTRL_T  MacInitCmd;
    struct _CMD_ATTRIBUTE attr = {0};

    INT32 ret = 0;
    os_zero_mem(&MacInitCmd, sizeof(EXT_CMD_MAC_INIT_CTRL_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_MAC_INIT_CTRL_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_MAC_INIT_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
	MacInitCmd.ucBand = BandIdx;
	MacInitCmd.ucMacInitCtrl = MAC_DFS_TXSTART;

	MtAndesAppendCmdMsg(msg, (char *)&MacInitCmd, sizeof(EXT_CMD_MAC_INIT_CTRL_T));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}
#endif

#ifdef PRE_CAL_TRX_SET1_SUPPORT
static VOID MtCmdGetRXDCOCCalResultRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
    EXT_CMD_GET_RXDCOC_RESULT_T *pDCOCResult = (EXT_CMD_GET_RXDCOC_RESULT_T *)Data;

	if(pDCOCResult->DirectionToCR == TRUE)	/* Flash/BinFile to CR */
	{
	    if(pDCOCResult->RxDCOCResult.ResultSuccess)
	       	;
	    else
	       	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
	            ("%s:(ret = %d) write CR for CH %d ,BW %d FAILED!\n"
	            , __FUNCTION__, pDCOCResult->RxDCOCResult.ResultSuccess
	            , pDCOCResult->RxDCOCResult.u2ChFreq, pDCOCResult->RxDCOCResult.ucBW));	    
	}
	else /* CR to Flash/BinFile */
	{
		if(pDCOCResult->RxDCOCResult.ResultSuccess)
	    {
	    	os_move_mem(msg->attr.rsp.wb_buf_in_calbk, &pDCOCResult->RxDCOCResult, sizeof(RXDCOC_RESULT_T));
	    	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("========== %s GOT result ========\n", __FUNCTION__));
	    }
	    else
	    {	    	
	    	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
	            ("%s:(ret = %d) calibration for CH %d ,BW %d FAILED!\n"
	            , __FUNCTION__, pDCOCResult->RxDCOCResult.ResultSuccess
	            , pDCOCResult->RxDCOCResult.u2ChFreq, pDCOCResult->RxDCOCResult.ucBW));
	    }
	}
}

INT32 MtCmdGetRXDCOCCalResult(RTMP_ADAPTER *pAd, BOOLEAN DirectionToCR
	,UINT16 CentralFreq,UINT8 BW,UINT8 Band,BOOLEAN IsSecondary80,BOOLEAN DoRuntimeCalibration, RXDCOC_RESULT_T *pRxDcocResult)
{
    struct cmd_msg *msg;
    EXT_CMD_GET_RXDCOC_RESULT_T CmdDCOCResult;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    if(pAd->E2pAccessMode != E2P_FLASH_MODE && pAd->E2pAccessMode != E2P_BIN_MODE)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
		("%s : Currently not in FLASH or BIN MODE,return. \n", __FUNCTION__));
		goto error;
	}

    os_zero_mem(&CmdDCOCResult, sizeof(EXT_CMD_GET_RXDCOC_RESULT_T));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GET_RXDCOC_RESULT_T));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    Len = sizeof(EXT_CMD_GET_RXDCOC_RESULT_T);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RXDCOC_CAL_RESULT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pRxDcocResult);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetRXDCOCCalResultRsp);

    MtAndesInitCmdMsg(msg, attr);
    CmdDCOCResult.DirectionToCR = DirectionToCR;
    CmdDCOCResult.RxDCOCResult.u2ChFreq = cpu2le16(CentralFreq);
    CmdDCOCResult.RxDCOCResult.ucBW = BW;
    CmdDCOCResult.RxDCOCResult.ucBand = Band;
    CmdDCOCResult.RxDCOCResult.DBDCEnable = pAd->CommonCfg.dbdc_mode;
    CmdDCOCResult.RxDCOCResult.bSecBW80 = IsSecondary80;
    CmdDCOCResult.ucDoRuntimeCalibration = DoRuntimeCalibration;

    if(DirectionToCR == TRUE)
    {
    	os_move_mem(&CmdDCOCResult.RxDCOCResult.ucDCOCTBL_I_WF0_SX0_LNA[0], \
    		&pRxDcocResult->ucDCOCTBL_I_WF0_SX0_LNA[0], RXDCOC_SIZE);
    }
#ifdef RT_BIG_ENDIAN
	RTMPEndianChange((UCHAR*)(&CmdDCOCResult.RxDCOCResult.ucDCOCTBL_I_WF0_SX0_LNA[0]),RXDCOC_SIZE);
#endif
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
	            ("%s: send cmd Direction[%s] Freq [%d] Input Cent[%d] BW[%d] Band[%d] SecBW80[%d]\n"
	            , __FUNCTION__,(CmdDCOCResult.DirectionToCR==TRUE)?"ToCR":"FromCR"
	            , le2cpu16(CmdDCOCResult.RxDCOCResult.u2ChFreq),CentralFreq, CmdDCOCResult.RxDCOCResult.ucBW
	            ,CmdDCOCResult.RxDCOCResult.ucBand,CmdDCOCResult.RxDCOCResult.bSecBW80));
    MtAndesAppendCmdMsg(msg, (char *)&CmdDCOCResult, sizeof(EXT_CMD_GET_RXDCOC_RESULT_T));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}

static VOID MtCmdGetTXDPDCalResultRsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
    EXT_CMD_GET_TXDPD_RESULT_T *pDPDResult = (EXT_CMD_GET_TXDPD_RESULT_T *)Data;

	if(pDPDResult->DirectionToCR == TRUE)/* Flash/BinFile to CR */
	{
	    if(pDPDResult->TxDpdResult.ResultSuccess)
	       	;
	    else
	       	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
	            ("%s:(ret = %d) write CR for CH %d ,BW %d FAILED!\n"
	            , __FUNCTION__, pDPDResult->TxDpdResult.ResultSuccess
	            , pDPDResult->TxDpdResult.u2ChFreq, pDPDResult->TxDpdResult.ucBW));	    
	}
	else /* CR to Flash/BinFile */
	{
		if(pDPDResult->TxDpdResult.ResultSuccess)
	    {
	    	os_move_mem(msg->attr.rsp.wb_buf_in_calbk, &pDPDResult->TxDpdResult, sizeof(TXDPD_RESULT_T));
	    	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("========== %s GOT result ========\n", __FUNCTION__));
	    }
	    else
	    {	    	
	    	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
	            ("%s:(ret = %d) calibration for CH %d ,BW %d FAILED!\n"
	            , __FUNCTION__, pDPDResult->TxDpdResult.ResultSuccess
	            , pDPDResult->TxDpdResult.u2ChFreq, pDPDResult->TxDpdResult.ucBW));
	    }
	}
}

INT32 MtCmdGetTXDPDCalResult(RTMP_ADAPTER *pAd, BOOLEAN DirectionToCR
	,UINT16 CentralFreq,UINT8 BW,UINT8 Band,BOOLEAN IsSecondary80, BOOLEAN DoRuntimeCalibration, TXDPD_RESULT_T *pTxDPDResult)
{
    struct cmd_msg *msg;
    EXT_CMD_GET_TXDPD_RESULT_T CmdDPDResult;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    if(pAd->E2pAccessMode != E2P_FLASH_MODE && pAd->E2pAccessMode != E2P_BIN_MODE)
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
		("%s : Currently not in FLASH or BIN MODE,return. \n", __FUNCTION__));
		goto error;
	}

    os_zero_mem(&CmdDPDResult, sizeof(EXT_CMD_GET_TXDPD_RESULT_T));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GET_TXDPD_RESULT_T));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    Len = sizeof(EXT_CMD_GET_TXDPD_RESULT_T);  

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TXDPD_CAL_RESULT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pTxDPDResult);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdGetTXDPDCalResultRsp);

    MtAndesInitCmdMsg(msg, attr);
    CmdDPDResult.DirectionToCR = DirectionToCR;
    CmdDPDResult.TxDpdResult.u2ChFreq = cpu2le16(CentralFreq);
    CmdDPDResult.TxDpdResult.ucBW = BW;
    CmdDPDResult.TxDpdResult.ucBand = Band;
    CmdDPDResult.TxDpdResult.DBDCEnable = pAd->CommonCfg.dbdc_mode;
    CmdDPDResult.TxDpdResult.bSecBW80 = IsSecondary80;
    CmdDPDResult.ucDoRuntimeCalibration = DoRuntimeCalibration;

    if(DirectionToCR == TRUE)
    {
    	os_move_mem(&CmdDPDResult.TxDpdResult.u4DPDG0_WF0_Prim, \
    		&pTxDPDResult->u4DPDG0_WF0_Prim, TXDPD_SIZE);
    }
#ifdef RT_BIG_ENDIAN
	CmdDPDResult.TxDpdResult.u4DPDG0_WF0_Prim = cpu2le32(CmdDPDResult.TxDpdResult.u4DPDG0_WF0_Prim);
	CmdDPDResult.TxDpdResult.u4DPDG0_WF1_Prim = cpu2le32(CmdDPDResult.TxDpdResult.u4DPDG0_WF1_Prim);
	CmdDPDResult.TxDpdResult.u4DPDG0_WF2_Prim = cpu2le32(CmdDPDResult.TxDpdResult.u4DPDG0_WF2_Prim);
	CmdDPDResult.TxDpdResult.u4DPDG0_WF2_Sec = cpu2le32(CmdDPDResult.TxDpdResult.u4DPDG0_WF2_Sec);
	CmdDPDResult.TxDpdResult.u4DPDG0_WF3_Prim = cpu2le32(CmdDPDResult.TxDpdResult.u4DPDG0_WF3_Prim);
	CmdDPDResult.TxDpdResult.u4DPDG0_WF3_Sec = cpu2le32(CmdDPDResult.TxDpdResult.u4DPDG0_WF3_Sec);
#endif
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
	            ("%s: send cmd Direction[%s] Freq [%d] Input Cent[%d] BW[%d] Band[%d] SecBW80[%d]\n"
	            , __FUNCTION__,(CmdDPDResult.DirectionToCR==TRUE)?"ToCR":"FromCR"
	            , le2cpu16(CmdDPDResult.TxDpdResult.u2ChFreq),CentralFreq, CmdDPDResult.TxDpdResult.ucBW
	            ,CmdDPDResult.TxDpdResult.ucBand,CmdDPDResult.TxDpdResult.bSecBW80));
    MtAndesAppendCmdMsg(msg, (char *)&CmdDPDResult, sizeof(EXT_CMD_GET_TXDPD_RESULT_T));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}


static VOID MtCmdRDCERsp(struct cmd_msg *msg, INT8 *Data, UINT16 Len)
{
    EXT_CMD_RDCE_VERIFY_T *pRDCEresult = (EXT_CMD_RDCE_VERIFY_T *)Data;

	if(pRDCEresult->Result)
	{
       	MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
            ("%s:(ret = %d) RDCE VERIFY [PASS]\n", __FUNCTION__, pRDCEresult->Result));	    
	}
	else
	{
		MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
	        ("%s:(ret = %d) RDCE VERIFY [FAIL]\n", __FUNCTION__, pRDCEresult->Result));	
	}
}

INT32 MtCmdRDCE(RTMP_ADAPTER *pAd, UINT8 type, UINT8 BW, UINT8 Band)
{
    struct cmd_msg *msg;
    EXT_CMD_RDCE_VERIFY_T CmdRDCE;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    os_zero_mem(&CmdRDCE, sizeof(EXT_CMD_RDCE_VERIFY_T));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_RDCE_VERIFY_T));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    Len = sizeof(EXT_CMD_RDCE_VERIFY_T);  

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RDCE_VERIFY);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, MtCmdRDCERsp);

    MtAndesInitCmdMsg(msg, attr);
	CmdRDCE.Result = FALSE;
	CmdRDCE.ucType = type;
	CmdRDCE.ucBW = BW;
	CmdRDCE.ucBand = Band;
	
    MtAndesAppendCmdMsg(msg, (char *)&CmdRDCE, sizeof(EXT_CMD_RDCE_VERIFY_T));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}

#endif /* PRE_CAL_TRX_SET1_SUPPORT */

#if defined(RLM_CAL_CACHE_SUPPORT) || defined(PRE_CAL_TRX_SET2_SUPPORT)
static INT32 MtCmdSetTxLpfCal(RTMP_ADAPTER *pAd, VOID* rlmCache)
{
    struct cmd_msg *msg;
    VOID *pCmdTxLpfInfo = NULL;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    //Len = TxLpfCalInfoAlloc(pAd, pAd->rlmCalCache, &pCmdTxLpfInfo);
    Len = TxLpfCalInfoAlloc(pAd, rlmCache, &pCmdTxLpfInfo);
    if (Len == 0)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    msg = MtAndesAllocCmdMsg(pAd, Len);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
    ("\x1b[35m%s: size %d, ucDataToFromFlash %d, ucDataValid %d, u2BitMap %x, cPreCalTemp %d\x1b[m\n", 
            __FUNCTION__, Len,
            ((P_TXLPF_CAL_INFO_T)pCmdTxLpfInfo)->ucDataToFromFlash,
            ((P_TXLPF_CAL_INFO_T)pCmdTxLpfInfo)->ucDataValid,
            ((P_TXLPF_CAL_INFO_T)pCmdTxLpfInfo)->u2BitMap,
            ((P_TXLPF_CAL_INFO_T)pCmdTxLpfInfo)->cPreCalTemp));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TXLPF_CAL_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RETRY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)pCmdTxLpfInfo, Len);
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}

static INT32 MtCmdSetTxIqCal(RTMP_ADAPTER *pAd, VOID* rlmCache)
{
    struct cmd_msg *msg;
    VOID *pCmdTxIqInfo = NULL;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    //Len = TxIqCalInfoAlloc(pAd, pAd->rlmCalCache, &pCmdTxIqInfo);
    Len = TxIqCalInfoAlloc(pAd, rlmCache, &pCmdTxIqInfo);
    if (Len == 0)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    msg = MtAndesAllocCmdMsg(pAd, Len);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
    ("\x1b[35m%s: size %d, ucDataToFromFlash %d, ucDataValid %d, u2BitMap %x\x1b[m\n", 
            __FUNCTION__, Len,
            ((P_TXIQ_CAL_INFO_T)pCmdTxIqInfo)->ucDataToFromFlash,
            ((P_TXIQ_CAL_INFO_T)pCmdTxIqInfo)->ucDataValid,
            ((P_TXIQ_CAL_INFO_T)pCmdTxIqInfo)->u2BitMap));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TXIQ_CAL_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RETRY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)pCmdTxIqInfo, Len);
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    if (pCmdTxIqInfo != NULL)
        os_free_mem(pCmdTxIqInfo);
    return ret;
}

static INT32 MtCmdSetTxDcCal(RTMP_ADAPTER *pAd, VOID* rlmCache)
{
    struct cmd_msg *msg;
    VOID *pCmdTxDcInfo = NULL;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    //Len = TxDcCalInfoAlloc(pAd, pAd->rlmCalCache, &pCmdTxDcInfo);
    Len = TxDcCalInfoAlloc(pAd, rlmCache, &pCmdTxDcInfo);
    if (Len == 0)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    msg = MtAndesAllocCmdMsg(pAd, Len);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
    ("\x1b[35m%s: size %d, ucDataToFromFlash %d, ucDataValid %d, u2BitMap %x\x1b[m\n", 
            __FUNCTION__, Len,
            ((P_TXDC_CAL_INFO_T)pCmdTxDcInfo)->ucDataToFromFlash,
            ((P_TXDC_CAL_INFO_T)pCmdTxDcInfo)->ucDataValid,
            ((P_TXDC_CAL_INFO_T)pCmdTxDcInfo)->u2BitMap));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TXDC_CAL_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RETRY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)pCmdTxDcInfo, Len);
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    if (pCmdTxDcInfo != NULL)
        os_free_mem(pCmdTxDcInfo);
    return ret;
}

static INT32 MtCmdSetRxFiCal(RTMP_ADAPTER *pAd, VOID* rlmCache)
{
    struct cmd_msg *msg;
    VOID *pCmdRxFiInfo = NULL;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    //Len = RxFiCalInfoAlloc(pAd, pAd->rlmCalCache, &pCmdRxFiInfo);
    Len = RxFiCalInfoAlloc(pAd, rlmCache, &pCmdRxFiInfo);
    if (Len == 0)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    msg = MtAndesAllocCmdMsg(pAd, Len);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
    ("\x1b[35m%s: size %d, ucDataToFromFlash %d, ucDataValid %d, u2BitMap %x\x1b[m\n", 
            __FUNCTION__, Len,
            ((P_RXFI_CAL_INFO_T)pCmdRxFiInfo)->ucDataToFromFlash,
            ((P_RXFI_CAL_INFO_T)pCmdRxFiInfo)->ucDataValid,
            ((P_RXFI_CAL_INFO_T)pCmdRxFiInfo)->u2BitMap));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RXFI_CAL_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RETRY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)pCmdRxFiInfo, Len);
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    if (pCmdRxFiInfo != NULL)
        os_free_mem(pCmdRxFiInfo);
    return ret;
}

static INT32 MtCmdSetRxFdCal(RTMP_ADAPTER *pAd, VOID* rlmCache, UINT32 chGroup)
{
    struct cmd_msg *msg;
    VOID *pCmdRxFdInfo;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    //Len = RxFdCalInfoAlloc(pAd, pAd->rlmCalCache, &pCmdRxFdInfo, chGroup);
    Len = RxFdCalInfoAlloc(pAd, rlmCache, &pCmdRxFdInfo, chGroup);
    if (Len == 0)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    msg = MtAndesAllocCmdMsg(pAd, Len);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
    ("\x1b[35m%s: size %d, ucDataToFromFlash %d, ucDataValid %d, u2BitMap %x, u4ChGroupId %d\x1b[m\n", 
            __FUNCTION__, Len,
            ((P_RXFD_CAL_INFO_T)pCmdRxFdInfo)->ucDataToFromFlash,
            ((P_RXFD_CAL_INFO_T)pCmdRxFdInfo)->ucDataValid,
            ((P_RXFD_CAL_INFO_T)pCmdRxFdInfo)->u2BitMap,
            ((P_RXFD_CAL_INFO_T)pCmdRxFdInfo)->u4ChGroupId));

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RXFD_CAL_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RETRY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)pCmdRxFdInfo, Len);
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    if (pCmdRxFdInfo != NULL)
        os_free_mem(pCmdRxFdInfo);
    return ret;
}

static INT32 MtCmdSetRlmPorCal(RTMP_ADAPTER *pAd, VOID* rlmCache)
{
    struct cmd_msg *msg;
    VOID *pCmdRlmPorInfo;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    Len = RlmPorCalInfoAlloc(pAd, pAd->rlmCalCache, &pCmdRlmPorInfo);
    if (Len == 0)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    msg = MtAndesAllocCmdMsg(pAd, Len);
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_POR_CAL_INFO);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_RETRY);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, Len);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)pCmdRlmPorInfo, Len);
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("\x1b[41m %s: (ret = %d)\x1b[m \n", __FUNCTION__, ret));

    if (pCmdRlmPorInfo != NULL)
        os_free_mem(pCmdRlmPorInfo);
    return ret;
}

VOID rlmCalCacheApply(RTMP_ADAPTER *pAd, VOID* rlmCache)
{
    UINT32 chGroup;

    // if DBDC_mode or CalCacheApply =1 , run rlmCalCacheApply
    if((pAd->CommonCfg.CalCacheApply == 0) && (pAd->CommonCfg.dbdc_mode==0))
        return;
    
    if (!rlmCalCacheDone(rlmCache))
    {
        MtCmdSetRlmPorCal(pAd, rlmCache);
        return;
    }

    MtCmdSetTxLpfCal(pAd, rlmCache);
    MtCmdSetTxIqCal(pAd, rlmCache);
    MtCmdSetTxDcCal(pAd, rlmCache);
    MtCmdSetRxFiCal(pAd, rlmCache);

    for (chGroup=0; chGroup<9; chGroup++)
        MtCmdSetRxFdCal(pAd, rlmCache, chGroup);
    MtCmdSetRlmPorCal(pAd, rlmCache);
}
#endif /* defined(RLM_CAL_CACHE_SUPPORT) || defined(PRE_CAL_TRX_SET2_SUPPORT) */

#ifdef PRE_CAL_TRX_SET2_SUPPORT
INT32 MtCmdGetPreCalResult(RTMP_ADAPTER *pAd, UINT8 CalId, UINT16 PreCalBitMap)
{
    struct cmd_msg *msg;
    EXT_CMD_GET_PRECAL_RESULT_T PreCalCtrl;
    INT32 ret = 0;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s----------------->\n",__FUNCTION__));

    PreCalCtrl.u2PreCalBitMap = cpu2le16(PreCalBitMap);
    PreCalCtrl.ucCalId = CalId;
     
    msg = MtAndesAllocCmdMsg(pAd, sizeof(PreCalCtrl));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }
    
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PRE_CAL_RESULT);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 60000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

    MtAndesAppendCmdMsg(msg, (char *)&PreCalCtrl, sizeof(PreCalCtrl));
    

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
        
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s<-----------------\n",__FUNCTION__));

    return ret;
        
error:
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
    ("%s:(ret = %d)\n", __FUNCTION__, ret));
           
    return ret;
}

INT32 MtCmdPreCalReStoreProc(RTMP_ADAPTER *pAd, INT32 *pPreCalBuffer)
{
    UINT32 IDOffset, LenOffset, Offset, Length;          
    UINT32 CalDataSize, HeaderSize, chGroup;
    UINT16 BitMap;
    RLM_CAL_CACHE *prlmFlash = NULL; 
    extern int DebugLevel;

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s----------------->\n",__FUNCTION__));

    /* Initialization */
    IDOffset = 0;
    LenOffset = 1; //Skip ID field
    
    /* Allocate memory for temp cahce buffer
*/
    if (os_alloc_mem(pAd, (UCHAR **)&prlmFlash, sizeof(RLM_CAL_CACHE)))
    {
        MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("\x1b[41m %s: Not enough memory for dynamic allocating !!!! \x1b[m\n", __FUNCTION__));
        
        return NDIS_STATUS_FAILURE;
    }

    os_zero_mem(prlmFlash, sizeof(RLM_CAL_CACHE));
      
    if (*(pPreCalBuffer + IDOffset) == PRECAL_TXLPF)
    {
        P_TXLPF_CAL_INFO_T pCalData = &prlmFlash->txLpfCalInfo;

        MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
        ("\x1b[41m %s PRECAL_TXLPF ------------>\x1b[m\n",__FUNCTION__));

        /* Update header size and calibration data size */
        HeaderSize = (UINT32) &((TXLPF_CAL_INFO_T *)NULL)->au4Data[0];
        CalDataSize = TXLPF_PER_GROUP_DATA_SIZE;
        
        /* Query length of pre-cal item */
        Length = *(pPreCalBuffer + LenOffset);

        /* Update the header */
        Offset = LenOffset + 1;
        os_move_mem(&pCalData->ucDataToFromFlash, pPreCalBuffer + Offset, HeaderSize);

        /* Get bitmap */
        BitMap = pCalData->u2BitMap;

        /* Update the calibration data */
        Offset += HeaderSize/sizeof(INT32);
        for (chGroup = 0; chGroup < CHANNEL_GROUP_NUM; chGroup++)
        {
            if (BitMap & (1 << chGroup))
            {
                os_move_mem(&pCalData->au4Data[chGroup * CalDataSize/sizeof(UINT32)], pPreCalBuffer + Offset, CalDataSize);
                Offset += CalDataSize/sizeof(INT32);
            }
        }
        
        /* Update offset of parameter */
        IDOffset = LenOffset + (Length/sizeof(INT32));
        LenOffset = IDOffset + 1; //Skip ID field  

        RLM_CAL_CACHE_TXLPF_CAL_DONE(prlmFlash);    

        hex_dump("PRECAL_TXLPF: ", (char *)pCalData, sizeof(TXLPF_CAL_INFO_T));
    }

    if(*(pPreCalBuffer + IDOffset) == PRECAL_TXIQ) 
    {
        P_TXIQ_CAL_INFO_T pCalData = &prlmFlash->txIqCalInfo;

        MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE,         
        ("\x1b[41m %s PRECAL_TXIQ ------------>\x1b[m\n",__FUNCTION__));

        /* Update header size and calibration data size */
        HeaderSize = (UINT32) &((TXIQ_CAL_INFO_T *)NULL)->au4Data[0];
        CalDataSize = TXIQ_PER_GROUP_DATA_SIZE;
        
        /* Query length of pre-cal item */
        Length = *(pPreCalBuffer + LenOffset);

        /* Update the header */
        Offset = LenOffset + 1;
        os_move_mem(&pCalData->ucDataToFromFlash, pPreCalBuffer + Offset, HeaderSize);

        /* Get bitmap */
        BitMap = pCalData->u2BitMap;

        /* Update the calibration data */
        Offset += HeaderSize/sizeof(INT32);
        for (chGroup = 0; chGroup < CHANNEL_GROUP_NUM; chGroup++)
        {
            if (BitMap & (1 << chGroup))
            {
                os_move_mem(&pCalData->au4Data[chGroup * CalDataSize/sizeof(UINT32)], pPreCalBuffer + Offset, CalDataSize);
                Offset += CalDataSize/sizeof(INT32);
            }
        }  
        
        /* Update offset of parameter */
        IDOffset = LenOffset + (Length/sizeof(INT32));
        LenOffset = IDOffset + 1; //Skip ID field  

        RLM_CAL_CACHE_TXIQ_CAL_DONE(prlmFlash);

        hex_dump("PRECAL_TXIQ: ", (char *)pCalData, sizeof(TXIQ_CAL_INFO_T));
    }
    
    if(*(pPreCalBuffer + IDOffset) == PRECAL_TXDC) 
    {
        P_TXDC_CAL_INFO_T pCalData = &prlmFlash->txDcCalInfo;

        MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE,        
        ("\x1b[41m %s PRECAL_TXDC ------------>\x1b[m\n",__FUNCTION__));

        /* Update header size and calibration data size */
        HeaderSize = (UINT32) &((TXDC_CAL_INFO_T *)NULL)->au4Data[0];
        CalDataSize = TXDC_PER_GROUP_DATA_SIZE;
        
        /* Query length of pre-cal item */
        Length = *(pPreCalBuffer + LenOffset);

        /* Update the header */
        Offset = LenOffset + 1;
        os_move_mem(&pCalData->ucDataToFromFlash, pPreCalBuffer + Offset, HeaderSize);

        /* Get bitmap */
        BitMap = pCalData->u2BitMap;

        /* Update the calibration data */
        Offset += HeaderSize/sizeof(INT32);
        for (chGroup = 0; chGroup < CHANNEL_GROUP_NUM; chGroup++)
        {
            if (BitMap & (1 << chGroup))
            {
                os_move_mem(&pCalData->au4Data[chGroup * CalDataSize/sizeof(UINT32)], pPreCalBuffer + Offset, CalDataSize);
                Offset += CalDataSize/sizeof(INT32);
            }
        } 

        /* Update offset of parameter */
        IDOffset = LenOffset + (Length/sizeof(INT32));
        LenOffset = IDOffset + 1; //Skip ID field  

        RLM_CAL_CACHE_TXDC_CAL_DONE(prlmFlash);

        hex_dump("PRECAL_TXDC: ", (char *)pCalData, sizeof(TXDC_CAL_INFO_T));
    }
    
    for (chGroup = 0; chGroup < CHANNEL_GROUP_NUM; chGroup++)
    {
        if (*(pPreCalBuffer + IDOffset) == PRECAL_RXFD)
        {
            P_RXFD_CAL_CACHE_T pCalData = &(prlmFlash->rxFdCalInfo[chGroup]);
            UINT32 chGroupID;

            MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("\x1b[41m %s PRECAL_RXFD group %d------------>\x1b[m\n",__FUNCTION__, chGroup));

            /* Update header size and calibration data size */
            HeaderSize = (UINT32) &((RXFD_CAL_INFO_T *)NULL)->au4Data[0];
            CalDataSize = RXFD_PER_GROUP_DATA_SIZE;
            
            /* Query length of pre-cal item */
            Length = *(pPreCalBuffer + LenOffset);

            /* Update the header */
            Offset = LenOffset + 1;
            os_move_mem(&pCalData->ucDataToFromFlash, pPreCalBuffer + Offset, HeaderSize);

            /* Get bitmap */
            BitMap = pCalData->u2BitMap;
            
            /* Get chgroup ID */
            chGroupID = pCalData->u4ChGroupId;

            /* Update the calibration data */
            Offset += HeaderSize/sizeof(INT32);
            if (BitMap & (1 << chGroupID))
            {
                os_move_mem(&pCalData->au4Data[0], pPreCalBuffer + Offset, CalDataSize);
                Offset += CalDataSize/sizeof(INT32);
            }          

            /* Update offset of parameter */
            IDOffset = LenOffset + (Length/sizeof(INT32));
            LenOffset = IDOffset + 1; //Skip ID field  

            RLM_CAL_CACHE_RXFD_CAL_DONE(prlmFlash, chGroup);

            hex_dump("PRECAL_RXFD: ", (char *)pCalData, sizeof(RXFD_CAL_CACHE_T));
        }        
    }

    if(*(pPreCalBuffer + IDOffset) == PRECAL_RXFI) 
    {
        P_RXFI_CAL_INFO_T pCalData = &prlmFlash->rxFiCalInfo;

        MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("\x1b[41m %s PRECAL_RXFI ------------>\x1b[m\n",__FUNCTION__));

        /* Update header size and calibration data size */
        HeaderSize = (UINT32) &((RXFI_CAL_INFO_T *)NULL)->au4Data[0];
        CalDataSize = RXFI_PER_GROUP_DATA_SIZE;
        
        /* Query length of pre-cal item */
        Length = *(pPreCalBuffer + LenOffset);

        /* Update the header */
        Offset = LenOffset + 1;
        os_move_mem(&pCalData->ucDataToFromFlash, pPreCalBuffer + Offset, HeaderSize);

        /* Get bitmap */
        BitMap = pCalData->u2BitMap;

        /* Update the calibration data */
        Offset += HeaderSize/sizeof(INT32);
        for (chGroup = 0; chGroup < CHANNEL_GROUP_NUM; chGroup++)
        {
            if (BitMap & (1 << chGroup))
            {
                os_move_mem(&pCalData->au4Data[chGroup * CalDataSize/sizeof(UINT32)], pPreCalBuffer + Offset, CalDataSize);
                Offset += CalDataSize/sizeof(INT32);
            }
        } 

        /* Update offset of parameter */
        IDOffset = LenOffset + (Length/sizeof(INT32));
        LenOffset = IDOffset + 1; //Skip ID field  

        RLM_CAL_CACHE_RXFI_CAL_DONE(prlmFlash);

        hex_dump("PRECAL_RXFI: ", (char *)pCalData, sizeof(RXFI_CAL_INFO_T));
    }


    /* Send restored calibration data to FW*/
    MtCmdSetTxLpfCal(pAd, prlmFlash);
    MtCmdSetTxIqCal(pAd, prlmFlash);
    MtCmdSetTxDcCal(pAd, prlmFlash);
    MtCmdSetRxFiCal(pAd, prlmFlash);
    for (chGroup = 0; chGroup < CHANNEL_GROUP_NUM; chGroup++)
    {
        MtCmdSetRxFdCal(pAd, prlmFlash, chGroup);
    } 

    os_free_mem(prlmFlash);
    
    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s----------------->\n",__FUNCTION__));

    return NDIS_STATUS_SUCCESS;    
}
#endif /* PRE_CAL_TRX_SET2_SUPPORT */

#ifdef PA_TRIM_SUPPORT
INT32 MtCmdCalReStoreFromFileProc(RTMP_ADAPTER *pAd, CAL_RESTORE_FUNC_IDX FuncIdx)
{
     INT32 Status = NDIS_STATUS_FAILURE;
     switch (FuncIdx)
     {
         case CAL_RESTORE_PA_TRIM:
            Status = MtCmdPATrimReStoreProc(pAd);
            break;
         default:
            MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("\x1b[41m%s : Not support for restoring this item !!\x1b[m\n", __FUNCTION__));                
            break;
     }
     return Status;
}

INT32 MtCmdPATrimReStoreProc(RTMP_ADAPTER *pAd)
{
    struct cmd_msg *msg;
    EXT_CMD_PA_TRIM_T PATrimCtrl;
    INT32 Status = NDIS_STATUS_FAILURE, i;
    //UINT32 Offset;
	UINT8 idx;
	UINT16 rdAddr;
	USHORT * pPATrimData = (USHORT *)&PATrimCtrl.u4Data[0];
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif

    MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s----------------->\n",__FUNCTION__));

    os_zero_mem(&PATrimCtrl, sizeof(EXT_CMD_PA_TRIM_T));
    PATrimCtrl.Header.ucFuncIndex = CAL_RESTORE_PA_TRIM;
    PATrimCtrl.Header.u4DataLen = PA_TRIM_SIZE;

    /* Load data from EEPROM */
	rdAddr = PA_TRIM_START_ADDR1;
	for(idx=0 ; idx < PA_TRIM_BLOCK_SIZE; idx++)
	{
		RT28xx_EEPROM_READ16(pAd,rdAddr, *pPATrimData);
		pPATrimData ++;
		rdAddr += 2; 
   }
	rdAddr = PA_TRIM_START_ADDR2;
	for(idx=0 ; idx < PA_TRIM_BLOCK_SIZE; idx++)
	{
		RT28xx_EEPROM_READ16(pAd,rdAddr, *pPATrimData);
		pPATrimData ++;
		rdAddr +=2; 
	}

    for (i = 0; i < (PA_TRIM_SIZE/sizeof(UINT32)); i++)
    {
        MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_OFF, 
        ("\x1b[32m%s: WF%d = 0x%08x \x1b[m\n", __FUNCTION__, i, PATrimCtrl.u4Data[i]));
    }    
     
    msg = MtAndesAllocCmdMsg(pAd, sizeof(PATrimCtrl));
    if (!msg)
    {
        Status = NDIS_STATUS_RESOURCES;
        goto error;
    }
    
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CAL_RESTORE_FROM_FILE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 60000);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);

#ifdef RT_BIG_ENDIAN
	PATrimCtrl.Header.u4DataLen = cpu2le32(PATrimCtrl.Header.u4DataLen);
	RTMPEndianChange(PATrimCtrl.u4Data, sizeof(PATrimCtrl.u4Data));
#endif

    MtAndesAppendCmdMsg(msg, (char *)&PATrimCtrl, sizeof(PATrimCtrl));
    

    Status  = pAd->chipOps.MtCmdTx(pAd,msg);
        
error:
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
    ("%s:(Status = %d)\n", __FUNCTION__, Status));
    
    MTWF_LOG(DBG_CAT_ALL, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
    ("%s<-----------------\n",__FUNCTION__));           

    return Status;
}
#endif /* PA_TRIM_SUPPORT */

INT32 MtCmdThermalReCalMode(RTMP_ADAPTER *pAd, UINT8 Mode)
{
	struct cmd_msg *msg;
	EXT_CMD_THERMAL_RECAL_MODE_CTRL_T ThermalReCalCtrl;
	INT32 ret = 0;
	
	struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE, 
	("%s----------------->\n",__FUNCTION__));

	ThermalReCalCtrl.ucMode= Mode;
     
	msg = MtAndesAllocCmdMsg(pAd, sizeof(ThermalReCalCtrl));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
    
	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_THERMAL_RECAL_MODE);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 60000);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

	MtAndesInitCmdMsg(msg, attr);

	MtAndesAppendCmdMsg(msg, (char *)&ThermalReCalCtrl, sizeof(ThermalReCalCtrl));
    
	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

	return ret;
        
error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, 
	("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

static VOID CmdWifiHifCtrlRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	struct _EVENT_EXT_CMD_RESULT_T *EventExtCmdResult =
                                    (struct _EVENT_EXT_CMD_RESULT_T *)Data;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.ucExTenCID = 0x%x\n",
            __FUNCTION__, EventExtCmdResult->ucExTenCID));
	EventExtCmdResult->u4Status = le2cpu32(EventExtCmdResult->u4Status);
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("%s: EventExtCmdResult.u4Status = 0x%x\n",
            __FUNCTION__, EventExtCmdResult->u4Status));

	os_move_mem(msg->attr.rsp.wb_buf_in_calbk, Data, Len);
}

INT32 MtCmdWifiHifCtrl(RTMP_ADAPTER *ad, UINT8 ucDbdcIdx, UINT8 ucHifCtrlId, VOID *pRsult)
{
	struct cmd_msg *msg;
	int ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	if (IS_MT7636(ad) || IS_MT7637(ad) || IS_MT7615(ad))
	{
		EXT_CMD_WIFI_HIF_CTRL_T	rCmdWifiHifCtrl = {0};

		msg = MtAndesAllocCmdMsg(ad, sizeof(EXT_CMD_WIFI_HIF_CTRL_T));
		if (!msg)
		{
			ret = NDIS_STATUS_RESOURCES;
			goto error;
		}

        SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
        SET_CMD_ATTR_TYPE(attr, EXT_CID);
        SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_WIFI_HIF_CTRL);
        SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
        SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
        SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
        SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, pRsult);
        SET_CMD_ATTR_RSP_HANDLER(attr, CmdWifiHifCtrlRsp);

        MtAndesInitCmdMsg(msg, attr);
		/* Need to conside eidden */
		/* Wifi Hif control ID */
		rCmdWifiHifCtrl.ucHifCtrlId = ucHifCtrlId;
		rCmdWifiHifCtrl.ucDbdcIdx = ucDbdcIdx;
		MtAndesAppendCmdMsg(msg, (char *)&rCmdWifiHifCtrl,
                        sizeof(EXT_CMD_WIFI_HIF_CTRL_T));

		ret = ad->chipOps.MtCmdTx(ad, msg);

error:
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
		return ret;
	}
	else
	{
		return ret;
	}
}


/*****************************************
	FW loading
******************************************/
NTSTATUS MtCmdPowerOnWiFiSys(RTMP_ADAPTER *pAd)
{
	NTSTATUS status = 0;
#if defined(RTMP_PCI_SUPPORT) || defined(MTK_UART_SUPPORT)
#elif defined(RTMP_USB_SUPPORT)
	status = os_usb_vendor_req(
		pAd,
		USBD_TRANSFER_DIRECTION_OUT,
		DEVICE_VENDOR_REQUEST_OUT,
		0x04,
		0,
		0x01,
		NULL,
		0);
#endif

	return status;
}


VOID CmdExtEventRsp(struct cmd_msg *msg, char *Data, UINT16 Len)
{
	INT i;
    UINT8 *pPayload = Data;
    UINT16 u2PayloadLen = Len;

    /* print event raw data */
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("CmdEID=0x%x, EVENT[%d] = ", msg->attr.ext_type, u2PayloadLen));
    for (i = 0; i < u2PayloadLen; i++)
    {
    	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
                                ("0x%x ", pPayload[i]));
    }
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF, ("\n"));
}

INT32 MtCmdSendRaw(RTMP_ADAPTER *pAd, UCHAR ExtendID, UCHAR *Input,
                                        INT len, UCHAR SetQuery)
{
	BOOLEAN ret = NDIS_STATUS_SUCCESS;
	struct cmd_msg *msg;
    struct _CMD_ATTRIBUTE attr = {0};

    /* send cmd to fw */
	msg = MtAndesAllocCmdMsg(pAd, len);
	if (!msg)
    {
	    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
            ("%s AndesAllocCmdMsg error !!! \n", __FUNCTION__));
		return NDIS_STATUS_RESOURCES;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, ExtendID);
    SET_CMD_ATTR_CTRL_FLAGS(attr, (SetQuery) ?
            INIT_CMD_SET_AND_WAIT_RSP : INIT_CMD_QUERY_AND_WAIT_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, CmdExtEventRsp);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)Input, len);

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

	return ret;
}

#ifdef MT_DFS_SUPPORT
//Remember add a RDM compiler flag - Jelly20150205
INT32 MtCmdRddCtrl(
    IN struct _RTMP_ADAPTER *pAd,
    IN UCHAR ucRddCtrl,
    IN UCHAR ucRddIdex,
    IN UCHAR ucRddInSel)
{
    struct cmd_msg *msg;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    EXT_CMD_RDD_ON_OFF_CTRL_T rRddOnOffCtrl;
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
                ("[MtCmdRddCtrl] dispath CMD start\n"));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(rRddOnOffCtrl));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_RDD_ON_OFF_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
    os_zero_mem(&rRddOnOffCtrl, sizeof(rRddOnOffCtrl));
    rRddOnOffCtrl.ucRddCtrl = ucRddCtrl;
    rRddOnOffCtrl.ucRddIdx = ucRddIdex;
	rRddOnOffCtrl.ucRddInSel = ucRddInSel;
    MtAndesAppendCmdMsg(msg, (char *)&rRddOnOffCtrl, sizeof(rRddOnOffCtrl));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("[MtCmdRddCtrl] dispath CMD complete\n"));
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
            ("[MtCmdRddCtrl] ret = %d\n", ret));
error:
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
                ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;
}
#endif

#ifdef BACKGROUND_SCAN_SUPPORT
INT32 MtCmdBgndScan(RTMP_ADAPTER *pAd, MT_BGND_SCAN_CFG BgScCfg)
{
	struct cmd_msg *msg;
	struct _EXT_CMD_CHAN_SWITCH_T CmdChanSwitch;
	INT32 ret = 0,i=0;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif
	if (BgScCfg.CentralChannel== 0)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("%s: central channel = 0 is invalid\n", __FUNCTION__));
		return -1;
	}


	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: control_ch = %d, central_chl = %d, BW = %d,TXStream = %d, RXPath = %d, BandIdx = %d, Reason(%d)\n", __FUNCTION__,
	BgScCfg.ControlChannel, BgScCfg.CentralChannel, BgScCfg.Bw, BgScCfg.TxStream, BgScCfg.RxPath, BgScCfg.BandIdx, BgScCfg.Reason));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdChanSwitch));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_SET_RX_PATH);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
#else


	MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_SET_RX_PATH/*EXT_CMD_CHANNEL_SWITCH*/,TRUE, 0,
							TRUE, TRUE, 8, NULL, EventExtCmdResult);
#endif
	os_zero_mem(&CmdChanSwitch, sizeof(CmdChanSwitch));

	CmdChanSwitch.ucPrimCh = BgScCfg.ControlChannel;
	CmdChanSwitch.ucCentralCh = BgScCfg.CentralChannel;
	CmdChanSwitch.ucTxStreamNum = BgScCfg.TxStream;
	CmdChanSwitch.ucRxStreamNum = BgScCfg.RxPath;//Rx Path
	CmdChanSwitch.ucDbdcIdx = BgScCfg.BandIdx;
	CmdChanSwitch.ucBW = GetCfgBw2RawBw(BgScCfg.Bw);
	CmdChanSwitch.ucSwitchReason = BgScCfg.Reason;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: control_ch = %d, central_chl = %d, BW = %d,TXStream = %d, RXStream = %d, BandIdx=%d, Reason(%d)\n", __FUNCTION__,
	CmdChanSwitch.ucPrimCh, CmdChanSwitch.ucCentralCh, CmdChanSwitch.ucBW, CmdChanSwitch.ucTxStreamNum, CmdChanSwitch.ucRxStreamNum, CmdChanSwitch.ucDbdcIdx, CmdChanSwitch.ucSwitchReason));

	for(i=0;i<SKU_SIZE;i++)
	{
		CmdChanSwitch.aucTxPowerSKU[i]=0xff;
	}

	MtAndesAppendCmdMsg(msg, (char *)&CmdChanSwitch, sizeof(CmdChanSwitch));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;
}

INT32 MtCmdBgndScanNotify(RTMP_ADAPTER *pAd, MT_BGND_SCAN_NOTIFY BgScNotify)
{
	INT32 ret = 0;
	struct cmd_msg *msg;
	struct _EXT_CMD_BGND_SCAN_NOTIFY_T CmdBgndScNotify;
#if (NEW_MCU_INIT_CMD_API)
    struct _CMD_ATTRIBUTE attr = {0};
#endif
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: NotifyFunc = %d, BgndScanStatus = %d\n", __FUNCTION__,
	BgScNotify.NotifyFunc, BgScNotify.BgndScanStatus));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdBgndScNotify));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
#if (NEW_MCU_INIT_CMD_API)
    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BGND_SCAN_NOTIFY);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RETRY_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);

    MtAndesInitCmdMsg(msg, attr);
#else
	MtAndesInitCmdMsg(msg, HOST2N9, EXT_CID, CMD_SET, EXT_CMD_ID_BGND_SCAN_NOTIFY,TRUE, 0,
							TRUE, TRUE, 8, NULL, EventExtCmdResult);
#endif
	os_zero_mem(&CmdBgndScNotify, sizeof(CmdBgndScNotify));

	CmdBgndScNotify.ucNotifyFunc = BgScNotify.NotifyFunc;
	CmdBgndScNotify.ucBgndScanStatus = BgScNotify.BgndScanStatus;

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE, ("%s: ucNotifyFunc = %d, ucBgndScanStatus = %d\n", __FUNCTION__,
	CmdBgndScNotify.ucNotifyFunc, CmdBgndScNotify.ucBgndScanStatus ));

	MtAndesAppendCmdMsg(msg, (char *)&CmdBgndScNotify, sizeof(CmdBgndScNotify));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO, ("%s:(ret = %d)\n", __FUNCTION__, ret));
	return ret;

}
#endif /* BACKGROUND_SCAN_SUPPORT */



INT32 MtCmdCr4Query(RTMP_ADAPTER *pAd, UINT32 arg0, UINT32 arg1, UINT32 arg2)
{

	struct cmd_msg *msg;
	INT32 Ret = 0;
	struct _EXT_CMD_CR4_QUERY_T  CmdCr4SetQuery;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
		        (":%s: option(%d)\n", __FUNCTION__, arg0));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdCr4SetQuery));
   	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		return Ret;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
    SET_CMD_ATTR_TYPE(attr, INIT_CMD_ID_CR4);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CR4_QUERY);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    os_zero_mem(&CmdCr4SetQuery, sizeof(CmdCr4SetQuery));

    CmdCr4SetQuery.u4Cr4QueryOptionArg0 = cpu2le32(arg0);
    CmdCr4SetQuery.u4Cr4QueryOptionArg1 = cpu2le32(arg1);
    CmdCr4SetQuery.u4Cr4QueryOptionArg2 = cpu2le32(arg2);

	MtAndesAppendCmdMsg(msg, (char *)&CmdCr4SetQuery,
									sizeof(CmdCr4SetQuery));

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		    ("%s: (ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}

INT32 MtCmdCr4QueryBssAcQPktNum (
    struct _RTMP_ADAPTER *pAd, 
    UINT32 u4bssbitmap)
{

	struct cmd_msg *msg;
	INT32 Ret = 0;
	struct _EXT_CMD_CR4_QUERY_T  CmdCr4SetQuery;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		        (":%s: u4bssbitmap(0x%08X)\n", __FUNCTION__, u4bssbitmap));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdCr4SetQuery));
   	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		return Ret;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, INIT_CMD_ID_CR4);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CR4_QUERY);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_QUERY);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

	MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&CmdCr4SetQuery, sizeof(CmdCr4SetQuery));

	CmdCr4SetQuery.u4Cr4QueryOptionArg0 = CR4_QUERY_OPTION_GET_BSS_ACQ_PKT_NUM;
	CmdCr4SetQuery.u4Cr4QueryOptionArg1 = cpu2le32(u4bssbitmap);
	CmdCr4SetQuery.u4Cr4QueryOptionArg2 = 0;
#ifdef RT_BIG_ENDIAN
	CmdCr4SetQuery.u4Cr4QueryOptionArg0 = cpu2le32(CmdCr4SetQuery.u4Cr4QueryOptionArg0);
	CmdCr4SetQuery.u4Cr4QueryOptionArg2 = cpu2le32(CmdCr4SetQuery.u4Cr4QueryOptionArg2);
#endif
	MtAndesAppendCmdMsg(msg, (char *)&CmdCr4SetQuery, sizeof(CmdCr4SetQuery));

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		    ("%s: (ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}

INT32 MtCmdCr4Set(RTMP_ADAPTER *pAd, UINT32 arg0, UINT32 arg1, UINT32 arg2)
{

	struct cmd_msg *msg;
	INT32 Ret = 0;
	struct _EXT_CMD_CR4_SET_T  CmdCr4SetSet;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
		        (":%s: arg0(%d) arg1(%d) arg2(%d)\n",
		            __FUNCTION__, arg0, arg1, arg2));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdCr4SetSet));
   	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		return Ret;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
    SET_CMD_ATTR_TYPE(attr, INIT_CMD_ID_CR4);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CR4_SET);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    os_zero_mem(&CmdCr4SetSet, sizeof(CmdCr4SetSet));

	CmdCr4SetSet.u4Cr4SetArg0 = cpu2le32(arg0);
    CmdCr4SetSet.u4Cr4SetArg1 = cpu2le32(arg1);
    CmdCr4SetSet.u4Cr4SetArg2 = cpu2le32(arg2);

	MtAndesAppendCmdMsg(msg, (char *)&CmdCr4SetSet, sizeof(CmdCr4SetSet));

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		    ("%s: (ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}

INT32 MtCmdCr4Capability(RTMP_ADAPTER *pAd, UINT32 option)
{

	struct cmd_msg *msg;
	INT32 Ret = 0;
	struct _EXT_CMD_CR4_CAPABILITY_T CmdCr4SetCapability;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
		        (":%s: option(%d)\n", __FUNCTION__, option));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdCr4SetCapability));
   	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		return Ret;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
    SET_CMD_ATTR_TYPE(attr, INIT_CMD_ID_CR4);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CR4_CAPABILITY);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    os_zero_mem(&CmdCr4SetCapability, sizeof(CmdCr4SetCapability));

	CmdCr4SetCapability.u4Cr4Capability = cpu2le32(option);

	MtAndesAppendCmdMsg(msg, (char *)&CmdCr4SetCapability,
                            sizeof(CmdCr4SetCapability));

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		    ("%s: (ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}


INT32 MtCmdCr4Debug(RTMP_ADAPTER *pAd, UINT32 option)
{

	struct cmd_msg *msg;
	INT32 Ret = 0;
	struct _EXT_CMD_CR4_DEBUG_T CmdCr4SetDebug;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
		        (":%s: option(%d)\n", __FUNCTION__, option));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CmdCr4SetDebug));
   	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		return Ret;
	}

    SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
    SET_CMD_ATTR_TYPE(attr, INIT_CMD_ID_CR4);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CR4_DEBUG);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    os_zero_mem(&CmdCr4SetDebug, sizeof(CmdCr4SetDebug));

	CmdCr4SetDebug.u4Cr4Debug = cpu2le32(option);

	MtAndesAppendCmdMsg(msg, (char *)&CmdCr4SetDebug,
                            sizeof(CmdCr4SetDebug));

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
		    ("%s: (ret = %d)\n", __FUNCTION__, Ret));
	return Ret;
}

INT32 MtCmdMUPowerCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      MUPowerForce,
          UCHAR        MUPowerCtrl,
          UCHAR        BandIdx
          )
{
    struct cmd_msg *msg;
    CMD_POWER_MU_CTRL_T rMUPowerCtrl;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: MUPowerForce: %d, MUPowerCtrl: %d, BandIdx: %d \n", __FUNCTION__, MUPowerForce, MUPowerCtrl, BandIdx));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_MU_CTRL_T));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    os_zero_mem(&rMUPowerCtrl, sizeof(CMD_POWER_MU_CTRL_T));

    rMUPowerCtrl.ucPowerCtrlFormatId = MU_TX_POWER_CTRL;
    rMUPowerCtrl.fgMUPowerForceMode  = MUPowerForce;
    rMUPowerCtrl.cMUPower            = MUPowerCtrl;
    rMUPowerCtrl.ucBandIdx           = BandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)&rMUPowerCtrl,
            sizeof(CMD_POWER_MU_CTRL_T));

    ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}

INT32 MtCmdTxPowerSKUCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgTxPowerSKUEn,
          UCHAR        BandIdx
          )
{
	struct cmd_msg *msg;
	CMD_POWER_SKU_CTRL_T PowerSKUCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: fgTxPowerSKUEn: %d, BandIdx: %d\n", __FUNCTION__, fgTxPowerSKUEn, BandIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_SKU_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&PowerSKUCtrl, sizeof(CMD_POWER_SKU_CTRL_T));

	PowerSKUCtrl.ucPowerCtrlFormatId = SKU_FEATURE_CTRL;
	PowerSKUCtrl.ucSKUEnable         = fgTxPowerSKUEn;
    PowerSKUCtrl.ucBandIdx           = BandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&PowerSKUCtrl,
            sizeof(CMD_POWER_SKU_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdTxPowerPercentCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgTxPowerPercentEn,
          UCHAR        BandIdx
          )
{
	struct cmd_msg *msg;
	CMD_POWER_PERCENTAGE_CTRL_T PowerPercentCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: fgTxPowerPercentEn: %d, BandIdx: %d\n", __FUNCTION__, fgTxPowerPercentEn, BandIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_PERCENTAGE_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&PowerPercentCtrl, sizeof(CMD_POWER_PERCENTAGE_CTRL_T));

	PowerPercentCtrl.ucPowerCtrlFormatId = PERCENTAGE_FEATURE_CTRL;
	PowerPercentCtrl.ucPercentageEnable  = fgTxPowerPercentEn;
    PowerPercentCtrl.ucBandIdx           = BandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&PowerPercentCtrl,
            sizeof(CMD_POWER_PERCENTAGE_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdTxBfBackoffCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgTxBFBackoffEn,
          UCHAR        BandIdx
          )
{
    struct cmd_msg *msg;
    CMD_POWER_BF_BACKOFF_CTRL_T TxBFBackoffCtrl;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: fgTxBFBackoffEn: %d, BandIdx: %d\n", __FUNCTION__, fgTxBFBackoffEn, BandIdx));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_BF_BACKOFF_CTRL_T));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    os_zero_mem(&TxBFBackoffCtrl, sizeof(CMD_POWER_BF_BACKOFF_CTRL_T));

    TxBFBackoffCtrl.ucPowerCtrlFormatId = BF_POWER_BACKOFF_FEATURE_CTRL;
    TxBFBackoffCtrl.ucBFBackoffEnable   = fgTxBFBackoffEn;
    TxBFBackoffCtrl.ucBandIdx           = BandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)&TxBFBackoffCtrl,
            sizeof(CMD_POWER_BF_BACKOFF_CTRL_T));

    ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}

INT32 MtCmdTxPwrUppBoundCtrl(
          RTMP_ADAPTER *pAd,
          UCHAR        PwrUppBoundCtrl)
{
	struct cmd_msg *msg;
	CMD_POWER_UPPER_BOUND_CTRL_T PowerUpperBoundCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: PwrUppBoundCtrl = %d\n", __FUNCTION__, PwrUppBoundCtrl));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_UPPER_BOUND_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&PowerUpperBoundCtrl, sizeof(CMD_POWER_UPPER_BOUND_CTRL_T));

	PowerUpperBoundCtrl.ucPowerCtrlFormatId = POWER_UPPER_BOUND_CTRL;
	PowerUpperBoundCtrl.ucUpperBoundCtrl  = PwrUppBoundCtrl;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&PowerUpperBoundCtrl,
            sizeof(CMD_POWER_UPPER_BOUND_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdTxPwrRfTxAntCtrl(
          RTMP_ADAPTER *pAd,
          UCHAR        TxAntCtrlEn,
          UCHAR        WIFI_En_0,
          UCHAR        WIFI_En_1,
          UCHAR        WIFI_En_2,
          UCHAR        WIFI_En_3)
{
	struct cmd_msg *msg;
	CMD_POWER_RF_TXANT_CTRL_T PowerRfTxAntCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: TxAntCtrlEn = %d \n", __FUNCTION__, TxAntCtrlEn));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: WIFI_En[0:1:2:3] = %d:%d:%d:%d \n", __FUNCTION__, WIFI_En_0, WIFI_En_1, WIFI_En_2, WIFI_En_3));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_RF_TXANT_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&PowerRfTxAntCtrl, sizeof(CMD_POWER_RF_TXANT_CTRL_T));

	PowerRfTxAntCtrl.ucPowerCtrlFormatId = RF_TXANT_CTRL;
    PowerRfTxAntCtrl.ucTxAntCtrlEn       = TxAntCtrlEn;
	PowerRfTxAntCtrl.ucWIFI_EN_0         = WIFI_En_0;
    PowerRfTxAntCtrl.ucWIFI_EN_1         = WIFI_En_1;
    PowerRfTxAntCtrl.ucWIFI_EN_2         = WIFI_En_2;
    PowerRfTxAntCtrl.ucWIFI_EN_3         = WIFI_En_3;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&PowerRfTxAntCtrl,
            sizeof(CMD_POWER_RF_TXANT_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdTxPwrShowInfo(
          RTMP_ADAPTER *pAd,
          UCHAR        TxPowerInfoEn)
{
	struct cmd_msg *msg;
	CMD_TX_POWER_SHOW_INFO_T TxPowerShowInfoCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: TxPowerInfoEn = %d \n", __FUNCTION__, TxPowerInfoEn));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_TX_POWER_SHOW_INFO_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&TxPowerShowInfoCtrl, sizeof(CMD_TX_POWER_SHOW_INFO_T));

	TxPowerShowInfoCtrl.ucPowerCtrlFormatId = TX_POWER_SHOW_INFO;
    TxPowerShowInfoCtrl.ucTxPowerInfoEn     = TxPowerInfoEn;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&TxPowerShowInfoCtrl,
            sizeof(CMD_TX_POWER_SHOW_INFO_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdTOAECalCtrl(
          RTMP_ADAPTER *pAd,
          UCHAR        TOAECtrl)
{
	struct cmd_msg *msg;
	CMD_TOAE_ON_OFF_CTRL TOAECalCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: TOAECtrl = %d \n", __FUNCTION__, TOAECtrl));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_TOAE_ON_OFF_CTRL));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&TOAECalCtrl, sizeof(CMD_TOAE_ON_OFF_CTRL));

    TOAECalCtrl.fgTOAEEnable = TOAECtrl;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TOAE_ENABLE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&TOAECalCtrl,
            sizeof(CMD_TOAE_ON_OFF_CTRL));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdEDCCACtrl(
          RTMP_ADAPTER *pAd,
          UCHAR        BandIdx,
          UCHAR        EDCCACtrl)
{
	struct cmd_msg *msg;
	CMD_EDCCA_ON_OFF_CTRL rEDCCACtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: BandIdx: %d, EDCCACtrl: %d \n", __FUNCTION__, BandIdx, EDCCACtrl));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_EDCCA_ON_OFF_CTRL));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&rEDCCACtrl, sizeof(CMD_EDCCA_ON_OFF_CTRL));

    rEDCCACtrl.fgEDCCAEnable = EDCCACtrl;
    rEDCCACtrl.ucDbdcBandIdx = BandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_EDCCA_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&rEDCCACtrl,
            sizeof(CMD_EDCCA_ON_OFF_CTRL));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdBFNDPATxDCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgNDPA_ManualMode,
          UINT8        ucNDPA_TxMode,
          UINT8        ucNDPA_Rate,
          UINT8        ucNDPA_BW,
          UINT8        ucNDPA_PowerOffset)
{
	struct cmd_msg *msg;
	CMD_BF_NDPA_TXD_CTRL_T rBFNDPATxDCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: fgNDPA_ManualMode: %d, ucNDPA_TxMode: %d, ucNDPA_Rate: %d, ucNDPA_BW: %d, ucNDPA_PowerOffset: %d \n", __FUNCTION__, fgNDPA_ManualMode, ucNDPA_TxMode, ucNDPA_Rate, ucNDPA_BW, ucNDPA_PowerOffset));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_BF_NDPA_TXD_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&rBFNDPATxDCtrl, sizeof(CMD_BF_NDPA_TXD_CTRL_T));

    rBFNDPATxDCtrl.ucPowerCtrlFormatId = BF_NDPA_TXD_CTRL;
    rBFNDPATxDCtrl.fgNDPA_ManualMode   = fgNDPA_ManualMode;
    rBFNDPATxDCtrl.ucNDPA_TxMode       = ucNDPA_TxMode;
    rBFNDPATxDCtrl.ucNDPA_Rate         = ucNDPA_Rate;
    rBFNDPATxDCtrl.ucNDPA_BW           = ucNDPA_BW;
    rBFNDPATxDCtrl.ucNDPA_PowerOffset  = ucNDPA_PowerOffset;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&rBFNDPATxDCtrl,
            sizeof(CMD_BF_NDPA_TXD_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtEPAcheck(RTMP_ADAPTER *pAd)
{
    struct cmd_msg *msg;
    CMD_SET_TSSI_TRAINING_T PA;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_SET_TSSI_TRAINING_T));

    if (!msg) {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&PA, sizeof(PA));

    PA.ucPowerCtrlFormatId = TSSI_WORKAROUND;
    PA.ucSubFuncId         = EPA_STATUS;

    MtAndesAppendCmdMsg(msg, (char *)&PA, sizeof(PA));
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

    error:
    MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;	
}

INT32 MtATETSSITracking(RTMP_ADAPTER *pAd, BOOLEAN fgEnable)
{
    struct cmd_msg *msg;
    CMD_SET_TSSI_TRAINING_T rTSSITracking;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s: fgEnable: %d \n", __FUNCTION__, fgEnable));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_SET_TSSI_TRAINING_T));

    if (!msg) {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&rTSSITracking, sizeof(rTSSITracking));

    rTSSITracking.ucPowerCtrlFormatId = TSSI_WORKAROUND;
    rTSSITracking.ucSubFuncId         = TSSI_TRACKING_ENABLE;
    rTSSITracking.fgEnable            = fgEnable;

    MtAndesAppendCmdMsg(msg, (char *)&rTSSITracking, sizeof(rTSSITracking));
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

    error:
    MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;	
}

INT32 MtATEFCBWCfg(RTMP_ADAPTER *pAd, BOOLEAN fgEnable)
{
    struct cmd_msg *msg;
    CMD_SET_TSSI_TRAINING_T rFCBWEnable;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s: fgEnable: %d \n", __FUNCTION__, fgEnable));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_SET_TSSI_TRAINING_T));

    if (!msg) {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&rFCBWEnable, sizeof(rFCBWEnable));

    rFCBWEnable.ucPowerCtrlFormatId = TSSI_WORKAROUND;
    rFCBWEnable.ucSubFuncId         = FCBW_ENABLE;
    rFCBWEnable.fgEnable            = fgEnable;

    MtAndesAppendCmdMsg(msg, (char *)&rFCBWEnable, sizeof(rFCBWEnable));
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

    error:
    MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;	
}

INT32 MtTSSICompBackup(RTMP_ADAPTER *pAd, BOOLEAN fgEnable)
{
    struct cmd_msg *msg;
    CMD_SET_TSSI_TRAINING_T rTSSICompBackup;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s: fgEnable: %d \n", __FUNCTION__, fgEnable));

    msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_SET_TSSI_TRAINING_T));

    if (!msg) {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&rTSSICompBackup, sizeof(rTSSICompBackup));

    rTSSICompBackup.ucPowerCtrlFormatId = TSSI_WORKAROUND;
    rTSSICompBackup.ucSubFuncId         = TSSI_COMP_BACKUP;
    rTSSICompBackup.fgEnable            = fgEnable;

    MtAndesAppendCmdMsg(msg, (char *)&rTSSICompBackup, sizeof(rTSSICompBackup));
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

    error:
    MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;	
}

INT32 MtTSSICompCfg(RTMP_ADAPTER *pAd)
{
    struct cmd_msg *msg;
    CMD_SET_TSSI_TRAINING_T rTSSICompCfg;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_SET_TSSI_TRAINING_T));

    if (!msg) {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    NdisZeroMemory(&rTSSICompCfg, sizeof(rTSSICompCfg));

    rTSSICompCfg.ucPowerCtrlFormatId = TSSI_WORKAROUND;
    rTSSICompCfg.ucSubFuncId         = TSSI_COMP_CONFIG;

    MtAndesAppendCmdMsg(msg, (char *)&rTSSICompCfg, sizeof(rTSSICompCfg));
    ret = pAd->chipOps.MtCmdTx(pAd,msg);

    error:
    MTWF_LOG(DBG_CAT_TEST, DBG_SUBCAT_ALL, DBG_LVL_INFO,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));
    return ret;	
}

INT32 MtCmdTemperatureCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgManualMode,
          CHAR         cTemperature)
{
	struct cmd_msg *msg;
	CMD_POWER_TEMPERATURE_CTRL_T TemperatureCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: fgManualMode: %d \n", __FUNCTION__, fgManualMode));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
        ("%s: cTemperature: %d \n", __FUNCTION__, cTemperature));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_TEMPERATURE_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&TemperatureCtrl, sizeof(CMD_POWER_TEMPERATURE_CTRL_T));

	TemperatureCtrl.ucPowerCtrlFormatId = THERMAL_MANUAL_CTRL;
    TemperatureCtrl.fgManualMode        = fgManualMode;
	TemperatureCtrl.cTemperature        = cTemperature;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&TemperatureCtrl,
            sizeof(CMD_POWER_TEMPERATURE_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

#ifdef NR_PD_DETECTION
INT32 MtCmdLinkTestTxCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgTxConfigEn)
{
	struct cmd_msg *msg;
	CMD_CMW270_TX_CTRL_T CMW270TxCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgTxConfigEn = %d\n", __FUNCTION__, fgTxConfigEn));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_CMW270_TX_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CMW270TxCtrl, sizeof(CMD_CMW270_TX_CTRL_T));

	CMW270TxCtrl.ucCMW270CtrlFormatId = CMW270_TX;
	CMW270TxCtrl.fgTxConfigEn         = fgTxConfigEn;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CMW270_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CMW270TxCtrl,
            sizeof(CMD_CMW270_TX_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);
    return ret;

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdLinkTestRxCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgRxConfigEn,
          UINT8        ucRxAntIdx)
{
	struct cmd_msg *msg;
	CMD_CMW270_RX_CTRL_T CMW270RxCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgRxConfigEn = %d, ucRxAntIdx = %d\n", __FUNCTION__, fgRxConfigEn, ucRxAntIdx));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_CMW270_RX_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CMW270RxCtrl, sizeof(CMD_CMW270_RX_CTRL_T));

	CMW270RxCtrl.ucCMW270CtrlFormatId = CMW270_RX;
	CMW270RxCtrl.fgRxConfigEn         = fgRxConfigEn;
    CMW270RxCtrl.ucRxAntIdx           = ucRxAntIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CMW270_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CMW270RxCtrl,
            sizeof(CMD_CMW270_RX_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);
    return ret;

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdLinkTestTxPwrCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgTxPwrConfigEn,
          UINT8        ucDbdcBandIdx)
{
	struct cmd_msg *msg;
	CMD_CMW270_TXPWR_CTRL_T CMW270TxPwrCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgTxPwrConfigEn = %d\n", __FUNCTION__, fgTxPwrConfigEn));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_CMW270_TXPWR_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CMW270TxPwrCtrl, sizeof(CMD_CMW270_TXPWR_CTRL_T));

	CMW270TxPwrCtrl.ucCMW270CtrlFormatId = CMW270_TXPWR;
	CMW270TxPwrCtrl.fgTxPwrConfigEn      = fgTxPwrConfigEn;
    CMW270TxPwrCtrl.ucDbdcBandIdx        = ucDbdcBandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CMW270_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CMW270TxPwrCtrl,
            sizeof(CMD_CMW270_TXPWR_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);
    return ret;

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdLinkTestACICtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgACIConfigEn,
          UINT8        ucDbdcBandIdx)
{
	struct cmd_msg *msg;
	CMD_CMW270_ACI_CTRL_T CMW270ACICtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgACIConfigEn = %d\n", __FUNCTION__, fgACIConfigEn));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_CMW270_ACI_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CMW270ACICtrl, sizeof(CMD_CMW270_ACI_CTRL_T));

	CMW270ACICtrl.ucCMW270CtrlFormatId = CMW270_ACI;
	CMW270ACICtrl.fgACIConfigEn        = fgACIConfigEn;
    CMW270ACICtrl.ucDbdcBandIdx        = ucDbdcBandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CMW270_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CMW270ACICtrl,
            sizeof(CMD_CMW270_ACI_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);
    return ret;

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdLinkTestRCPICtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgRCPIConfigEn)
{
	struct cmd_msg *msg;
	CMD_CMW270_RCPI_CTRL_T CMW270RCPICtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgRCPIConfigEn = %d\n", __FUNCTION__, fgRCPIConfigEn));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_CMW270_RCPI_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CMW270RCPICtrl, sizeof(CMD_CMW270_RCPI_CTRL_T));

	CMW270RCPICtrl.ucCMW270CtrlFormatId = CMW270_RCPI;
	CMW270RCPICtrl.fgRCPIConfigEn       = fgRCPIConfigEn;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CMW270_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CMW270RCPICtrl,
            sizeof(CMD_CMW270_RCPI_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);
    return ret;

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdLinkTestCSDCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgCSDConfigEn,
          UINT8        ucDbdcBandIdx)
{
	struct cmd_msg *msg;
	CMD_CMW270_CSD_CTRL_T CMW270CSDCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgCSDConfigEn = %d\n", __FUNCTION__, fgCSDConfigEn));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_CMW270_CSD_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CMW270CSDCtrl, sizeof(CMD_CMW270_CSD_CTRL_T));

	CMW270CSDCtrl.ucCMW270CtrlFormatId = CMW270_CSD;
	CMW270CSDCtrl.fgCSDConfigEn        = fgCSDConfigEn;
    CMW270CSDCtrl.ucDbdcBandIdx        = ucDbdcBandIdx;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CMW270_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CMW270CSDCtrl,
            sizeof(CMD_CMW270_CSD_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);
    return ret;

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

INT32 MtCmdLinkTestSeIdxCtrl(
          RTMP_ADAPTER *pAd,
          BOOLEAN      fgSeIdxConfigEn)
{
	struct cmd_msg *msg;
	CMD_CMW270_SEIDX_CTRL_T CMW270SeIdxCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: fgSeIdxConfigEn = %d\n", __FUNCTION__, fgSeIdxConfigEn));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_CMW270_SEIDX_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CMW270SeIdxCtrl, sizeof(CMD_CMW270_SEIDX_CTRL_T));

	CMW270SeIdxCtrl.ucCMW270CtrlFormatId = CMW270_SEIDX;
	CMW270SeIdxCtrl.fgSeIdxConfigEn      = fgSeIdxConfigEn;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_CMW270_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CMW270SeIdxCtrl,
            sizeof(CMD_CMW270_SEIDX_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);
    return ret;

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}
#endif /* NR_PD_DETECTION */

#ifdef GREENAP_SUPPORT
INT32 MtCmdExtGreenAPOnOffCtrl(
          RTMP_ADAPTER *pAd,
          MT_GREENAP_CTRL_T GreenAPCtrl)
{
    struct cmd_msg *msg;
    EXT_CMD_GREENAP_CTRL_T rGreenAPCtrl;
    INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_GREENAP_CTRL_T));

    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    os_zero_mem(&rGreenAPCtrl, sizeof(EXT_CMD_GREENAP_CTRL_T));

    rGreenAPCtrl.ucDbdcIdx = GreenAPCtrl.ucDbdcIdx;
    rGreenAPCtrl.ucGreenAPOn = GreenAPCtrl.ucGreenAPOn;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_GREENAP_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    MtAndesAppendCmdMsg(msg, (char *)&rGreenAPCtrl, sizeof(EXT_CMD_GREENAP_CTRL_T));

    ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}
#endif /* GREENAP_SUPPORT */

#ifdef TPC_SUPPORT
INT32 MtCmdTpcFeatureCtrl(
          RTMP_ADAPTER *pAd,
          INT8 TpcPowerValue,
          UINT8 BandIdx,
          UINT8 CentralChannel)
{
	struct cmd_msg *msg;
	CMD_POWER_TPC_CTRL_T TpcMaxPwrCtrl;
	INT32 ret = 0;
	struct _CMD_ATTRIBUTE attr = {0};

	if (CentralChannel == 0)
	{
		MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR, ("(X) invalid Channel setting\n"));
		ret = NDIS_STATUS_INVALID_DATA;
		goto error;
	}

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_POWER_TPC_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&TpcMaxPwrCtrl, sizeof(CMD_POWER_TPC_CTRL_T));

	TpcMaxPwrCtrl.ucPowerCtrlFormatId = TPC_FEATURE_CTRL;
	TpcMaxPwrCtrl.cTPCPowerValue = TpcPowerValue;
	TpcMaxPwrCtrl.ucBand = BandIdx;
	TpcMaxPwrCtrl.ucCentralChannel = CentralChannel;
	TpcMaxPwrCtrl.ucChannelBand = TxPowerGetChBand(BandIdx, CentralChannel);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
		("%s: pwr=%d=0x%02X, BandIdx=%d, CentralChannel=%d, ChBand=%d\n", __FUNCTION__, 
		TpcPowerValue, TpcPowerValue, TpcMaxPwrCtrl.ucBand, 
		TpcMaxPwrCtrl.ucCentralChannel, TpcMaxPwrCtrl.ucChannelBand));

	SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

	MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&TpcMaxPwrCtrl,
			sizeof(CMD_POWER_TPC_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
		("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}
#endif /* TPC_SUPPORT */


INT32 MtCmdATEModeCtrl(
          RTMP_ADAPTER *pAd,
          UCHAR        ATEMode)
{
	struct cmd_msg *msg;
	CMD_ATE_MODE_CTRL_T ATEModeCtrl;
	INT32 ret = 0;
    struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: ATEMode = %d\n", __FUNCTION__, ATEMode));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_ATE_MODE_CTRL_T));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&ATEModeCtrl, sizeof(CMD_ATE_MODE_CTRL_T));

	ATEModeCtrl.ucPowerCtrlFormatId = ATEMODE_CTRL;
	ATEModeCtrl.ucATEModeCtrl  = ATEMode;

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_TX_POWER_FEATURE_CTRL);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&ATEModeCtrl,
            sizeof(CMD_ATE_MODE_CTRL_T));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

#ifdef PKT_BUDGET_CTRL_SUPPORT
INT32 MtCmdPktBudgetCtrl(struct _RTMP_ADAPTER *pAd,UINT8 bss_idx,UINT16 wcid,UCHAR type)
{
	struct cmd_msg *msg;
	INT32 Ret = 0;
	struct _CMD_PKT_BUDGET_CTRL_T  pbc;
	struct _CMD_PKT_BUDGET_CTRL_ENTRY_T *entry;
	struct _CMD_ATTRIBUTE attr = {0};
	UCHAR i;
	UINT32 size = sizeof(pbc);

	MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_OFF,
				(":%s: bssid(%d),wcid(%d),type(%d)\n", __FUNCTION__, bss_idx,wcid,type));

	if(type >= PBC_TYPE_END){
		MTWF_LOG(DBG_CAT_CFG, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
					(":%s: set wrong type (%d) for PBC!\n", __FUNCTION__,type));
		return Ret;
	}

	msg = MtAndesAllocCmdMsg(pAd, size);
	if (!msg)
	{
		Ret = NDIS_STATUS_RESOURCES;
		return Ret;
	}

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_PKT_BUDGET_CTRL_CFG);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET_AND_WAIT_RSP);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 8);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, EventExtCmdResult);


	MtAndesInitCmdMsg(msg, attr);
	os_zero_mem(&pbc, size);

	pbc.wlan_idx= cpu2le16(wcid);
	pbc.bss_id = bss_idx;
	pbc.queue_num = PBC_NUM_OF_PKT_BUDGET_CTRL_QUE;

	switch(type){
	case PBC_TYPE_NORMAL:
	{
		for(i=0;i<PBC_NUM_OF_PKT_BUDGET_CTRL_QUE;i++){
			entry = &pbc.aacQue[i];
			entry->lower_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
			entry->upper_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
		}
	}	
	break;
	case PBC_TYPE_WMM:
	{
		for(i=0;i<PBC_NUM_OF_PKT_BUDGET_CTRL_QUE;i++){
			entry = &pbc.aacQue[i];
			entry->lower_bound = cpu2le16(PBC_BOUNDARY_RESET_TO_DEFAULT);
			entry->upper_bound = cpu2le16(pAd->pbc_bound[i]);
		}

	}
	break;
	}

	MtAndesAppendCmdMsg(msg, (char *)&pbc,
									size);

	Ret = pAd->chipOps.MtCmdTx(pAd, msg);

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_INFO,
			("%s: (ret = %d)\n", __FUNCTION__, Ret));
	return Ret;

}
#endif /*PKT_BUDGET_CTRL_SUPPORT*/

INT32 MtCmdSetBWFEnable(RTMP_ADAPTER *pAd, UINT8 Enable)
{
    struct cmd_msg *msg;
    EXT_CMD_ID_BWF_LWC_ENABLE_T CmdBWFEnable;
    
    struct _CMD_ATTRIBUTE attr = {0};
    INT32 ret = 0;
    INT32 Len = 0;

    os_zero_mem(&CmdBWFEnable, sizeof(EXT_CMD_ID_BWF_LWC_ENABLE_T));
	/* send to N9 */
    msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ID_BWF_LWC_ENABLE_T));
    if (!msg)
    {
        ret = NDIS_STATUS_RESOURCES;
        goto error;
    }

    Len = sizeof(EXT_CMD_ID_BWF_LWC_ENABLE_T);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BWF_LWC_ENABLE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    CmdBWFEnable.ucBwfLwcEnable = Enable;    
	
	MtAndesAppendCmdMsg(msg, (char *)&CmdBWFEnable, sizeof(EXT_CMD_ID_BWF_LWC_ENABLE_T));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
	            ("%s: send cmd to N9 CmdBWFEnable.ucBwfLwcEnable [%d] Enable[%d]\n"
	            , __FUNCTION__,CmdBWFEnable.ucBwfLwcEnable,Enable));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);

	/* send the same msg to CR4 */	
	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ID_BWF_LWC_ENABLE_T));
		if (!msg)
		{
			ret = NDIS_STATUS_RESOURCES;
			goto error;
		}
    SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_BWF_LWC_ENABLE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
    CmdBWFEnable.ucBwfLwcEnable = Enable;    
	
	MtAndesAppendCmdMsg(msg, (char *)&CmdBWFEnable, sizeof(EXT_CMD_ID_BWF_LWC_ENABLE_T));

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_ERROR,
	            ("%s: send cmd to CR4 CmdBWFEnable.ucBwfLwcEnable [%d] Enable[%d]\n"
	            , __FUNCTION__,CmdBWFEnable.ucBwfLwcEnable,Enable));

    ret  = pAd->chipOps.MtCmdTx(pAd,msg);
error:

    MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_OFF,
            ("%s:(ret = %d)\n", __FUNCTION__, ret));

    return ret;
}
#if  defined(CONFIG_HOTSPOT_R2) || defined(DSCP_QOS_MAP_SUPPORT)
INT32 MtCmdHotspotInfoUpdate(RTMP_ADAPTER *pAd, EXT_CMD_ID_HOTSPOT_INFO_UPDATE_T InfoUpdateT)
{
	struct cmd_msg *msg;
	EXT_CMD_ID_HOTSPOT_INFO_UPDATE_T CmdHotspotInfoUpdate;	
	struct _CMD_ATTRIBUTE attr = {0};
	INT32 ret = 0;
	INT32 Len = 0;

	os_zero_mem(&CmdHotspotInfoUpdate, sizeof(EXT_CMD_ID_HOTSPOT_INFO_UPDATE_T));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(EXT_CMD_ID_HOTSPOT_INFO_UPDATE_T));
	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}
	
	Len = sizeof(EXT_CMD_ID_HOTSPOT_INFO_UPDATE_T);
	os_move_mem(&CmdHotspotInfoUpdate, &InfoUpdateT, Len);

	SET_CMD_ATTR_MCU_DEST(attr, HOST2CR4);
	SET_CMD_ATTR_TYPE(attr, EXT_CID);
	SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_HOTSPOT_INFO_UPDATE);
	SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
	SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
	SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, 0);
	SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
	SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

	MtAndesInitCmdMsg(msg, attr);
	
	MtAndesAppendCmdMsg(msg, (char *)&CmdHotspotInfoUpdate, sizeof(EXT_CMD_ID_HOTSPOT_INFO_UPDATE_T));

	//MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,("%s: send to CR4 \n", __FUNCTION__));

	ret  = pAd->chipOps.MtCmdTx(pAd,msg);
	
error:

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
			("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;
}

#endif /* CONFIG_HOTSPOT_R2 */


#ifdef RACTRL_LIMIT_MAX_PHY_RATE
/*****************************************
    ExT_CID = 0x74
*****************************************/
INT32 MtCmdSetMaxPhyRate(RTMP_ADAPTER *pAd, UINT16 u2MaxPhyRate)
{
	struct cmd_msg *msg;
    CMD_SET_MAX_PHY_RATA CmdSetMaxPhyRate;
	INT32 ret = 0;
	struct _CMD_ATTRIBUTE attr = {0};

	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s: Max Phy rate = %d\n", __FUNCTION__, u2MaxPhyRate));

	msg = MtAndesAllocCmdMsg(pAd, sizeof(CMD_SET_MAX_PHY_RATA));

	if (!msg)
	{
		ret = NDIS_STATUS_RESOURCES;
		goto error;
	}

	os_zero_mem(&CmdSetMaxPhyRate, sizeof(CMD_SET_MAX_PHY_RATA));
    CmdSetMaxPhyRate.u2MaxPhyRate = cpu2le16(u2MaxPhyRate);

    SET_CMD_ATTR_MCU_DEST(attr, HOST2N9);
    SET_CMD_ATTR_TYPE(attr, EXT_CID);
    SET_CMD_ATTR_EXT_TYPE(attr, EXT_CMD_ID_SET_MAX_PHY_RATE);
    SET_CMD_ATTR_CTRL_FLAGS(attr, INIT_CMD_SET);
    SET_CMD_ATTR_RSP_WAIT_MS_TIME(attr, 0);
    SET_CMD_ATTR_RSP_EXPECT_SIZE(attr, MT_IGNORE_PAYLOAD_LEN_CHECK);
    SET_CMD_ATTR_RSP_WB_BUF_IN_CALBK(attr, NULL);
    SET_CMD_ATTR_RSP_HANDLER(attr, NULL);

    MtAndesInitCmdMsg(msg, attr);
	MtAndesAppendCmdMsg(msg, (char *)&CmdSetMaxPhyRate,
            sizeof(CMD_SET_MAX_PHY_RATA));

	ret = pAd->chipOps.MtCmdTx(pAd, msg);

error:
	MTWF_LOG(DBG_CAT_FW, DBG_SUBCAT_ALL, DBG_LVL_TRACE,
        ("%s:(ret = %d)\n", __FUNCTION__, ret));

	return ret;

}
#endif /* RACTRL_LIMIT_MAX_PHY_RATE */

