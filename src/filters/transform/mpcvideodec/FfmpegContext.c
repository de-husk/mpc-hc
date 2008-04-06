/* 
 * $Id: FfmpegContext.c 249 2007-09-26 11:07:22Z casimir666 $
 *
 * (C) 2006-2007 see AUTHORS
 *
 * This file is part of mplayerc.
 *
 * Mplayerc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mplayerc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#define HAVE_AV_CONFIG_H
#define CONFIG_H264_PARSER
#define CONFIG_VORBIS_DECODER
#define CONFIG_H264_DECODER
#define H264_MERGE_TESTING

#include <windows.h>
#include <winnt.h>
#include "FfmpegContext.h"
#include "dsputil.h"
#include "avcodec.h"
#include "mpegvideo.h"
#include "golomb.h"

#include "h264.h"
#include "h264data.h"
#include "vc1.h"


int av_h264_decode_slice_header (struct AVCodecContext* pAVCtx, BYTE* pBuffer, UINT nSize, int* sp_for_switch_flag);
int av_h264_decode_frame(struct AVCodecContext* avctx, uint8_t *buf, int buf_size);
int av_vc1_decode_frame(AVCodecContext *avctx, uint8_t *buf, int buf_size);


char* GetFFMpegPictureType(int nType)
{
	static char*	s_FFMpegPictTypes[] = { "? ", "I ", "P ", "B ", "S ", "SI", "SP" };
	int		nTypeCount = sizeof(s_FFMpegPictTypes)/sizeof(TCHAR)-1;

	return s_FFMpegPictTypes[min(nType, nTypeCount)];
}


void FFH264DecodeBuffer (struct AVCodecContext* pAVCtx, BYTE* pBuffer, UINT nSize)
{
	if (pBuffer != NULL)
		av_h264_decode_frame (pAVCtx, pBuffer, nSize);
}


int FFH264CheckCompatibility(int nWidth, int nHeight, struct AVCodecContext* pAVCtx, BYTE* pBuffer, UINT nSize)
{
	H264Context*	pContext	= (H264Context*) pAVCtx->priv_data;
	SPS*			cur_sps;
	PPS*			cur_pps;

	if (pBuffer != NULL)
		av_h264_decode_frame (pAVCtx, pBuffer, nSize);

	cur_sps		= pContext->sps_buffers[0];
	cur_pps		= pContext->pps_buffers[0];

	if (cur_sps != NULL)
	{
		// Check aspect ratio
		if (   (cur_sps->sar.num ==  0) ||	// Weird but happen...
			  ((cur_sps->sar.num ==  1) && (cur_sps->sar.den == 1)) ||
			  ((cur_sps->sar.num ==  4) && (cur_sps->sar.den == 3)) ||
			  ((cur_sps->sar.num ==  5) && (cur_sps->sar.den == 4)) ||
			  ((cur_sps->sar.num == 16) && (cur_sps->sar.den == 9)) )
		{
			// Check max num reference frame according to resolution
			if ( ((nWidth > 720)  && (nWidth*nHeight*cur_sps->ref_frame_count > 1920*1088*4)) ||	// HD limit
				 ((nWidth <= 720) && (nWidth*nHeight*cur_sps->ref_frame_count >  720*576*11)) )		// SD limit
				return 2;	// Too much ref frames
		}
		else
			return 1;	// Wrong SAR
	}

	return 0;
}


HRESULT FFH264ReadSlideHeader (DXVA_PicParams_H264* pDXVAPicParams, DXVA_Qmatrix_H264* pDXVAScalingMatrix, struct AVCodecContext* pAVCtx, BYTE* pBuffer, UINT nSize)
{
	H264Context*			h			= (H264Context*) pAVCtx->priv_data;
	SPS*					cur_sps;
	PPS*					cur_pps;
    MpegEncContext* const	s = &h->s;
	int						sp_for_switch_flag = 0;
	int						field_pic_flag;
	HRESULT					hr = E_FAIL;

	av_h264_decode_slice_header (pAVCtx, pBuffer, nSize, &sp_for_switch_flag);

	field_pic_flag = (h->s.picture_structure != PICT_FRAME);

	cur_sps	= &h->sps;
	cur_pps = &h->pps;

	if (cur_sps && cur_pps)
	{
		pDXVAPicParams->wFrameWidthInMbsMinus1			= cur_sps->mb_width  - 1;		// pic_width_in_mbs_minus1;
		pDXVAPicParams->wFrameHeightInMbsMinus1			= cur_sps->mb_height * (2 - cur_sps->frame_mbs_only_flag) - 1;		// pic_height_in_map_units_minus1;
		pDXVAPicParams->num_ref_frames					= cur_sps->ref_frame_count;		// num_ref_frames;
		pDXVAPicParams->field_pic_flag					= field_pic_flag;
		pDXVAPicParams->MbaffFrameFlag					= (h->sps.mb_aff && (field_pic_flag==0));
		pDXVAPicParams->residual_colour_transform_flag	= cur_sps->residual_colour_transform_flag;
		pDXVAPicParams->sp_for_switch_flag				= sp_for_switch_flag;
		pDXVAPicParams->chroma_format_idc				= cur_sps->chroma_format_idc;
		pDXVAPicParams->RefPicFlag						= (h->nal_ref_idc != 0);
		pDXVAPicParams->constrained_intra_pred_flag		= cur_pps->constrained_intra_pred;
		pDXVAPicParams->weighted_pred_flag				= cur_pps->weighted_pred;
		pDXVAPicParams->weighted_bipred_idc				= cur_pps->weighted_bipred_idc;
		pDXVAPicParams->frame_mbs_only_flag				= cur_sps->frame_mbs_only_flag;
		pDXVAPicParams->transform_8x8_mode_flag			= cur_pps->transform_8x8_mode;
		pDXVAPicParams->IntraPicFlag					= (h->slice_type == FF_I_TYPE );

		pDXVAPicParams->bit_depth_luma_minus8			= cur_sps->bit_depth_luma   - 8;	// bit_depth_luma_minus8
		pDXVAPicParams->bit_depth_chroma_minus8			= cur_sps->bit_depth_chroma - 8;	// bit_depth_chroma_minus8
	//	pDXVAPicParams->StatusReportFeedbackNumber		= SET IN DecodeFrame;


	//	pDXVAPicParams->CurrFieldOrderCnt						= SET IN UpdateRefFramesList;
	//	pDXVAPicParams->FieldOrderCntList						= SET IN UpdateRefFramesList;
	//	pDXVAPicParams->FrameNumList							= SET IN UpdateRefFramesList;
	//	pDXVAPicParams->UsedForReferenceFlags					= SET IN UpdateRefFramesList;
	//	pDXVAPicParams->NonExistingFrameFlags
		pDXVAPicParams->frame_num						= h->frame_num;
	//	pDXVAPicParams->SliceGroupMap


		pDXVAPicParams->log2_max_frame_num_minus4				= cur_sps->log2_max_frame_num - 4;					// log2_max_frame_num_minus4;
		pDXVAPicParams->pic_order_cnt_type						= cur_sps->poc_type;								// pic_order_cnt_type;
		pDXVAPicParams->log2_max_pic_order_cnt_lsb_minus4		= cur_sps->log2_max_poc_lsb - 4;					// log2_max_pic_order_cnt_lsb_minus4;
		pDXVAPicParams->delta_pic_order_always_zero_flag		= cur_sps->delta_pic_order_always_zero_flag;
		pDXVAPicParams->direct_8x8_inference_flag				= cur_sps->direct_8x8_inference_flag;
		pDXVAPicParams->entropy_coding_mode_flag				= cur_pps->cabac;									// entropy_coding_mode_flag;
		pDXVAPicParams->pic_order_present_flag					= cur_pps->pic_order_present;						// pic_order_present_flag;
		pDXVAPicParams->num_slice_groups_minus1					= cur_pps->slice_group_count - 1;					// num_slice_groups_minus1;
		pDXVAPicParams->slice_group_map_type					= cur_pps->mb_slice_group_map_type;					// slice_group_map_type;
		pDXVAPicParams->deblocking_filter_control_present_flag	= cur_pps->deblocking_filter_parameters_present;	// deblocking_filter_control_present_flag;
		pDXVAPicParams->redundant_pic_cnt_present_flag			= cur_pps->redundant_pic_cnt_present;				// redundant_pic_cnt_present_flag;
		pDXVAPicParams->slice_group_change_rate_minus1			= cur_pps->slice_group_change_rate_minus1;

		pDXVAPicParams->chroma_qp_index_offset					= cur_pps->chroma_qp_index_offset[0];
		pDXVAPicParams->second_chroma_qp_index_offset			= cur_pps->chroma_qp_index_offset[1];
		pDXVAPicParams->num_ref_idx_l0_active_minus1			= cur_pps->ref_count[0]-1;							// num_ref_idx_l0_active_minus1;
		pDXVAPicParams->num_ref_idx_l1_active_minus1			= cur_pps->ref_count[1]-1;							// num_ref_idx_l1_active_minus1;
		pDXVAPicParams->pic_init_qp_minus26						= cur_pps->init_qp - 26;
		pDXVAPicParams->pic_init_qs_minus26						= cur_pps->init_qs - 26;

		//if (field_pic_flag)
		//	pDXVAPicParams->CurrPic.AssociatedFlag	= h->delta_poc[0];

		pDXVAPicParams->CurrFieldOrderCnt[0] = h->poc_lsb;		// m_Slice.pic_order_cnt_lsb;
		pDXVAPicParams->CurrFieldOrderCnt[1] = h->poc_lsb;		// m_Slice.pic_order_cnt_lsb;

		memcpy (pDXVAScalingMatrix, cur_pps->scaling_matrix4, sizeof (DXVA_Qmatrix_H264));
		hr = S_OK;
	}

	return hr;
}



int FFVC1UpdatePictureParam (DXVA_PictureParameters* pPicParams, struct AVCodecContext* pAVCtx, BYTE* pBuffer, UINT nSize)
{
	int				refpic;
	VC1Context*		vc1 = (VC1Context*) pAVCtx->priv_data;

	if (pBuffer)
	{
		av_vc1_decode_frame (pAVCtx, pBuffer, nSize);
	}

	refpic = ((vc1->s.pict_type != FF_B_TYPE) || (vc1->s.pict_type != FF_BI_TYPE));

	pPicParams->bPicIntra				= (vc1->s.pict_type == FF_I_TYPE);
	pPicParams->bPicBackwardPrediction	= (vc1->s.pict_type == FF_B_TYPE);

	//   Ok     Todo    Todo    Todo    Ok
	// iWMV9 - i9IRU - iOHIT - iINSO - iWMVA		| Section 3.2.5
	pPicParams->bBidirectionalAveragingMode	= ((vc1->profile == PROFILE_ADVANCED)<<3) | (1 << 7);

	// Section 3.2.20.3
	pPicParams->bPicSpatialResid8	= (vc1->panscanflag   << 7) | (vc1->refdist  << 6) |
									  (vc1->s.loop_filter << 5) | (vc1->fastuvmc << 4) | 
									  (vc1->extended_mv   << 3) | (vc1->dquant   << 1) | 
									  (vc1->vstransform);

	// Section 3.2.20.4
	pPicParams->bPicOverflowBlocks  = (vc1->quantizer_mode  << 6) | (vc1->multires << 5) |
									  (vc1->s.resync_marker << 4) | (vc1->rangered << 3) |
									  (vc1->s.max_b_frames);

	// Section 3.2.20.2
	pPicParams->bPicDeblockConfined	= (vc1->postprocflag << 7) | (vc1->broadcast  << 6) |
									  (vc1->interlace    << 5) | (vc1->tfcntrflag << 4) | 
									  (vc1->finterpflag  << 3) | (refpic		  << 2) |
									  (vc1->psf << 1)		   | vc1->extended_dmv;


	//				TODO section 3.2.20.6
	//pPicParams->bPicDeblocked		= 
	//pPicParams->bPicDeblockConfined
	//pPicParams->wBitstreamFcodes

	return vc1->s.pict_type;
}
