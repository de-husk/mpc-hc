/*
 * $Id$
 *
 * (C) 2006-2010 see AUTHORS
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

#pragma once

#include "AllocatorCommon.h"
#include "RenderersSettings.h"
#include <d3d9.h>
#include <d3dx9.h>
#include "../SubPic/SubPicAllocatorPresenterImpl.h"


namespace DSObjects
{

class CDX9RenderingEngine
	: public CSubPicAllocatorPresenterImpl
{
protected:
	enum RenderingPath
	{
		RENDERING_PATH_STRETCHRECT,
		RENDERING_PATH_DRAW,
	};

	static const int MAX_VIDEO_SURFACES = 60;

	// Variables initialized/managed by the allocator-presenter!
	CComPtr<IDirect3DDevice9>	m_pD3DDev;
	D3DCAPS9					m_Caps;
	D3DFORMAT					m_BackbufferType;
	D3DFORMAT					m_DisplayType;
	CSize						m_ScreenSize;
	int							m_nNbDXSurface;					// Total number of DX Surfaces
	int							m_nCurSurface;					// Surface currently displayed

	bool						m_bHighColorResolution;
	bool                        m_bForceInputHighColorResolution;
	bool                        m_bFullFloatingPointProcessing;

	// Variables initialized/managed by this class but can be accessed by the allocator-presenter
	bool						m_bD3DX;
	RenderingPath				m_RenderingPath;
	D3DFORMAT					m_SurfaceType;
	CComPtr<IDirect3DTexture9>	m_pVideoTexture[MAX_VIDEO_SURFACES];
	CComPtr<IDirect3DSurface9>	m_pVideoSurface[MAX_VIDEO_SURFACES];


	CDX9RenderingEngine(HWND hWnd, HRESULT& hr, CString *_pError);

	void InitRenderingEngine();
	void CleanupRenderingEngine();

	HRESULT CreateVideoSurfaces(D3DFORMAT format);
	void FreeVideoSurfaces();

	HRESULT RenderVideo(IDirect3DSurface9* pRenderTarget, const CRect& srcRect, const CRect& destRect, CRenderersSettings& settings);

	HRESULT DrawRect(DWORD _Color, DWORD _Alpha, const CRect &_Rect);
	HRESULT	AlphaBlt(RECT* pSrc, RECT* pDst, IDirect3DTexture9* pTexture);

	HRESULT SetCustomPixelShader(LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace);


private:
	class CExternalPixelShader
	{
	public:
		CComPtr<IDirect3DPixelShader9> m_pPixelShader;
		CStringA m_SourceData;
		CStringA m_SourceTarget;
		HRESULT Compile(CPixelShaderCompiler *pCompiler)
		{
			HRESULT hr = pCompiler->CompileShader(m_SourceData, "main", m_SourceTarget, 0, &m_pPixelShader);
			if(FAILED(hr))
				return hr;

			return S_OK;
		}
	};

	// D3DX functions
	typedef D3DXFLOAT16* (WINAPI* D3DXFloat32To16ArrayPtr)(
		D3DXFLOAT16 *pOut,
		CONST FLOAT *pIn,
		UINT		 n);


	CAutoPtr<CPixelShaderCompiler>	 m_pPSC;

	// Settings
	bool                             m_bColorManagement;
	VideoSystem                      m_InputVideoSystem;
	GammaCurve                       m_Gamma;
	ColorRenderingIntent             m_RenderingIntent;

	// Custom pixel shaders
	CAtlList<CExternalPixelShader>	 m_pCustomPixelShaders;
	CComPtr<IDirect3DTexture9>		 m_pTemporaryVideoTextures[2];

	// Screen space pipeline
	int                              m_ScreenSpacePassCount;
	int                              m_ScreenSpacePassSrc;
	int                              m_ScreenSpacePassDest;
	CSize                            m_TemporaryScreenSpaceTextureSize;
	CComPtr<IDirect3DTexture9>		 m_pTemporaryScreenSpaceTextures[2];
	IDirect3DSurface9*               m_pRenderTarget;

	// Resizers
	float							 m_BicubicA;
	CComPtr<IDirect3DPixelShader9>	 m_pResizerPixelShaders[4]; // bl, bc1, bc2_1, bc2_2

	// Final pass
	bool                             m_bFinalPass;
	int                              m_Lut3DSize;
	int								 m_Lut3DEntryCount;
	CComPtr<IDirect3DVolumeTexture9> m_pLut3DTexture;
	CComPtr<IDirect3DTexture9>       m_pDitherTexture;
	CComPtr<IDirect3DPixelShader9>   m_pFinalPixelShader;

	// Custom screen space pixel shaders
	CAtlList<CExternalPixelShader>	 m_pCustomScreenSpacePixelShaders;

	// StetchRect rendering path
	D3DTEXTUREFILTERTYPE			 m_StretchRectFilter;

	// D3DX function pointers
	D3DXFloat32To16ArrayPtr          m_pD3DXFloat32To16Array;


	// Video rendering paths
	HRESULT RenderVideoDrawPath(IDirect3DSurface9* pRenderTarget, const CRect& srcRect, const CRect& destRect, CRenderersSettings& settings);
	HRESULT RenderVideoStretchRectPath(IDirect3DSurface9* pRenderTarget, const CRect& srcRect, const CRect& destRect, CRenderersSettings& settings);

	// Custom pixel shaders
	HRESULT InitTemporaryVideoTextures(int count);

	// Screen space pipeline
	HRESULT InitScreenSpacePipeline(int passCount, IDirect3DSurface9* pRenderTarget);
	HRESULT InitTemporaryScreenSpaceTextures(int count);
	HRESULT BeginScreenSpacePass();

	// Resizers
	HRESULT InitResizers(float bicubicA);
	HRESULT TextureResize(IDirect3DTexture9* pTexture, Vector dst[4], D3DTEXTUREFILTERTYPE filter, const CRect &srcRect);
	HRESULT TextureResizeBilinear(IDirect3DTexture9* pTexture, Vector dst[4], const CRect &srcRect);
	HRESULT TextureResizeBicubic1pass(IDirect3DTexture9* pTexture, Vector dst[4], const CRect &srcRect);
	//HRESULT TextureResizeBicubic2pass(IDirect3DTexture9* pTexture, Vector dst[4], const CRect &srcRect);

	// Final pass
	HRESULT InitFinalPass(CRenderersSettings& settings);
	void CleanupFinalPass();
	HRESULT CreateIccProfileLut(TCHAR* profilePath, float* lut3D);
	HRESULT FinalPass(IDirect3DTexture9* pTexture);

	HRESULT TextureCopy(IDirect3DTexture9* pTexture);
	bool ClipToSurface(IDirect3DSurface9* pSurface, CRect& s, CRect& d);
};

}